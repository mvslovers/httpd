# HTTPD Installation Guide

This guide installs HTTPD on an MVS 3.8j system (TK4-, TK5, MVS/CE, or a custom
Hercules build) from the release distribution archive.

The installation is managed by **SMP Release 4** — the SMP that ships with
MVS 3.8j, not SMP/E. That means the system keeps a record of what was
installed, a supported way to replace it with the next release (see
[Upgrading from 4.0.x](#12a-upgrading-from-40x) if that is where you are
coming from) and a supported way back out (see
[Removing HTTPD](#12-removing-httpd)).

## Getting help

**Report problems as a GitHub issue:**
<https://github.com/mvslovers/httpd/issues>

That is the only place bug reports are tracked. Please include the two build
stamps HTTPD writes at startup (`HTTPD000I` and `HTTPD005I`, see
[step 10](#10-start-and-verify)) — they identify the exact build — plus the
console messages and, for an install problem, the job output.

**Questions and general support** are on Discord:
<https://discord.gg/gaUAFKGCR>

---

One placeholder is used throughout:

| | |
|---|---|
| `<version>` | the release, e.g. `4.4.0` — it appears in every shipped file name |

It is already filled in inside the shipped jobs; you only need it to recognise
which file is which.

**The dataset names carry no version.** `HTTPD.LINKLIB` is `HTTPD.LINKLIB` in
every release, so there is one installation on the system and no working out
which release a name belongs to. Up to and including 4.0.2 they were versioned
— `HTTPD.V4R0M2.LINKLIB` — and two releases could sit side by side; 4.1.0 ends
that.

**Upgrading from 4.3.0 needs nothing uninstalled first.** This release's
SYSMOD carries `DELETE(THTP430)`, so SMP deletes the 4.3.0 modules from
`HTTPD.LINKLIB` and copies the new ones into it during the same APPLY, and
ownership of each module moves with them. You stop the server (step 5), skip
step 4 because the libraries are already there, and submit the install job.

**Coming from an older release, go through each release in turn.** This
release deletes `THTP430` and nothing else. On a 4.2.0 system the modules still
belong to `THTP420`, and a SYSMOD that does not own a module cannot replace it:
SMP selects none of them, copies nothing, and every step still ends `RC 00`. So
4.2.0 upgrades to 4.3.0 first, with the 4.3.0 package, and then to this
release; a 4.1.0 system goes through 4.2.0 and 4.3.0 in the same way. A 4.0.x
system starts with 4.1.0, the release that crosses from `THTP400` and the one
move to the unversioned dataset names —
[section 12a](#12a-upgrading-from-40x) has that order, done with the **4.1.0**
package — then 4.2.0, then 4.3.0, then this release.

The webroot disk is never touched by any of this: it holds your content, not
ours. See step 9.

**Does `HTTPD.LINKLIB` already exist on a system that has no HTTPD from SMP?**
Then the allocation job in step 4 does not do what you want, and it can even end
with RC 0. The library has to be moved out of the way first. That is the case
on TK5, which ships HTTPD 3.3 in its own `HTTPD.LINKLIB`, and on a system where
somebody filled one by hand. Read
[step 4](#a-library-that-smp-does-not-own) before you submit anything.

> **SECURITY — before you start the new server, check every route in your
> Parmlib member for an `AUTH=`.**
>
> SMP never touches your PARMLIB, so an upgrade starts the new server on the
> member you wrote for the old one. **A route without `AUTH=` is public**, and
> so is every path no route claims — there is no global login policy to fall
> back on any more. A member written against a release that had one can carry
> routes that were protected by it and are now open to anyone who can reach the
> port:
>
> ```
> MOD=MVSMF /zosmf/*                   <- public: no AUTH=
> MOD=MVSMF /zosmf/*  AUTH=TOKEN       <- gated
> ```
>
> Open the member and read it line by line: every `MOD=` and `LOC=` line has an
> `AUTH=`, and `AUTH=NONE` stands only where the route is public on purpose.
> Then ask for a gated route without a credential — it must answer `401`
> (`AUTH=BASIC` and `AUTH=TOKEN`) or the HTML login page (`AUTH=FORM`), never
> the content. [Step 7](#editing-httpprm0) has the modes.
>
> Do not take a `401` from `/zosmf/*` as proof that the HTTPD gate is closed.
> mvsMF refuses a request that carries no credential on its own account, so
> the route answers `401` with or without `AUTH=`. A display module and a
> static `LOC=` prefix have no such second check.

---

## 1. What is in the archive

| File | What it is |
|------|------------|
| `README.md` | this guide |
| `httpd-<version>-load.xmit` | the five load modules, as a TSO RECEIVE-ready XMIT |
| `httpd-<version>-samplib.xmit` | the sample library: STC procedure and configuration pattern |
| `httpd-<version>-alloc.jcl` | allocates the datasets SMP installs into — **run once** |
| `httpd-<version>-inst.jcl` | receives everything and installs it — repeatable |
| `httpd-webroot.img` | the document root as a UFS370 disk image — outside SMP, see [step 9](#9-the-webroot-disk) |

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

> **The four are diagnostic, debugging and development tools.** They ship so
> that a problem on a live system can be looked at without a private build —
> reading a control block beats guessing at one, and `/.dmtt` is often the
> fastest way to the console log. They are not production endpoints: a server
> whose job is hosting an API has no business offering an arbitrary-storage
> reader alongside it, and each of them needs an `AUTH=` of its own the moment
> you route to it. Every 4.x release up to and including this one delivers
> them.
>
> Nothing depends on them. `HTTPD` is the only module the started task names,
> and a server that never routed to the other four behaves the same without
> them.

The sample library holds three members: `HTTPD` (the started task procedure),
`HTTPPRM0` (the configuration pattern) and `HTTPWEBR` (a job that allocates the
dataset the webroot image is uploaded to).

Where everything ends up:

```
HTTPD.LINKLIB     the load modules -- point STEPLIB here
HTTPD.SAMPLIB     the patterns you copy from in step 7
HTTPD.AHTTPLOD    SMP's distribution library, the base a RESTORE returns to
HTTPD.WEBROOT.UFS the document root, if you upload the shipped one (step 9)
```

The last one is not SMP's: it is a filesystem image, and it holds site content
that an `APPLY` must never touch. Step 9 is where it comes from.

---

## 2. Prerequisites

- **MVS 3.8j** on Hercules, up and IPLed, with SMP 4 usable (the `SMPREC` and
  `SMPAPP` procedures and the `SYS1.SMP*` datasets — present on TK4-/TK5 and
  MVS/CE as shipped).
- **RAKF** — see [Authorisation](#authorisation) below. HTTPD does not start
  without either RAKF or an APF list entry, and no configuration option turns
  that off.
- **UFSD**, only if you want to serve static files — see below. The archive
  ships a document root for it as a disk image (step 9).
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

HTTPD 4.4.0 is built against `libufs` 1.5.1 (UFSD 1.5.1). Running it against a
UFSD older than 1.5.0 has not been tested; from 1.5.0 on, `ufs_ferror()` reports
a read error reliably, which the response to a failed file read relies on. This
is a note, not a prerequisite: the SYSMOD declares none.

Note that `HTTPD001I` still reads `READY - SERVING /www` afterwards: it names
the **configured** document root, not a working filesystem. `HTTPD044W` is the
line that tells you the filesystem is not there, and it is written earlier in
the start. On a deployment that will never have UFSD, `UFS=0` in the
configuration member skips the initialisation altogether and the warning with
it.

With UFSD there, the archive has a document root ready for it —
`httpd-webroot.img`, a formatted filesystem holding the welcome page, which
[step 9](#9-the-webroot-disk) uploads and mounts.

Install HTTPD now and add UFSD later if you want it:
<https://github.com/mvslovers/ufsd/releases>, with its own guide at
[ufsd/docs/installation.md](https://github.com/mvslovers/ufsd/blob/main/docs/installation.md).

Note that UFSD has to be **running**, not just installed — it is a started task
HTTPD talks to across address spaces.

### mvsMF is what the shipped configuration expects

`HTTPPRM0` ships with four active routes, all of them to **mvsMF** — the
z/OSMF-compatible REST API, which is a separate product:

```
MOD=MVSMF /zosmf/info                    AUTH=NONE
MOD=MVSMF /zosmf/services/authenticate   AUTH=NONE
MOD=MVSMF /zosmf/restconsoles/*          AUTH=TOKEN RES=FACILITY:MVSMF.CONSOLE
MOD=MVSMF /zosmf/*                       AUTH=TOKEN
```

The first two carry `AUTH=NONE` on purpose: they are not gated by HTTPD
because mvsMF decides for them. **Neither is anonymous.** Without a credential
`/zosmf/info` answers `401` like every other mvsMF path — there is no
anonymous reachability probe — and the token login endpoint has to be reached
even when the login fails, so that it can answer with the z/OSMF-shaped `401`.
The catch-all behind them is where the datasets, jobs and files live, and it is
gated by HTTPD as well. First match wins, so the order matters. mvsMF's own
installation guide has its side of this.

The third line is the **console service**. It issues operator commands, and
any user who can log in could stop a started task or vary a device. So the
console gets a route of its own that also needs READ on the RAKF resource
`FACILITY MVSMF.CONSOLE` — define it in step 8. What mvsMF does on its own
depends on its level:

| mvsMF | Check inside mvsMF | Without the profile |
|---|---|---|
| up to 1.3.0 | none: the route is the only gate | every user who can log in may issue operator commands |
| 1.4.0 and up | READ on `FACILITY MVSMF.CONSOLE`, under the user's own identity | mvsMF refuses every user (`500`, category 4) until the profile is defined |

HTTPD's check comes first at every level and answers `403` to a user without
READ. Keep the route and its `RES=` at every level. The route has to stand
**before** `/zosmf/*`: listed after the catch-all it is never reached, and
nothing warns you.

If mvsMF is not installed, HTTPD starts and those routes register normally — the
program is not looked for until a request first matches. The failure then shows
up per request as

```
HTTPD908E EXTERNAL PROGRAM MVSMF was not found
```

Comment the four lines out, or install mvsMF:
<https://github.com/mvslovers/mvsmf/releases>. Its load library has to be in the
STC's STEPLIB concatenation — HTTPD dispatches a module through the MVS LINK
SVC, which searches the task's own libraries.

#### mvsMF's jobs API and the JES2 DDs

The same mechanism decides more than where the module is found. A module runs
**in HTTPD's task**, so every ddname it opens has to be allocated by the STC
procedure — a CGI has no allocations of its own.

mvsMF's jobs API reads the JES2 checkpoint and spool directly, by DD name, and
the shipped procedure allocates them:

```
//HASPCKPT DD  DISP=SHR,DSN=SYS1.HASPCKPT,UNIT=3350,VOL=SER=MVS000
//HASPACE1 DD  DISP=SHR,DSN=SYS1.HASPACE,UNIT=3350,VOL=SER=SPOOL1
```

**Check `UNIT` and `VOL=SER` against your own JES2 procedure** before the first
start — they are site values, not constants, and the ones above are only what
the pattern has always carried.

The two DDs are needed up to mvsMF 1.3.0. With mvsMF 1.4.0 and later they can
go: its jobs API allocates the JES2 checkpoint and spool data sets itself when
the procedure has no DD for them. The exception is a JES2 whose `$DSNPRFX` is
not `SYS1` — there the DDs stay mandatory, because the allocation only looks for
the `SYS1` names. The procedure shipped with this release still carries both
DDs, and leaving them in is always safe.

Where the DDs are needed (mvsMF 1.3.0 or earlier, or a `$DSNPRFX` other than
`SYS1`) and missing, the server starts normally and everything else works. The
jobs API does not — and it does not fail uniformly:

| request | answer |
|---|---|
| job list (`GET /zosmf/restjobs/jobs`) | `500`, `REASON_INCORRECT_JES_VSAM_HANDLE` |
| spool retrieval | `500`, same reason |
| anything resolving one job by name and id | **`404` job not found** |

The 404 is the one to know about. `find_job_by_name_and_id()` returns NULL
whether JES2 is unreachable or the job genuinely is not there, and its callers
report the second. A client is told the job does not exist while it does — so
read the console, not the status code:

```
Unable to open checkpoint dataset DD:HASPCKPT
MVSMF201E UNABLE TO OPEN THE JES2 CHECKPOINT AND SPOOL DATA SETS
```

Job submit is unaffected; it dynallocs its own INTRDR.

If you serve no JES2 traffic at all, the DDs can go. Nothing else in HTTPD opens
them.

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
that comes from RAKF**. The clean alternative is to add `HTTPD.LINKLIB` to
the APF list in `SYS1.PARMLIB(IEAAPF00)`. On MVS 3.8j the APF list is only read
at IPL, but the library name carries no version (since 4.1.0): the entry is
added once, costs one IPL, and stays right for every later release. An entry
is the data set name and the volume, so keep the library on that volume. An
entry whose data set no longer exists on that volume stays valid for any data
set created later under that name on that volume: remove a stale entry at the
next IPL.

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

### Authors of server modules: the HTTPD level

A server module that calls `http_new_session()` learns **why** a call failed only
from HTTPD 4.5.0: `-2` RACF refused the logon, `-3` no free session slot, `-4`
out of storage or RACF could not be asked. An earlier HTTPD returns `-1` for every
failure. A module that must also run on an earlier HTTPD treats `-1` as a generic
error, not as "not authenticated". The function vector has no run-time test for
the entry; see [server-modules.md](https://github.com/mvslovers/httpd/blob/main/docs/server-modules.md) for the values and the
duty to call `http_logout()`. The SYSMOD declares no requirement for this: it is
a note for the module author, not a prerequisite.

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

The third file that has to reach the host, `httpd-webroot.img`, is **not** one of
these: it is a filesystem image with its own record format, it is not received
by anything, and it is only needed once the server runs. It has its own step —
[step 9](#9-the-webroot-disk) — and nothing here applies to it.

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
[Step 9](#9-the-webroot-disk) walks an IND$FILE upload through in full, for the
webroot image; the same emulator handling applies here.

---

## 4. Allocate the product datasets

Submit `httpd-<version>-alloc.jcl` unchanged, unless you want a specific unit or
volume — the `UNIT=SYSDA` and the space on each DD are the only things worth
editing.

It creates `HTTPD.LINKLIB` and `HTTPD.AHTTPLOD` and nothing else.
The libraries the next step receives into are deliberately **not** allocated
here: TSO RECEIVE creates its own target and refuses to merge into an existing
dataset.

Expect `COND CODE 0000`.

> **Run this once.** There is no DELETE step in it, on purpose. After the
> install, `HTTPD.AHTTPLOD` holds SMP's accepted copy of the modules; a
> re-run that scratched it would leave the SMP inventory reporting an install
> that is no longer on the system, and nothing would say so. To start over, cut
> the FMID out of the inventory first — see
> [Removing HTTPD](#12-removing-httpd). It is `UCLIN`, not `REJECT`: an
> accepted function SYSMOD refuses both `RESTORE` and `REJECT`.
>
> **Skip this step entirely when upgrading from 4.1.0 or later.** The names no
> longer change between releases, so `HTTPD.LINKLIB` and `HTTPD.AHTTPLOD` are
> already allocated and already hold the previous release — which is exactly
> what the APPLY expects to find and replace. A failure here saying the dataset
> exists is that situation, not an error to work around.
>
> **Coming from 4.0.x**, install 4.1.0 first, following
> [section 12a](#12a-upgrading-from-40x) — that is where this step runs. Then
> upgrade to 4.2.0, 4.3.0 and this release, skipping it each time.

### A library that SMP does not own

The job allocates `HTTPD.LINKLIB` and `HTTPD.AHTTPLOD` new, and it cannot do
that over a name that is already catalogued. A system can have such a library
without an SMP installation behind it:

- TK5 ships HTTPD 3.3 in `HTTPD.LINKLIB`, started by the procedure `HTTPD33`.
- On MVS/CE somebody may have filled `HTTPD.LINKLIB` by hand, with the `HTTPD`
  module and whatever was copied in beside it (an mvsMF module, for one).

SMP owns none of the members of such a library, and a SYSMOD cannot replace
what it does not own. This guide installs only into a library the allocation
job created, so the existing one is moved out of the way first and the install
then runs as on a first installation. The old library stays on the disk until
the new server has started, which is also your way back.

**Do not read the allocation job's return code as the answer.** Measured on
TK5, allocating a name that is already catalogued ends the step with
`COND CODE 0000`, and the only sign is a line in the job log:

```
IEF287I   HTTPD.LINKLIB                           NOT CATLGD  2
```

The existing library is untouched, and the new one exists on a volume without a
catalog entry. If you go on, the install job writes into the old library, SMP
selects nothing, and every step ends `RC 00` (see the troubleshooting table). If
you find that line in the output, scratch the stray copy on the volume the
following `IEF287I` line names (ISPF 3.4 with that volume serial, since it has
no catalog entry) before you continue. Both libraries carry the same name, so
scratch only on that volume: the volume of the catalogued old library is left
alone, and a delete through the catalog would remove the old library, not the
stray one.

1. **Stop the server that uses the library**, whatever its procedure is
   called (`/P HTTPD33` on TK5). A library in use cannot be renamed: the rename
   job starts and then waits, without ending, until the server is gone.
2. **List the members** (ISPF 3.4) and write
   down everything that is not one of HTTPD's own modules. You deal with it in
   the last point.
3. **Rename the library** with IDCAMS. It changes the catalog entry and the
   entry on the volume together:

   ```jcl
   //HTTPDREN JOB (SYS),'HTTPD RENAME',
   //             CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1),
   //             REGION=4096K
   //RENAME   EXEC PGM=IDCAMS
   //SYSPRINT DD  SYSOUT=*
   //SYSIN    DD  *
     ALTER HTTPD.LINKLIB NEWNAME(HTTPD.OLD.LINKLIB)
   /*
   ```

   Expect `IDC0531I ENTRY HTTPD.LINKLIB ALTERED` and `COND CODE 0000`, and
   check it in ISPF 3.4: `HTTPD.OLD.LINKLIB` is listed and `HTTPD.LINKLIB` is
   gone. The procedure of the old server still
   names `HTTPD.LINKLIB` in its `STEPLIB`; point it at the new name if the old
   server is to stay startable, and leave it alone if it is not.
4. **Look at the APF list** (`SYS1.PARMLIB(IEAAPF00)`) for `HTTPD.LINKLIB`;
   [Authorisation](#authorisation) says why it matters. An entry is a data set
   name *and a volume*, and it authorises whichever data set carries that name
   on that volume. If there is one, allocate the new library on the volume it
   names: add `VOL=SER=volume` to the two DDs of the allocation job, beside
   `UNIT=SYSDA`. Without that the new library may land on another volume, the
   entry no longer matches it, and HTTPD falls back to SVC 244 (`HTTPD002I`
   says which route it got). Moving the entry to the new volume instead costs an
   IPL, because the list is read only then. An entry whose data set no longer
   exists on its volume stays valid for any data set created later under that
   name on that volume, so remove a stale entry at the next IPL.
5. **Install** as on a first installation: steps 3, 4 and 6. The `DELOLD` step
   of the install job scratches `HTTPD.HTTPLOAD` and `HTTPD.SAMPLIB`; if you
   keep something of your own under either name, rename it the same way first.
6. **Start the new server and read its banner** (step 10), then deal with what
   was not HTTPD's. A module of another product is best installed again by that
   product's own guide; `IEBCOPY` from `HTTPD.OLD.LINKLIB` will do if all you
   need is the server running. Scratch `HTTPD.OLD.LINKLIB` only when everything
   runs from the new library.

---

## 5. Stop a running HTTPD

Only relevant when you are upgrading, and since 4.1.0 it is **not optional**.
The APPLY deletes the old modules from `HTTPD.LINKLIB` and writes the new ones
there — the one library, the same one a running HTTPD has its modules loaded
from, because the names no longer carry a version. Up to 4.0.2 each release had
its own library and an older server could keep running through the install;
that is no longer true.

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

**`/P HTTPD` returns at once; the server does not end at once.** With clients
still connected, the workers are waited for one by one, and the address space
needs about 45 to 50 seconds to end (measured with six keep-alive clients). Wait
for `$HASP395 HTTPD ENDED` before you submit the install job, because the APPLY
writes into the library the old server still has open, and before any `/S HTTPD`
([restarting](#restarting), step 10).

---

## 6. Install

Open `httpd-<version>-inst.jcl` and replace the two placeholder dataset names
with what you uploaded in step 3:

```
  RECEIVE INDSN('CHANGE.ME.HTTPLOAD') -     <- IBMUSER.HTTPD.LOAD.XMIT
  RECEIVE INDSN('CHANGE.ME.SAMPLIB') -      <- IBMUSER.HTTPD.SAMP.XMIT
```

On TK5 those are the only lines you have to change. **On MVS/CE, name the
volume**: add a `VOLUME()` operand to both of these `RECEIVE` commands (steps
`RECV1` and `RECV2`) — `PUB000` on a stock MVS/CE, or another volume of yours.
TSO RECEIVE creates its target itself and has to find a volume to put it on;
whether it finds one depends on the system, and an explicit volume costs
nothing where it would have found one anyway. For the load library:

```
  RECEIVE INDSN('IBMUSER.HTTPD.LOAD.XMIT') -
    DATASET('HTTPD.HTTPLOAD') -
    VOLUME('PUB000')
```

Submit it.

The job runs eight steps, each conditional on the one before, so it stops at
the first failure rather than building on it:

| Step | What it does |
|------|--------------|
| `DELOLD` | scratches the RECEIVE targets, so the job can be re-run |
| `RECV1` | load XMIT → `HTTPD.HTTPLOAD` (a staging library) |
| `RECV2` | samplib XMIT → `HTTPD.SAMPLIB` |
| `RECV` | receives the SYSMOD into the SMP inventory |
| `APPLYCHK` | dry run — `APPLY` only proceeds if this ends RC 0 |
| `APPLY` | deletes the previous release's modules from `HTTPD.LINKLIB`, then copies the five new ones in |
| `ACCEPT` | makes this level the base a later `RESTORE` returns to, and deletes the previous level from the distribution zone |
| `CLEANUP` | scratches the staging library, which is now spent |

The SYSMOD travels inline in the job — there is no third file to upload.

**What a good run looks like.** Every step `COND CODE 0000`, and in the SMP
output:

```
HMA3930    SYSMOD THTP440 SUCCESSFULLY RECEIVED
HMA2240    SUCCESSFULLY DELETED LMOD=HTTPD
HMA2380    COPY SUCCESSFUL - MOD=HTTPD - LMOD=HTTPD - LIBRARY=LINKLIB
           - RETURN CODE=00
```

There is one `HMA2380` line **per module** — five of them. A run that copies
fewer has lost one, and the `APPLY` still ends successfully, so count them. The
`HMA2240` lines appear only when a previous release was there to delete; on a
first install there are none.

> **On an upgrade the `APPLY` ends `COND CODE 0004`, and that is correct.** The
> deleted level has no `SMPSCDS` backup entry and never will, so SMP reports
> `HMA2461 … NOT FOUND ON SMPSCDS LIBRARY` *after* `HMA2270 … SUCCESSFULLY
> COMPLETED`. The `ACCEPT` step is gated `COND=(4,LT,APPLY.HMASMP)` for exactly
> this, so it still runs. A first install ends RC 00 throughout.

**Then list the target library, and treat that as the actual result.** It is
the only check that distinguishes a real install from a silent one: SMP reports
the library by **ddname**, and a ddname says nothing about which dataset was
behind it, while a SYSMOD that does not own a module reports `NOT SEL` in one
column of the element summary, copies nothing, and leaves every condition code
reading success. ISPF 3.4 does, or:

```jcl
//LIST    EXEC PGM=IEHLIST
//SYSPRINT DD  SYSOUT=*
//DD1      DD  UNIT=SYSDA,VOL=SER=your-volume,DISP=SHR
//SYSIN    DD  *
 LISTPDS DSNAME=HTTPD.LINKLIB,VOL=SYSDA=your-volume
/*
```

`your-volume` is the volume the library is on. The `DD1` statement is
required: without it `IEHLIST` ends `IEH106I UNAVAILABLE DEVICE TYPE OR VOLUME
I.D. SPECIFIED`, whatever the `VOL=` operand says. That message also comes
back when the device type in `VOL=type=volser` does not match the volume.
`SYSDA=` worked on TK5 and on MVS/CE; `SYSALLDA=` did not on TK5. If your
system does not know `SYSDA`, write the volume's real device type instead
(`3350`, `3380` or `3390`), for example `VOL=3390=your-volume`.

The directory lists the five members `HTTPD`, `HTTPDSRV`, `HTTPDM`,
`HTTPDMTT` and `ABEND0C1`, and any module that was copied in beside them
(`MVSMF` is the usual one). `IEHLIST` prints each member's attributes only as
hex, so it does not show the `AC(1)` authorisation code; `AMBLIST` below
does.

SMP **copies** these modules rather than re-binding them, which is why the
`AC(1)` authorisation code on `HTTPD` and every module's custom entry point are
exactly what the build produced. `AMBLIST LISTLOAD OUTPUT=MODLIST` on `HTTPD`
reports `APFCODE 00000001` with `RENT`/`REUS` intact, so the library only needs
to be APF-authorised — the module already carries what it needs.

---

## 7. Install the procedure and the configuration

SMP does not touch your PROCLIB or PARMLIB, and that is deliberate: **the
product owns the patterns, your system owns the copies.** If SMP owned the
running procedure, every change you made to it would be silently replaced by
the next update. So this step is yours, and it is the one place where you have
to read what you are copying.

Copy from `HTTPD.SAMPLIB`:

| Member | Copy to | Adjust |
|--------|---------|--------|
| `HTTPD` | a PROCLIB in the started-task concatenation | usually not — `STEPLIB` already names this release's LINKLIB. Add mvsMF's library to it if you use mvsMF |
| `HTTPPRM0` | a PARMLIB | **yes — see below** |

`SYS2.PROCLIB` is the usual home for the procedure.

The third member, `HTTPWEBR`, is not copied anywhere: it is a job you submit
straight from the sample library when you get to [step 9](#9-the-webroot-disk).

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

### The region — `REGION=`

The shipped procedure runs the server with `REGION=8M`. A mixed load of 6
parallel requests across all module types ran clean in it.

**Raise it before you raise `MAXTASK`, and when you expect many REXX or Lua
requests at the same time.** Every request that runs a server module takes one
contiguous block of storage for the C stack of that module: 262,328 bytes for a
module that sets no stack size of its own, which is the case for HTTPREXX and
HTTPLUA, and about 64 KB for mvsMF. Up to `MAXTASK` of them run at once, each
on a worker thread with a stack of 64 KB of its own. Against the 4.4.0 release
candidate, 60 REXX requests with 9 at a time failed in some runs with
`REGION=8M`, and in none with `REGION=16M` or with `RESIDENT=NO` (one run each).

To raise it, change the `REGION=` of the `EXEC` statement in your copy of the
procedure:

```
//HTTPD    EXEC PGM=HTTPD,REGION=16M,TIME=1440,
```

If 16M is not enough, lower `MAXTASK`. The numbers, the table of what each part
costs and the full recommendation are in
[configuration.md](https://github.com/mvslovers/httpd/blob/main/docs/configuration.md#region-size-and-parallel-module-requests-region-maxtask).
The symptom of a region that is too small is under *Troubleshooting* at the end
of this guide.

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
plus the four active `MOD=MVSMF` route lines. Four things are worth a look
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
them the console log. They are debugging tools (step 1) — enable them while you
are diagnosing something, and gate them whenever you do:

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

1. Add **one line** for the HTTPD user to `SYS1.SECURE.CNTL(USERS)`. The member
   is a table with fixed columns, not RACF command syntax:

   | Columns | Holds | For HTTPD |
   |---|---|---|
   | 1-8 | userid | the `STCUSER` of step 7 (`HTTPD`) |
   | 10-17 | group | the `STCGRP` of step 7 (`USER`) |
   | 19-26 | password | MVS/CE (RAKF 2.0.0): blank. TK5 (RAKF 1.2.x): **required** |
   | 28 | Operations | `N` |
   | 30 | Special | `N`, or blank |

   MVS/CE:

   ```
   123456789012345678901234567890
   HTTPD    USER              N
   ```

   TK5 (`CHANGEME` is a placeholder: choose the password before you paste
   this line):

   ```
   123456789012345678901234567890
   HTTPD    USER     CHANGEME N
   ```

   (The first line of each block is a ruler for the columns, not part of the
   member.) On TK5 a line without a password is refused when the table is
   reloaded: `S RAKFUSER` ends with `RAKFUIDS2 INPUT DATA INVALID OR OUT OF
   SEQ.`, `RAKFUIDSX ** PROGRAM TERMINATED **` and `ABEND U0200`. HTTPD never
   uses the password — it logs on without one — but a userid with a password is
   an account anyone can try to log in to, so choose one you would give an
   account of that group, not `CHANGEME`.

   RAKF 1.2.x keeps the passwords in `SYS1.SECURE.CNTL(USERS)` as typed, in
   clear text. That member holds the password of every user on the system, so
   it must not be readable by ordinary users: give READ on
   `SYS1.SECURE.CNTL` only to the people who administer RAKF.

   **Why the line is needed.** RAKF has no table of started tasks: every
   started task runs as `STC`/`STCGROUP`, and on the stock profiles that group
   holds ALTER on data sets. At startup HTTPD replaces that identity by logging
   on `STCUSER` in `STCGRP` through RACINIT, without a password, and writes
   `HTTPD004I`. Without a `USERS` line the logon fails
   (`RAKF0004 INVALID ATTEMPT TO ACCESS SYSTEM`, then `HTTPD004W RACINIT
   ENVIR=CREATE FAILED … RC=28`). That is not fatal, and that is the danger:
   HTTPD runs on as `STC`/`STCGROUP`, with far more access than intended.

   **The group that counts is the one on the `USERS` line.** RACINIT accepts
   the group passed from the procedure, but data set access follows the group
   on the user's `USERS` line. Grant access to that group, and keep it equal to
   `STCGRP` so that the two do not drift apart.

   Leave Operations at `N`: with `Y` the account gets operations authority, that
   is data set access without a profile.

   The account needs **READ** on whatever HTTPD itself opens before a client
   identity exists: the Parmlib data set (`DD HTTPPRM`), the document root and
   any other data set it opens. The stock `DATASET *` rule already gives that;
   if you have narrowed it, give the group on the `USERS` line READ on those
   data sets. Without READ on the Parmlib data set HTTPD stops at startup
   (`RAKF0005`, `IEC150I 913-38`, `ABEND S913`).

   It does **not** need `FACILITY SVC244`. HTTPD acquires APF authorisation at
   start and releases it at `/P HTTPD`, and it restores the STC account it
   started under immediately before the release — so both SVC 244 calls are made
   by the same account, rather than requiring the profile on every system.

2. Define the profile for every route that carries `RES=class:resource`.
   They are checked against the identity of the client on each request, so the
   HTTPD user needs none of them. A `RES=` naming a resource **no profile
   covers** is not an error and does not deny: SAF calls an unprotected
   resource allowed, so the authorisation stage does nothing and only the
   `AUTH=` stage is left. HTTPD says so at startup:

   ```
   HTTPD425W NO PROFILE FOR FACILITY:HTTPD.ADMIN -- /admin/* NOT GATED
   ```

   The shipped `HTTPPRM0` has one such route, the mvsMF console service, so
   `HTTPD425W NO PROFILE FOR FACILITY:MVSMF.CONSOLE -- /zosmf/restconsoles/*
   NOT GATED` is what an installation without the profile prints. The warning
   is about HTTPD's own route. With mvsMF up to 1.3.0 it means the console is
   open: until the profile exists every user who can log in may issue operator
   commands. From mvsMF 1.4.0 the module checks READ on the same profile
   itself and refuses every user (`500`, category 4) until it is defined, so
   the console stays closed to everyone.
   **Do not edit `PROFILES` by hand; RAKF's maintainer recommends the commands.**
   On MVS/CE (RAKF 2.0.0) define the profile with `RDEFINE` and `PERMIT`.
   `RDEFINE` writes the universal line of a profile, `PERMIT` adds a line for a
   group or a user id. Run `RDEFINE` first. A hand-edited member that is wrong
   aborts the reload, and on RAKF 2.0.0 it can keep JES2 from starting at the
   next IPL.

   Run the commands in a TSO batch step, each one starting in column 1:

   ```
   //RAKFCMD  JOB (1),'RAKF CMD',CLASS=A,MSGCLASS=H
   //TSO      EXEC PGM=IKJEFT01,DYNAMNBR=20
   //SYSPRINT DD SYSOUT=*
   //SYSTSPRT DD SYSOUT=*
   //SYSTSIN  DD *
   RDEFINE MVSMF.CONSOLE CLASS(FACILITY) UACC(NONE)
   PERMIT MVSMF.CONSOLE CLASS(FACILITY) ID(MVSCONS) ACCESS(READ)
   RDEFINE HTTPD.ADMIN CLASS(FACILITY) UACC(NONE)
   PERMIT HTTPD.ADMIN CLASS(FACILITY) ID(ADMIN) ACCESS(READ)
   /*
   ```

   `SYSTSPRT` shows the answers:

   ```
   RDEFINE MVSMF.CONSOLE CLASS(FACILITY) UACC(NONE)
   RDEFINE: FACILITY profile defined: MVSMF.CONSOLE
   RDEFINE: Run RAKFPROF to activate the changes
   PERMIT MVSMF.CONSOLE CLASS(FACILITY) ID(MVSCONS) ACCESS(READ)
   PERMIT:  access READ    granted to FACILITY/MVSMF.CONSOLE
   PERMIT: ID = MVSCONS
   PERMIT: Run RAKFPROF to activate the changes
   RDEFINE HTTPD.ADMIN CLASS(FACILITY) UACC(NONE)
   RDEFINE: FACILITY profile defined: HTTPD.ADMIN
   RDEFINE: Run RAKFPROF to activate the changes
   PERMIT HTTPD.ADMIN CLASS(FACILITY) ID(ADMIN) ACCESS(READ)
   PERMIT:  access READ    granted to FACILITY/HTTPD.ADMIN
   PERMIT: ID = ADMIN
   PERMIT: Run RAKFPROF to activate the changes
   ```

   The first pair is the console profile that the shipped `HTTPPRM0` needs; the
   second is the `/admin/*` route of the `HTTPD425W` example above, shown for the
   pattern. Both commands accept a group or a user id in `ID()`.
   `PERMIT … ID(TESTER)` grants one more person without a group of their own.

   **Who may run them.** The user must be connected to the group `RAKFADM` and
   have the SPECIAL authority (column 30 of `USERS`, item 1). Without
   SPECIAL `RDEFINE` answers `RDEFINE: Not authorized, no SPECIAL attribute`;
   without the `RAKFADM` group it answers `RDEFINE: RAKFPROF access denied`.
   Operations authority is not needed.

   The commands change `SYS1.SECURE.CNTL(PROFILES)` only. Item 3 puts the
   change in effect. Afterwards the member holds these lines for the console
   profile (class in columns 1–8, resource from column 9, group or user id from
   column 53, access from column 61); check them, do not type them:

   ```
   FACILITYMVSMF.CONSOLE                                       NONE
   FACILITYMVSMF.CONSOLE                               MVSCONS READ
   ```

   On TK5 (RAKF 1.2.x) there are no such commands: a TSO step answers
   `IKJ56500I COMMAND RDEFINE NOT FOUND`, and editing the member is the only
   route. Put the same two lines into `SYS1.SECURE.CNTL(PROFILES)` by hand, with
   the universal entry (blank group) first and the lines in ascending order.

   Access is READ or nothing, and READ means every operator command, from
   console 0 and without a `UCMAUTH` filter: nothing narrows it per command.
   `MVSCONS` is a group name chosen for this example: use a group of its own
   that holds just the people who may issue operator commands, not a broad one
   such as `ADMIN` that exists for other reasons. **The development user needs
   the profile too:** `mbt run restart` and `mbt target console` issue `P`/`S`
   through `/zosmf/restconsoles`. Without READ the request is refused (`403`);
   mbt warns and falls back to the target's Hercules console if it has one, and
   fails with "not delivered" if it has not.

3. Reload the tables with the RAKF started tasks. After item 2 the profiles
   table is the one that matters:

   ```
   /S RAKFPROF                   the profiles table
   /S RAKFUSER                   the USERS table
   ```

   The console answers `RAKFPROF7  RAKF PROFILES UPDATED` and `RAKFUIDS4  USER
   TABLE UPDATED`. `/S RAKF` does both in one step (it adds `RAKF005I RAKF is
   already active`) on MVS/CE and on TK5. There is no `/F RAKF,RELOAD`: it
   answers `IEE341I RAKF NOT ACTIVE` on both.

   You will see whether the line took when HTTPD starts: `HTTPD004I STC IDENTITY
   SET TO HTTPD/USER VIA RACINIT` (step 10).

4. On TK5, once HTTPD is running (step 10), check that the placeholder is not
   the password. With mvsMF installed, try to log in as `HTTPD` with the
   password of the example above; the answer must be a refusal:

   ```
   $ curl -s -o /dev/null -w '%{http_code}\n' -u HTTPD:CHANGEME \
       -X POST -H 'X-CSRF-ZOSMF-HEADER: x' \
       http://host:8080/zosmf/services/authenticate
   401
   ```

   `200` means the placeholder is still the password. The `200` is itself a
   login: HTTPD caches the credential, and a cached login is not checked
   against RACF again until it ends. It ends after `SESSION_TIMEOUT` minutes
   without a request (30 by default) or `SESSION_MAXAGE` minutes after the
   login (480 by default), whichever comes first; see
   [Session lifetime](https://github.com/mvslovers/httpd/blob/main/docs/configuration.md#session-lifetime-session_timeout-session_maxage).
   A client that keeps sending requests keeps the login alive until
   `SESSION_MAXAGE`, and changing the password and reloading the table does
   not end it, so the placeholder keeps working. After a `200`, change the
   password, run `/S RAKFUSER`, then stop and start HTTPD, or wait out
   `SESSION_MAXAGE`.
   A login with the placeholder should never have been accepted; if it was,
   restart HTTPD after the password change.

---

## 9. The webroot disk

`DOCROOT` names a path in the UFSD filesystem, so the pages HTTPD serves live on
a UFS disk and not in a library. The archive carries one ready to use:
`httpd-webroot.img`, a 1 MB UFS370 filesystem holding the welcome page. Three
steps put it on the system — allocate, upload, mount — and none of them belongs
to SMP.

**Why it travels on its own.** SMP has no element type for a `DSORG=PS`,
`RECFM=U` image, and site content is precisely what an `APPLY` must never
touch. TSO RECEIVE is no better: it allocates its own target and refuses to
merge into an existing dataset, which makes it a first-install-only transport
for something you will replace. So the disk ships as a plain file and goes up
over **IND$FILE**, which needs nothing on the target beyond a 3270 session. FTP
and zowe work as well, into the dataset allocated in a); see
[b)](#b-upload-it-with-indfile).

**The dataset is called `HTTPD.WEBROOT.UFS` throughout this guide.** That is
what `HTTPWEBR` allocates, what the upload names and what the `MOUNT` names. The
UFSD guides use `HTTPD.WEBROOT` in their own examples; a command copied from
there and a `MOUNT` copied from here name two different data sets, and the mount
fails for want of the one that was never uploaded. Use one name for all three.

This step needs **UFSD** (step 2). Without it there is nothing to mount and
nothing static to serve; every `MOD=` route works regardless.

**Already have a webroot? Skip this step entirely.** Since 4.1.0 the name
carries no version, so `HTTPD.WEBROOT.UFS` is very likely the disk you are
already serving — this is the one dataset an upgrade must leave alone, and
section 12 never scratches it. The allocate below is `DISP=(NEW,CATLG)` and so
fails on the existing dataset rather than replacing it, but do not rely on that:
if you want the newly shipped content, upload it to a dataset of your own
choosing and mount it, or copy the files in over UFSD.

### a) Allocate the dataset

`HTTPD.SAMPLIB(HTTPWEBR)` does exactly this and nothing else:

```jcl
//ALLOC    EXEC PGM=IEFBR14
//WEBROOT  DD  DSN=HTTPD.WEBROOT.UFS,DISP=(NEW,CATLG),
//             UNIT=SYSDA,SPACE=(4096,256),
//             DCB=(DSORG=PS,RECFM=U,BLKSIZE=4096)
```

**Submitting it from a workstation.** The job card of `HTTPWEBR` has no
`NOTIFY` operand, so nothing on it depends on how you submit it: it runs
through mvsMF's jobs API and by FTP. The output stays in the held class
(`MSGCLASS=H`) until you fetch it. If you add `NOTIFY=` to the card, name your
own userid. `&SYSUID` is a TSO symbol: `SUBMIT` from TSO and mvsMF replace it,
but FTP passes the card as written, and the job ends with `IEF627I INCORRECT
USE OF AMPERSAND IN THE NOTIFY FIELD` and `JOB NOT RUN - JCL ERROR`.

**Run it before the upload, not after.** IND$FILE allocates an *existing*
dataset `DISP=SHR` and takes the DCB from its label; only when the dataset is
missing does it invent one of its own. Allocating it here is what pins
`BLKSIZE=4096` — the block size the image was formatted with, and the one thing
about the transfer that has to be right.

`SPACE` is counted in blocks of 4096, so 256 blocks is the megabyte the image
occupies. **Primary extent only**: the filesystem is addressed by block number
within the primary extent, so anything in a secondary would never be reached.

### b) Upload it with IND$FILE

The image must arrive byte for byte: **binary**, no ASCII translation, no CRLF
conversion. With the dataset allocated, the command carries no options at all —
which also makes it independent of which IND$FILE build your system has:

```
IND$FILE PUT HTTPD.WEBROOT.UFS
```

Binary is IND$FILE's default; `ASCII` and `CRLF` are what you would have to add
to break it.

Most emulators issue that command for you. x3270, c3270 and wc3270 take it as
one action, and their file-transfer dialog asks for the same fields:

```
Transfer(Direction=send,
         LocalFile=httpd-webroot.img,
         HostFile=HTTPD.WEBROOT.UFS,
         Host=tso, Mode=binary, Exist=replace)
```

Leave `Recfm`, `Lrecl`, `Blksize` and the space fields **unset**: the dataset
exists, so its label decides, and an emulator's allocation defaults are not
worth auditing.

Either way the session has to sit on an input field that can accept the command
before the transfer starts — a cleared TSO **READY** screen, or ISPF option 6.
The emulator types `IND$FILE` into whatever field the cursor is on; it does not
find one for you.

If you would rather let IND$FILE create the dataset, it needs to be told
everything the allocation job otherwise says:

```
IND$FILE PUT HTTPD.WEBROOT.UFS (RECFM(U) BLKSIZE(4096) TRACKS SPACE(70)
```

70 tracks holds a megabyte even on a 3350, where a 4096-byte block packs four to
a track; a 3380 or 3390 needs half that. There is no secondary quantity on
purpose (see above).

A megabyte over a 3270 session takes minutes rather than seconds. That is the
transfer, not a hang.

**Without a 3270 session**, FTP and zowe do the same job, again into the dataset
allocated in a) — binary, and name the dataset this guide uses:

```sh
# FTP
ftp mvs-host
binary
put httpd-webroot.img 'HTTPD.WEBROOT.UFS'

# zowe CLI
zowe files upload file-to-data-set httpd-webroot.img "HTTPD.WEBROOT.UFS" --binary
```

These commands are taken from the UFSD
[disk setup guide](https://github.com/mvslovers/ufsd/blob/main/docs/disk-setup.md),
which also has `ufsd-utils upload` for an image of your own. The same rule holds
for all of them: the dataset exists before the transfer, with `RECFM=U` and
`BLKSIZE=4096`, and the transfer is binary.

### c) Mount it

The mount belongs to **UFSD**, not to HTTPD. In its Parmlib member:

```
MOUNT    DSN(HTTPD.WEBROOT.UFS)  PATH(/www)  MODE(RO)
```

Use the Parmlib statement for the default `DOCROOT=/www`. The path keeps the
case you wrote.

UFSD also accepts the mount as an operator command, without a restart. The
operator form spells its operands with `=` and commas where the Parmlib
statement uses parentheses:

```
/F UFSD,MOUNT DSN=HTTPD.WEBROOT.UFS,PATH=/WWW,MODE=RO
```

**The operator command cannot produce a lower-case mount point.** UFSD
upper-cases the whole command before it parses it, so `PATH=/www` mounts `/WWW`.
Paths are case-sensitive, and HTTPD's default `DOCROOT=/www` finds nothing there:
every request answers `404`. With the operator command, set `DOCROOT=/WWW` in
`HTTPPRM0` as well and restart HTTPD. For `/www`, use the Parmlib statement.

`/www` is HTTPD's default `DOCROOT`; if you changed it in `HTTPPRM0`, mount that
path instead, with the same case. `MODE(RO)` is the default and the right
setting here — the shipped disk is product content and nothing in the server
writes to it.

**UFSD first, then HTTPD.** The filesystem has to be up and mounted before the
server that serves from it — a runtime dependency SMP cannot express, which is
why the SYSMOD declares no prerequisite on UFSD at all.

### d) Verify

```
/F UFSD,MOUNT LIST
curl -v http://your-mvs-host:8080/
```

`/` serves `/www/index.html`: HTTPD appends `index.html` to a request for a
directory.

| What you see | What it means |
|---|---|
| `UFSD062E SUPERBLOCK VALIDATION FAILED FOR …` | the dataset's `BLKSIZE` is not the image's 4096 — or the transfer was not binary |
| `HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM` | UFSD is not running (step 2) |
| `404` on `/` | mounted somewhere other than `DOCROOT` — also `/WWW` against `DOCROOT=/www`: `MOUNT LIST` shows the mount point as UFSD holds it |
| a page of mojibake | the transfer translated the bytes; upload again in binary |

### Replacing the content later

**Release the dataset before you re-upload.** IND$FILE allocates `DISP=SHR`, so it
will happily rewrite a dataset UFSD has open — under the server's own buffers.

For a disk mounted at `/www` by the Parmlib statement, stop both address spaces,
upload, and start them again, UFSD first:

```
/P HTTPD
    ... wait for $HASP395 HTTPD ENDED ...
/P UFSD
    ... upload ...
/S UFSD
/S HTTPD
```

`/P HTTPD` returns at once, and the server needs up to about 50 seconds to end
(step 5). Stop UFSD only after `$HASP395 HTTPD ENDED`.

The operator `UNMOUNT` cannot do this for `/www`: it upper-cases `PATH=/www` to
`/WWW` and finds no such mount. It works for a mount point that is upper case,
for example the `/WWW` of the operator mount in step 9c:

```
/F UFSD,UNMOUNT PATH=/WWW
    ... upload ...
/F UFSD,MOUNT DSN=HTTPD.WEBROOT.UFS,PATH=/WWW,MODE=RO
```

For pages of your own, build the image on your workstation with
[ufsd-utils](https://github.com/mvslovers/ufsd-utils) and upload it the same way:

```
ufsd-utils create mysite.img --size 1M --blksize 4096 --owner IBMUSER
ufsd-utils cp -r ./mysite/ mysite.img:/
```

Text files are stored in **IBM-1047**, which is what `ufsd-utils` writes by
default and what HTTPD's static file path translates back with — a UFS file is
converted with that table regardless of the `CODEPAGE=` setting, which governs
MVS datasets and the server's own output. So there is nothing to configure here,
and nothing to match up.

With mvsMF installed there is a second route for single files —
`PUT /zosmf/restfiles/fs/www/index.html` — but only into a filesystem mounted
`MODE(RW)`, which the shipped disk is not.

One thing the image is not: reproducible. It carries its own creation
timestamp, so two builds of the same commit differ byte for byte. Compare it
against the archive you downloaded, never against one you rebuilt.

---

## 10. Start and verify

```
/S HTTPD                      default member (HTTPPRM0)
/S HTTPD,M=HTTPPRM1           alternate config member
/S HTTPD,D='SYS1.PARMLIB'     alternate PARMLIB -- TK5, see step 7
```

A healthy start ends in `HTTPD001I … READY`:

```
HTTPD000I HTTPD <version> (A69A370) STARTING
HTTPD005I LIBC370 2.25.1 (FCDE4E8)
HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)
HTTPD004I STC IDENTITY SET TO HTTPD/USER VIA RACINIT
HTTPD036I MODULE MVSMF REGISTERED FOR /zosmf/info
HTTPD036I MODULE MVSMF REGISTERED FOR /zosmf/services/authenticate
HTTPD036I MODULE MVSMF REGISTERED FOR /zosmf/restconsoles/*
HTTPD036I MODULE MVSMF REGISTERED FOR /zosmf/*
HTTPD054I LISTENING ON ANY PORT 8080
HTTPD061I STARTING SOCKET THREAD    TCB(9CD9D0) TASK(10CFC8) STACKSIZE(32768)
HTTPD061I STARTING WORKER(11ED48)   TCB(9CD420) TASK(120FC8) STACKSIZE(65536)
HTTPD001I HTTPD <version> READY - SERVING /www
```

`READY` is written only once the socket thread, which takes in the connections,
is running. If it is not, `HTTPD064E` takes the place of `READY` and the server
stops (see below).

The build stamps in that transcript are an example, not what to match. The
first shows the version of the release you installed and the commit it was built
from; the second shows the libc370 it was linked against, and that changes from
release to release. Compare them with the release page, not with this one.

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
string.** A refused start ends `CC 0008`; a clean `/P HTTPD`
ends `CC 0000`. The first console line is the cause (`HTTPD037E`, `HTTPD028E`,
`HTTPD030E`, `HTTPD031E`, `HTTPD420E`, `HTTPD033E`, `HTTPD064E` or `HTTPD090E`);
everything under it is the ordinary stop sequence and says nothing about what
went wrong. `HTTPD064E` is the one that comes late: the listener and the threads
were already started, and the socket thread turned out not to be running. Start
the server again; if it repeats, report it with the console log.
The full contract is in
[messages.md](https://github.com/mvslovers/httpd/blob/main/docs/messages.md).

Then reach it from a client:

```
curl -v -u IBMUSER:password http://your-mvs-host:8080/zosmf/info    mvsMF installed
curl -v http://your-mvs-host:8080/                                   static, needs UFSD
```

`/zosmf/info` needs a login: without `-u` mvsMF answers `401`, which is a working
server saying no, not a fault. (The route carries `AUTH=NONE`, so HTTPD lets the
request through; the `401` is mvsMF's own.) Use an account known to RAKF.

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

### Restarting

After `/P HTTPD`, wait for `$HASP395 HTTPD ENDED` before `/S HTTPD`. With
connected clients that takes about 50 seconds (44 to 49 seconds measured with
six keep-alive clients): the listener is closed early, but the workers are waited
for one by one before the address space ends.

- **`S HTTPD` too early ends with `HTTPD037E`.** The old address space still
  holds the port's ENQ, so the new instance refuses to start and ends with
  `CC 0008`. Nothing is damaged; start it again once `ENDED` has appeared.
- **`HTTPD030I` lines are a wait, not an error.** The old address space is gone
  and the TCP/IP stack still reports the port as taken. The new instance tries
  again every `BIND_SLEEP` seconds (default 10), at most `BIND_TRIES` times
  (default 10), and starts by itself as soon as the stack lets the port go.
  Both keywords are set in the Parmlib member and take 1 to 100. If the last
  retry fails too, `HTTPD030E` is written again and the instance ends.
- On the stock MVS/CE and TK5 images the new instance bound within a few seconds
  of the old one ending. If `HTTPD030I` runs for tens of seconds on your system,
  open an issue (see [Getting help](#getting-help)) and include the Hercules
  version and host OS, the times of `HTTPD098I`/`HTTPD099I` and of
  `$HASP395 HTTPD ENDED` against the first `HTTPD030I`, whether
  `IEF450I … ABEND SA03` appeared at the `P`, and when `S HTTPD` was issued
  relative to `ENDED`.

---

## 11. What HTTPD serves

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
[server-modules.md](https://github.com/mvslovers/httpd/blob/main/docs/server-modules.md).

---

## 12. Removing HTTPD

Because the installation is SMP-managed, there is a defined way back — but it is
**not** the `RESTORE` followed by `REJECT` that SMP documentation leads you to
expect. The install job accepts the FMID in the same run as the APPLY, and an
accepted function SYSMOD refuses both: `RESTORE` because it was accepted,
`REJECT` because accepting removed from `SYS1.SMPPTS` the control statements it
works from. The route that does work is a `UCLIN` job.

What this release put on the system:

| | |
|---|---|
| FMID | `THTP440` |
| Load modules | `HTTPD`, `HTTPDSRV`, `HTTPDM`, `HTTPDMTT`, `ABEND0C1` |
| Target library | `HTTPD.LINKLIB` |
| Distribution library | `HTTPD.AHTTPLOD` |
| Sample library | `HTTPD.SAMPLIB` |

> **This is not the upgrade path.** Installing a newer HTTPD needs nothing from
> this page: each release's SYSMOD carries `DELETE(<previous FMID>)` and SMP
> replaces the modules during the APPLY. Use section 12 to remove HTTPD from a
> system, or to clean up after a test install. The 4.0.x guide sent upgrades
> here; following that now would uninstall a working server for no reason.

**Leftovers from 4.0.x.** If this system was upgraded from 4.0.x rather than
installed fresh, the old versioned libraries are still catalogued and are no
longer referenced by anything — `HTTPD.V4R0M2.*` for 4.0.2, `V4R0M1` for 4.0.1,
`V4R0M0` for 4.0.0. Their SMP inventory entries went with the `DELETE` at
upgrade time, so only the datasets remain. Scratch them at your leisure; check
each name against ISPF 3.4 first, and do not confuse them with the unversioned
`HTTPD.*` libraries that are in use.

**1. Stop the server:** `/P HTTPD`

**2. Cut the FMID out of the SMP inventory.** Submit this — it edits the CDS and
the ACDS and touches no library. It is long on purpose, and on a system that does
not hold all of these ids its `UCLIN` step ends with RC 08; the explanation
follows the job.

```
//HTTPDUCL JOB (SYS),'HTTPD UNINSTALL',
//             CLASS=A,MSGCLASS=H,MSGLEVEL=(1,1),
//             REGION=4096K
//UCLIN   EXEC SMPAPP
//SMPCNTL  DD  *
 UCLIN CDS .
  DEL SYSMOD(THTP440) MOD(HTTPD) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL SYSMOD(THTP440) MOD(HTTPDSRV) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL SYSMOD(THTP440) MOD(HTTPDM) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL SYSMOD(THTP440) MOD(HTTPDMTT) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL SYSMOD(THTP440) MOD(ABEND0C1) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL MOD(HTTPD) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL MOD(HTTPDSRV) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL MOD(HTTPDM) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL MOD(HTTPDMTT) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL MOD(ABEND0C1) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL LMOD(HTTPD) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL LMOD(HTTPDSRV) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL LMOD(HTTPDM) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL LMOD(HTTPDMTT) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL LMOD(ABEND0C1) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL SYSMOD(THTP440) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL SYSMOD(THTP430) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL SYSMOD(THTP420) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL SYSMOD(THTP410) .
 ENDUCL .
 RESETRC .
 UCLIN CDS .
  DEL SYSMOD(THTP400) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP440) MOD(HTTPD) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP440) MOD(HTTPDSRV) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP440) MOD(HTTPDM) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP440) MOD(HTTPDMTT) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP440) MOD(ABEND0C1) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL MOD(HTTPD) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL MOD(HTTPDSRV) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL MOD(HTTPDM) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL MOD(HTTPDMTT) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL MOD(ABEND0C1) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP440) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP430) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP420) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP410) .
 ENDUCL .
 RESETRC .
 UCLIN ACDS .
  DEL SYSMOD(THTP400) .
 ENDUCL .
 RESETRC .
/*
//LIST    EXEC SMPAPP
//SMPCNTL  DD  *
 RESETRC .
 LIST CDS  SYSMOD(THTP440) .
 LIST CDS  SYSMOD(THTP430) .
 LIST CDS  SYSMOD(THTP420) .
 LIST CDS  SYSMOD(THTP410) .
 LIST CDS  SYSMOD(THTP400) .
 LIST ACDS SYSMOD(THTP440) .
 LIST ACDS SYSMOD(THTP430) .
 LIST ACDS SYSMOD(THTP420) .
 LIST ACDS SYSMOD(THTP410) .
 LIST ACDS SYSMOD(THTP400) .
/*
//
```

**Why every `DEL` has a block of its own.** The job holds 35 `DEL` statements,
20 against the CDS and 15 against the ACDS. Each one stands in its own
`UCLIN`/`ENDUCL` pair, and a `RESETRC .` follows every `ENDUCL .`. Three
facts of SMP 4 make that necessary (measured on TK5 with SMP 4,
`HMASMP LVL 04.48`):

- **A `DEL` that names an entry the zone does not hold is an error.** It
  reports `HMA2532 ** SYSMOD ENTRY THTP410 TO BE DELETED DOES NOT EXIST` and
  `HMA2560 UPDATE PROCESSING TERMINATED - UPDATE NOT COMPLETE`, and its block
  ends RC 08. A `DEL MOD` or `DEL LMOD` for a missing module reports the same
  pair with `MODULE ENTRY` or `LMOD ENTRY` in the first message (each run in a
  step of its own). `LIST` is different: an id it does not find is RC 04.
- **After such an error SMP ignores every further `UCLIN` in the same step.**
  Each one reports `HMA2293 ** CONTROL STATEMENT IGNORED DUE TO PREVIOUS ERROR`,
  ends RC 12, and its `DEL` statements are discarded without a trace. Separate
  blocks are not enough: with a `UCLIN`/`ENDUCL` pair around every `DEL` but no
  `RESETRC`, the failure at `THTP410` silenced everything after it, and
  `THTP430` and the tombstone `THTP420` were still in the ACDS afterwards.
- **`RESETRC .` between the blocks lifts it.** With it, no `HMA2293` was
  reported and every `DEL` that found its entry was carried out. A new
  `EXEC SMPAPP` step lifts it as well, because each step is a separate SMP
  run.

**Read the return codes as expected, not as a verdict.** The figures in this
paragraph were measured with the 33-statement form of this job, which named
`THTP430` as the newest id, on a system that held `THTP430` and the tombstone
`THTP420` but never saw `THTP410` or `THTP400`. There two blocks per zone
failed, four of the 33, and the `UCLIN` step ended RC 08. The other 29
reported `HMA2550 UPDATE COMPLETE`. The `LIST` step ended RC 04. The job
printed above has 35 statements: it adds the id `THTP440` to each zone and
names it in the five `SYSMOD(...) MOD(...)` blocks. On a system that never held
`THTP440` those blocks find no entry and each ends RC 08 with `HMA2532`, as
every `DEL` for a missing entry does; that was not run. Neither number says
whether the removal worked; the stanzas of the `LIST` in step 3 do. Running the
33-statement job a second time, when no id was left, gave RC 08 again with no
`HMA2550` and no `HMA2293`, and the same `LIST`.

**The modules go as well.** After the job, `LIST CDS MOD(...)`,
`LIST CDS LMOD(...)` and `LIST ACDS MOD(...)` for the five module names answer
RC 04, not found. Before it they list all five, and the CDS load module
entries show `SYSTEM LIBRARY = LINKLIB`.

**What was not measured.** The 35-statement job as printed was not run. A
system where single `MOD` or `LMOD` entries are missing while the newest id is
present was not run either. Measured is only that such a
line, alone in its step, reports `HMA2532` and `HMA2560` with RC 08. That
`RESETRC .` lifts the `HMA2293` cascade after it, too, is concluded from the
cases above, not measured.

**A new release adds one block per zone and moves five.** When a later release
is added to this job, its `DEL SYSMOD(<id>)` goes into the CDS and the ACDS as
one more four-line block each (`UCLIN`, `DEL`, `ENDUCL`, `RESETRC`), the five
`DEL SYSMOD(<id>) MOD(...)` blocks per zone are changed to name the new id
instead of the old one (they are not added to), and two lines go into the
`LIST` step.

**3. Read the LIST — this is the actual result.** Every id must answer, in
both zones:

```
THE FOLLOWING SELECTED ENTRIES WERE NOT FOUND OR WERE NOT ELIGIBLE
FOR PROCESSING
 TYPE        NAME
 SYSMOD      THTP440
```

with `HIGHEST RETURN CODE IS 04`. **RC 04 and an empty list means the FMID is
free.** Both zones matter: the CDS records what is applied, the ACDS what is
accepted, and they are separate inventories — an id gone from one and present in
the other is not free.

**Read the stanzas, not the step's return code.** A surviving tombstone (below)
answers RC 00 while the cleared ids answer RC 04, so it does not raise the
step's highest return code above the 04 you were expecting. A stanza naming
`TYPE = FUNCTION` and `DELBY` is an id that is still there.

### The tombstones — one `DEL SYSMOD` each, and there may be several

A system that was upgraded also carries the **tombstone** of every level it
came through: `THTP430` listed as `TYPE = FUNCTION` with `DELBY = THTP440` and
nothing else. `LIST` answers **RC 00** for those, not RC 04, so the
"RC 04 + empty list = free" rule above reads them as occupied — which is
correct, the ids are spent and can never be reused.

**Tombstones accumulate, one per upgrade.** A system that went 4.0.x → 4.1.0 →
4.2.0 → 4.3.0 → 4.4.0 carries four: `THTP400 DELBY=THTP410`,
`THTP410 DELBY=THTP420`, `THTP420 DELBY=THTP430` and `THTP430 DELBY=THTP440`. A job
that names only the release you are removing and its immediate predecessor
leaves the older one standing for good. That is why the job above names
**every id HTTPD has ever spent** — `THTP440`, `THTP430`, `THTP420`, `THTP410`, `THTP400` — rather
than the ones this system happens to have seen.

`DEL SYSMOD` does work on a tombstone: on a system holding `THTP430` and the
tombstone `THTP420`, the job above removed both ids from both zones, measured on
TK5 with SMP 4 (`HMASMP LVL 04.48`). On a system that never saw an id the line
is not harmless: it fails with `HMA2532` and RC 08, as step 2 describes, and
the block and the `RESETRC` around it absorb that. Naming every id
unconditionally means the list needs no judgement about which releases this
system has seen, and getting that judgement wrong is exactly the failure it
prevents. Each release adds its own id here.

**4. Scratch the libraries.** `UCLIN` edits the inventory only; both datasets are
still there, and a re-install's allocation job would fail on them:

```
  DELETE HTTPD.LINKLIB  NONVSAM SCRATCH PURGE
  DELETE HTTPD.AHTTPLOD NONVSAM SCRATCH PURGE
```

> **List the members before you scratch the LINKLIB.** It is a library, not a
> file, and it holds whatever was put there — HTTPD ships five modules, but a
> CGI module from another product is commonly copied in beside them (`MVSMF` is
> the usual one). Those are not SMP's and nothing above deletes them from any
> inventory; the `SCRATCH` simply takes them with it. ISPF 3.4, or
> `IEHLIST LISTPDS`, then re-install what belonged to something else.
>
> This is new in 4.1.0. Under the versioned names an upgrade never scratched
> the library it was installing beside, so the hazard did not arise.

Leave `HTTPD.SAMPLIB` alone if you like — the install job's `DELOLD` step
scratches it on its own.

**5. What is not removed, because SMP never owned it:** the procedure and the
configuration member you copied in step 7, your RAKF definitions, and the
webroot disk `HTTPD.WEBROOT.UFS` with the `MOUNT` statement that names it.
Those are yours to delete — unmount the disk before you scratch it.

> The `RESTORE`/`REJECT` behaviour above was measured on 2026-08-14 against an
> accepted FMID from a package built by this same generator, on an MVS/CE system
> running SMP 4 level 04.48. It is a property of SMP 4 and of accepting a
> function SYSMOD, not of any one product. The `UCLIN` job above was run end to
> end on TK5 with SMP 4 (`HMASMP LVL 04.48`) on a system holding `THTP430` and
> the tombstone `THTP420`; it was not run on a system holding all four ids, or
> on one with single `MOD` or `LMOD` entries missing. Read the `LIST` output,
> not the `UCLIN` return codes.

---

## 12a. Upgrading from 4.0.x

> **Do this with the 4.1.0 package, not this release's.** This release deletes
> `THTP430` only, so on a 4.0.x system — where the modules belong to `THTP400` —
> its install selects nothing and copies nothing while every step ends `RC 00`.
> Download 4.1.0 from the releases page, follow the steps below with it, and
> confirm the 4.1.0 banner. Then upgrade to 4.2.0, 4.3.0 and this release the
> ordinary way, one after the other: stop, install, start, with no allocation
> step.

Those releases put their datasets in `HTTPD.V4R0M0.*`, `HTTPD.V4R0M1.*` or
`HTTPD.V4R0M2.*`, so nothing of 4.1.0 lands on top of them. That makes
this upgrade a **move** rather than a replacement — but it is still just the
ordinary install, in order, with three extra things to remember.

**There is nothing to uninstall first.** `DELETE(THTP400)` does the inventory
work even though the old modules are in a library 4.1.0 never names.

The reason is that **SMP does not record which data set an element lives in —
it records the ddname the install job used.** Both generations' libraries end
in `LINKLIB`, and the ddname comes from that last qualifier, so both installs
use ddname `LINKLIB`: pointing at `HTTPD.V4R0M2.LINKLIB` in the one case and
`HTTPD.LINKLIB` in the other. When 4.1.0's `APPLY` deletes the
predecessor's load modules it resolves `LINKLIB` **in its own job**, which
points at the new library. It deletes from there, reports success, and copies
the new modules in. `HTTPD.V4R0M2.LINKLIB` is never opened, and nothing in the
job log mentions it. (That explanation is `mvslovers/ftpd`'s, from the same
crossing at `TFTP110`.)

Measured on 2026-09-14 on a test system with throwaway ids: SMP
reported `HMA2240 SUCCESSFULLY DELETED LMOD …` for each module against the new,
still-empty library without complaint, copied all five in
(`HMA2380 … SYSMOD=<new>`), and `MOD(HTTPD)` came back reading
`FMID = <new>  RMID = <new>`. The predecessor became a `DELBY` tombstone in both
zones, and **its library was left untouched, every member still in place.** ftpd
measured the identical shape on a second system — two systems, two products,
same result.

**So the old library keeps a working, `AC(1)` copy of the previous release**,
and that is what makes steps 4 and 5 below the ones that decide whether you are
running 4.1.0 at all. Miss them and HTTPD starts, serves, and logs no error —
it is simply the old build.

1. **`/P HTTPD`.**
2. **Run step 4.** This is the one upgrade that allocates: `HTTPD.LINKLIB` and
   `HTTPD.AHTTPLOD` are new names on this system. They are created beside the
   old libraries, not over them.
3. **Steps 3 and 6, with the 4.1.0 files** — upload and install; the SMP
   output names `THTP410`, not the id step 6 shows. Expect the `APPLY` to end
   `COND CODE 0004` on `HMA2461`; that is the deleted level having no `SMPSCDS`
   backup entry, and the `ACCEPT` still runs.
4. **Re-point `STEPLIB`** in your `HTTPD` procedure to `HTTPD.LINKLIB`.
5. **Replace the APF entry — do not add one.** If you took the APF route rather
   than RAKF, `SYS1.PARMLIB(IEAAPF00)` names the old library. **Remove that
   line**, then add the new one. An entry is `dsname volser` and the list is
   read only at IPL, so an old entry left beside the new one survives until the
   next one — and it stays dangerous after the library is scratched: allocate
   anything with that name on that volume later and it is APF-authorised
   without anyone asking. For an `AC(1)` product that is a loaded weapon lying
   in a drawer. **This is the last time the entry needs changing**, because the
   name no longer carries a version.
6. **Copy over anything that was not ours.** A CGI module from another product
   lives in the same library and SMP knows nothing about it — `MVSMF` is the
   usual one. It is still in the old `HTTPD.V4R0M*.LINKLIB` and it is *not* in
   the new one. Its own installation is the right way to put it back; a plain
   `IEBCOPY` works if you only need the server up.
7. **Copy what you want out of the old `SAMPLIB`.**
8. **Check every route for an `AUTH=`** (the security note at the top of this
   guide), then **`/S HTTPD`, and read the banner before going further.** The
   banner is a gate, not a formality:

   ```
   HTTPD000I HTTPD 4.1.0 (xxxxxxx) STARTING
   HTTPD005I LIBC370 1.0.6
   ```

   Either line still showing the release you came from means step 4 did not
   take and you are running the **old** build out of the old library. Nothing
   else reports this — the install succeeded, SMP is satisfied, and every
   condition code says so. Do not continue to step 9 until the banner is right.
9. **Only now scratch the release you came from:**
   `DELETE HTTPD.V4R0M*.LINKLIB / .AHTTPLOD / .SAMPLIB NONVSAM SCRATCH PURGE`.

**If you stop after step 3 you have not upgraded**, and that is why step 8 is a
step rather than a footnote. Steps 4 to 9 are a **deliberate rollback window,
then a scratch**: both installations sit on the disk side by side until you
close it, so a bad start is one procedure edit away from being undone. Treat it
as a window you chose and then closed, not a state to leave the system in. It
exists only because the names change, so this is the last upgrade that has one.

The alternative — scratching the old libraries *first*, so a forgotten repoint
fails loudly with `S806` instead of quietly running the old build — buys that
safety by giving up the rollback. A positive check is worth more than an absent
crash, which is why the banner gate replaces it here.

The webroot disk is not part of this. It keeps its name, it is not SMP's, and
nothing above touches it.

---

## Troubleshooting

| Symptom | Likely cause |
|---------|--------------|
| Allocation job fails, or ends `COND CODE 0000` with `IEF287I … NOT CATLGD 2`, dataset already exists | Either it was already run, or you are upgrading from 4.1.0 or later, where the libraries are already there and the APPLY replaces their contents. Skip step 4 — do not force it. If this is a TK5 or hand-filled system and HTTPD was never installed from SMP, the library is one SMP does not own: [rename it first](#a-library-that-smp-does-not-own) |
| `RECV1`/`RECV2` fails on MVS/CE, no volume to allocate on | Add `VOLUME('PUB000')` (or a volume of yours) to both `RECEIVE` commands — step 6 |
| `RECV1`/`RECV2` fails, target exists | A previous release's `HTTPD.HTTPLOAD`/`HTTPD.SAMPLIB`, or something else allocated it. RECEIVE refuses to merge; scratch it and re-run |
| `APPLY` RC 00, no `HMA2380` at all, library empty | The modules still belong to another FMID (`MOD … NOT SEL` in the element summary). The SYSMOD's `++VER` is missing the `DELETE` for whatever owns them — this is a packaging fault, not an operator one; report it |
| `APPLY` ends `COND CODE 0004`, `HMA2461 … NOT FOUND ON SMPSCDS` | Normal on an upgrade: the deleted level has no backup entry. The `ACCEPT` is gated `(4,LT,…)` and still runs |
| `APPLYCHK` ends non-zero, `APPLY` skipped | Read the SMP output — the check exists to stop before anything is written. A missing DD is the usual cause |
| `APPLY` RC 00 but fewer than five `HMA2380` lines | A module was lost. Count them, then look in the library itself |
| SMP reports success, but a module is not where you expected | A ddname says nothing about the dataset behind it. Check the JCL, then look at the library itself |
| `S806` (module not found) at `/S HTTPD` | `STEPLIB` in the procedure does not name the LINKLIB the APPLY wrote to |
| `/S HTTPD` rejected — procedure not found | Procedure not copied into a PROCLIB in the started-task concatenation |
| `HTTPD012E … UNABLE TO DYNAMICALLY OBTAIN APF AUTHORIZATION`, no listener | No RAKF (so no SVC 244) and no APF entry — step 2. This one is fatal, not a warning |
| `HTTPD014E`/`015E`/`016E` and the step ends before any banner | A `SYSPRINT`, `SYSTERM` or `SYSIN` DD in the procedure — step 7 |
| `HTTPD037E HTTPD IS ALREADY ACTIVE ON PORT n` | An older instance still holds the port: still running, or still ending (up to about 50 s after `/P HTTPD` with clients connected). Wait for `$HASP395 HTTPD ENDED` — steps 5 and 10 |
| `HTTPD030I EADDRINUSE, WAITING …`, repeating | The old instance is gone and the TCP/IP stack has not released the port yet. The new one retries every `BIND_SLEEP` s, `BIND_TRIES` times, and comes up by itself — [Restarting](#restarting) |
| `HTTPD030E BIND() FAILED …` and the step ends | Every retry failed. The port is held by something else, or by the stack for longer than `BIND_TRIES` × `BIND_SLEEP` seconds — [Restarting](#restarting) |
| `HTTPD004W RACINIT ENVIR=CREATE FAILED` | The `STCUSER`/`STCGRP` userid is not defined to RAKF — step 8. Not fatal, and that is the danger: the identity switch runs before the listener opens, so the server still comes up (`HTTPD054I`, `HTTPD001I`) on the inherited `STC`/`STCGROUP` identity, which holds ALTER on data sets on the stock profiles. A module that sets no identity of its own runs with it. Keep clients away, or `/P HTTPD`, until a start prints `HTTPD004I` instead |
| `S RAKFUSER` ends `RAKFUIDS2 INPUT DATA INVALID OR OUT OF SEQ.`, `ABEND U0200` (TK5) | A `USERS` line has no password in columns 19-26, or a field sits in the wrong column — step 8 |
| `HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM` | UFSD is not running. Module routes are unaffected; only static files fail — step 2. `HTTPD001I` still names the configured `DOCROOT`, so it is not the line to read here |
| `HTTPD045E UFS READ ERROR RC=n, RESPONSE ABORTED: path, s SUPPRESSED` | UFSD could not read a file it had opened; `RC=52` is an I/O error. `path` is the page, or the included file when the error was inside a server-side include. At most one line is written per 60 seconds; `s` counts the errors held back in between. Look at UFSD's own messages and at the volume behind the webroot. The client gets no error status, because `200` was already sent: the body stops short and the connection is closed. A client on HTTP/1.0 that was sent an HTML page cannot tell that from the end of the file, so this line is the only sign. See [messages.md](https://github.com/mvslovers/httpd/blob/main/docs/messages.md) |
| `HTTPD065W CLIENT a.b.c.d ABANDONED, NO SEND PROGRESS FOR 10 SEC, n BYTES SENT, s SUPPRESSED` | A client took no data at all for 10 seconds while HTTPD was sending it a chunked (HTTP/1.1) response, so the response was cut off and the connection closed. Not a fault of the server: usually a client that stopped reading or a peer that went away. If many different addresses appear together, look at TCP/IP on the MVS side. At most one line is written per 60 seconds; `s` counts the clients abandoned in between and not reported. See *Abandoned clients and the flood guard* in [messages.md](https://github.com/mvslovers/httpd/blob/main/docs/messages.md) |
| One request out of many takes 25 to 150 seconds under sustained load, the rest answer in about a second, and no console message appears | A known issue in this release, independent of `RESIDENT=`: single requests were seen to stall for 25 to 150 seconds in soak runs of 20,000 to 30,000 requests, with the median at 0.2 s and the 95th percentile near 1 s. The cause is not known and it has not been reproduced on demand. A request that a client abandoned at 60 s and sent again was answered the second time. Nothing to configure; use a client timeout and a retry. Tracked in [#350](https://github.com/mvslovers/httpd/issues/350) |
| `HTTPD908E EXTERNAL PROGRAM MVSMF …` on every `/zosmf/` request | mvsMF is not in the STC's STEPLIB — step 2. `was not found` is the LINK completion code `S806`: the member is in neither `STEPLIB` nor the link list. `could not be loaded` is any other LINK failure; the real code is in the `IEA703I` line beside it. `106-0F` is not a storage shortage (that is `106-0C`): IBM describes it as an uncorrectable I/O error, or an error in the load module that made the channel program fail. Look at the library and the member, not at the region. A failed fetch costs the module's storage until the server stops; `RESIDENT=YES` (the default) avoids the per-request fetch, see [server-modules.md](https://github.com/mvslovers/httpd/blob/main/docs/server-modules.md) and [configuration.md](https://github.com/mvslovers/httpd/blob/main/docs/configuration.md#resident-modules-resident) |
| `@@CRT0 - No storage for C stack` on the console, `HTTPD908E EXTERNAL PROGRAM … failed with U0801 ABEND`, the client gets `503`; only under load, several module requests at the same time | The region has no free block of 262,328 contiguous bytes (about 64 KB for mvsMF) for the stack of one more module request. Raise `REGION` in the procedure (16M held in the one measured run) before you raise `MAXTASK`; if 16M is not enough, lower `MAXTASK`. `HTTPD904E` and `HTTPD999E` in the same window say the same of the server's own storage. Step 7, *The region*, and [configuration.md](https://github.com/mvslovers/httpd/blob/main/docs/configuration.md#region-size-and-parallel-module-requests-region-maxtask) |
| `HTTPD420E ROUTE AUTHORIZATION POLICY INCOMPLETE`, `CC 0008` | A route's `AUTH=` value is not `NONE`/`FORM`/`BASIC`/`TOKEN`, or a `LOGIN=` line survives in the member — step 7 |
| `HTTPD064E SOCKET THREAD IS NOT RUNNING`, `CC 0008`, no `HTTPD001I` | The thread that takes in connections ended or never began to accept. The server stops rather than say `READY` over a listener nobody serves. Start it again; if it repeats, report it with the console log from `HTTPD000I` on |
| `HTTPD038E READ ERROR ON DD:HTTPPRM`, `CC 0008` | The member was found but could not be read to the end — a media or DCB problem, not a configuration one. Check the PARMLIB dataset and whether the `HTTPPRM` DD overrides DCB attributes that disagree with it |
| `HTTPD425W NO PROFILE FOR …` | A `RES=` names a resource no RAKF profile covers, so that route is gated by `AUTH=` alone — step 8. For `FACILITY:MVSMF.CONSOLE` that means, with mvsMF up to 1.3.0, that every user who can log in may issue operator commands. From mvsMF 1.4.0 the module refuses every user itself until the profile is defined, so the warning concerns HTTPD's route only |
| `403` on `/zosmf/restconsoles/…`, plain-text body | The user has no READ on `FACILITY MVSMF.CONSOLE` — step 8. A `403` is HTTPD's gate, not mvsMF's |
| `500` on `/zosmf/restconsoles/…`, z/OSMF error report with category 4 | mvsMF 1.4.0 and up: the check inside mvsMF refused the user. HTTPD's `403` comes first, so this shows when HTTPD let the request through: the profile is not defined (`HTTPD425W` at start) — step 8 — or the route stands after `/zosmf/*` and HTTPD's check is never made |
| `/zosmf/restconsoles/…` answers `200` to a user the profile excludes | The usual causes: the route stands after `/zosmf/*` (check the order of the `HTTPD036I` lines at startup), or the profile is not loaded yet — reload it, step 8 |
| A route answers `200` to anyone | It carries no `AUTH=`, or no route claims the path at all. Both are public by design — step 7. `/.dsrv?target=MOD` prints the policy each route actually got |
| `401` on a route showing `AUTH=NONE` | Not HTTPD's gate. mvsMF runs its own auth track on `/zosmf/*` — establish which layer answered before debugging HTTPD's |
| `S106` at start on a freshly installed library | The XMIT was uploaded in text mode. Re-upload in **binary** and re-run the install job |
| `UFSD062E SUPERBLOCK VALIDATION FAILED` at `MOUNT` | The webroot dataset was not allocated `RECFM=U BLKSIZE=4096`, or the image was uploaded in text mode — step 9 |
| `404` on `/` with UFSD running | The webroot is mounted somewhere other than `DOCROOT`, or nothing is mounted there at all — step 9 |
| The step ends `CC 0000` but nothing was ever served | A clean `/P HTTPD` and a refused start are `CC 0000` and `CC 0008`. If it is 0000, the server ran — look for what stopped it |

For the complete message reference, see
[messages.md](https://github.com/mvslovers/httpd/blob/main/docs/messages.md).
