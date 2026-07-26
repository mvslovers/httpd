# httpd integration tests

Runnable checks against a live HTTPD address space on MVS. Unlike the
`test/*.c` unit tests (built with mbtcheck), these drive the server over the
wire and inspect the MVS console.

## `shutdown-acceptance.sh` — acceptance test for #122

Acceptance criterion for [#122](https://github.com/mvslovers/httpd/issues/122)
(epic [mvsmf#177](https://github.com/mvslovers/mvsmf/issues/177), Workstream D):
after a request handler faults, `P HTTPD` must terminate the address space
cleanly — **no `S33E`**, **no repeated `__CRTGET CRT for TCB(...) was not found
in PPA(...)`**, **no SVC dump**, and the address space must **end normally**
(no `IEF450I ... ABEND`, no `$HASP310 ... TERMINATED AT END OF MEMORY`). The
`$HASP395 ... ENDED` / `IEF404I` end records are used only to confirm the end
was *captured* in the window — they appear on clean and abnormal ends alike and
do not themselves prove health.

The code fix lives in libc370 (worker DETACH gating + recovery-exit hardening).
This test observes the symptom in httpd's own address space.

### What it does

1. Records the current byte offset of `CONSOLE_LOG` (the assertion window
   starts here — see "Windowed assertion" below).
2. Provokes the pre-stop state under test (`PROVOKE`, see "Provocation modes"):
   either faulting handlers (`abend`) or workers parked in an in-flight long
   poll (`longpoll`, the `#179` / `SA03` path).
3. Issues `P HTTPD` (see adapters below).
4. Waits up to `WAIT_SECS` for the listener to stop accepting.
5. Asserts over the console-log slice appended during this run.

### Provocation modes (`PROVOKE`)

Which shutdown path the run exercises:

- **`abend`** (default) — `ABEND_HITS` requests to `ABEND_PATH` (default
  `/abend`, whose handler executes `DC H'0'` → S0C1). Handlers fault *fast*, so
  this drives libc370's **recovery-drain** path. Proof-of-scenario is a fault
  marker (`HTTPD062E` / `MVSMF99E`) in the window.
- **`longpoll`** — launches `LONGPOLL_N` (default 4) concurrent, blocking
  long-poll requests (`LONGPOLL_CMD`) and checks that at least one is *still in
  flight* `LONGPOLL_SETTLE` s later, then issues `P HTTPD` while they are parked.
  This is the only mode that reproduces `#179` / the `SA03`: a worker parked in a
  long poll cannot drain inside libc370's ~5 s window. Proof-of-scenario is the
  in-flight count — **not** a fault marker (a clean parked poll faults nothing).

> A green **`abend`** run does **not** establish the `#179` fix — it tests a
> different path. Use **`longpoll`** to gate `#179` (mvslovers/mvsmf#179).

The easiest way to drive this is the bundled runner
(`tests/run-shutdown-longpoll.sh`) — a file, so there is no fragile multi-line
shell command to mis-paste:

```sh
CONSOLE_LOG=/path/to/hardcopy.log sh tests/run-shutdown-longpoll.sh check   # setup check, no P HTTPD
CONSOLE_LOG=/path/to/hardcopy.log sh tests/run-shutdown-longpoll.sh         # real run, prompts for P HTTPD
```

It reads API credentials from `HTTPD_AUTH` or from `MBT_MVS_USER`/`MBT_MVS_PASS`
in `./.env`, and uses a non-matching `unsol-key` so the workers stay parked; see
the script header for all knobs. To build the poll request by hand instead:

Example `LONGPOLL_CMD` — one synchronous unsolicited-message detection PUT that
blocks up to 60 s (mvsMF console **issue-command** endpoint,
`PUT /zosmf/restconsoles/consoles/{console-name}` — *not* the `/solmsgs` collect
sub-resource):

```sh
LONGPOLL_CMD='curl -s -o /dev/null -u USER:PASS -X PUT \
  -H "Content-Type: application/json" \
  -d "{\"cmd\":\"D T\",\"unsol-key\":\"ZZNOMATCHZZ\",\"unsol-detect-sync\":\"Y\",\"unsol-detect-timeout\":\"60\"}" \
  http://HOST:PORT/zosmf/restconsoles/consoles/defcn'
```

> `unsol-key` must be a token that will **not** appear during the poll — do
> **not** use one the command's own response emits (`D T` emits `IEE136I`), or
> detection fires immediately and the worker never parks (→ INCONCLUSIVE).

The long poll must **still be running when `P HTTPD` is issued** — that is the
whole point. The in-flight count is measured `LONGPOLL_SETTLE` s after launch,
but the stop lands later. In `STOP_ADAPTER=cmd` mode the stop follows
immediately, so a `unsol-detect-timeout` of ~60 s is ample. In `manual` mode the
operator's reaction time is unbounded: set `unsol-detect-timeout` comfortably
above how long you expect to take typing `P HTTPD`, or the polls will have
already returned (no worker parked → INCONCLUSIVE, not a real reproduction).

### Result

- **FAIL** — a fault marker is present *and* either a crash signature (`S33E` /
  `__CRTGET` spam / SVC dump) **or** an abnormal address-space end (`IEF450I ...
  ABEND`, `$HASP310 ... TERMINATED AT END OF MEMORY`) appears in the window.
  After the 2026-07-23 recovery-exit relink the residual symptom is the abnormal
  end (`SA03`), so this class matters even when no crash spam appears.
- **PASS** — a handler faulted, no crash signature and no abnormal end appeared
  (`IEF450I` / `$HASP310` / SVC dump all absent), and the address-space end was
  captured in the window (`$HASP395 ... ENDED` or `IEF404I`). A PASS on a build
  that still carries the libc370 defect is suspicious — the banner says so.
  `HTTPD002I` / `HTTPD060I` worker-shutdown WTOs corroborate a clean drain but do
  **not** gate the result; a PASS with none of them present emits a CAUTION.
- **INCONCLUSIVE** — either the intended scenario didn't run (in `abend`, no
  fault marker `HTTPD062E` / `MVSMF99E`; in `longpoll`, no long-poll still in
  flight at `P HTTPD`), or no crash/abnormal signature yet the address-space end
  was not captured (`$HASP395 ... ENDED` / `IEF404I` both absent — a truncated
  log looks the same as a clean end). Fix the provocation or extend the capture
  and re-run.

Exit codes: `0` pass, `1` fail, `2` config error, `3` inconclusive.

### RAKF counts (diagnostic only)

Because the test already drives `P HTTPD`, it reports two RAKF counts over the
same window for free:

| Reported | Meaning |
|----------|---------|
| `RAKF0005 in window` | any resource-access violation — context only, and often unrelated on a shared system |
| `RAKF HTTPX/FACILITY` | the specific signature from [#27](https://github.com/mvslovers/httpd/issues/27) — a `RAKF000A` line naming `HTTPX` in `FACILITY` |

Neither **ever** affects the verdict. A nonzero HTTPX/FACILITY count on an
otherwise-passing run emits a CAUTION and nothing more.

This is deliberate. `RAKF0004` (failed logon) is routine in this window and
unrelated; `RAKF0005` can be raised by any resource on a shared system; and #27
was retired as stale because no current build reproduced it. Turning any of this
into a FAIL condition would misfire and mask real results. The CAUTION exists so
that if the #27 signature ever does reappear, it is captured on a current build
rather than re-argued from the 2026-03-18 log.

### Windowed assertion (important)

`CONSOLE_LOG` is usually an **append-only** Hercules hardcopy log that already
contains `S33E` / `__CRTGET` lines from earlier real crashes. The script
records the log's byte offset *before* provoking and asserts only over bytes
appended afterwards. Without this the test would report FAIL forever — even
after the fix. Do not remove it.

### Fault marker

Proof that a handler faulted is `HTTPD062E` **or** `MVSMF99E`:

- `HTTPD062E` — a **core** abend caught by the worker's `try(serve_client)`
  (`src/httpd.c`). This is what `/abend` produces.
- `MVSMF99E` — an **mvsMF CGI** abend caught by mvsMF's inner ESTAE; the
  worker `try()` then returns 0, so `HTTPD062E` does not fire.

Accepting either keeps the test valid whether `ABEND_PATH` is a core path or a
CGI endpoint.

### Issuing `P HTTPD` and capturing the console log (adapters)

The repo has no console/SYSLOG tooling, so both are pluggable.

**`STOP_ADAPTER=manual` (default).** The script pauses; the operator issues
`P HTTPD` at the MVS console and presses Enter. `CONSOLE_LOG` points at a
captured console/hardcopy log (e.g. the Hercules hardcopy log file, or a
SYSLOG extract) that covers the shutdown window. Most portable across
TK4-/TK5/MVSCE/Hercules.

**`STOP_ADAPTER=cmd`.** Set `STOP_CMD` to a command that issues `P HTTPD`
unattended — for local Hercules, a pipe to the console port; or a one-shot
operator-command REST call. For CI. Note the self-stop race: do **not** issue
`P HTTPD` through an mvsMF console endpoint served *by the httpd under test* —
that address space is terminating, so a follow-up read of live SYSLOG is
unreliable (see libc370#4). Capture `CONSOLE_LOG` out of band.

### Configuration

Environment variables (or a sourced `.env`):

| Variable | Default | Meaning |
|---|---|---|
| `HTTPD_HOST` | `MBT_MVS_HOST` or `127.0.0.1` | host serving the HTTPD listener |
| `HTTPD_PORT` | `8080` | HTTPD listener port |
| `ABEND_PATH` | `/abend` | path whose handler faults |
| `ABEND_HITS` | `12` | number of provocation requests |
| `FAULT_MARK` | `HTTPD062E\|MVSMF99E` | proof-of-fault regex |
| `HTTPD_AUTH` | (unset) | `user:pass` if `LOGIN` gates GET |
| `WAIT_SECS` | `120` | max wait for the AS to end |
| `JOBNAME` | `HTTPD` | MVS jobname of the HTTPD STC (scopes the end-capture assertion) |
| `PROVOKE` | `abend` | pre-stop scenario: `abend` (fault handlers) or `longpoll` (park workers) |
| `LONGPOLL_CMD` | (required for `longpoll`) | one blocking long-poll request, run `LONGPOLL_N`× concurrently |
| `LONGPOLL_N` | `4` | `longpoll`: concurrent in-flight long-polls |
| `LONGPOLL_SETTLE` | `3` | `longpoll`: seconds to let them park before `P HTTPD` |
| `STOP_ADAPTER` | `manual` | `manual` or `cmd` |
| `STOP_CMD` | (unset) | command to issue `P HTTPD` when `cmd` |
| `CONSOLE_LOG` | (required) | captured console/hardcopy log to assert over |

### Example

```sh
CONSOLE_LOG=/var/hercules/mvs/hardcopy.log \
HTTPD_HOST=mvs HTTPD_PORT=8080 \
sh tests/shutdown-acceptance.sh
```
