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

*Last reconciled against the tracker: 2026-08-23, three issues open (#250 filed
that day; #237 closed by PR #249, and before it #245 by PR #248, #233 by
PR #244, #242 by PR #246 and #243 by PR #247).*

---

## The order

| | Issue | Kind | Waiting on |
|---|---|---|---|
| 1 | #250 | `type:research` — the `type:docs` half landed | **MVS time**, two `/.dm` calls |
| — | #198 | hygiene, explicitly not a bug | **milestone 4.1.0** — see below |
| — | #176 | security, the heaviest by a wide margin | **RAKF** — see *Deferred* |

**Nothing open waits on a decision any more,** and with #237 merged nothing
open is a code bug either. Only #250 is ranked; the other two are parked, one
on a release boundary and one on another organisation.

**The return-code work is finished.** #226 and #245 between them settled every
exit that could end a refused start `CC 0000`; nothing in that thread is open,
and `docs/messages.md` §*A refused start* is where the resulting contract is
written down.

---

### 1 · #250 — does a LINKed module survive in the Job Pack Area?

*what is left of it needs a live server and nothing else*

~~**(a)** Five statements across `docs/configuration.md` and
`docs/development.md` described module dispatch as a startup `__load()` and a
direct HTTPX call at ~10µs, where `httppcgi.c:47` LINKs on every dispatch.~~
Landed as PR #251 before 4.0.0 — they were the user-facing documents.

**(b) is the open half, and it is the one worth having.** Whether the copy LINK
brings in stays in the JPA to the next request, or is fetched from the library
again each time, is unsettled: the project's own sources said both, which is
what made #250 worth filing. Two `/.dm` calls against a running server decide
it — compare free storage between them.

**(c)** re-scopes #198's second step on that answer: placement hygiene if the
copy persists, a per-request disk fetch worth eliminating if it does not.

The docs now record (b) as an open question and say not to write either answer
down until it is measured. Honour that — a plausible guess written into
`development.md` is exactly how the five wrong statements got there.

---

## Deferred — milestone 4.1.0

### #198 — Pre-allocate lazy first-use storage at startup

*hygiene, explicitly not a bug*

The recurring planter died with libc370#115/#119. What remains is that one-time
lazy initialization lands mid-region during traffic: worker-pool growth (64 K
stack per new worker) and each module's first LINK.

**Deferred to 4.1.0 on 2026-08-23** — not because it is risky. Step 1 behind a
`PREALLOC` keyword defaulting off is a parser line and a `cthread_worker_add()`
loop; the code would not even run in the shipping default. It is deferred
because it fixes nothing anyone reports, and pre-release attention is the scarce
resource. This file already said "whenever the region map is being looked at
anyway", so the milestone only records what was already true.

Two things the scoping turned up that the issue text does not have:

- Step 1 wants `PREALLOC`, **not** a changed default. MAXTASK at startup is
  576 K of worker stacks against 192 K — free for a server that sees
  concurrency, since the pool never shrinks back, but 384 K permanently spent on
  a server that never does.
- Step 2 as written cannot use the existing path: `__linkds()` would *run* the
  module with no request. `__load()` (libc370 `clibos.h:202`) is the primitive
  that loads without executing. And it is gated on #250(b).

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

- **#250(a)** — module dispatch documented five ways, four of them wrong
  (PR #251). Corrected before 4.0.0, because `configuration.md` and
  `development.md` are what users read and they described an architecture the
  server does not have — a startup `__load()` and a `~10µs` direct call, against
  a LINK SVC on every dispatch that P7 costs at ~50 ms. The HTTPX half was right
  and stayed. #250 remains open for the measurement.
- **#237** — `httpclos()` DEQed the lock `process_clients()` was holding
  (PR #249). Capture the `lock()` rc, unlock only on 0 — the form the rest of
  the server already uses. What the audit added over the issue: the conditional
  `unlock()` sits *before* the cleanup block, so in the rc-8 path that block now
  runs under the caller's lock, and nothing reachable from it re-ENQs `httpd`
  (the resource name comes from the address alone, libc370 `@@lk.c`).
  `process_clients()` is the only rc-8 caller — `terminate()` and both
  accept-path calls hold no lock. The duplicate `http_process_clients()` is
  confirmed and gone; it was a second state-machine pump per pass in the
  no-worker fallback, unobservable there because `mgr` NULL leaves `select()`
  non-blocking and the loop spinning.
- **#245** — `HTTPD090E` ended the step `CC 0000` (PR #248). The last `main()`
  exit #226 did not reach; `rc = 8` before the `goto quit`, `initialize()`'s
  refusal code rather than a second one. Not measured and not measurable — the
  path needs `__gtcom()` to fail, which the spare-port method cannot provoke and
  a host test cannot reach. In `docs/messages.md` it is deliberately *not* filed
  under the refused-start sequence: it jumps to `quit:`, not `cleanup:`, so
  `HTTPD098I`/`HTTPD416I`/`HTTPD099I` never follow it. Same `CC 0008`, different
  shape.
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
