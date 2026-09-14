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

*Last reconciled against the tracker: 2026-09-14 after PR #267 merged, ten
issues open — #266 filed out of the ftpd 1.1.0 test install and #265 out of the
libc370 v1.0.6 release; #259 and #269 closed by #267; #264, #263 and #262 filed
out of the 4.0.2 work itself, #259 filed by a user, and #258, #254, #250, #198,
#176 carried over. #260 was filed and closed on 2026-09-05 by PR #261, the
relink 4.0.2 delivers. Before them: #256 filed and fixed by PR #257 on
2026-08-25, #252 filed and closed 2026-08-24 by
PR #253, #237 by PR #249, #245 by PR #248, #233 by PR #244, #242 by PR #246 and
#243 by PR #247.*

---

## The order

| | Issue | Kind | Waiting on |
|---|---|---|---|
| 1 | #263 | `type:bug` — a UFS read error is served as a truncated body | **an error vector for UFS**, which does not exist yet |
| 2 | #254 | `type:docs` — six Parmlib keywords undocumented | nothing |
| 3 | #262 | `type:research` — TSTSP `SA03` keeps the whole suite red | **MVS time**, and possibly a libc370 1.0.3 sysroot |
| 4 | #250 | `type:research` — the `type:docs` half landed | **MVS time**, two `/.dm` calls |
| 5 | #258 | `type:research` — cleanup-only recovery WTOs flood the console | **a decision** (libc370 API shape) + one MTT check |
| 6 | #264 | `type:bug` — a dead `HTTPDBG` is silent since 1.0.4 | **a decision**: report once and stop, or make the decorative `rc` honest |
| — | #265 | the libc370 v1.0.6 relink | **nothing** — audit posted, ready to close |
| — | #266 | packaging | **nothing** — all three items landed, ready to close |
| — | #198 | hygiene, explicitly not a bug | **#250(b)**, then milestone 4.1.0 |
| — | #176 | security, the heaviest by a wide margin | **RAKF** — see *Deferred* |

**#259 and #269 landed together as PR #267 — `main` is 4.1.0-dev.**
It was parked because `lklib`, `target` and `distlib` go into the FMID's JCLIN,
so the names are a one-shot choice per functional level — changing them is a new
level, not a patch. Nothing but docs had landed since v4.0.2, so no 4.0.3 was
owed and the cut cost nothing to take now; it would only have got more expensive
once a code fix landed. `THTP400` → `THTP410`, `HTTPD.@VRM@.*` → `HTTPD.*`,
following ftpd#121 which made the same change at `TFTP110`.

**What that inverts, and why it is the interesting part rather than a rename.**
One set of names means one installation that cannot drift from its inventory.
The upgrade mechanics came out of mbt#98, bumped onto this branch: the SYSMOD
carries `DELETE(THTP400)`, so SMP deletes the old modules from the target and
copies the new ones in during the same APPLY, and module ownership transfers.
That settles what #266 described — **and it shows that versioning the datasets
never bought a clean cut in the first place**, because SMP keys ownership on
`MOD(name)` in the CDS and not on the library the element lives in (mbt#98
measured a fresh, free FMID losing to the module-name owner while installing
into an unrelated dataset, `JOB00291`).

So the guide rewrite is smaller than it first looked, and different: there is
nothing to uninstall before an upgrade, section 12 is for *removal* only, and
an upgrade from 4.1.0 onward **skips the allocation step** because the
libraries are already there holding the predecessor. What does still bind:
stopping the server is no longer optional (the APPLY rewrites the library the
running one loads from), the APPLY legitimately ends RC 04 on an upgrade
(`HMA2461`, no SMPSCDS backup for a deleted level — the ACCEPT gate is relaxed
to `(4,LT,…)` for it), and scratching `HTTPD.LINKLIB` during a *removal* takes
any other product's module copied in beside ours — `MVSMF` on mvsdev. The
webroot keeps a name of its own kind and is never scratched.

**`THTP410` is verified free, on one stand.** `LIST CDS/ACDS SYSMOD(THTP410)`
answered RC 04 `NOT FOUND` in both zones on mvsdev (FMIDCHK `JOB00288`,
2026-09-14). It was **not** run on drnmig3a, where `THTP400` and `TFTP110` both
were — run it there before the 4.1.0 tag, not before the merge. The same job
found `THTP400` `REC APP ACC` on mvsdev with all five modules, so that stand is
a live 4.0.x installation and the first real test of the `DELETE` upgrade path.
The `DELETE` upgrade, including the hop across the rename, was rehearsed end to
end on drnmig3a 2026-09-14 with throwaway ids and an isolated `TSTH` HLQ
(`T402INST JOB00038` → `T410INST JOB00040` → `OWNCHK JOB00042`). It works, and
it needs **no** UCLIN — which is the opposite of what section 12a said when it
was written from ufsd's guide rather than from a run. The old library came
through untouched, so the crossing is recoverable as well as automatic.

**That sentence used to read "nothing open is a code bug." It no longer does.**
#263 is one, and it is the first real one since 4.0.0 shipped: a UFS read error
leaves `http_send_file()` with no state that notices, so the client gets a
truncated body on a chunked stream that never receives its terminator, and the
console blames `HTTPD901E SPIN` — a `9xx` id `docs/messages.md` tells the reader
is worth a bug report about the state machine. It ranks first because it is the
only item that changes what a client receives. It is not ranked first because it
is quick: no error vector for UFS exists, #260's lying-`BLKSIZE` DD does not
reach `ufs_fopen()`, and a guard shipped without one would be reviewed rather
than measured.

#254 stays high for the reason it always did — it costs no MVS time. #262 is
above the other research items because a suite whose top line is permanently red
stops being read at all, which costs more than the one test it hides. #250 is
above the two parked items even though it is milestoned out, and the two are not
in conflict: the milestone says it is not a 4.0.x deliverable, the rank says it
is the next thing worth doing, because it is read-only, costs minutes, and
#198's second step cannot be estimated until it is answered. #176 stays last for
the reason it always did, not because it is small.

**Three of the ten came out of the 4.0.2 work, and none of them is a
regression.** #263 has behaved this way since 4.0.0; #262 reproduces on
unmodified `main`; #264 is the write side of the libc370 change #260 fixed the
read side of. They are visible now because the relink made that whole class of
failure worth looking at, not because the relink caused them.

**#265 and #264 are one pass through the write side, which is why they rank
together.** libc370#149 shipped in v1.0.6, so #264 is not blocked upstream any
more; the `[toolchain]` pin is bumped and the tree rebuilds clean, so what is
left of #265 is the audit. Appended at the tail rather than ranked against the
existing five — #264 is `priority:low`. See #265.

**#266 is done, by a mix of tooling and documentation.** Items 1 and 2 were
answered by #269 rather than by prose: `++VER DELETE` removes the
element-ownership wall, so there is no "run the UCLIN first" step left to get
wrong. Item 3 — the `IEHLIST LISTPDS` at the end of the install — landed in
`58e4ad4`, copied from ftpd and ufsd, who both had it. Ready to close.

**#265 is answered too.** The write-path audit found nine unchecked stdio write
sites — the seven `dbg*.c` writers and the two streams `cgistart.c` installs as
a module's `stdout`/`stderr` — and **none of them on the request path**.
`http_send()` goes to `send()` on the socket, never through a `FILE*`, so
libc370's fail-fast cannot reach an HTTP response. What is left is diagnostic
output only, which is #264.

**#264 is now the only open half, and its scope is exact:** those nine sites.
The open question is the shape of the fix, not where it goes — report once by
WTO and disable tracing deliberately, or simply stop `dbgs()`/`dbgf()` and the
rest from returning a decorative `int rc = 0` that no write ever touches.

### Right after the tag

**Move `fmid` and `delete` in the commit that follows the version bump.**
`make release` bumps `VERSION` and stops, so the tree comes out of v4.1.0 at
`4.1.1-dev` with `fmid = "THTP410"` — the id that release just spent. Set
`THTP411` / `delete = ["THTP410"]` before anything is built from that tree.
`project.toml` carries the warning at the `fmid` line now; this is the reminder
that the moment to read it is the bump, not the next package. ftpd was sitting
in exactly this state an hour after releasing 1.1.0.

### Before the tag

Both gates are closed — kept here because the reasoning is what the runs
corrected, and because the second one is the evidence behind section 12a:

1. ~~`THTP410` free on both stands.~~ **Done 2026-09-14** — mvsdev `FMIDCHK
   JOB00288` and drnmig3a `FMIDCHK JOB00034`, each RC 04 `NOT FOUND` in both
   zones. `THTP400` is free on drnmig3a too, so no httpd is installed there.
   Name both stands whenever this is recorded: a check that quietly covers one
   where the previous release covered two reads as equally thorough.
2. ~~**The 4.0.x → 4.1.0 upgrade, run for real.**~~ **Done 2026-09-14** on
   drnmig3a — see below. Both gates are closed; the tag is not blocked.

   *(original note, kept because the reasoning is what the run corrected)* Section 12a of the installation
   guide is written from ufsd's measurement of the same crossing, not from one
   of ours. drnmig3a is the right stand for it *because* it is clean: install
   4.0.2 there first, then upgrade across the rename, and mvsdev's live server
   is never at risk.

   **This test is the documented exception to "throwaway module names".** Root
   `CLAUDE.md` requires them so a test install measures its own subject rather
   than the element-ownership wall — but here the wall *is* the subject:
   `THTP400` has to genuinely own `MOD(HTTPD)` for the crossing to mean
   anything. So real module names, real dataset names, and a throwaway id on
   the 4.1.0 side only.

   Worth running **both ways**: what ufsd settled is that the UCLIN-first path
   works, not what happens without it. Whether skipping it fails loud, fails
   silent, or quietly succeeds decides whether 12a's step 2 is a hard
   requirement or belt-and-braces, and nothing on record answers it.

   **Answered: it quietly succeeds, so the step is gone.** The no-UCLIN run was
   the first one tried and it worked, which made the UCLIN-first variant moot
   — nobody should now be told to take it. It was therefore not run, and that
   is the one thing this rehearsal leaves untested. It still matters to
   `mvslovers/ufsd`, whose 1.3.0 guide ships that instruction: worth telling
   them the step is unnecessary, and worth someone checking that following it
   anyway is at least harmless, since the SYSMOD's `DELETE` would then name an
   id that no longer exists.

**The return-code work is finished.** #226 and #245 between them settled every
exit that could end a refused start `CC 0000`; nothing in that thread is open,
and `docs/messages.md` §*A refused start* is where the resulting contract is
written down.

**v4.0.2 shipped on 2026-09-05:** the libc370 1.0.4 relink (#260/PR #261), plus
the `<vrm>` correction to the shipped installation guide. `THTP400` again — an
FMID names a functional level and 4.0.2 is a patch — so an upgrade runs the
`UCLIN` step of section 12 and stops there. The libraries move where the FMID
does not: 4.0.1 installed `HTTPD.V4R0M1.*`, 4.0.2 installs `HTTPD.V4R0M2.*`, and
saying "V4R0M0 for 4.0.x" is what scratched the wrong generation in ftpd. That
sentence was in httpd's guide too, which ships in the archive *as* `README.md`,
so the 4.0.1 package named datasets its own install job never created.

**4.0.0 shipped on 2026-08-24 and was withdrawn on 2026-08-25; v4.0.1 replaced
it.** The release work was never in the tracker — it is in
[`smp-todo.md`](smp-todo.md) — and it was done: `THTP400` free in the CDS *and*
the ACDS on both stands (O1), the full install rehearsed under the throwaway
FMID `TTST400` and cleaned back off with UCLIN, every step CC 0000 (O2), the
document root shipping as a UFS image (O3, #252). What is left there is a
checksum for the release assets and O4, a convenience sample nobody is blocked
on.

What none of that rehearsal caught is #256: it verified that the install
*works*, never that the procedure it installs still allocates what the CGI it
routes to opens. Worth adding to `smp-todo.md` before the next cut — a rehearsal
that ends at "the job ran CC 0000" cannot see this class of defect.

**The FMID did not move**, then or since. No source changed between 4.0.0 and
4.0.1, so there was no functional level to cut; 4.0.2 changed source but is still
a patch, and `THTP400` names all of 4.0.x either way. A system that
installed 4.0.0 needs nothing from SMP — the corrected member is in the sample
library, which is not an SMP element. It needs the two DD statements added to
its PROCLIB member, and specifically *not* the new member copied over: that
one's STEPLIB names `HTTPD.V4R0M1.LINKLIB`, which does not exist there.

Both release bodies are preserved under
[`docs/release-notes/`](release-notes/) — GitHub is not a source of record for
a body that gets deleted with its release, or regenerated by a tag re-push.
The `v4.0.0` **tag** was kept; only the release was removed.

---

### 1 · #263 — a UFS read error is served as a truncated body

*the only open item that changes what a client receives*

`http_send_file()` asks `ufs_feof()` and never `ufs_ferror()`, though libufs
exports it (`libufs.h:243`). On a read error `ufs_fread()` returns 0 with EOF
unset, so `substate` stays `SSTATE_SENDING` and no pass of the function can ever
notice. `serve_client()`'s spin guard ends it at 1000 fruitless passes, which
bounds the damage and mislabels it: the client keeps a chunked body that never
got its `0\r\n\r\n`, and the console says `HTTPD901E SPIN` — an id
`docs/messages.md` tells the reader to file a bug about the state machine.

**The work is the error vector, not the check.** #260's — one data set behind two
DDs that disagree about `BLKSIZE` — cannot reach `ufs_fopen()`, which goes
through UFSD rather than a DD. Without one, a guard is reviewed and not measured,
and `CLAUDE.md` is explicit that "feels fixed" is not fixed. Finding it may be
the larger half, and it may belong in ufsd rather than here.

The second decision is what to tell a client once the status line has gone out.
An unterminated chunked body is the only honest signal left on the wire; that is
a deliberate choice to make, not a fallback to stumble into.

### 2 · #254 — six Parmlib keywords the parser accepts are undocumented

*nothing blocks it; it is small and nobody has picked it up*

Ranked above #250 only because it costs no MVS time. `httpprm.c` is the
authority for what the parser accepts; `docs/configuration.md` is what an
operator reads, and the two disagree by six keywords.

### 3 · #262 — TSTSP keeps the whole suite red

*it hides one test and devalues the other seventeen*

490 assertions pass and `make test-mvs` still reports `2 step(s) FAILED`, because
TSTSP's step ends `ABEND SA03` — the step task ending with a subtask still
active. Ranked here, above the other research items, for what it costs rather
than what it is: a suite whose top line is always red trains everyone to stop
reading it, and then the next real failure is invisible too.

It is not a product defect and not a 4.0.2 regression — it reproduces on
unmodified `main` (JOB03098). The mechanism is specific: the test does wait, with
`cthread_wait(&task->termecb)` before `cthread_delete(&task)`, and libc370 still
answers "TCB has not ended, task and stack retained". So the termination ECB is
posted before the subtask terminates, which is a libc370 question and not a
`test/mvs/tstsp.c` one.

Two things it should settle. Why httpd's own worker pool is unaffected — `P
HTTPD` tears cthreads down cleanly, measured twice on 2026-09-05 — because that
difference probably *is* the answer. And whether 1.0.4 introduced it, which is
undetermined: the sysroot was already 1.0.4 before the relink work began, so
1.0.3 was never measured. That test needs a sysroot rebuilt from a libc370 v1.0.3
checkout, which is why it has not been run.

### 4 · #250 — does a LINKed module survive in the Job Pack Area?

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

### 5 · #258 — thirteen console lines that tell the operator nothing

*the only open item that waits on a decision, and the fix is not in this repo*

`C HTTPD` produced thirteen `libc370` recovery WTOs between `IEE301I` and
`IEF450I … ABEND S222`. Both message sites are libc370's `SDWACLUP` guards
(`@@abrpt.c` `recovery()`, `@@@try.c` `failed()`), the guards themselves are
correct, and the burst grows with the worker pool and with `try()` nesting —
mvsMF adds a layer of its own. It lands on any termination-time entry, an S878
memterm included, not only on a cancel.

The issue deliberately leaves the count unexplained: six of the thirteen come
from the `try()` exit, and `try()` deletes its ESTAE before returning while an
idle worker waits *outside* it, so the obvious per-task reading does not hold.
That is question one, and it decides whether the burst is bounded by the task
count or by something larger.

Two things rank it below the measurement work rather than above it. The obvious
fix — emit once — **cannot be built at the message site**: the path is
deliberately CRT-free and a `static` counter is key-0 module storage (#197). And
the two sites are asymmetric: `abendrpt()` already carries a caller option word,
`try()` carries none, so "make it optional" solves one message and not the other.

It is ranked at all because the decision is cheap and the measurement is one
`/.dmtt` look — whether a `MCSFLAG=HRDCPY` WTO still reaches the Master Trace
Table, which is the whole value of the middle option. Its deliverable is an
implementing issue in libc370 (see *Cross-repo*), not a PR here.

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

- **#256** — the STC procedure stopped allocating what its modules open
  (PR #257, shipped in v4.0.1). `HASPCKPT`/`HASPACE1` went out with `HTTPJES2`
  because nothing in `httpd/src/` opened them. True, and the wrong question: a
  CGI is dispatched by the LINK SVC *into HTTPD's task*, so it opens every
  ddname against the STC's allocations, and mvsMF's jobs API opens both through
  libc370's `jesopen()`. 4.0.0 shipped a procedure that broke the API its own
  default configuration routes to.
  Three things worth keeping out of the issue thread. **`migration.md` was the
  more severe half** — a missing DD breaks a *new* install; that file told a
  *working* 3.3.x system to delete one it needs. **The failure is not uniform**:
  job list and spool read answer 500, but `find_job_by_name_and_id()` returns
  NULL for an unreachable JES2 exactly as for an absent job, so single-job
  lookup answers `404 job not found` for a job that exists — reported as
  `mvslovers/mvsmf#357`, and the reason the console pair is the signal, not the
  status. **The class was checked, not just the instance**: re-running the
  removal audit for `stck2tv`, `httpds_`, `HTTPJES2` and `HTTPDSL` across mvsmf,
  httplua and httprexx found nothing, so this was the one case. The rule is in
  `CLAUDE.md` now.
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

Filed out of #256 on 2026-08-25, neither blocking 4.0.1:
`mvslovers/mvsmf#357` states the DD requirement where a mvsMF installer will
look for it — httpd documents it from one side of the boundary, nothing
documented it from the other. `mvslovers/libc370#142` is the fix that removes
the class: `jesopen()` should dynalloc the checkpoint and spool the way
`jesiropn()` already dynallocs the INTRDR, which also gets the site-specific
`VOL=SER` out of every procedure in the ecosystem.

#258 is filed here because httpd is where the noise was seen and where the pool
size that multiplies it is configured, but both message sites are libc370's and
every consumer shares them — ftpd, mvsmf, ufsd, rexx370, httplua, httprexx. It
closes on an implementing issue in libc370, which should be settled together with
`mvslovers/libc370#17` (consolidate the two `try()` wrappers): same file, and the
`@@@try.c` message is the half with no cheap option channel.

Closed *not planned* on 2026-08-23: `mvslovers/ufsd#65` — reasoning in its
closing comment and in `docs/identity-redesign.md` §3.2. The per-client
permission question that replaces it is `mvslovers/ufsd#67`.
