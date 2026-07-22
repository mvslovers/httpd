# httpd integration tests

Runnable checks against a live HTTPD address space on MVS. Unlike the
`test/*.c` unit tests (built with mbtcheck), these drive the server over the
wire and inspect the MVS console.

## `shutdown-acceptance.sh` — acceptance test for #122

Acceptance criterion for [#122](https://github.com/mvslovers/httpd/issues/122)
(epic [mvsmf#177](https://github.com/mvslovers/mvsmf/issues/177), Workstream D):
after a request handler faults, `P HTTPD` must terminate the address space
cleanly — **no `S33E`**, **no repeated `__CRTGET CRT for TCB(...) was not found
in PPA(...)`**, **no SVC dump**.

The code fix lives in libc370 (worker DETACH gating + recovery-exit hardening).
This test observes the symptom in httpd's own address space.

### What it does

1. Records the current byte offset of `CONSOLE_LOG` (the assertion window
   starts here — see "Windowed assertion" below).
2. Provokes a faulting handler: `ABEND_HITS` requests to `ABEND_PATH`
   (default `/abend`, whose handler executes `DC H'0'` → S0C1 inside the
   worker request pipeline). Repeated hits poison the whole worker pool.
3. Issues `P HTTPD` (see adapters below).
4. Waits up to `WAIT_SECS` for the listener to stop accepting.
5. Asserts over the console-log slice appended during this run.

### Result

- **FAIL** — a fault marker is present *and* any of `S33E` / `__CRTGET` spam /
  SVC dump appears. This is the #122 bug reproduced and is the **expected
  result on the current build**.
- **PASS** — a handler faulted but shutdown was clean. The post-fix target.
  A PASS on a build that still carries the libc370 defect is suspicious — the
  banner says so.
- **INCONCLUSIVE** — no fault marker (`HTTPD062E` / `MVSMF99E`) in the window,
  so no handler actually faulted. The test did not exercise #122; fix the
  provocation and re-run.

Exit codes: `0` pass, `1` fail, `2` config error, `3` inconclusive.

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
| `STOP_ADAPTER` | `manual` | `manual` or `cmd` |
| `STOP_CMD` | (unset) | command to issue `P HTTPD` when `cmd` |
| `CONSOLE_LOG` | (required) | captured console/hardcopy log to assert over |

### Example

```sh
CONSOLE_LOG=/var/hercules/mvs/hardcopy.log \
HTTPD_HOST=mvs HTTPD_PORT=8080 \
sh tests/shutdown-acceptance.sh
```
