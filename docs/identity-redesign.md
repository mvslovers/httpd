# HTTPD / mvsMF / ftpd Identity — Analysis & Redesign

**Status:** design / planning (2026-08-17); Phase 1 status refreshed 2026-08-23.
Phase 1 is **3 of 4 done** — mvslovers/mvsmf#228 and #229 landed 2026-08-19,
httpd#137 on 2026-08-21. Still open: the resting identity (ftpd#97, ufsd#65),
and the **restjobs policy**, the half of §3.1 item 2 that #229 did not cover —
now tracked as mvslovers/mvsmf#345. **Phase 2 is deferred by decision
(2026-08-23), not merely stalled:** anything requiring a RAKF change is flagged
`blocked:rakf` and ranked out until upstream moves. All three of
MVS-sysgen/RAKF#5, #6 and #8 have been open and uncommented since 2026-08-06 /
08-17, and RAKF is in another organisation. See §3.2.
**Scope:** the *identity* story across **httpd**, **mvsMF**, **ftpd** (and, for
the resting identity, **ufsd**): which ACEE an authorization decision runs
under, what identity the address space rests on, and what changes without RAKF
changes (phase 1) versus with them (phase 2).

Successor to `auth-redesign.md`, whose scope — the *credential* model
(who is this client?) — is implemented as of httpd 4.0.0. This document covers
what that one deliberately did not: the identity in effect when an
*authorization* decision is made (may this client do that?), and its lifetime.

Structural tickets this document backs: mvslovers/httpd#176, mvslovers/ftpd#64.

---

## 1. Current state (Ist) — grounded in code and measurements

### 1.1 One field, address-space-wide

On MVS 3.8j the only ambient ACEE anchor is **ASXBSENV** (ASXB+0xC8). The TCB
has no ACEE field — `TCBSENV` is a z/OS-era control block member; RAKF resolves
the ACEE from the caller's parameter list, else ASXBSENV, and nothing else
(RACHECK `ICHSFR00.hlasm:111-116`, same pattern for FRACHECK/RACDEF/RACXTRT).
libc370 is consistent: `racf_set_acee()`/`racf_get_acee()` address ASXBSENV
directly (`racsacee.c:15`), and `racauth.c:116` documents the scope.

Meanwhile the servers run **one TCB per session** (`ATTACH EP=CTHREAD`). So any
per-request identity kept in ASXBSENV is shared by construction: *a
save/restore of an address-space-wide field cannot be made correct in a server
with concurrent sessions* — every remedy that keeps switching is serialization
under another name.

### 1.2 Who runs as what

| Component | Pattern | Consequence |
|---|---|---|
| httpd core | passes `httpc->cred->acee` explicitly (`httpxauth.c`) | safe |
| mvsMF | per-request `racf_set_acee()` switch + restore; dataset ops are plain `fopen()` | the ambient value *is* the authorization (mvslovers/mvsmf#228) |
| ftpd | explicit `racf_auth()` pre-checks + an OPEN-path switch; stage-1 hardening in ftpd#78 | entitlement safe; transient window remains (ftpd#64) |
| httprexx / httplua | never touch an ACEE | inherit whatever rests in ASXBSENV |
| ufsd | no identity handling at all; per-client checks are a Phase-1 simplification | every container OPEN runs under the resting identity (ufsd#65) |

**The resting identity is the exposure, not the leftovers.** RAKF has no
started-procedures table: every STC gets the hardcoded `STC`/`STCGROUP` account
(`ICHSFR00.hlasm` `RACISSTC`; upstream: MVS-sysgen/RAKF#8), and with the usual
profile (`DATASET * STCGROUP ALTER`) that identity holds ALTER on every data
set. Mitigations: httpd logs on to a dedicated low-privilege userid at startup
(`stc_identity()`, `STCUSER=`/`STCGROUP=` JCL PARMs — httpd#177); ftpd does the
equivalent with hardcoded literals (ftpd#97); ufsd not yet (ufsd#65).

### 1.3 How enforcement actually works (measured)

- **RAKF enforces inside stock OPEN.** The check OPEN already makes is answered
  by RAKF; the abend is issued by `IFG0194C`, an IBM OPEN executor. RAKF logs
  denials only (`RAKF0005`).
- **A denial is an S913 ABEND, not a return code.** For mvsMF every denied
  access abends the CGI; the ESTAE answers a misleading 500. Since the storage
  reclaim (httpd#154/#174) made abends survivable, this is routine, not rare.
- **`racf_auth()` is race-free**: it serializes its own set/RACHECK/restore
  under `lock(asxb)` (`racauth.c:68,120,135,148`) and takes the ACEE from the
  parameter list. Decisions made through it have no ambient dependency.
- **`http_check_auth()`** exposes that through the HTTPX vector
  (contract: 0 permitted, 8+ refused, −1 unauthenticated; SAF rc 4 normalized
  to 0 — never test `!= 0`). mvsMF (mvslovers/mvsmf#228) is its first consumer.

### 1.4 The RACINIT finding (from RAKF source)

`RACINIT ENVIR=CREATE` stores the new ACEE into ASXBSENV **only** when the
caller supplies no `ACEE=` pointer (plist shorter than x'34', or a zero word at
+x'34' — `ICHSFR00.hlasm:536-538, 557-564`). libc370's `racf_login()` always
passes one (`raclogin.c:125-126`, full 0x38 plist), so **no login through the
credential store ever displaces the resting identity**. The short-plist path is
how the STC's *initial* `STC`/`STCGROUP` identity arrives (IBM callers use the
short form).

Consequence: the client identity measured at rest in httpd#177 has no innocent
explanation — it must come from a set/restore bracket (request interleaving, or
the abend path). The leftover httpd#176 predicts has, in all likelihood,
already been observed live.

### 1.5 The fail-open chain (derived from source, not yet reproduced end-to-end)

1. A denied access abends the CGI (S913, routine — §1.3); the client's ACEE
   stays in ASXBSENV.
2. The client goes idle; the reaper frees the CRED and its ACEE
   (`httpd.c:607` → `cred_reap()` → `credfree.c:33` `racf_logout()`).
3. `racf_logout()` clears ASXBSENV by hand when it points at the ACEE just
   deleted (`raclgout.c:68-74`; RAKF leaves the dead pointer). The field is now
   **zero**.
4. RAKF treats a zero ASXBSENV as *permitted* (`ICHSFR00.hlasm:116`,
   `BZ RACHGOOD`).

So denial → abend → idle timeout ends with every ACEE-less authorization in
the address space passing, until something rests a new identity. ftpd met this
class and fixed it in ftpd#78 by always restoring the STC ACEE; httpd has no
equivalent restore point because no httpd code performed the switch.

Verification recipe: provoke a denial abend as an ordinary user, wait out
`SESSION_TIMEOUT`, read ASXBSENV via `/.dm` (PSA+0x224 → ASCB+0x6C →
ASXB+0xC8), expect zero.

---

## 2. Target model (Soll)

> **No module touches an ACEE. httpd answers "may this client access resource
> X with attribute Y."** The resting identity is a deliberate low-privilege
> choice. The platform grows a per-task identity, so the implicit OPEN gate
> becomes *correct* rather than being removed.

Three planks:

1. **Explicit authorization decisions** — `http_check_auth()` before each
   operation (ACEE in the parameter list, self-serializing). This is what makes
   a decision race-free today, returns 403 on the wire instead of abending,
   and stops producing identity leftovers.
2. **A deliberate resting identity** — dedicated low-privilege userid per
   server: via RAKF's started-procedures table once upstream
   (MVS-sysgen/RAKF#8), via the startup logon meanwhile.
3. **A per-task ACEE** (phase 2) — the fix for the class, not the symptom:
   with an identity anchor per TCB, per-session identity stops being
   serialization-by-another-name, and the implicit OPEN gate checks the right
   client without any switch.

**The trade-off, stated plainly.** Today the OPEN gate is implicit and
universal — every `fopen()` is checked, including the ones nobody thought
about, just under an identity drawn from a shared field. A pre-check-only model
is explicit and correct **but only where it was written**: a forgotten
`http_check_auth()` is not a weak gate, it is no gate. That is why phase 1
keeps the ambient switch as a second, implicit net, and why the preferred
endgame is the per-task ACEE (keeps the universal gate *and* makes it correct)
rather than removing the switch.

---

## 3. Migration path

### 3.1 Phase 1 — no RAKF changes

1. ✅ **(done — mvslovers/mvsmf#228, PR #316)** **mvsMF: explicit
   authorization** — the keystone. `http_check_auth()` before each dataset
   operation, through a `require_access()` wrapper at ten call sites in
   `dsapi.c`. Member operations authorize on the **library** (RACF has no
   member granularity), a rename checks both names, and every check runs ahead
   of the first catalog or VTOC access, so a refusal cannot be told from "does
   not exist". A denial answers the reference's `500` / category 4 body instead
   of S913 → the generic abend one, which cuts the §1.5 chain at link 1.
   **Left open:** `datasetCreateHandler` (POST) uses SVC 99, not an OPEN, so it
   fell outside the site list — the only data set-mutating operation still
   without an explicit check.
2. ✅ **(decided — mvslovers/mvsmf#229)** **mvsMF: enumeration policy** — the
   listing stays open, deliberately. Measured against the reference: real
   z/OSMF lists a data set with full attributes to a userid that provably
   cannot open it, so gating per entry would make mvsMF stricter than the thing
   it clones and put a RACHECK per entry on the request path. Recorded as a
   decision in mvsMF's `CLAUDE.md`; the constraint stands if it is revisited —
   a refusal must not be distinguishable from "does not exist".
   ⬜ **The restjobs half is still undecided** — now tracked as
   mvslovers/mvsmf#345 (the mvsMF mirror of ftpd#90 — spool access has no
   platform authorization model). `jobsapi.c` carries no authorization check at
   all: `owner` defaults to the caller but the client can widen it with `*`, and
   the by-jobid paths — including spool-content read and purge — never consult
   an owner. #229 does not settle it by precedent: that decision was about
   *enumeration*, and three of these four paths are not that.
3. ✅ **httpd: `RES=` startup warning** (httpd#137) — both halves settled.
   **Fail-open stays**: making `RES=` deny on an undefined profile is precisely
   the outage #136 removed (`RES=FACILITY:MVSMF.ACCESS` on a system without
   that profile would 403 every `/zosmf/*` request), and a Parmlib knob for it
   is opt-in to the same outage with nobody asking for it. What changes is that
   it no longer fails open *quietly*: `res_probe()` RACHECKs every `RES=`
   resource once after the Parmlib is parsed and writes `HTTPD425W` for each
   one no profile covers. It probes for *existence*, which is why the server's
   own identity suffices — an uncovered resource answers 4 whoever asks, and
   identity only moves the answer between 0 and 8, both meaning a profile is
   there. **Measured on mvsdev**, all three arms in one start as `HTTPD/USER`:
   an undefined resource warned, while one permitting READ (rc 0) and one
   refusing it (rc 8) both stayed silent — so a refusal is not mistaken for a
   missing profile. Should a hardened system ever want fail-closed, it is one
   line in `auth_gate()` and this warning becomes the error.
4. ⬜ **ftpd/ufsd resting identity** — ftpd#97 (replace the hardcoded
   `FTPD`/`USER` with PARM keywords), ufsd#65 (adopt the startup logon). Both
   still open; httpd has had its startup logon since #177.
5. **Keep mvsMF's ambient switch.** It is the second net (§2); removing it in
   phase 1 would maximize the window with neither net.

### 3.2 Phase 2 — RAKF changes (deferred, 2026-08-23)

**Decision:** none of this is scheduled. `mvslovers/httpd#176` and
`mvslovers/ftpd#64` carry the `blocked:rakf` label and are ranked out of their
repos' active lists; they stay open rather than closed, because the exposure is
real and the resolution is simply not ours to time.

**What the deferral changes for Phase 1.** With the started-procedures table
(§3.2 item 1) parked, the local startup logon stops being defence-in-depth and
becomes the only thing between an STC and the `STC`/`STCGROUP` identity that
holds ALTER on every data set. §3.1 item 4 is therefore the priority of this
document — but its two halves are **not** in the same state:

- **ftpd already switches.** `racf_login("FTPD", NULL, "USER")` at
  `ftpd.c:317-345`, measured live: `FTPD004I STC IDENTITY SET TO FTPD/USER VIA
  RACINIT`. Note that `RAKF0010I ... STARTED USING DEFAULT STC ACCOUNT` does not
  contradict this — RAKF issues it when the task is *created*, before ftpd's own
  code runs. What ftpd#97 still holds is the three "to settle" bullets: the
  literals cannot be changed without a rebuild (httpd has `STCUSER=`/`STCGROUP=`
  since #177), the entitlement set is not enumerated, and the failure policy is
  implicit — a failed RACINIT logs `FTPD004W` and continues on the inherited
  identity.
- **ufsd does not switch at all.** `racf_login`/`racf_set_acee` appear nowhere
  in its `src/`. Every container OPEN runs under whatever the address space
  rests on. **ufsd#65 is the real gap of the two**, and the deferral is what
  makes it urgent rather than tidy.

**And it promotes httpd#176's fallback.** *Remove the ambient switch entirely* —
modules never set an ACEE, opens run under the server identity — needs no RAKF
change and becomes the only schedulable resolution. It deserves evaluating on
its merits rather than being carried as a second choice; that evaluation is the
live question on #176 and is not deferred. Its cost lands squarely on the
resting identity, which is why it and §3.1 item 4 have to be weighed together.

1. **Started-procedures table** (MVS-sysgen/RAKF#8) — proc name → userid/group,
   hardcoded account as fallback. Root fix for §1.2's resting identity; the
   local startup logons become defense-in-depth.
2. **Per-task ACEE** (MVS-sysgen/RAKF#5) — a convention between RAKF and
   libc370 on a TCB word free on 3.8j, consulted by
   RACHECK/FRACHECK/RACDEF/RACXTRT ahead of ASXBSENV. Not a RAKF toggle: the
   TCB layout is MVS's, so it is a two-project control-block agreement. The
   enforcement point is RAKF's own code behind stock OPEN (§1.3), so the field
   is consultable there. Resolves httpd#176 and the remainder of ftpd#64;
   mvsMF's switch becomes correct — or unnecessary.
3. **Owner short-circuit fix** (MVS-sysgen/RAKF#6) — the DATASET owner check
   compares a name prefix, not the first qualifier; independent upstream
   security fix, same review.
4. **Optional: a JESSPOOL class** — would lift ftpd#90 and the mvsMF restjobs
   policy from invented rules (level 1 + FACILITY widening) to SAF-delegated
   profiles (level 2). Only sensible after 1 and 2.

After phase 2, httpd#176 and ftpd#64 close. Whether the ambient switch is then
removed or kept as documented redundancy is the last §4 decision.

---

## 4. Open decisions

- **Second net after the per-task ACEE:** once the anchor exists and
  `http_check_auth()` covers the work lists, is the ambient switch removed
  (simpler, one identity mechanism) or kept (redundant gate for code nobody
  audited)?
- **Refusal shape:** a refusal must be indistinguishable from "does not exist"
  on enumeration-adjacent paths (mvsmf#229), but a 403 on direct access —
  where is the line drawn per endpoint?
- **restjobs / JES spool policy** (mvsMF mirror of ftpd#90): owner-only with a
  FACILITY widening resource, or deliberately open on closed systems —
  document either way.
- **ufsd per-client permissions:** the Phase-1 "no ACEE checks" simplification
  is out of scope for the resting-identity work (ufsd#65) but is the next
  identity question once phase 1 lands.
- **§1.5 verification:** run the `/.dm` recipe once against a live stand and
  record the result on httpd#176.

## 5. References

- `auth-redesign.md` — the credential model this builds on (implemented, 4.0.0)
- mvslovers/httpd#176, #177, #137 · mvslovers/mvsmf#228, #229, #329, #345 ·
  mvslovers/ftpd#64, #78, #90, #97 · mvslovers/ufsd#65
- MVS-sysgen/RAKF#5, #6, #8
- Key code: `libc370/src/racf/{racsacee,racauth,raclogin,raclgout}.c`,
  `httpd/src/{httpxauth,httpd}.c`, `httpd/credentials/src/credfree.c`,
  `mvsmf/src/{mvsmf,dsapi}.c`, `RAKF/SRCLIB/ICHSFR00.hlasm`
