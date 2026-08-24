# HTTPD Installation Guide

This guide installs HTTPD on an MVS 3.8j system (TK4-, TK5, MVS/CE, or a custom
Hercules build) from the release distribution archive.

The installation is managed by **SMP Release 4** — the SMP that ships with
MVS 3.8j, not SMP/E. That means the system keeps a record of what was
installed, and there is a supported way back out (see
[Removing HTTPD](#11-removing-httpd)).

## Getting help

**Report problems as a GitHub issue:**
<https://github.com/mvslovers/httpd/issues>

That is the only place bug reports are tracked. Please include the two build
stamps HTTPD writes at startup (`HTTPD000I` and `HTTPD005I`, see
[step 9](#9-start-and-verify)) — they identify the exact build — plus the
console messages and, for an install problem, the job output.

**Questions and general support** are on Discord:
<https://discord.gg/gaUAFKGCR>

---

Two placeholders are used throughout:

| | |
|---|---|
| `<version>` | the release, e.g. `4.0.0` — it appears in every shipped file name |
| `<vrm>` | the same release as MVS dataset qualifier, e.g. `V4R0M0` |

Both are already filled in inside the shipped jobs; you only need them to
recognise which file is which.

---

## 1. What is in the archive

| File | What it is |
|------|------------|
| `README.md` | this guide |
| `httpd-<version>-load.xmit` | the five load modules, as a TSO RECEIVE-ready XMIT |
| `httpd-<version>-samplib.xmit` | the sample library: STC procedure and configuration pattern |
| `httpd-<version>-alloc.jcl` | allocates the datasets SMP installs into — **run once** |
| `httpd-<version>-inst.jcl` | receives everything and installs it — repeatable |

The load library holds five modules:

| Module | What it is |
|--------|------------|
| `HTTPD` | the server itself — the only one a started task names |
| `HTTPDSRV` | display module: server control blocks (`/.dsrv`) |
| `HTTPDM` | display module: arbitrary storage (`/.dm`) |
| `HTTPDMTT` | display module: Master Trace Table, i.e. the console log (`/.dmtt`) |
| `ABEND0C1` | a deliberate-abend diagnostic used to exercise storage reclaim |

**Only `HTTPD` runs unless you ask for the others.** Since 4.0.0 no module is
reachable unless a `MOD=` line in the configuration member names it, so the four
above are inert on a fresh install. That is deliberate: `/.dm` and `/.dmtt` hand
out arbitrary storage and the console log, and `ABEND0C1` abends on purpose.
See [step 7](#7-install-the-procedure-and-the-configuration).

> **The four are diagnostic, debugging and development tools, and they are on
> their way out.** They ship in 4.0.x so that a problem on a live system can be
> looked at without a private build — reading a control block beats guessing at
> one, and `/.dmtt` is often the fastest way to the console log. They are not
> production endpoints, and **the next release will not deliver them**: a server
> whose job is hosting an API has no business shipping an arbitrary-storage
> reader alongside it. Build them from source when you need them.
>
> Nothing depends on them. `HTTPD` is the only module the started task names,
> and removing the other four changes no behaviour of a server that never
> routed to them.

The sample library holds two members, `HTTPD` (the started task procedure) and
`HTTPPRM0` (the configuration pattern).

Where everything ends up:

```
HTTPD.<vrm>.LINKLIB     the load modules -- point STEPLIB here
HTTPD.<vrm>.SAMPLIB     the patterns you copy from in step 7
HTTPD.<vrm>.AHTTPLOD    SMP's distribution library, the base a RESTORE returns to
```

---

## 2. Prerequisites

- **MVS 3.8j** on Hercules, up and IPLed, with SMP 4 usable (the `SMPREC` and
  `SMPAPP` procedures and the `SYS1.SMP*` datasets — present on TK4-/TK5 and
  MVS/CE as shipped).
- **RAKF** — see [Authorisation](#authorisation) below. HTTPD does not start
  without either RAKF or an APF list entry, and no configuration option turns
  that off.
- **UFSD**, only if you want to serve static files — see below.
- **mvsMF**, only if you want the REST API the shipped configuration routes to
  — see below.
- A userid authorised to submit jobs, to update a PROCLIB in the started-task
  concatenation, and to update a PARMLIB.
- A way to upload a **binary** file to the host — see step 3.
- Roughly 15 cylinders of DASD: 10 for the two libraries the allocation job
  creates, the rest for the two TSO RECEIVE targets. One of those, the staging
  library, is scratched again by the last step of the install job.

### UFSD is optional, and what it costs you is static files

HTTPD serves static content from the **UFSD filesystem and nowhere else** — the
DD-based document root of 3.3.x is gone (4.0.0). `DOCROOT` names a UFS path, so
without the UFSD started task there is nothing to serve statically.

Nothing about the *install* needs it: the SYSMOD declares no prerequisite,
`libufs` is linked statically into the load modules, and a UFS that does not
initialise is a warning, not a failure:

```
HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM
```

The server starts, listens, and every `MOD=` route — mvsMF, the display modules
— works normally. Only static file requests fail. A CGI-only HTTPD is an
ordinary deployment.

Note that `HTTPD001I` still reads `READY - SERVING /www` afterwards: it names
the **configured** document root, not a working filesystem. `HTTPD044W` is the
line that tells you the filesystem is not there, and it is written earlier in
the start. On a deployment that will never have UFSD, `UFS=0` in the
configuration member skips the initialisation altogether and the warning with
it.

Install HTTPD now and add UFSD later if you want it:
<https://github.com/mvslovers/ufsd/releases>, with its own guide at
[ufsd/docs/installation.md](https://github.com/mvslovers/ufsd/blob/main/docs/installation.md).

Note that UFSD has to be **running**, not just installed — it is a started task
HTTPD talks to across address spaces.

### mvsMF is what the shipped configuration expects

`HTTPPRM0` ships with three active routes, all of them to **mvsMF** — the
z/OSMF-compatible REST API, which is a separate product:

```
MOD=MVSMF /zosmf/info                    AUTH=NONE
MOD=MVSMF /zosmf/services/authenticate   AUTH=NONE
MOD=MVSMF /zosmf/*                       AUTH=TOKEN
```

The first two are public on purpose — the anonymous reachability probe and the
token login endpoint do their own auth, so the gate must not challenge them.
The catch-all behind them is where the datasets, jobs and files live, and it is
gated. First match wins, so the order matters.

If mvsMF is not installed, HTTPD starts and those routes register normally — the
program is not looked for until a request first matches. The failure then shows
up per request as

```
HTTPD908E EXTERNAL PROGRAM MVSMF could not be loaded (not found in STEPLIB?)
```

Comment the three lines out, or install mvsMF:
<https://github.com/mvslovers/mvsmf/releases>. Its load library has to be in the
STC's STEPLIB concatenation — HTTPD dispatches a module through the MVS LINK
SVC, which searches the task's own libraries.

### RAKF is required for logins

HTTPD has no user database of its own. Every login — the HTML form, HTTP Basic,
and the token endpoint — is verified by RAKF (RACINIT, SVC 244), and a `RES=`
route option is checked against a RAKF profile under the logged-in user's
identity. **On a system without RAKF nobody can log in.**

A server whose routes are all `AUTH=NONE` never authenticates anybody and does
not need RAKF for that — but it still needs it for authorisation, below.

The setup — the `HTTPD` user and its group — is step 8.

### Authorisation

HTTPD is link-edited `AC(1)` and needs to be APF-authorised: without it the LINK
SVC that dispatches every module and the RACF services behind every login are
both unavailable.

It obtains authorisation itself at startup, which goes through **SVC 244 — and
that comes from RAKF**. The clean alternative is to add `HTTPD.<vrm>.LINKLIB` to
the APF list in `SYS1.PARMLIB(IEAAPF00)`. On MVS 3.8j the APF list is only read
at IPL, and the library name carries the version — so this is one IPL per
release, not one IPL ever.

**With neither, HTTPD does not start.** Unlike FTPD, which warns and keeps
running, HTTPD treats this as fatal:

```
HTTPD012E HTTPD UNABLE TO DYNAMICALLY OBTAIN APF AUTHORIZATION
```

and `main()` returns the failing setup's return code without starting a
listener. Resolve this before step 5 — an install that completes cleanly will
still not start.

The two routes are not equivalent in one respect that matters if you ever have
to debug HTTPD. An **APF entry** authorises the job step *before* program fetch,
so MVS loads HTTPD into subpool 252 **key 0** — authorised code must not be
patchable by problem-key code. **SVC 244** sets `JSCBAUTH` *after* the fetch and
cannot relabel storage that is already allocated, so the module stays key 8.
HTTPD runs problem state key 8 either way, which means that under an APF entry
it must never store into its own module storage. It does not, as of 4.0.0: the
server block is `main()`'s automatic storage, and the state that used to be
file-scope — the codepage pair, the saved STC ACEE, the Basic realm — lives
there with it (issue #197).

You do not have to work out which route you got: HTTPD says so at startup, on
the line after the version banner.

```
HTTPD002I AUTHORIZED BY LIBRARY (MODULE KEY 0)     APF list entry
HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)         SVC 244, from RAKF
```

The key is inferred from the route rather than measured — an authorised job step
has its module fetched key 0, an unauthorised one key 8, and SVC 244 arrives too
late to change either. Neither line is a warning: both routes end in an
authorised started task, and which one you want is a site decision. Quote the
line in a bug report — it decides the storage key HTTPD's own module runs in.

---

## 3. Upload the two XMIT files

Both `.xmit` files are EBCDIC NETDATA streams. Upload them **in binary** — no
ASCII/EBCDIC translation, no CRLF conversion — into sequential datasets with
`RECFM=FB LRECL=80 BLKSIZE=3120`.

**The dataset names are yours to choose.** The install job names them on its
own `RECEIVE` commands, so nothing depends on what you call them; you will
enter them once in step 6. This guide uses `IBMUSER.HTTPD.LOAD.XMIT` and
`IBMUSER.HTTPD.SAMP.XMIT`.

Whether you have to allocate them first depends on the upload path: **FTPD**
and **IND$FILE** create the dataset from the attributes you supply, mvsMF and
most other FTP servers need it to exist. Where a method says *pre-allocate*,
allocate both as `DSORG=PS, RECFM=FB, LRECL=80, BLKSIZE=3120` — 50 tracks
primary for the load XMIT (roughly 650 KB), 5 for the sample library, secondary
about 10% of each. Method b) below shows this with zowe; TSO or ISPF 3.2 does
the same job.

Pick **one** method:

### a) FTP — mvslovers/ftpd (no pre-allocation)

FTPD creates the datasets from the attributes it is given (default port 2121):

```
ftp -P 2121 your-mvs-host
> binary
> put httpd-<version>-load.xmit    'IBMUSER.HTTPD.LOAD.XMIT'
> put httpd-<version>-samplib.xmit 'IBMUSER.HTTPD.SAMP.XMIT'
> quit
```

This is also the upload path to prefer when you are **upgrading**: the HTTPD you
are replacing is the one serving mvsMF, so uploading through mvsMF means
uploading through the thing you are about to stop.

### b) mvsMF (z/OSMF-compatible REST API), via zowe

Only when a *different* server is up, or you are not replacing this one. It
requires the datasets to **exist first**, so allocate them, here with zowe
itself:

```
zowe files create ps "IBMUSER.HTTPD.LOAD.XMIT" \
     --recfm FB --lrecl 80 --blksize 3120 --size 50TRK
zowe files create ps "IBMUSER.HTTPD.SAMP.XMIT" \
     --recfm FB --lrecl 80 --blksize 3120 --size 5TRK
```

then upload:

```
zowe zos-files upload file-to-data-set httpd-<version>-load.xmit \
     "IBMUSER.HTTPD.LOAD.XMIT" --binary
zowe zos-files upload file-to-data-set httpd-<version>-samplib.xmit \
     "IBMUSER.HTTPD.SAMP.XMIT" --binary
```

`--size` sets the primary allocation and gives you a secondary of about 10% of
it. The values above are for a 3390 and leave room to spare — the load XMIT is
roughly 650 KB, the sample library well under 20 KB. On a smaller device, or if
you hit an `SB37`, raise them.

### c) Another FTP server (pre-allocate)

*Pre-allocate* both datasets, then transfer **binary** into them — a plain
`binary` + `put`. Any `SITE` keywords for dataset attributes vary between
MVS 3.8j TCP/IP stacks (they are **not** the z/OS syntax); pre-allocating makes
them unnecessary.

### d) IND$FILE (3270 emulator)

No pre-allocation needed. Use your emulator's file transfer in **binary** mode
(no ASCII/CRLF translation) with `RECFM=FB LRECL=80 BLKSIZE=3120`. The exact
option syntax is client-dependent — consult its file-transfer documentation.

---

## 4. Allocate the product datasets

Submit `httpd-<version>-alloc.jcl` unchanged, unless you want a specific unit or
volume — the `UNIT=SYSDA` and the space on each DD are the only things worth
editing.

It creates `HTTPD.<vrm>.LINKLIB` and `HTTPD.<vrm>.AHTTPLOD` and nothing else.
The libraries the next step receives into are deliberately **not** allocated
here: TSO RECEIVE creates its own target and refuses to merge into an existing
dataset.

Expect `COND CODE 0000`.

> **Run this once.** There is no DELETE step in it, on purpose. After the
> install, `HTTPD.<vrm>.AHTTPLOD` holds SMP's accepted copy of the modules; a
> re-run that scratched it would leave the SMP inventory reporting an install
> that is no longer on the system, and nothing would say so. To start over,
> reject the SYSMOD first — see [Removing HTTPD](#11-removing-httpd).

---

## 5. Stop a running HTTPD

Only relevant when you are upgrading. The APPLY writes into
`HTTPD.<vrm>.LINKLIB`, and each release has its own — so a running *older* HTTPD
does not block the install. It does, however, keep running the old modules until
you restart it (step 9) against the procedure you copy in step 7.

```
/P HTTPD
```

Two things worth knowing before you do:

- **HTTPD refuses to start a second instance on a port already served**
  (`HTTPD037E`), so a forgotten `/P` becomes a refused start rather than a
  confusing half-working one. That check is keyed by port, so a deliberate
  second server on another port still starts — which is a good way to try a new
  build without touching the running one.
- If mvsMF is your upload or console channel, stopping HTTPD takes it with it.
  Have another way in (FTPD, TSO, the Hercules console) before you stop the
  server.

---

## 6. Install

Open `httpd-<version>-inst.jcl` and replace the two placeholder dataset names
with what you uploaded in step 3:

```
  RECEIVE INDSN('CHANGE.ME.HTTPLOAD') -     <- IBMUSER.HTTPD.LOAD.XMIT
  RECEIVE INDSN('CHANGE.ME.SAMPLIB') -      <- IBMUSER.HTTPD.SAMP.XMIT
```

Those are the only lines you have to change. Submit it.

The job runs eight steps, each conditional on the one before, so it stops at
the first failure rather than building on it:

| Step | What it does |
|------|--------------|
| `DELOLD` | scratches the RECEIVE targets, so the job can be re-run |
| `RECV1` | load XMIT → `HTTPD.<vrm>.HTTPLOAD` (a staging library) |
| `RECV2` | samplib XMIT → `HTTPD.<vrm>.SAMPLIB` |
| `RECV` | receives the SYSMOD into the SMP inventory |
| `APPLYCHK` | dry run — `APPLY` only proceeds if this ends RC 0 |
| `APPLY` | copies the five load modules into `HTTPD.<vrm>.LINKLIB` |
| `ACCEPT` | makes this level the base a later `RESTORE` returns to |
| `CLEANUP` | scratches the staging library, which is now spent |

The SYSMOD travels inline in the job — there is no third file to upload.

**What a good run looks like.** Every step `COND CODE 0000`, and in the SMP
output:

```
HMA3930    SYSMOD THTP400 SUCCESSFULLY RECEIVED
HMA2380    COPY SUCCESSFUL - MOD=HTTPD - LMOD=HTTPD - LIBRARY=LINKLIB
           - RETURN CODE=00
HMA2050    APPLY PROCESSING COMPLETED - HIGHEST RETURN CODE IS 00
```

There is one `HMA2380` line **per module** — five of them. A run that copies
fewer has lost one, and the `APPLY` still ends RC 00, so count them.

Then check `HTTPD.<vrm>.LINKLIB` really holds all five (ISPF 3.4). Do look: SMP
reports the library by **ddname**, and a ddname says nothing about which dataset
was behind it.

SMP **copies** these modules rather than re-binding them, which is why the
`AC(1)` authorisation code on `HTTPD` and every module's custom entry point are
exactly what the build produced.

---

## 7. Install the procedure and the configuration

SMP does not touch your PROCLIB or PARMLIB, and that is deliberate: **the
product owns the patterns, your system owns the copies.** If SMP owned the
running procedure, every change you made to it would be silently replaced by
the next update. So this step is yours, and it is the one place where you have
to read what you are copying.

Copy from `HTTPD.<vrm>.SAMPLIB`:

| Member | Copy to | Adjust |
|--------|---------|--------|
| `HTTPD` | a PROCLIB in the started-task concatenation | usually not — `STEPLIB` already names this release's LINKLIB. Add mvsMF's library to it if you use mvsMF |
| `HTTPPRM0` | a PARMLIB | **yes — see below** |

`SYS2.PROCLIB` is the usual home for the procedure.

### Which PARMLIB

The shipped procedure defaults to `D='SYS2.PARMLIB'`. **On TK5 that dataset
does not exist** — put the member in `SYS1.PARMLIB` and either edit `D=` in
your copy of the procedure, or override it when starting:

```
/S HTTPD,D='SYS1.PARMLIB'
```

On MVS/CE, `SYS2.PARMLIB` exists and the default is fine. `M=` selects the
member (default `HTTPPRM0`), so a second configuration can live alongside the
first.

### The STC identity — `STCUSER` and `STCGRP`

The shipped procedure logs the server on to `HTTPD/USER` at startup:

```
//            STCUSER=HTTPD,
//            STCGRP=USER
//HTTPD    EXEC PGM=HTTPD,REGION=8M,TIME=1440,
//            PARM='STCUSER=&STCUSER STCGROUP=&STCGRP'
```

RAKF has no started-procedures table: it decides only *that* a caller is an STC,
never *which* one, and hands every started task the same `STC/STCGROUP` account
— which on a stock profile set holds **ALTER on every dataset on the system**.
So HTTPD replaces it with a dedicated identity of its own (issue #177). Define
that userid in step 8.

Two traps if you write your own procedure:

- The START symbolic is `STCGRP` while the keyword the program parses is
  `STCGROUP=` — a JCL symbolic parameter name is at most seven characters.
- `__start()` splits the PARM into `argv` on **blanks, not commas**. Joining the
  two keywords with a comma hands the whole string to `STCUSER=`, and the group
  is then silently wrong.

If the logon fails the server **continues** on the inherited identity and says
so with `HTTPD004W`, so a profile typo is not an outage — but it is also not
what you asked for. Watch for that message where you expect `HTTPD004I`.

### The DD statements — do not add SYSPRINT, SYSTERM or SYSIN

HTTPD writes its own STDOUT/STDERR and reads STDIN through private DDs
(`HTTPDOUT`, `HTTPDERR`, `HTTPDIN`) so that a module cannot scribble into the
JES datasets an operator reads. The three conventional names are **refused**,
before `main()` runs:

```
HTTPD014E SYSPRINT DD NOT ALLOWED, HTTPD USES HTTPDOUT FOR STDOUT
HTTPD015E SYSTERM DD NOT ALLOWED, HTTPD USES HTTPDERR FOR STDERR
HTTPD016E SYSIN DD NOT ALLOWED, HTTPD USES HTTPDIN FOR STDIN
```

The shipped procedure already has the right set. Leave it alone unless you know
why you are changing it.

### Editing HTTPPRM0

Every keyword in the shipped member is commented out, so a member copied
unchanged runs on the built-in defaults — **port 8080, document root `/www`** —
plus the three active `MOD=MVSMF` route lines. Four things are worth a look
before the first start:

```
PORT=8080
DOCROOT=/www
REALM=MVS Development System
MOD=MVSMF /zosmf/*    AUTH=TOKEN
```

- `PORT` is 8080 by default. Nothing on a stock MVS 3.8j contends for it.
- `DOCROOT` is a **UFS path**, and it must exist in a mounted UFSD filesystem.
  Without UFSD, leave it and expect `HTTPD044W` (step 2).
- `REALM` defaults to the system's SMF ID — unambiguous, but meaningless to a
  user. It is what a browser shows in its Basic credential dialog.
- The `MOD=MVSMF` lines are the ones to review first. Comment them out if you
  have no mvsMF.

**Read the auth model before you publish anything.** Authentication is declared
per route and nowhere else — there is no server-wide login keyword, and the
`LOGIN` keyword of earlier versions is retired:

```
MOD=MVSMF /zosmf/*      AUTH=TOKEN    program route, gated
LOC=/admin/*            AUTH=BASIC    static prefix, gated
LOC=/*                  AUTH=NONE     static prefix, public on purpose
```

- **A route without `AUTH=` is public**, and so is any path no route claims at
  all. There is no global default left to fall back on.
- The modes are `NONE`, `FORM` (HTML login form), `BASIC` (401 +
  `WWW-Authenticate`) and `TOKEN` (a bare 401 for API clients that handle it
  themselves). A value that is none of these refuses the route and the server
  **does not start** — `HTTPD411E` then `HTTPD420E`. That is on purpose: a
  policy silently weakened is worse than a server that will not start.
- `AUTH=` does not select which credentials are accepted. Every route accepts
  all of them, because they are resolved before the route is matched. It selects
  *whether* a login is needed and *how* a missing one is challenged.
- A member still carrying `LOGIN=ALL` / `CGI` / `GET` / `HEAD` / `POST` is a
  **fatal** configuration error (`HTTPD048E`), because ignoring it would publish
  every route without an `AUTH=` of its own. `LOGIN=NONE` was the default and is
  merely warned about (`HTTPD048W`). Convert to `AUTH=` per route, then delete
  the line.

The display modules deserve their own warning. `MOD=HTTPDM /.dm` with no `AUTH=`
hands anyone who can reach the port arbitrary storage reads, and `/.dmtt` hands
them the console log. They are debugging tools that will not be shipped after
4.0.x (step 1) — enable them while you are diagnosing something, and gate them
whenever you do:

```
MOD=HTTPDSRV  /.dsrv    AUTH=FORM
MOD=HTTPDM    /.dm      AUTH=BASIC
MOD=HTTPDMTT  /.dmtt    AUTH=BASIC
```

A `#` or `*` in the first column comments a line out. The full keyword reference
is [configuration.md](https://github.com/mvslovers/httpd/blob/main/docs/configuration.md).

---

## 8. Set up RAKF

Without this, HTTPD cannot authorise itself and does not start (step 2), and no
client can log in.

1. Add the `HTTPD` user to `SYS1.SECURE.CNTL(USERS)`, default group `USER`:

   ```
   HTTPD    USER  DEFAULT-GROUP(USER)
   ```

   This is the identity from `STCUSER`/`STCGRP` in step 7. It needs **READ** on
   whatever HTTPD itself opens before a client identity exists: the PARMLIB
   member, the document root, the log DDs.

   It does **not** need `FACILITY SVC244`. HTTPD acquires APF authorisation at
   start and releases it at `/P HTTPD`, and it restores the STC account it
   started under immediately before the release — so both SVC 244 calls are made
   by the same account, rather than requiring the profile on every system.

2. Optionally define profiles for any route that carries `RES=class:resource`.
   A `RES=` naming a resource **no profile covers** is not an error and does not
   deny: SAF calls an unprotected resource allowed, so the authorisation stage
   does nothing and only the `AUTH=` stage is left. HTTPD says so at startup:

   ```
   HTTPD425W NO PROFILE FOR FACILITY:HTTPD.ADMIN -- /admin/* NOT GATED
   ```

3. Reload: `/F RAKF,RELOAD`

---

## 9. Start and verify

```
/S HTTPD                      default member (HTTPPRM0)
/S HTTPD,M=HTTPPRM1           alternate config member
/S HTTPD,D='SYS1.PARMLIB'     alternate PARMLIB -- TK5, see step 7
```

A healthy start ends in `HTTPD001I … READY`:

```
HTTPD000I HTTPD <version> (A69A370) STARTING
HTTPD005I LIBC370 1.0.3 (58767B3)
HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)
HTTPD004I STC IDENTITY SET TO HTTPD/USER VIA RACINIT
HTTPD036I MODULE MVSMF REGISTERED FOR /zosmf/info
HTTPD036I MODULE MVSMF REGISTERED FOR /zosmf/*
HTTPD054I LISTENING ON ANY PORT 8080
HTTPD061I STARTING SOCKET THREAD    TCB(9CD9D0) TASK(10CFC8) STACKSIZE(32768)
HTTPD061I STARTING WORKER(11ED48)   TCB(9CD420) TASK(120FC8) STACKSIZE(65536)
HTTPD001I HTTPD <version> READY - SERVING /www
```

That transcript is a system with UFSD running. Without it, `HTTPD044W` appears
between `HTTPD004I` and `HTTPD054I` — and the last line still reads
`SERVING /www`, because it names the configured document root rather than a
working filesystem (step 2).

The two build stamps identify exactly what is running: `HTTPD000I` gives the
version and the commit it was built from, `HTTPD005I` the libc370 it was linked
against — quote both in a bug report. A build made from a modified working tree
marks its hash `-DIRTY` and adds `HTTPD006W`; a released build never does.

**The configuration is not echoed at startup** — not the member, not the
codepage, not the task limits. `/F HTTPD,D CONFIG` reports all of it on demand,
and that one look after the first start is worth taking: it prints what HTTPD
*parsed*, not what the member says, so a typo that fell back to a default is
visible there and nowhere else.

**A refused start is recognised by the step return code, never by a console
string.** A start refused by initialisation ends `CC 0008`; a clean `/P HTTPD`
ends `CC 0000`. The first console line is the cause (`HTTPD037E`, `HTTPD028E`,
`HTTPD030E`, `HTTPD031E`, `HTTPD420E`, `HTTPD033E` or `HTTPD090E`); everything
under it is the ordinary stop sequence and says nothing about what went wrong.
The full contract is in
[messages.md](https://github.com/mvslovers/httpd/blob/main/docs/messages.md).

Then reach it from a client:

```
curl -v http://your-mvs-host:8080/zosmf/info      mvsMF installed
curl -v http://your-mvs-host:8080/                static, needs UFSD
```

Operator commands:

```
/F HTTPD,DISPLAY Config      the configuration actually in effect  (D C)
/F HTTPD,DISPLAY Stats       request/error/byte counters, SMF level (D S)
/F HTTPD,DISPLAY Threads     the worker pool, block by block        (D T)
/F HTTPD,DISPLAY Ports       the listening port                     (D P)
/F HTTPD,DISPLAY Login       who is logged in                       (D L)
/F HTTPD,DISPLAY Version     build stamps again                     (D V)
/F HTTPD,DISPLAY TIme        server time and offset                 (D TI)
/F HTTPD,DISPLAY Memory xxxxxx[,nnn]                                (D M)
/F HTTPD,SET MIntask n       resize the worker pool
/F HTTPD,SET MAxtask n
/F HTTPD,SET Stats NONE|ERROR|AUTH|ALL [RESET]
/F HTTPD,HELP                every MODIFY command
/P HTTPD                     orderly stop (HTTPD099I SHUTDOWN COMPLETE)
```

Commands abbreviate to the capitalised prefix, which is why `DISPLAY Threads`
and `DISPLAY TIme` need one and two letters respectively.

---

## 10. What HTTPD serves

Two kinds of route, both declared in the configuration member, both carrying the
same per-route auth policy:

- **`MOD=` — a program.** An MVS load module dispatched on the worker thread's
  own TCB through the LINK SVC, once per request. mvsMF is the one that matters
  in practice; the three display modules ship with the server.
- **`LOC=` — a static prefix.** No program: the request falls through to the
  static file handler on `DOCROOT`, but the prefix still carries `AUTH=`/`RES=`.
  This is how a static subtree gets a login without a CGI.

Routes are tested **in order** and matched exactly unless they carry a `*`, so
list specific prefixes before a catch-all — a `LOC=/*` placed above
`MOD=MVSMF /zosmf/*` shadows it.

HTTP/1.1 with persistent connections and chunked transfer encoding is the
default for 1.1 clients; 1.0 clients always get `Connection: close`.

The full reference is
[configuration.md](https://github.com/mvslovers/httpd/blob/main/docs/configuration.md);
writing your own module is
[development.md](https://github.com/mvslovers/httpd/blob/main/docs/development.md).

---

## 11. Removing HTTPD

Because the installation is SMP-managed, there is a defined way back — but it is
**not** the `RESTORE` followed by `REJECT` that SMP documentation leads you to
expect. The install job accepts the FMID in the same run as the APPLY, and an
accepted function SYSMOD refuses both: `RESTORE` because it was accepted,
`REJECT` because accepting removed from `SYS1.SMPPTS` the control statements it
works from. The route that does work is a `UCLIN` job.

What this release put on the system:

| | |
|---|---|
| FMID | `THTP400` |
| Load modules | `HTTPD`, `HTTPDSRV`, `HTTPDM`, `HTTPDMTT`, `ABEND0C1` |
| Target library | `HTTPD.<vrm>.LINKLIB` |
| Distribution library | `HTTPD.<vrm>.AHTTPLOD` |
| Sample library | `HTTPD.<vrm>.SAMPLIB` |

**1. Stop the server:** `/P HTTPD`

**2. Cut the FMID out of the SMP inventory.** Submit this — it edits the CDS and
the ACDS and touches no library:

```
//HTTPDUCL JOB (SYS),'HTTPD UNINSTALL',
//             CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1),
//             REGION=4096K
//UCLIN   EXEC SMPAPP
//SMPCNTL  DD  *
 UCLIN CDS .
  DEL SYSMOD(THTP400) MOD(HTTPD) .
  DEL SYSMOD(THTP400) MOD(HTTPDSRV) .
  DEL SYSMOD(THTP400) MOD(HTTPDM) .
  DEL SYSMOD(THTP400) MOD(HTTPDMTT) .
  DEL SYSMOD(THTP400) MOD(ABEND0C1) .
  DEL MOD(HTTPD) .
  DEL MOD(HTTPDSRV) .
  DEL MOD(HTTPDM) .
  DEL MOD(HTTPDMTT) .
  DEL MOD(ABEND0C1) .
  DEL LMOD(HTTPD) .
  DEL LMOD(HTTPDSRV) .
  DEL LMOD(HTTPDM) .
  DEL LMOD(HTTPDMTT) .
  DEL LMOD(ABEND0C1) .
  DEL SYSMOD(THTP400) .
 ENDUCL .
 UCLIN ACDS .
  DEL SYSMOD(THTP400) MOD(HTTPD) .
  DEL SYSMOD(THTP400) MOD(HTTPDSRV) .
  DEL SYSMOD(THTP400) MOD(HTTPDM) .
  DEL SYSMOD(THTP400) MOD(HTTPDMTT) .
  DEL SYSMOD(THTP400) MOD(ABEND0C1) .
  DEL MOD(HTTPD) .
  DEL MOD(HTTPDSRV) .
  DEL MOD(HTTPDM) .
  DEL MOD(HTTPDMTT) .
  DEL MOD(ABEND0C1) .
  DEL SYSMOD(THTP400) .
 ENDUCL .
/*
//LIST    EXEC SMPAPP
//SMPCNTL  DD  *
 RESETRC .
 LIST CDS  SYSMOD(THTP400) .
 LIST ACDS SYSMOD(THTP400) .
/*
//
```

Every `DEL` reports `HMA2550 UPDATE COMPLETE`, and each `UCLIN` block ends
`RC 00`.

**3. Read the LIST — this is the actual result.** Both zones must answer:

```
THE FOLLOWING SELECTED ENTRIES WERE NOT FOUND OR WERE NOT ELIGIBLE
FOR PROCESSING
 TYPE        NAME
 SYSMOD      THTP400
```

with `HIGHEST RETURN CODE IS 04`. **RC 04 and an empty list means the FMID is
free.** Both zones matter: the CDS records what is applied, the ACDS what is
accepted, and they are separate inventories — an id gone from one and present in
the other is not free.

**4. Scratch the libraries.** `UCLIN` edits the inventory only; both datasets are
still there, and a re-install's allocation job would fail on them:

```
  DELETE HTTPD.<vrm>.LINKLIB  NONVSAM SCRATCH PURGE
  DELETE HTTPD.<vrm>.AHTTPLOD NONVSAM SCRATCH PURGE
```

Leave `HTTPD.<vrm>.SAMPLIB` alone if you like — the install job's `DELOLD` step
scratches it on its own.

**5. What is not removed, because SMP never owned it:** the procedure and the
configuration member you copied in step 7, and your RAKF definitions. Those are
yours to delete.

> The `RESTORE`/`REJECT` behaviour above was measured on 2026-08-14 against an
> accepted FMID from a package built by this same generator, on an MVS/CE system
> running SMP 4 level 04.48. It is a property of SMP 4 and of accepting a
> function SYSMOD, not of any one product — but the `UCLIN` job above has not
> itself been run against `THTP400`. Read the `LIST` output rather than trusting
> the `DEL` cards.

---

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| Allocation job fails, dataset already exists | It was already run. Do not force it — see the warning in step 4 |
| `RECV1`/`RECV2` fails, target exists | Something else allocated it. RECEIVE refuses to merge; scratch it and re-run |
| `APPLYCHK` ends non-zero, `APPLY` skipped | Read the SMP output — the check exists to stop before anything is written. A missing DD is the usual cause |
| `APPLY` RC 00 but fewer than five `HMA2380` lines | A module was lost. Count them, then look in the library itself |
| SMP reports success, but a module is not where you expected | A ddname says nothing about the dataset behind it. Check the JCL, then look at the library itself |
| `S806` (module not found) at `/S HTTPD` | `STEPLIB` in the procedure does not name the LINKLIB the APPLY wrote to |
| `/S HTTPD` rejected — procedure not found | Procedure not copied into a PROCLIB in the started-task concatenation |
| `HTTPD012E … UNABLE TO DYNAMICALLY OBTAIN APF AUTHORIZATION`, no listener | No RAKF (so no SVC 244) and no APF entry — step 2. This one is fatal, not a warning |
| `HTTPD014E`/`015E`/`016E` and the step ends before any banner | A `SYSPRINT`, `SYSTERM` or `SYSIN` DD in the procedure — step 7 |
| `HTTPD037E HTTPD IS ALREADY ACTIVE ON PORT n` | An older instance still holds the port. `/P HTTPD` first — step 5 |
| `HTTPD004W RACINIT ENVIR=CREATE FAILED` | The `STCUSER`/`STCGRP` userid is not defined to RAKF — step 8. Not fatal; the server runs on the inherited STC identity |
| `HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM` | UFSD is not running. Module routes are unaffected; only static files fail — step 2. `HTTPD001I` still names the configured `DOCROOT`, so it is not the line to read here |
| `HTTPD908E EXTERNAL PROGRAM MVSMF …` on every `/zosmf/` request | mvsMF is not in the STC's STEPLIB — step 2. The parenthetical "(not found in STEPLIB?)" is a guess; the real abend code is in the `IEA703I` line beside it, and `106-0F` means storage, not a missing member |
| `HTTPD420E ROUTE AUTHORIZATION POLICY INCOMPLETE`, `CC 0008` | A route's `AUTH=` value is not `NONE`/`FORM`/`BASIC`/`TOKEN`, or a `LOGIN=` line survives in the member — step 7 |
| `HTTPD425W NO PROFILE FOR …` | A `RES=` names a resource no RAKF profile covers, so that route is gated by `AUTH=` alone — step 8 |
| A route answers `200` to anyone | It carries no `AUTH=`, or no route claims the path at all. Both are public by design — step 7. `/.dsrv?target=MOD` prints the policy each route actually got |
| `401` on a route showing `AUTH=NONE` | Not HTTPD's gate. mvsMF runs its own auth track on `/zosmf/*` — establish which layer answered before debugging HTTPD's |
| `S106` at start on a freshly installed library | The XMIT was uploaded in text mode. Re-upload in **binary** and re-run the install job |
| The step ends `CC 0000` but nothing was ever served | A clean `/P HTTPD` and a refused start are `CC 0000` and `CC 0008`. If it is 0000, the server ran — look for what stopped it |

For the complete message reference, see
[messages.md](https://github.com/mvslovers/httpd/blob/main/docs/messages.md).
