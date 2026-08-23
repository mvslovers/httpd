# Operator Messages

Every console message HTTPD can write, in one place. The literals live in
[`include/httpdmsg.h`](../include/httpdmsg.h) — that header is the source of
record, this page is its operator-facing form. Keep the two in step.

## What is on the console and what is not

HTTPD writes a WTO only for **what an operator can act on or must know**.

Anything the client caused — a malformed request, a route that does not exist,
a file that is not there, a failed login — is reported in the HTTP response and
**nowhere else**. Those are the client's to fix, and the operator cannot do
anything about them. A server that logged them would turn one bad client into a
flood.

That is not only tidiness. Everything written here also lands in the **Master
Trace Table**, which is what `/.dmtt` and mvsMF's `restconsoles` API read back
out. A chatty server pollutes its own diagnostic channel.

## Conventions

- **Ids are `HTTPDnnn` + a severity letter**, three digits, one text per id.
  Two call sites that need different wording need different ids; a varying
  detail becomes an argument.
- **Severity is `I`, `W` or `E`.** No `D`, no `T`. The `D` on
  `HTTPD900D`/`902D`/`903D` was never a severity — they were #159's debug
  counters, retired with it (#186).
- **Literals are uppercase** — 3270 console convention. Substituted values are
  passed through unchanged: most are MVS names and already uppercase, but URL
  paths, UFS paths and free user text must not be folded.
- **No trailing newline.** `wtof()` writes one console line.
- **Keep new text inside the old length.** A single-line WTO truncates around
  70–90 characters.

Ranges:

| Range | Area |
|---|---|
| `HTTPD0xx` | server lifecycle: start, identity, APF, listener, threads, stop |
| `HTTPD1xx` | operator console — MODIFY / DISPLAY output |
| `HTTPD4xx` | configuration (`DD:HTTPPRM`), routes and SMF |
| `HTTPD9xx` | diagnostics: abends, recovery, storage, internal faults |

One wart, stated rather than tidied away: configuration **also** occupies
`HTTPD020`–`HTTPD048`, which predate the `4xx` block. Renumbering them would
churn every id an operator has ever seen in a log for no gain, so both halves
are listed under *HTTPD4xx — configuration* below. When looking an id up,
search the page rather than trusting the range.

## A healthy start and stop

This is what a normal start looks like. Nothing else should appear; anything
that does is either a warning below or a misconfiguration.

```
HTTPD000I HTTPD 4.0.0-DEV (A69A370) STARTING
HTTPD005I LIBC370 1.0.2 (58767B3)
HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)
HTTPD004I STC IDENTITY SET TO HTTPD/USER VIA RACINIT
HTTPD036I MODULE MVSMF REGISTERED FOR /zosmf/info
HTTPD036I MODULE MVSMF REGISTERED FOR /zosmf/*
HTTPD036I MODULE HTTPDSRV REGISTERED FOR /.dsrv
HTTPD054I LISTENING ON ANY PORT 8080
HTTPD061I STARTING SOCKET THREAD    TCB(9CD9D0) TASK(10CFC8) STACKSIZE(32768)
HTTPD061I STARTING WORKER(11ED48)   TCB(9CD420) TASK(120FC8) STACKSIZE(65536)
HTTPD001I HTTPD 4.0.0-DEV READY - SERVING /wwwroot
```

and a normal `P HTTPD`:

```
HTTPD098I HTTPD SHUTTING DOWN
HTTPD060I SHUTDOWN SOCKET THREAD    TCB(9CD9D0) TASK(10CFC8) STACKSIZE(32768)
HTTPD060I SHUTDOWN WORKER(11EBC8)   TCB(9BA8E8) TASK(142FC8) STACKSIZE(65536)
HTTPD416I STATS: 3302 REQUESTS, 858 ERRORS, 31887467 BYTES
HTTPD099I HTTPD SHUTDOWN COMPLETE
```

## A refused start

A start the server refuses looks like this — here `S HTTPD` on a port a running
instance already serves (#223):

```
HTTPD037E HTTPD IS ALREADY ACTIVE ON PORT 8090, THIS INSTANCE ENDS
HTTPD098I HTTPD SHUTTING DOWN
HTTPD416I STATS: 0 REQUESTS, 0 ERRORS, 0 BYTES
HTTPD099I HTTPD SHUTDOWN COMPLETE
```

**The first line is the cause**, and it is the only one that varies:
`HTTPD037E`, `HTTPD028E`, `HTTPD030E`, `HTTPD031E` or `HTTPD420E`, each naming
what actually failed. Everything below it is the ordinary stop sequence, byte
for byte what a clean `P HTTPD` writes — so past that first line the log does
not tell the two apart. There is no summary line: `HTTPD400E` used to follow
every one of these and named the configuration on four of the five, sending the
operator into the Parmlib after a mistake that was not there (#233).

**The step return code tells them apart.** A refused start ends `CC 0008`
(#226), a clean `P HTTPD` ends `CC 0000`. Automation that has to recognize a
failed start — `$DJ`, a COND CODE check, an IEFACTRT exit, a rule that restarts
a dead STC — keys on that, never on a console string.

The configuration is **not** echoed at start — not the member, not the codepage,
not the task limits. `F HTTPD,D CONFIG` reports all of it on demand, at any point
in the server's life. Nor is APF status: that it was obtained is only interesting
when it fails, and `HTTPD012E` covers that.

## HTTPD0xx — server lifecycle

| Id | Text | Meaning and action |
|---|---|---|
| `HTTPD000I` | `HTTPD vers (commit) STARTING` | First line of every start. `commit` is the short git hash the module was built from. If it does not match what you deployed, the STC is running an older load module. |
| `HTTPD001I` | `HTTPD vers READY - SERVING path` | The listener is up and requests are being accepted. |
| `HTTPD001I` | `HTTPD vers READY - NO DOCUMENT ROOT` | As above, but no `DOCROOT` is configured. CGI routes work; static files 404. Informational, not a warning — a CGI-only server is an ordinary deployment. |
| `HTTPD002I` | `AUTHORIZED BY LIBRARY (MODULE KEY 0)` | The job step was **already authorized when program fetch ran** — every dataset in the STEPLIB concatenation carries an APF entry and the module is linked `AC(1)`. MVS then takes the job pack area in subpool 252, key 0, so HTTPD's own module storage is read-only to it. The key is inferred from the route, not measured. `UFSD007I` / `FTPD008I` are the same line in the other two servers. |
| `HTTPD002I` | `AUTHORIZED BY SVC (MODULE KEY 8)` | The STC authorized itself with SVC 244 after program fetch, which cannot relabel storage already allocated, so the module stays key 8. This is the normal route on a stock TK4-/TK5. Note one non-APF dataset anywhere in the STEPLIB concatenation is enough to put you here even when `HTTPD.LINKLIB` itself is APF-authorized. |
| `HTTPD003W` | `RACINIT SKIPPED, CANNOT ENTER SUPERVISOR STATE` | The identity switch needs key 0 and could not get it — the STC is not APF authorized (see `HTTPD012E`). It keeps the inherited `STC/STCGROUP` identity. |
| `HTTPD004I` | `STC IDENTITY SET TO user/group VIA RACINIT` | The server dropped the default `STC/STCGROUP` identity, which holds ALTER on every data set (issue #177). |
| `HTTPD004W` | `RACINIT ENVIR=CREATE FAILED FOR user/group RC=n` | The userid is not defined to RAKF, or the group is wrong. The server **continues** on the inherited identity — define the userid, or start with `S HTTPD,STCUSER=x,STCGRP=y`. |
| `HTTPD005I` | `LIBC370 v (commit)` | Which C runtime this module linked against. A sysroot/STC mismatch shows here. |
| `HTTPD006W` | `BUILT FROM A MODIFIED WORKING TREE` | The build carried uncommitted tracked changes, so `HTTPD000I`'s commit does not fully describe it. Never expected from a release artifact. |
| `HTTPD007I` | `USER u IP a.b.c.d LOGIN SUCCESSFUL ACEE(x)` | A client completed the form login. |
| `HTTPD008I` | `USER u IP a.b.c.d LOGOUT SUCCESSFUL` | The credential was released. |
| `HTTPD008W` | `USER u IP a.b.c.d LOGOUT FAILED` | Logout for a credential that was not found — normally a session that had already expired. |
| `HTTPD010I` | `pgm IS APF AUTHORIZED` | **No longer written on the healthy path.** Reserved; `httpauth.c` still references it. |
| `HTTPD011I` | `pgm WAS APF AUTHORIZED VIA SVC 244` | **No longer written on the healthy path.** Reserved, as above. |
| `HTTPD012E` | `pgm UNABLE TO DYNAMICALLY OBTAIN APF AUTHORIZATION` | Without APF the LINK SVC and the RACF services are unavailable: CGIs will not load and the identity switch is skipped. APF-authorize the LINKLIB. |
| `HTTPD013I` | `STEPLIB IS NOW APF AUTHORIZED` | **No longer written on the healthy path.** Reserved, as above. |
| `HTTPD014E` | `SYSPRINT DD NOT ALLOWED, HTTPD USES HTTPDOUT FOR STDOUT` | The PROC allocates a DD HTTPD reserves. Remove it; the server refuses to start. |
| `HTTPD015E` | `SYSTERM DD NOT ALLOWED, HTTPD USES HTTPDERR FOR STDERR` | As above. |
| `HTTPD016E` | `SYSIN DD NOT ALLOWED, HTTPD USES HTTPDIN FOR STDIN` | As above. |
| `HTTPD017W` | `HTTPDOUT DD NOT DEFINED, STDOUT IS NOT AVAILABLE` | The PROC is missing a DD. The server does not start. |
| `HTTPD018W` | `HTTPDERR DD NOT DEFINED, STDERR IS NOT AVAILABLE` | As above. |
| `HTTPD019W` | `HTTPDIN DD NOT DEFINED, STDIN IS NOT AVAILABLE` | As above. |
| `HTTPD027I` | `CLOSING STALE SOCKET n ON PORT p` | A socket from a previous instance still held the port. Expected after an abend; a flood of them is not. |
| `HTTPD033E` | `UNABLE TO CREATE SOCKET THREAD` | Nothing will ever be accepted. The server gives up. Usually storage. |
| `HTTPD034W` | `UNABLE TO CREATE WORKER THREADS, DYNAMIC DOCUMENTS DISABLED` | The pool could not be built. The server runs, but no request can be dispatched. Raise the region. |
| `HTTPD040I` | `WAITING FOR SOCKET THREAD TO TERMINATE (n)` | Normal during shutdown, up to ten times, one per second. |
| `HTTPD041I` | `FORCE DETACHING SOCKET THREAD` | The socket thread did not end within ten seconds and was detached. Worth reporting with the surrounding log. |
| `HTTPD044W` | `UNABLE TO INITIALIZE FILE SYSTEM` | UFSD is not up, or `libufs` could not reach it. CGI routes still work; static files do not. |
| `HTTPD047E` | `UNABLE TO INITIALIZE SERVER CREDENTIAL KEY, RC=n` | No credential can be sealed, so form login and session cookies are dead. Basic auth still works. |
| `HTTPD050E` | `MAXSOCK IS ZERO, THE LISTENER IS IN AN INVALID STATE` | Internal: `select()` was handed an empty descriptor set. The socket thread ends. |
| `HTTPD051E` | `SELECTEX() FAILED, RC=n ERRNO=e` | The accept loop cannot continue and the socket thread ends. The server must be restarted. |
| `HTTPD052E` | `ACCEPT() FAILED, RC=n ERRNO=e` | One connection was lost; the listener keeps running. |
| `HTTPD053E` | `UNABLE TO SET NON-BLOCKING I/O FOR SOCKET n` | The connection is dropped rather than risk stalling a worker on it. |
| `HTTPD054I` | `LISTENING ON ANY PORT p` | The bind and listen succeeded. `ANY` is literal — HTTPD always binds `INADDR_ANY`. |
| `HTTPD060I` | `SHUTDOWN name TCB(x) TASK(x) STACKSIZE(n)` | One server thread ended. Expect one per worker plus the socket thread. |
| `HTTPD061I` | `STARTING name TCB(x) TASK(x) STACKSIZE(n)` | One server thread started. At start you should see the socket thread plus `MINTASK` workers. |
| `HTTPD062E` | `ABEND xxxxxxxx IN WORKER(w) CLIENT(c) SOCKET(s)` | A worker's ESTAE caught an abend. Only that request dies; the worker is recycled and the server continues. Always worth a bug report — include the abend code and the request. |
| `HTTPD070E` | `UNKNOWN CODEPAGE "name", USING CP037` | The `CODEPAGE` value is not one this build knows. Check the spelling. |
| `HTTPD071I` | *(retired)* | The codepage in effect is a `D CONFIG` value (`HTTPD134I`), not a startup line. |
| `HTTPD090E` | `UNABLE TO INITIALIZE CONSOLE INTERFACE` | No MODIFY and no STOP would be possible, so the server does not start. |
| `HTTPD098I` | `HTTPD SHUTTING DOWN` | `P HTTPD` was accepted and the quiesce has begun. |
| `HTTPD099I` | `HTTPD SHUTDOWN COMPLETE` | Everything was released. If the address space ends without this line, shutdown did not finish cleanly. |

## HTTPD1xx — operator console

Everything in this range is written because an operator asked for it, so the
"only what you can act on" rule does not apply: the console line *is* the answer.

| Id | Command | Shows |
|---|---|---|
| `HTTPD100I` | — | The MODIFY text as received. Console attach/detach is no longer announced — `S`/`P HTTPD` are already in the log twice over. |
| `HTTPD101E` | — | The verb or operand was not recognized. `F HTTPD,?` lists the set. |
| `HTTPD102I` | `D PORTS` | The listener port. |
| `HTTPD103I`–`HTTPD120I`, `HTTPD141I`, `HTTPD199I` | `D THREADS` | Per-thread control block dump: `CTHDTASK`, `CTHDMGR`, worker state, connected client. `HTTPD199I` is the separator between blocks. |
| `HTTPD121I`, `HTTPD122I` | `D LOGIN` | Logged-in users and their ACEE addresses. |
| `HTTPD123I`, `HTTPD124I` | `D STATS` | SMF level and the request/error/byte/active counters. |
| `HTTPD125I`, `HTTPD126I`, `HTTPD126E`, `HTTPD127W` | `S STATS` | Counter reset, level change, and the two operand errors. |
| `HTTPD128I`–`HTTPD136I` | `D CONFIG` | Port, task limits, timeouts, session lifetime (idle and max-age), document root, codepage, realm/server name, SMF, and which Parmlib member was read. |
| `HTTPD140I` | `D VERSION` | Version and build commit. |
| `HTTPD142I`, `HTTPD143I` | `D TIME` | GMT and local time with the offset in minutes. |
| `HTTPD144E`–`HTTPD148I` | `D MEMORY` | Address out of range, end of dump, ESTAE failure, an abend while reading, and the usage line. Reading storage is done under recovery, so a bad address is caught rather than fatal. |
| `HTTPD150I`, `HTTPD151I` | `?` / `HELP` | The command list. |

## HTTPD4xx — configuration, routes and SMF

Parsing errors are warnings and the line is skipped — **except** the two that
would leave a route answering without the authorization it asked for. Those are
fatal by design (see [configuration.md](configuration.md)).

| Id | Text | Meaning and action |
|---|---|---|
| `HTTPD020W` | `CANNOT OPEN DD:HTTPPRM -- USING DEFAULTS` | No Parmlib member. The server starts on port 8080 with **no CGI routes at all**. Almost always a PROC problem. |
| `HTTPD020E` | `FOPEN FOR DD:HTTPDBG FAILED, ERRNO=n` | `DEBUG 1` is set but the DD is unusable. |
| `HTTPD021I` | `DEBUG/TRACE OUTPUT WILL BE TO DD:SYSTERM` | Where the trace went instead. |
| `HTTPD022I` | *(retired)* | Which member was read is a `D CONFIG` value (`HTTPD133I`). Every parse error names its own offending line, so nothing depended on this being announced first. |
| `HTTPD023E` | `INVALID PORT VALUE (n)` | Outside 1–65535. The default is kept. |
| `HTTPD024W` | `CONFIG=x IS IGNORED, CONFIGURATION COMES FROM THE HTTPPRM DD` | A parameter from the retired Lua engine. |
| `HTTPD024I` | `USE S HTTPD,M=MEMBER TO SELECT A DIFFERENT MEMBER` | How to do what `CONFIG=` used to do. |
| `HTTPD025W` | `TZOFFSET IS NO LONGER USED, THE SYSTEM OFFSET GMT ±hh:mm APPLIES` | The keyword is retired (#145). |
| `HTTPD025I` | `SET TZ IN THE SYSENV OR ENVIRON DD TO OVERRIDE IT FOR ALL TASKS` | How to set the timezone instead. |
| `HTTPD026W` | `UNRECOGNIZED CONFIGURATION LINE: …` | The line is not `KEYWORD VALUE`. Skipped. |
| `HTTPD026E` | `SESSION_TIMEOUT (n MIN) <= CLIENT_TIMEOUT (n SEC), RAISE IT` | The credential reaper could free a credential a request still holds. Raise `SESSION_TIMEOUT` well above the longest request. |
| `HTTPD032E` | `SESSION_MAXAGE (n MIN) <= CLIENT_TIMEOUT (n SEC), RAISE IT` | The same hazard as `HTTPD026E`, reached through the hard max-age: it reaps on the same path. Raise `SESSION_MAXAGE` well above the longest request. |
| `HTTPD028E` | `SOCKET() FAILED, RC=n ERRNO=e` | The listener socket could not be created; the server does not start. |
| `HTTPD029W` | `UNKNOWN CONFIGURATION KEYWORD: k` | Correctly shaped line, unknown keyword. Usually a typo or a keyword from an older release. |
| `HTTPD030E` | `BIND() FAILED FOR HTTP PORT, RC=n ERRNO=e` | The port is taken. Retried per `BIND_TRIES`, then fatal. |
| `HTTPD030I` | `EADDRINUSE, WAITING FOR TCPIP TO RELEASE HTTP PORT p` | A retry is in progress. Normal after an abrupt restart. |
| `HTTPD031E` | `LISTEN() FAILED, RC=n ERRNO=e` | Bound but will not accept; the server does not start. |
| `HTTPD037E` | `HTTPD IS ALREADY ACTIVE ON PORT p, THIS INSTANCE ENDS` | `S HTTPD` while a server is already serving that port. This instance ends with CC 0008 and the running one is untouched — it is refused *before* the stale-port sweep, which would otherwise close the live listener. Not a bind failure — no `HTTPD030E` accompanies it, and no summary line follows it: this message *is* the report. To run a second server deliberately, give it another port (`S HTTPD,M=HTTPPRM1`). |
| `HTTPD035W` | `UNABLE TO REGISTER MODULE m FOR path` | The region ran out of storage while the Parmlib was being read. Nothing else reaches this message: there is no route-table limit, and a duplicate pattern is never detected (it registers as a second route and is simply unreachable, first match winning). **The path is not dark** — it is still served, statically from the document root and ungated, just without the CGI. Standing alone the message also tells you the route carried no binding auth policy; one that did would bring `HTTPD419E`/`HTTPD420E` and the server would not start. |
| `HTTPD036I` | `MODULE m REGISTERED FOR path` | One active CGI route. One line per route at start. |
| `HTTPD048W` | `LOGIN IS RETIRED -- USE AUTH= ON EACH MOD=/LOC= ROUTE` | The member carries `LOGIN=NONE`, or `LOGIN=` with an empty value. `NONE` was the default, so nothing changes; delete the line. |
| `HTTPD048E` | `LOGIN v IS RETIRED -- ROUTES WITHOUT AUTH= WOULD BECOME PUBLIC` | **Fatal**, followed by `HTTPD420E`. The operand *required* a login, so ignoring it would publish every route in the member without its own `AUTH=`. Convert those routes to `AUTH=`, then delete the line. See [configuration.md](configuration.md). |
| `HTTPD400E` | *(retired)* | A summary that named the configuration on four of the five paths reaching it, where the real fault was the port, `socket()`, `bind()` or `listen()` — and on the fifth only repeated `HTTPD420E` (#233). Every refused start already names its own cause; see [A refused start](#a-refused-start) for the sequence and the `CC 0008` it ends with. |
| `HTTPD410W` | `CGI= IS DEPRECATED, USE MOD= INSTEAD` | The 3.3.x spelling. Still honoured. |
| `HTTPD411W` | `IGNORING UNKNOWN AUTH MODE 'm'` | Not one of `NONE`/`BASIC`/`FORM`/`DEFAULT`. The route falls back to the global policy — check this is what you want. |
| `HTTPD412W` | `IGNORING MALFORMED RES= 'v' (NEED CLASS:RESOURCE)` | The route keeps its `AUTH=` but gains no resource check. |
| `HTTPD413W` | `IGNORING UNKNOWN ROUTE OPTION 'o'` | Unknown keyword on a `MOD=`/`LOC=` line. |
| `HTTPD414W` | `AUTH=NONE IGNORES RES=c:r (PUBLIC ROUTE)` | A public route cannot also demand a profile. One of the two is wrong. |
| `HTTPD415W` | `LOC= REQUIRES A PATH (E.G. LOC /ADMIN/* AUTH=BASIC)` | The line is skipped. |
| `HTTPD416I` | `STATS: n REQUESTS, n ERRORS, n BYTES` | Written once at shutdown. |
| `HTTPD417I` | `LOCATION path REGISTERED` | A program-less static prefix is active. |
| `HTTPD418E` | `NO STORAGE FOR RES=c:r` | **Fatal.** The route asked for a resource check and did not get one. |
| `HTTPD419E` | `X=path COULD NOT BE REGISTERED -- ITS AUTH POLICY IS LOST` | **Fatal.** The route is absent, so nothing gates its requests at all — for a protected subtree that would serve it to anyone. |
| `HTTPD420E` | `ROUTE AUTHORIZATION POLICY INCOMPLETE -- HTTPD WILL NOT START` | Follows `HTTPD418E`/`HTTPD419E`. The port is never bound. |
| `HTTPD421W` | `MOD= REQUIRES A PROGRAM NAME` | The line is skipped. |
| `HTTPD422W` | `x IS RETIRED -- CGI STORAGE RECLAIM IS ALWAYS ON` | `RECLAIM=` no longer does anything (#175). |
| `HTTPD423W` | `UNABLE TO REGISTER LOCATION path` | As `HTTPD035W`, for a `LOC=` prefix: out of storage, with no table limit and no duplicate check behind it. The prefix keeps being served from the document root, but ungated instead of under the policy the line gave it. |
| `HTTPD424W` | `INVALID REALM VALUE: v` | The `REALM` value is empty, longer than 64 characters, or contains `"`, `\`, `<`, `>`, `&` or a control character — it lands inside the quoted-string of the Basic challenge and in the login form's HTML, so those are refused. The SMF ID default stays. |
| `HTTPD425W` | `NO PROFILE FOR c:r -- path NOT GATED` | Written at start, once per `RES=` route whose resource no profile covers. The route still serves — SAF calls an unprotected resource *allowed*, not denied — so its authorization stage does nothing and only `AUTH=` is left standing. Define the profile, or correct the class or resource name; if the route was meant to be gated by authentication alone, drop the `RES=`. `AUTH=NONE` routes are not probed — `HTTPD414W` already reports those. |
| `HTTPD430I` | `SMF TYPE n LEVEL l` | Reported by `D CONFIG`, not at start. |
| `HTTPD430W` | `SMF TYPE n CONFIGURED BUT SMF INACTIVE` | Written at start: records are being produced for a writer that is not there. Either start SMF or set `SMF NONE`. |
| `HTTPD431W` | `INVALID SMF LEVEL "l"` | Not `NONE`/`ERROR`/`AUTH`/`ALL`. |
| `HTTPD432W` | `INVALID SMF TYPE=n (128-255)` | Outside the user record range. |

## HTTPD9xx — diagnostics

Nothing in this range is routine. A `9xx` line means the server hit a state its
own code did not expect; **every one of them is worth a bug report**, with the
surrounding log and what the client was doing.

| Id | Text | Meaning |
|---|---|---|
| `HTTPD900D` `HTTPD902D` `HTTPD903D` | *(retired)* | Probes for #159, removed with it (#186). `HTTPD903D` wrote a census line every 60 seconds on an idle server. The ids are reserved, not reused. |
| `HTTPD901E` | `SPIN IN SERVE_CLIENT: … -- FORCING CLOSE` | A client sat in one state past every deadline. The connection is closed so the worker is not lost. |
| `HTTPD904E` | `NO STORAGE FOR ENVIRONMENT VARIABLE v CLIENT(c)` | The request cannot be completed. Region too small, or a leak. |
| `HTTPD905E` | `ENVIRONMENT VARIABLE v TOO LARGE CLIENT(c)` | The value exceeds what an `HTTPV` holds. |
| `HTTPD906W` | `STORAGE RECLAIM FOR pgm FAILED, RC=n` | A CGI's subpool survived it. Storage leaks until restart. |
| `HTTPD907W` | `STALE BUSY ENTRY FOR CLIENT(c) … -- CLEARED` | A busy-list entry outlived its request; cleared and processing resumed. |
| `HTTPD908E` | `EXTERNAL PROGRAM pgm reason` | The CGI could not be linked — `reason` says why. An abend after loading is reported with its code; a load failure carries none — the console log has the cause (e.g. `IEA703I` for a storage failure). |
| `HTTPD909E` | `UNSUPPORTED HTTP STATUS n REQUESTED, SENT 500` | A handler asked for a status code this build has no reason phrase for. See `httpstat.c`. |
| `HTTPD910E` | `ABEND Sxxx DETECTED` / `ABEND Unnnn DETECTED` | The main task's recovery caught an abend. |
| `HTTPD911E` | `func LOCK FAILURE` | A lock could not be taken. |
| `HTTPD912E` | `TERMINATE FAILED WITH RC=xxxxxxxx` | The shutdown path itself abended; cleanup is incomplete. |
| `HTTPD913E` | `UNEXPECTED SUBTYPE n IN HTTP_GET()` | Internal: a content subtype with no branch. A 500 is sent. |
| `HTTPD914E` | `HTTP_SEND() POSITION UNDERFLOW n` | Internal buffer accounting fault. |
| `HTTPD915E` | `func LENGTH INCONSISTENCY, LEN=n BUFLEN=n` | The file reader's length accounting disagrees with itself. |
| `HTTPD916E` | `HTTP_SET_BUSY() FAILED` | The client could not be marked busy and cannot be served. |
| `HTTPD920E` | `pgm MUST BE CALLED BY THE HTTPD SERVER` | A CGI module was run from TSO or batch. Not a fault — it just cannot work that way. |
| `HTTPD921W` | `ABEND0C1 ALLOCATED n KB IN n BLOCKS, ABENDING WITHOUT FREEING` | The deliberate-abend test module. Only ever seen if `ABEND0C1` is registered. |
| `HTTPD922E` | `HTTPD CONTROL BLOCK NOT FOUND BY func` | A CGI could not reach the server block through the GRT. A traceback follows. |
| `HTTPD923E` | `HTTPC CONTROL BLOCK NOT FOUND BY func` | As above for the client block. |
| `HTTPD999E` | `OUT OF MEMORY` | The address space is out of storage; the connection is dropped. Raise the region. |
