# HTTPD — Open Work, Ranked

**State lives on GitHub, not here.** `gh issue list --repo mvslovers/httpd` is
the source of truth for what is open, closed or newly filed. What this file adds
is the part the tracker cannot hold: the **order**, the reason for it, and which
items wait on a decision rather than on code.

**It carries nothing that is copied.** Where a chain of reasoning already has an
owner — the issue thread, the PR, a design document — this file points at it and
stops. `CLAUDE.md` forbids a task list in itself because a copy of a tracker is
wrong the first time someone closes something, and the only defence that works
is to hold nothing worth going stale.

*Last reconciled against the tracker: 2026-08-23, four issues open (#233 closed
by PR #244, #242 by PR #246 and #243 by PR #247; #245 filed out of #233).*

---

## The order

| | Issue | Kind | Waiting on |
|---|---|---|---|
| 1 | #245 | `type:bug` | nothing |
| 2 | #237 | `type:bug` | nothing |
| 3 | #198 | hygiene, explicitly not a bug | nothing |
| — | #176 | security, the heaviest by a wide margin | **RAKF** — see *Deferred* |

**Nothing open waits on a decision any more.** #243 was the one that did, and
it is settled — the three remaining items are code, and #176 is parked on
another organisation.

---

### 1 · #245 — `HTTPD090E` refuses the start and the step ends `CC 0000`

*#226, finished*

`main()` reaches the console check only when the APF setup returned zero, and
nothing between the two writes `rc` — so `HTTPD090E UNABLE TO INITIALIZE
CONSOLE INTERFACE`, which is fatal by design, returns 0. A restart rule keyed
on COND CODE reads a clean stop and leaves the system without an HTTP server.

#226 gave `initialize()` a return value and never reached `main()`'s own early
exits. This is the last one that is silent: the DD checks in `httpstrt.c` end
`EXIT_FAILURE`, `HTTPD012E` carries `__autask()`'s rc, and `HTTPD033E` sets
`initrc = 8`. Ranked here because it is a two-line fix with an operational
consequence, not because it is likely.

---

### 2 · #237 — `httpclos()` releases the lock `process_clients()` is holding

*latent, cheap, and the reading behind it is already done in #235*

`httpclos()` brackets its walk with an unconditional `lock(httpd,0)` /
`unlock(httpd,0)`. ENQ ownership is per TCB and not counted (libc370 `@@lk.c`),
so when `process_clients()` calls `http_close()` while holding that lock, the
inner `lock()` gets rc 8 and the inner DEQ releases the *outer* holder's ENQ.

Nothing breaks today — only a `break` follows the `http_close()` — and it is the
one place in the server that does not use the `if (lockrc==0) unlock(...)` form
every other call site uses. Fix it for the next reader, not for a live symptom.

The issue carries a second, unconfirmed observation for the same change:
`process_clients()` appears to call `http_process_clients()` twice per pass
(`httpd.c:684` and `:691`).

---

### 3 · #198 — Pre-allocate lazy first-use storage at startup

*hygiene, explicitly not a bug*

The recurring planter died with libc370#115/#119. What remains is that one-time
lazy initialization lands mid-region during traffic: worker-pool growth (64 K
stack per new worker) and each module's first LINK. Two concrete steps — start
the pool at MAXTASK (or add a `PREALLOC` keyword), and touch every
Parmlib-registered `MOD=` once during initialization, so both sit below the
high-water mark before the first request.

Whenever the region map is being looked at anyway.

---

## Deferred — blocked on RAKF

Maintainer decision, 2026-08-23: anything needing a change in `MVS-sysgen/RAKF`
is flagged `blocked:rakf` and deferred. RAKF is in another organisation, and #5,
#6 and #8 have been open and uncommented since 2026-08-06 / 08-17. These items
stay open and stay ranked out — not closed, not forgotten, and not waited on.

### #176 — Modules must stop switching ambient identity

*security · `blocked:rakf`*

ASXBSENV is address-space-wide and 3.8j has no per-task ACEE. The measured
fail-open chain is **not restated here** — `docs/identity-redesign.md` §1.5 owns
it, with the source citations (`credfree.c:33`, `raclgout.c:68-74`,
`ICHSFR00.hlasm:116`) and the cross-project picture this file cannot hold.

The **per-task ACEE** (`MVS-sysgen/RAKF#5`) is the preferred resolution and is
what the deferral parks: it fixes the class rather than routing around it, and
it is a two-project agreement on a TCB word, not a RAKF feature toggle.

**Two parts are ours and are not deferred with it:**

- **The fallback needs no RAKF.** *Remove the switch entirely* — modules never
  set an ACEE, opens run under the server identity — is all in our own code. The
  deferral promotes it from second choice to the only schedulable resolution, so
  it wants evaluating on its merits. Its cost: every open then runs under the
  resting identity, which makes the startup logon load-bearing rather than
  defence-in-depth. httpd has had one since #177 and ftpd since before
  `mvslovers/ftpd#97` (hardcoded literals; that issue is now about configuring
  them). ufsd has none and is staying that way: `mvslovers/ufsd#65` closed
  *not planned* on 2026-08-23, because nothing untrusted can steer an OPEN in
  that address space (`docs/identity-redesign.md` §3.2). The cost of the
  fallback therefore lands on httpd and ftpd, not there. ftpd#97 is unblocked.
- **The §1.5 verification** is unblocked but not free. Before booking a run:
  link 1 of the chain is a *denied* access abending, and `mvslovers/mvsmf#228`
  is exactly the change that stops data set denials from reaching the S913 — the
  obvious provocation path was removed by the fix that shrank the exposure.
  Create (`mvslovers/mvsmf#329`) does not substitute: `__dsalcf()` returns an rc,
  it does not abend. Another abending path has to stand in, or the run observes
  nothing.

Note that `mvslovers/mvsmf#345` is **not** evidence for this issue and is not
blocked by it: JES spool has no RAKF gate on 3.8j at all, so no ACEE — ambient or
explicit — decides anything there. `mvslovers/mvsmf#329` *is* this bug's shape.

---

## Recently landed

Pointers only. The reasoning lives in the closing comments, which is where this
project already writes it down properly.

- **#243** — a misspelled `AUTH=` value published the route (PR #247). Decided
  as *refuse the start*, the option #105's own reasoning argues for: `pol->failed`
  refuses the route before it is registered, and the `HTTPD419E`/`HTTPD420E`
  chain that already existed ends the start — with a `RES=` on the line or
  without, which was the asymmetry. Measured both ways on a spare port beside
  the STC. `HTTPD411W` retired in favour of `HTTPD411E`; the id is not reused.
- **#242** — the dead listener guard on that same path (PR #246). `do_bind()`
  is the only writer that stores a socket in `httpd->listen`, and it does so as
  its last act before `return 0`.
- **#233** — `HTTPD400E` retired (PR #244). It named the configuration on four
  of the five paths that reached it, where the fault was the port or the stack.
  Nothing replaced it; the `CC 0008` fact it used to carry now sits in
  `docs/messages.md` under *A refused start*, next to the healthy start and stop.
  Both refusal branches measured on mvsdev — `HTTPD037E` and `HTTPD420E`, each
  ending `CC 0008` with no summary line. Spawned #242, #243 and #245.
- **#235** — `build_fd_set()` reads `httpd->httpc[]` without the lock (PR #236).
  Settled by reading rather than locking; spawned #237. The open question it
  deliberately did not settle — whether a server with no worker pool should
  start at all — is deferred to #226.
- **#229** — the array contract, written down instead of hedged against
  (PR #234). Note: bare `#229` means httpd's here; `mvslovers/mvsmf#229` is the
  data set enumeration decision, and both get cited in the same conversations.
- **#137** — `RES=` startup warning (PR #219). Fail-open stays, deliberately;
  `res_probe()` writes `HTTPD425W` for a resource no profile covers.
- **#105** — the auth model cleanup, as #222 (global `LOGIN` bitmask retired,
  `AUTH=DEFAULT` gone) and #227 (`HTTPCGI` → `HTTPROUTE`).

## Cross-repo

This file is httpd-only, and the identity work is not. `docs/identity-redesign.md`
owns that story across httpd, mvsMF, ftpd, ufsd and RAKF. Do not rank those here;
update the status line there.

Unblocked and outside this repo: `mvslovers/ftpd#97` (now the last Phase 1
item), `mvslovers/mvsmf#329`, `mvslovers/mvsmf#345`.
Also `blocked:rakf`: `mvslovers/ftpd#64`.

Closed *not planned* on 2026-08-23: `mvslovers/ufsd#65` — reasoning in its
closing comment and in `docs/identity-redesign.md` §3.2. The per-client
permission question that replaces it is `mvslovers/ufsd#67`.
