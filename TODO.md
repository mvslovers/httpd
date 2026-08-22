# HTTPD — Open Work, Ranked

**State lives on GitHub, not here.** `gh issue list --repo mvslovers/httpd` is
the source of truth for what is open, closed or newly filed. What this file adds
is the part the tracker cannot hold: the **order**, the reason for it, and which
items are waiting on a decision rather than on code.

Refresh it after every merge — an entry that outlives its issue is worse than no
file, because it reads as current.

*Last reconciled against the tracker: 2026-08-22, four issues open (#235 closed
by PR #236, #237 filed out of it).*

---

## 1 · #176 — Modules must stop switching ambient identity

*no label · security · the heaviest by a wide margin*

ASXBSENV is address-space-wide and 3.8j has no per-task ACEE. The issue thread
carries a **fail-open chain measured against sources**:

1. A denied access abends the CGI (S913). That is the *routine* outcome of a
   denial, not an edge case — OPEN does not refuse politely.
2. The requesting client's ACEE is left in ASXBSENV.
3. The idle reaper frees that CRED and `racf_logout()` zeroes the field
   (`libc370` `raclgout.c:68-74`) — necessary, or the system would rest on freed
   storage.
4. RAKF reads ASXBSENV == 0 as **permitted** (`ICHSFR00.hlasm:111-116`, comment
   "WHO KNOWS").

Independent of that chain: the resting identity is `STC`/`STCGROUP`, which holds
ALTER on every data set. Any ACEE-unaware CGI inherits it — `httprexx` and
`httplua` reference `racf_set_acee` nowhere, so both do.

**Changed since the thread was written:** mvslovers/mvsmf#228 is **closed**. An
explicit authorization check before the open answers 403 and never reaches the
S913, which cuts the chain at link 1 and materially lowers live exposure. It
does **not** resolve this issue: the resting identity and the interleaving
window between workers are untouched.

**Blocked on a decision, not on implementation.** Two candidate resolutions:

- *Per-task ACEE* (preferred) — a convention between RAKF and libc370 on a TCB
  word free on 3.8j, consulted ahead of ASXBSENV. Fixes the class rather than
  routing around it. Not a RAKF feature toggle: the TCB layout is MVS's, so it
  is a two-project agreement, and RAKF is not in this org.
- *Remove the switch entirely* — modules never set an ACEE, opens run under the
  server identity. Fallback if the first does not land.

---

## 2 · #233 — `HTTPD400E` blames the configuration for failures that are not

*priority:medium · type:cleanup · the fastest win*

Four of the five paths reaching `HTTPD400E ERRORS OCCURRED PROCESSING THE
CONFIGURATION` are not configuration errors at all — port already held
(`HTTPD037E`), `socket()`, `bind()`, `listen()`. The Parmlib was fine, and the
message sends the operator into `SYS2.PARMLIB(HTTPPRM0)` hunting a mistake that
is not there — at exactly the moment it costs most, a failed start.

On the fifth path it is redundant: `HTTPD420E` already ends with
`HTTPD WILL NOT START`.

One call site (`httpd.c:324`), tightly scoped.

---

## 3 · #237 — `httpclos()` releases the lock `process_clients()` is holding

*type:bug · latent, cheap*

`httpclos()` brackets its walk with an unconditional `lock(httpd,0)` /
`unlock(httpd,0)`. ENQ ownership is per TCB and not counted (libc370 `@@lk.c`),
so when `process_clients()` calls `http_close()` while holding that lock, the
inner `lock()` gets rc 8 and the inner DEQ releases the *outer* holder's ENQ.

Nothing breaks today — only a `break` follows the `http_close()` — and it is the
one place in the server that does not use the `if (lockrc==0) unlock(...)` form
every other call site uses. Filed out of #235, which also bounds it: the walk
only finds a client to close when the server came up without a worker pool, so
the nesting is rare on top of being harmless. Fix it for the next reader, not
for a live symptom.

The issue carries a second, unconfirmed observation for the same change:
`process_clients()` appears to call `http_process_clients()` twice per pass
(`httpd.c:684` and `:691`).

---

## 4 · #198 — Pre-allocate lazy first-use storage at startup

*no label · hygiene, explicitly not a bug*

The recurring planter died with libc370#115/#119. What remains is that
one-time lazy initialization lands mid-region during traffic: worker-pool growth
(64 K stack per new worker) and each module's first LINK. Two concrete steps —
start the pool at MAXTASK (or add a `PREALLOC` keyword), and touch every
Parmlib-registered `MOD=` once during initialization, so both sit below the
high-water mark before the first request.

Nothing is broken today; this is placement hygiene.

---

## Suggested sequencing

Not the same as the ranking above, because #176's next step is a conversation
rather than code.

1. **#233** — small, self-contained, operator-facing.
2. **#237** — smaller still, and the reading behind it is already done in #235.
3. **#176** — needs a direction decided first (per-task ACEE vs. removing the
   switch). That is a decision, not code.
4. **#198** — whenever the region map is being looked at anyway.

---

## Recently landed

- **#235** — *`build_fd_set()` reads `httpd->httpc[]` without the lock*
  (PR #236). Settled by reading, not by locking: the array has exactly one
  writer, the socket thread, so the lock-free read stands. The issue's
  mitigating assumption holds, and for a firmer reason than it gave —
  `initialize()` holds `lock(httpd,0)` across both the socket-thread create and
  `cthread_manager_init()`, and the socket loop's first act ENQs that same
  resource (`RET=HAVE`, so it waits), so the thread cannot accept before `mgr`
  is decided. No startup window; the array is populated only when the manager
  failed to initialize, and then no worker threads exist at all. The shutdown
  force-detach path (`HTTPD041I`) is the one second-writer exception and is now
  written down. The three `if (!httpc) continue;` guards are gone. Spawned #237.

  **Left open deliberately:** the array now has exactly one producer, the
  no-worker fallback. Whether that fallback should exist — whether a server with
  no worker pool ought to start at all — is the question `httpd.c:379-385`
  defers to #226, not something this PR settled.

- **#229** — *Write down the array contract instead of hedging against it*
  (PR #234, merged 2026-08-22, `3820a3f`). Removed five dead NULL guards on
  `httpd->route` / `httpc->env`, corrected the false `array_del()` comment in
  `build_fd_set()` that was the source of the belief in holes, and documented
  the contract in `docs/development.md`. Verified with a live acceptance run on
  mvsdev (throwaway instance on port 8229): the route array dumped 28 bytes —
  7 × 4 — with all seven pointers non-zero. Spawned #235.
