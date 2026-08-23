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

*Last reconciled against the tracker: 2026-08-23, five issues open (#233 closed
by PR #244, #243 and #242 filed out of it).*

---

## The order

| | Issue | Kind | Waiting on |
|---|---|---|---|
| 1 | #243 | `type:bug` `priority:medium` | **a decision** — see below |
| 2 | #237 | `type:bug` | nothing |
| 3 | #198 | hygiene, explicitly not a bug | nothing |
| 4 | #242 | `type:cleanup` | nothing |
| — | #176 | security, the heaviest by a wide margin | **RAKF** — see *Deferred* |

---

### 1 · #243 — a misspelled `AUTH=` value publishes the route

*the only open item with an exposure behind it*

A route line that names `AUTH=` and gets the value wrong — `AUTH=BASCI` — warns
`HTTPD411W` and registers **public**. The `continue` in `parse_kv_tail()` skips
`has_auth = 1`, so the memset's `HTTP_AUTH_NONE` stands and `policy_binds()`
sees nothing to lose. A `RES=` on the same line rescues it to `BASIC` by
accident; authentication-only routes, which is most of them, are not rescued.

**It waits on a decision, not on code.** Refusing the start (`HTTPD_FLAG_CFGERR`
→ `HTTPD420E`, what #105's own reasoning argues for), resolving to `BASIC`, or
documenting the exposure are three different servers. The issue lays out all
three; the change itself is small whichever wins.

The stale `HTTPD411W` text — still naming `DEFAULT` and a global policy both
retired by #105, still missing `TOKEN` from #121 — rides along with it.

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

### 4 · #242 — dead `if (httpd->listen)` guard on the config failure path

*ranked last because nothing behind it can misbehave*

`do_bind()` assigns `httpd->listen` as its last statement before `return 0`, so
every non-zero return from `http_config()` leaves it zero and the guard in
`httpd.c`'s failure branch is never taken. Same shape as the guards PR #236
dropped once `httpd->httpc` had a single writer: one writer, one point, and a
test written as though there might be more.

Filed out of #233, which found it and left it out on purpose — folding it in
would have made a message diff into a control-flow diff.

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

- **#233** — `HTTPD400E` retired (PR #244). It named the configuration on four
  of the five paths that reached it, where the fault was the port or the stack.
  Nothing replaced it; the `CC 0008` fact it used to carry now sits in
  `docs/messages.md` under *A refused start*, next to the healthy start and stop.
  Spawned #242 and #243.
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
