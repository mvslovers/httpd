#import "../bookmaster/bookmaster.typ": *

= The Started Task and the Package <ref-stc>

#idx("started task")
#idx("procedure", "started task")
#idx("package", "SMP")
HTTPD runs as a started task. This chapter lists the procedure that ships with
HTTPD and every DD name in it. It then shows how the server gets its
authorization and its identity, which calls it makes to the security product,
the order of the start, and the return codes of the job step. The last part is
a map of the package: the FMID, the data sets and the members.

The _HTTPD User's Guide_ shows how to install the package and how to run the
task, in "Installing HTTPD" and "Operating HTTPD".

== The Procedure <ref-stc-proc>

#idx("HTTPD procedure")
#idx("SYS2.PROCLIB(HTTPD)")
The member #cmd("HTTPD") of the sample library is the procedure. The operator
copies it to the procedure library, for example to #cmd("SYS2.PROCLIB(HTTPD)").
This is the member without its comment lines.

```
//HTTPD    PROC M=HTTPPRM0,
//            D='SYS2.PARMLIB',
//            STCUSER=HTTPD,
//            STCGRP=USER
//HTTPD    EXEC PGM=HTTPD,REGION=8M,TIME=1440,
//            PARM='STCUSER=&STCUSER STCGROUP=&STCGRP'
//STEPLIB  DD  DISP=SHR,DSN=HTTPD.LINKLIB
//SYSUDUMP DD  SYSOUT=*
//HTTPDERR DD  SYSOUT=*
//HTTPDOUT DD  SYSOUT=*
//HTTPDIN  DD  DUMMY
//SNAP     DD  SYSOUT=*
//HTTPDBG  DD  SYSOUT=*
//HTTPSTAT DD  SYSOUT=*
//HTTPPRM  DD  DSN=&D(&M),DISP=SHR,FREE=CLOSE
```

The member also has the statements #cmd("HASPCKPT") and #cmd("HASPACE1"), as
comment lines (@ref-stc-jes).

The package build writes the name of the library that SMP installs into, here
#cmd("HTTPD.LINKLIB"), into the #cmd("STEPLIB") statement of the member.

=== The symbolic parameters <ref-stc-sym>

#idx("symbolic parameters")
#idx("STCGRP")
#tab(caption: [The symbolic parameters of the procedure])[
  #table(columns: (1.1in, 1.3in, 1fr),
    [Symbolic], [Default], [Meaning],
    [#cmd("M")], [#cmd("HTTPPRM0")], [The member of the Parmlib. It is the member
      name in the #cmd("HTTPPRM") DD.],
    [#cmd("D")], [#cmd("SYS2.PARMLIB")], [The data set that holds the member.],
    [#cmd("STCUSER")], [#cmd("HTTPD")], [The user id that the server takes as
      its identity (@ref-stc-id).],
    [#cmd("STCGRP")], [#cmd("USER")], [The group of that user id. The name is
      #cmd("STCGRP") and not #cmd("STCGROUP"), because the name of a JCL
      symbolic parameter has at most 7 characters. The PARM keyword that the
      program reads is #cmd("STCGROUP=").],
  )
]

The START command overrides a symbolic:

```
S HTTPD
S HTTPD,M=HTTPPRM1
S HTTPD,D='MY.PARMLIB'
S HTTPD,STCUSER=WEBSRV,STCGRP=WEBGRP
```

=== The EXEC statement <ref-stc-exec>

#idx("PARM keywords")
#idx("STCGROUP", "PARM keyword")
#idx("REGION", "started task")
The program is #cmd("HTTPD"). The procedure sets #cmd("REGION=8M") and
#cmd("TIME=1440"). The server reads its arguments from the PARM string. It
splits the string at blanks and not at commas. It knows these keywords.

#tab(caption: [The PARM keywords])[
  #table(columns: (1.3in, 1fr),
    [Keyword], [Meaning],
    [#cmd("STCUSER=name")], [The user id for the identity of the task. The
      default is #cmd("HTTPD"). A keyword without a value gives the default.],
    [#cmd("STCGROUP=name")], [The group for the identity. The default is
      #cmd("USER"). A keyword without a value gives the default.],
    [#cmd("CONFIG=name")], [Has no effect. HTTPD writes
      #cmd("HTTPD024W CONFIG=name IS IGNORED, CONFIGURATION COMES FROM THE HTTPPRM DD")
      and then #cmd("HTTPD024I USE S HTTPD,M=MEMBER TO SELECT A DIFFERENT MEMBER").
      The symbolic #cmd("M") selects the member.],
  )
]

HTTPD ignores any other word in the PARM string, without a message. The
configuration of the server comes from the Parmlib member and not from the PARM
string. @ref-prm lists the keywords of the member.

=== The region <ref-stc-region>

#idx("REGION", "size")
#idx("U0801")
#idx("C stack")
The #cmd("REGION") of the #cmd("EXEC") statement is the one limit on all the
storage that the server holds. It is not a Parmlib keyword. A server that runs
many module requests at the same time may need more than the #cmd("8M") of the
shipped procedure.

Every request that runs a module takes one contiguous block of storage for the
C stack of the module, from the region, for as long as the module runs.

#tab(caption: [What a module request costs])[
  #table(columns: (2.2in, 1fr),
    [What], [Size],
    [C stack of a module that sets nothing], [262,328 bytes, about 256 KB. It
      is the default of the C runtime (#cmd("@@CRT0") of LIBC/370). A module
      that sets #cmd("__stklen") gets that size (@ref-api-start).],
    [C stack of a module with its own #cmd("__stklen")], [The value plus 48
      bytes. mvsMF sets 64 KB, so a request of mvsMF takes 65,584 bytes.],
    [Stack of a worker thread], [64 KB, once per worker thread.],
    [Stack of the socket thread], [32 KB, once.],
    [Resident copy of a module], [The size of the load module, once per module
      and not per request (#cmd("RESIDENT"), @ref-prm-route).],
  )
]

A request runs on a worker thread, so at most #cmd("MAXTASK") module requests
run at once, and their stacks add up to #cmd("MAXTASK") times the stack of the
module. At the default #cmd("MAXTASK=9") and modules with the default stack
that is 9 times 262,328 = 2,360,952 bytes, plus 9 times 64 KB for the worker
threads. The stack is one contiguous block, so the free storage of the region
has to be whole enough to hold it and not only large enough in sum. Each step
of #cmd("MAXTASK") costs at least one more worker stack and one more module
stack, before the working storage of the module itself.

If the request cannot get its stack, the module does not start. The console
shows

#screen(```
@@CRT0 - No storage for C stack
HTTPD908E EXTERNAL PROGRAM REXX failed with U0801 ABEND
```)

and the client gets status 503 with the text #cmd("External program REXX failed
with U0801 ABEND"). The other requests of the server go on. A module that is
linked against a LIBC/370 older than 1.0.3 does not ask for the stack with a
condition and abends #cmd("S80A") instead, from inside the system.
#cmd("HTTPD904E") and #cmd("HTTPD999E") are the same shortage in the storage of
the server itself.

#idx("MAXTASK", "and REGION")
Only a measurement shows the size that a site needs. This one was made against
a release candidate of 4.4.0: 60 requests to REXX pages, 9 at a time. The
table gives the failed requests of the 60, one entry per run.

#tab(caption: [A measurement of parallel module requests])[
  #table(columns: (2.6in, 1fr),
    [Set-up], [Failed requests of 60],
    [#cmd("REGION=8M"), #cmd("RESIDENT=YES")], [6, 32, 0],
    [#cmd("REGION=8M"), HTTPD 4.3.0 with the same module libraries], [0, 4, 0],
    [#cmd("REGION=8M"), #cmd("RESIDENT=NO")], [0 (one run)],
    [#cmd("REGION=16M")], [0 (one run)],
  )
]

A mixed load of 6 requests at a time across all module types was clean in every
set-up. The sample is small and the runs are not independent. It shows an order
of magnitude and not a threshold. It was not run with Lua requests. Three rules
follow from it.

- For many parallel REXX or Lua requests, set #cmd("REGION=16M") in the
  procedure before you raise #cmd("MAXTASK"). 16M is the largest value that was
  tried. If it is not enough, lower #cmd("MAXTASK") and do not raise it.
- Raise #cmd("MAXTASK") only together with the region.
- Do not use #cmd("RESIDENT=NO") as the cure. It brings back the fetch of the
  module on every request.

== The DD Statements <ref-stc-dd>

#idx("DD names")
#idx("HTTPPRM", "DD name")
#idx("HTTPDOUT")
#idx("HTTPDERR")
#idx("HTTPDIN")
#idx("HTTPDBG")
#idx("HASPCKPT")
#idx("HASPACE1")
#idx("SYSUDUMP")
#idx("SNAP", "DD name")
#idx("HTTPSTAT", "DD name")
The next table shows what each DD of the procedure is for.

#tab(caption: [The DD statements of the procedure])[
  #table(columns: (0.9in, 2.0in, 1fr),
    [DD name], [In the procedure], [Use],
    [#cmd("STEPLIB")], [#cmd("DISP=SHR,") #cmd("DSN=HTTPD.LINKLIB")], [The load library
      of the server and of the modules of the routes (@ref-stc-steplib).],
    [#cmd("SYSUDUMP")], [#cmd("SYSOUT=*")], [Not opened by HTTPD. The system
      writes an abend dump to it.],
    [#cmd("HTTPDERR")], [#cmd("SYSOUT=*")], [Standard error of the server. The
      server opens it at the start. The start code of a server module opens it as
      well.],
    [#cmd("HTTPDOUT")], [#cmd("SYSOUT=*")], [Standard output of the server,
      opened in the same way.],
    [#cmd("HTTPDIN")], [#cmd("DUMMY")], [Standard input of the server. When the
      DD is missing, HTTPD opens the data set #cmd("NULLFILE") in its place.],
    [#cmd("SNAP")], [#cmd("SYSOUT=*")], [Not opened by HTTPD.],
    [#cmd("HTTPDBG")], [#cmd("SYSOUT=*")], [The debug output. HTTPD opens it
      only when the Parmlib member sets #cmd("DEBUG") to a value above 0
      (@ref-prm). When it cannot open the DD, it writes
      #cmd("HTTPD020E FOPEN FOR DD:HTTPDBG FAILED, ERRNO=n") and
      #cmd("HTTPD021I DEBUG/TRACE OUTPUT WILL BE TO DD:SYSTERM"), and it sends
      the debug output to the standard error stream, which is the DD
      #cmd("HTTPDERR"). When a write fails, HTTPD writes #cmd("HTTPD039W")
      once and stops the debug output until the next start.],
    [#cmd("HTTPSTAT")], [#cmd("SYSOUT=*")], [Not opened by HTTPD.],
    [#cmd("HTTPPRM")], [#cmd("DSN=&D(&M),") #cmd("DISP=SHR,FREE=CLOSE")], [The
      Parmlib member (@ref-stc-prm).],
    [#cmd("HASPCKPT")], [A comment line: #cmd("DISP=SHR,DSN=SYS1.HASPCKPT,") #cmd("UNIT=3350,VOL=SER=MVS000")],
      [The checkpoint data set of JES2, for server modules. Optional
      (@ref-stc-jes).],
    [#cmd("HASPACE1")], [A comment line: #cmd("DISP=SHR,DSN=SYS1.HASPACE,") #cmd("UNIT=3350,VOL=SER=SPOOL1")],
      [The spool data set of JES2, for server modules. Optional
      (@ref-stc-jes).],
  )
]

=== The Parmlib member <ref-stc-prm>

The server reads the DD #cmd("HTTPPRM") once, at the start, and to the end.
These cases are possible.

- The DD is missing. HTTPD writes
  #cmd("HTTPD020W CANNOT OPEN DD:HTTPPRM -- USING DEFAULTS") and starts with its
  built-in defaults and no routes.
- The DD is there and the read fails before the end of the member. HTTPD writes
  #cmd("HTTPD038E READ ERROR ON DD:HTTPPRM -- CONFIGURATION INCOMPLETE") and
  does not start (@ref-stc-rc).
- Otherwise the server uses the member (@ref-prm).

=== The standard stream DDs <ref-stc-std>

#idx("SYSPRINT", "not allowed")
#idx("SYSTERM", "not allowed")
#idx("SYSIN", "not allowed")
The procedure must not contain the DD names #cmd("SYSPRINT"), #cmd("SYSTERM")
and #cmd("SYSIN"). HTTPD tests for them before anything else. It uses the DD
names #cmd("HTTPDOUT"), #cmd("HTTPDERR") and #cmd("HTTPDIN") for its standard
streams instead. The routine in front of the server (#cmd("__premain"), in the
file #cmd("httpstrt.c")) writes these messages.

#tab(caption: [The messages for the standard stream DDs])[
  #table(columns: (0.95in, 1fr),
    [Message], [Text and cause],
    [#cmd("HTTPD014E")], [#cmd("SYSPRINT DD NOT ALLOWED, HTTPD USES HTTPDOUT FOR STDOUT")],
    [#cmd("HTTPD015E")], [#cmd("SYSTERM DD NOT ALLOWED, HTTPD USES HTTPDERR FOR STDERR")],
    [#cmd("HTTPD016E")], [#cmd("SYSIN DD NOT ALLOWED, HTTPD USES HTTPDIN FOR STDIN")],
    [#cmd("HTTPD017W")], [#cmd("HTTPDOUT DD NOT DEFINED, STDOUT IS NOT AVAILABLE").
      The DD cannot be opened.],
    [#cmd("HTTPD018W")], [#cmd("HTTPDERR DD NOT DEFINED, STDERR IS NOT AVAILABLE").
      The DD cannot be opened.],
    [#cmd("HTTPD019W")], [#cmd("HTTPDIN DD NOT DEFINED, STDIN IS NOT AVAILABLE").
      The DD cannot be opened, and neither can the data set #cmd("NULLFILE").],
  )
]

The routine first tests the three forbidden DDs and writes a message for each
one that it finds. When it found one, it stops there. Otherwise it opens the
three DDs of the server and writes a message for each that fails. Each of the
six conditions ends the routine with #cmd("EXIT_FAILURE"), and the server does
not start. The messages with the letter W stop the start as well.

=== The JES2 data sets <ref-stc-jes>

#idx("JES2 data sets")
#idx("mvsMF", "jobs API and HASPCKPT")
HTTPD does not open #cmd("HASPCKPT") or #cmd("HASPACE1") itself. A server module
runs in the task of the server, because HTTPD starts it with the LINK macro. The
module has no allocations of its own and opens a DD name in the allocations of
the started task. The jobs API of mvsMF reads the checkpoint and the spool of
JES2 through the function #cmd("jesopen()") of the C library.

#idx("jesopen")
The two DDs are optional. The sample member has both as comment lines. What
#cmd("jesopen()") does depends on the level of the C library that the module is
built with:

- With LIBC/370 2.24.0 and later, a DD in the procedure wins. When the
  procedure has no #cmd("HASPCKPT"), the function allocates #cmd("SYS1.HASPCKPT")
  itself, through the catalog or, when the data set is not cataloged, on the
  online volume that holds it. When the procedure has no #cmd("HASPACE1"), it
  allocates #cmd("SYS1.HASPACE") on the spool volume that the checkpoint names.
  mvsMF 1.4.0 and later are built with such a level.
- With an older level, the function opens only the DD names. The jobs API of
  mvsMF 1.3.0 and earlier is built this way, and it needs both DDs.

Uncomment both lines of the sample member in these two cases:

- You run mvsMF 1.3.0 or earlier.
- The JES2 of the system builds its data set names with a prefix other than
  #cmd("SYS1") (its #cmd("$DSNPRFX") value). The allocation looks only for the
  #cmd("SYS1") names. When it finds no checkpoint, the C library writes the
  console lines
  #cmd("SYS1.HASPCKPT is not cataloged and is on no online volume") and
  #cmd("JES2 uses another dataset name prefix than SYS1, or there is no JES2 checkpoint").

Leaving the DDs active in any other system does no harm, because a DD wins.
When you uncomment them, #cmd("UNIT") and #cmd("VOL=SER") must match the JES2
procedure of the system. Check them there before the first start. The DDs are
#cmd("DISP=SHR"), because JES2 owns the data sets.

When the data sets cannot be reached, the server still starts, and everything
except the jobs API works. This holds in both cases above: mvsMF 1.3.0 or
earlier without the DDs, and any mvsMF level on a system with another prefix
than #cmd("SYS1") without the DDs. The effect differs by request:

#tab(caption: [A request when the JES2 data sets cannot be opened])[
  #table(columns: (1.6in, 1fr),
    [Request], [Answer],
    [The job list], [Status 500, with the message "Unable to open JES2
      checkpoint and spool datasets".],
    [Any request that names a job by name and id, a spool read included],
      [Status 404, "job not found", although the job exists. The function that
      looks the job up cannot tell an unreachable JES2 from an unknown job.],
  )
]

Each of these requests writes the console line
#cmd("MVSMF201E UNABLE TO OPEN THE JES2 CHECKPOINT AND SPOOL DATA SETS"), and the
C library writes the line that names the data set that it could not open. Read
the console, not the status code. Submitting a job is not affected, because the
submit allocates its own internal reader.

== The STEPLIB <ref-stc-steplib>

#idx("STEPLIB")
#idx("DEV library")
#idx("HTTPD.DEV.LINKLIB")
#idx("HTTPD.LINKLIB")
#idx("load library")
HTTPD finds the modules of its routes by the standard search of the LINK macro.
It names the module from the #cmd("MOD=") keyword and passes no DCB. The system
looks in the STEPLIB of the task first, and then in the link list. The server
itself is the program of the EXEC statement. So the STEPLIB holds the server
and the modules of the routes, or the link list holds the modules.

The system takes the first library in the concatenation that has the module.
Two libraries are in use.

- *HTTPD.LINKLIB.* The library that SMP installs into. The sample procedure
  names it. Only the install job writes to it.
- *HTTPD.DEV.LINKLIB.* The target of #cmd("mbt deploy") in the project file. A
  deploy deletes the target before it receives into it, because RECEIVE does not
  merge. For this reason the deploy target is a library of its own and never
  the production library. Deploying does not activate anything. To run a
  development build, name #cmd("HTTPD.DEV.LINKLIB") in a STEPLIB deliberately.
  The usual way is a second HTTPD on another port. HTTPD refuses a second
  listener on a port that another HTTPD serves (HTTPD037E, @ref-stc-rc), so the
  production task keeps running. Put the DEV library before #cmd("HTTPD.LINKLIB")
  in the concatenation, so that its modules win.

Stop the task before you replace the load library. The task loads from it.

== Authorization <ref-stc-apf>

#idx("APF authorization")
#idx("SVC 244")
#idx("HTTPD002I")
#idx("authorization", "library or SVC")
The server needs APF authorization. It tests at entry whether the job step is
authorized, and it then makes the task authorized in either case.

+ *Test.* The server issues one TESTAUTH. The test runs before anything else,
  because after the next step every task is authorized.
+ *Setup.* The server authorizes its task with SVC 244. When the C runtime has
  recorded that, the server authorizes the STEPLIB in the same way and marks
  its own entry in the contents directory of the task as an authorized library
  entry. It then identifies the entry #cmd("CTHREAD") to the system and marks
  that entry the same way. #cmd("CTHREAD") is an entry point inside the load
  module #cmd("HTTPD") and not a member of a library. The server creates its
  threads through it. The server does this setup also when the step was
  authorized at entry.
+ *Message.* After the version banner the server tells how the step was
  authorized.

#tab(caption: [The two ways to be authorized])[
  #table(columns: (1.9in, 1fr),
    [Message], [Meaning],
    [#cmd("HTTPD002I AUTHORIZED BY LIBRARY (MODULE KEY 0)")], [The job step was
      authorized at entry. The libraries of the STEPLIB are in the list of
      APF-authorized libraries, or the module is in an authorized library of the
      link list. The system fetches the module with the protection key 0.],
    [#cmd("HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)")], [The job step was not
      authorized at entry. The server authorized the task itself. The system
      fetched the module with the key 8.],
  )
]

The key in the message is not measured. The server infers it from the route: a
step that is authorized at entry has its module fetched with the key 0, and an
unauthorized step has the key 8, and SVC 244 comes too late to change either.
The same module behaves differently under the two keys. @ref-api tells what a
server module may keep in storage.

The load module #cmd("HTTPD") is linked with the authorization code 1
(#cmd("ac = 1") in the module definition). SMP copies the module into the
library and does not link it again, so the code stays.

The next screen shows a start of the server in a library that is not in the APF
list. The server authorized itself by SVC.

#screen(raw(read("../ex/ug-ops/start-healthy.txt")))

When the setup fails, HTTPD writes
#cmd("HTTPD012E name UNABLE TO DYNAMICALLY OBTAIN APF AUTHORIZATION"), where
#cmd("name") is the name of the program as the system passed it to the server.
It does not write #cmd("HTTPD002I"), and the step ends with the return code of
the failed call (@ref-stc-rc).

At the end the server gives up what it took. It first puts back the security
environment that the task had at the start (@ref-stc-id), then it releases the
authorization of the STEPLIB and of the task with SVC 244.

== The Identity of the Task <ref-stc-id>

#idx("STC identity")
#idx("STCUSER")
#idx("STCGROUP")
#idx("ACEE", "started task")
#idx("HTTPD003W")
#idx("HTTPD004I")
#idx("HTTPD004W")
#idx("RACINIT")
RAKF has no table of started procedures. It uses the name of the procedure only
to find out that the caller is a started task, and it gives every started task
the same account, STC/STCGROUP. On a standard RAKF profile set this account
holds ALTER authority on every data set of the system. HTTPD does not keep it.
At the start the server logs on as the user id of #cmd("STCUSER") and
#cmd("STCGROUP") (@ref-stc-exec).

The server reads these two keywords from the PARM string and not from the
Parmlib member. The logon must happen before the server reads the member and
opens the file system, and these accesses should already run under the new
identity.

The logon works in this order:

+ The server enters supervisor state with the key 0. When it cannot, it writes
  #cmd("HTTPD003W") and skips the logon.
+ It remembers the security environment (ACEE) that the task has.
+ It calls RACINIT with #cmd("ENVIR=CREATE") for the user id and the group,
  without a password.
+ When the call works, the new ACEE becomes the resting identity of the address
  space, and the server writes #cmd("HTTPD004I"). When it fails, the server
  writes #cmd("HTTPD004W") and keeps the identity that the task had.

#tab(caption: [The messages for the identity])[
  #table(columns: (0.95in, 1fr),
    [Message], [Text and cause],
    [#cmd("HTTPD004I")], [#cmd("STC IDENTITY SET TO user/group VIA RACINIT").
      The logon worked.],
    [#cmd("HTTPD004W")], [#cmd("RACINIT ENVIR=CREATE FAILED FOR user/group RC=n").
      The logon failed, for example because the user id is not defined. The
      server goes on and keeps the identity that it inherited.],
    [#cmd("HTTPD003W")], [#cmd("RACINIT SKIPPED, CANNOT ENTER SUPERVISOR STATE").
      The server could not get the key 0 that RACINIT needs. It goes on with the
      inherited identity.],
  )
]

A failed logon is a warning and does not stop the start. The inherited identity
is the more powerful one, so the warning needs action: define the user id.

The new ACEE is not freed while the server runs. The server also keeps the old
ACEE and does not release it. When the logon fails, the task would otherwise be
left without any identity. At the stop the server puts the old ACEE back before
it releases the authorization (@ref-stc-apf). SVC 244 is gated by the profile
SVC244 in the class FACILITY, and the task took the authorization out under the
old account. The server therefore gives it back under the same account, and the
user id of #cmd("STCUSER") needs no access to that profile.

The user id of #cmd("STCUSER") needs READ authority on the Parmlib member and on
the document root. A server module that sets no ACEE of its own uses the
identity of the task for its accesses.

== What the Server Asks of the Security Product <ref-stc-rakf>

#idx("RAKF")
#idx("SAF")
#idx("RES=", "profile check")
#idx("HTTPD425W")
#idx("HTTPD426W")
The profiles belong to the site. HTTPD names none of them. The route decides
which resource a request is checked against, with the keyword #cmd("RES=")
(@ref-route). The server makes these calls.

#tab(caption: [The calls to the security product])[
  #table(columns: (1.6in, 1fr),
    [Call], [Use],
    [Logon of the task, RACINIT without a password], [The user id of
      #cmd("STCUSER") must be defined. Without it the server writes
      #cmd("HTTPD004W") (@ref-stc-id).],
    [Logon of a client, RACINIT with user id and password], [A route with
      #cmd("AUTH=BASIC"), #cmd("AUTH=FORM") or #cmd("AUTH=TOKEN") checks the
      user id and the password of the client. A form login that works gives
      #cmd("HTTPD007I"), and the release of a credential gives
      #cmd("HTTPD008I"). From HTTPD 4.5 a logon that fails for
      a reason other than the password gives
      #cmd("HTTPD009W LOGIN SESSION NOT CREATED, RACINIT FAILED RC=n"). A wrong
      password is the client's doing and is not written to the console.
      @ref-route has the policies.],
    [Check of a resource for a route with #cmd("RES=class:resource")], [The
      server asks for the access READ. The return codes 0 and 4 allow the
      request. Return code 4 means that no profile covers the resource. With
      #cmd("RES_NOPROFILE=DENY") the return code 4 refuses the request
      (@ref-prm-auth). @ref-route describes what happens on a refusal.],
    [Probe at the start], [For each route with #cmd("RES=") and an #cmd("AUTH=")
      other than #cmd("NONE"), the server asks once whether a profile covers the
      resource. It asks as its own identity. Only the return code 4 gives
      #cmd("HTTPD425W NO PROFILE FOR class:resource -- path NOT GATED"). The
      route still works, and every request that reaches the check passes. With
      #cmd("RES_NOPROFILE=DENY") the message is
      #cmd("HTTPD426W NO PROFILE FOR class:resource -- path REFUSES ALL REQUESTS"),
      and every request that reaches the check gets status 403. A typing error
      in the resource name looks like this. A class that RAKF does not define looks like this
      with RAKF 1.2.x and does not with RAKF 2.0.0, see below.],
    [Authorization by SVC 244], [The security product allows or refuses the SVC
      for the account of the task. The server does not check the profile
      itself.],
  )
]

A refusal is not a missing profile. A resource that is defined and has nobody
permitted gives the return code 8, and the probe stays silent. The return code 4
for an unprotected resource is a property of RAKF. Under RACF with PROTECTALL,
an unprotected resource gives 8 for an ordinary user and 0 for a SPECIAL user,
and a system with no security product gives 0. On those systems the probe stays
silent without a profile as well, so a silent probe does not prove that a
profile exists. #cmd("RES_NOPROFILE=DENY") acts on the return code 4 only. It
does not change what a system answers with 0 or with 8.

#idx("RAKF0012")
The level of RAKF decides what a class that it does not define gives. RAKF 2.0.0
(MVS/CE) gives the return code 8 for a class that is not in the class
descriptor table of the system. It writes #cmd("RAKF0012 UNDEFINED CLASS class")
to the console. The route refuses every request, the probe stays silent and
#cmd("RES_NOPROFILE") changes nothing. A class with a typing error gives this
answer. RAKF 1.2.x (measured on TK5) gives the return code 4 for such a class,
with no message: the probe writes #cmd("HTTPD425W") or, with
#cmd("RES_NOPROFILE=DENY"), #cmd("HTTPD426W"), and without the keyword the route
is open. A resource name with a typing error gives the return code 4 on both
levels when no profile covers the misspelled name. A measurement with the RAKF
of MVS/CE (RAKF 2.0.0) and with that of TK5 (RAKF 1.2.x) gave the return code 0
on both for a batch user, the operator #cmd("IBMUSER"), for the classes
#cmd("TAPEVOL"), #cmd("DASDVOL") and #cmd("TERMINAL") without a profile. The
start probe, which runs with the identity of the server, stays silent for these
classes, so its answer is not 4. HTTPD allows a request on the return code 0, so
#cmd("RES_NOPROFILE=DENY") does not close such a route. The measurement did not
use the ACEE of a client.

The _HTTPD User's Guide_ shows how to define the profiles with RAKF, in
"Authentication and Security".

== The Start in Order <ref-stc-start>

#idx("start-up sequence")
#idx("HTTPD001I")
#idx("READY message")
The server does these steps in this order.

+ The routine in front of the server tests the standard stream DDs
  (@ref-stc-std).
+ The server tests the authorization and sets it up (@ref-stc-apf). When the
  setup fails, it writes the banner and #cmd("HTTPD012E") and ends.
+ The banner: #cmd("HTTPD000I HTTPD version (commit) STARTING"), then
  #cmd("HTTPD005I") with the version of the C runtime, then
  #cmd("HTTPD006W BUILT FROM A MODIFIED WORKING TREE") only for a build that
  contains changes that were not committed, then #cmd("HTTPD002I").
+ The server gets the console interface. Without it, it writes
  #cmd("HTTPD090E UNABLE TO INITIALIZE CONSOLE INTERFACE") and ends.
+ The server reads the PARM string and sets the identity (@ref-stc-id).
+ The server reads the Parmlib member (@ref-stc-prm). Each route is registered
  as it is read, with #cmd("HTTPD036I") for a module and #cmd("HTTPD417I") for a
  location. When a route lost its policy, the server writes
  #cmd("HTTPD420E ROUTE AUTHORIZATION POLICY INCOMPLETE -- HTTPD WILL NOT START")
  and ends before it opens the port.
+ The server sets up the codepage tables, initializes the file system and opens
  the debug DD when the member asks for it. A file system that does not start
  gives #cmd("HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM"). The server goes on
  without static file serving.
+ The server binds the listener (@ref-stc-bind).
+ The server tests the SMF recording. When a level is set and SMF is not
  active, it writes #cmd("HTTPD430W SMF TYPE n CONFIGURED BUT SMF INACTIVE")
  (@ref-smf).
+ The server probes the #cmd("RES=") profiles (@ref-stc-rakf).
+ The server holds the modules of the routes, unless the Parmlib member
  switches #cmd("RESIDENT") off (@ref-prm). For each module it looks up the
  entry in the library and then loads it once. Three messages are possible, and
  the route works in each case. The module is fetched for each request.
  - #cmd("HTTPD440W MODULE name NOT FOUND AT STARTUP, NOT HELD") — the module is
    not in the STEPLIB or the link list.
  - #cmd("HTTPD441W MODULE name NOT REUSABLE, FETCHED PER REQUEST") — the module
    is linked neither reentrant nor serially reusable.
  - #cmd("HTTPD442W MODULE name LOAD FAILED, ABEND CODE Sxxx REASON rr, FETCHED PER REQUEST").
+ The server creates the socket thread with a stack of 32 KB. When it cannot,
  it writes #cmd("HTTPD033E UNABLE TO CREATE SOCKET THREAD") and ends.
+ The server creates the pool of worker threads. A worker has a stack of 64 KB.
  A #cmd("MAXTASK") of 0 gives 9, and a #cmd("MINTASK") of 0 gives 3. When
  #cmd("MINTASK") is larger than #cmd("MAXTASK"), the server raises
  #cmd("MAXTASK") to the value of #cmd("MINTASK"). When it cannot create the
  pool, it writes
  #cmd("HTTPD034W UNABLE TO CREATE WORKER THREADS, DYNAMIC DOCUMENTS DISABLED")
  and goes on.
+ Each thread writes #cmd("HTTPD061I STARTING ...") when it starts.
+ The server sets up the key for the credentials. When that fails, it writes
  #cmd("HTTPD047E UNABLE TO INITIALIZE SERVER CREDENTIAL KEY, RC=n") and goes
  on. The key is new at every start.
+ The server waits until the socket thread is in its accept loop. It tests up to
  50 times at an interval of 100 ms, which is 5 seconds. A healthy start does
  not wait. When the thread is not running, the server writes
  #cmd("HTTPD064E SOCKET THREAD IS NOT RUNNING, START ABANDONED") and stops.
+ The server allows 5 queued console commands (@ref-con) and writes
  #cmd("HTTPD001I HTTPD version READY - SERVING docroot"). Without a document
  root the text is #cmd("HTTPD001I HTTPD version READY - NO DOCUMENT ROOT").
  The version in the message is in upper case.

The main task then waits for console commands.

The next screen shows a start with a second member of the Parmlib, which defines
more routes. The screen is the real console output of a test system.

#screen(raw(read("../ex/ug-ops/start-lab-config.txt")))

=== The listener <ref-stc-bind>

#idx("listener")
#idx("HTTPD054I")
#idx("HTTPD037E")
#idx("BIND_TRIES")
#idx("ENQ", "port")
The server binds to all interfaces of the host (INADDR_ANY). The steps are:

+ The server takes an ENQ of the scope SYSTEM with the major name #cmd("HTTPD") and
  the minor name #cmd("HTTPD.PORT.nnnnn"), where #cmd("nnnnn") is the port with
  5 digits. The server asks for the resource and does not wait. When another
  address space holds the resource, or the ENQ fails for another reason, HTTPD
  writes #cmd("HTTPD037E HTTPD IS ALREADY ACTIVE ON PORT n, THIS INSTANCE ENDS")
  and ends. A second server on another port starts. The system releases the ENQ
  when the task ends, also after an abend.
+ The server creates the socket. When that fails, it writes
  #cmd("HTTPD028E SOCKET() FAILED, RC=n ERRNO=n").
+ The server binds the socket. When the bind fails with EADDRINUSE, the server
  closes every socket that is bound to the port in the socket table. Sockets of
  an earlier instance that ended without closing them are among these. When it
  closed any, it waits and tries the bind once more.
+ When the bind still fails, the server writes
  #cmd("HTTPD030E BIND() FAILED FOR HTTP PORT, RC=n ERRNO=n"). With EADDRINUSE it
  tries again up to #cmd("BIND_TRIES") times. Before each try it writes
  #cmd("HTTPD030I EADDRINUSE, WAITING FOR TCPIP TO RELEASE HTTP PORT n") and
  waits #cmd("BIND_SLEEP") seconds. When no try works, it writes
  #cmd("HTTPD030E") a second time and ends. A bind that fails with another
  error gets the second #cmd("HTTPD030E") without a retry.
+ The server calls listen with the queue length of #cmd("LISTEN_QUEUE"). When
  that fails, it writes
  #cmd("HTTPD031E LISTEN() FAILED, RC=n ERRNO=n") and ends.
+ On success the server writes #cmd("HTTPD054I LISTENING ON ANY PORT n").

@ref-prm lists the keywords #cmd("PORT"), #cmd("BIND_TRIES"), #cmd("BIND_SLEEP")
and #cmd("LISTEN_QUEUE") with their ranges.

== Return Codes and Abends <ref-stc-rc>

#idx("return codes")
#idx("completion codes")
#idx("abend codes")
The step ends with return code 0 after a normal stop with #cmd("P HTTPD")
(@ref-con). The server issues no user abend. The next table lists every other
way that the step ends by the decision of the server.

#tab(caption: [The return codes of the job step])[
  #table(columns: (0.7in, 1fr),
    [Return code], [Cause and message],
    [8], [The server refused the start while it read the Parmlib or opened the
      port. The causes are a member that could not be read to the end
      (#cmd("HTTPD038E")), a route whose policy is incomplete
      (#cmd("HTTPD420E")), a port that another HTTPD serves
      (#cmd("HTTPD037E")), a failed socket call (#cmd("HTTPD028E")), a failed
      bind (#cmd("HTTPD030E")) and a failed listen (#cmd("HTTPD031E")).],
    [8], [The socket thread could not be created (#cmd("HTTPD033E")), or it is
      not running after 5 seconds (#cmd("HTTPD064E")).],
    [8], [The task has no COM area, so the commands MODIFY and STOP cannot work
      (#cmd("HTTPD090E")).],
    [not 0], [The authorization failed. When SVC 244 fails, HTTPD writes
      #cmd("HTTPD012E") and the step ends with the return code of that call.
      When a later part of the setup fails, the step ends with its return code,
      for example 4 when the module entry is not found, and the server writes
      no message and no #cmd("HTTPD002I").],
  )
]

In the first two rows, the server has started its stop sequence when it ends.
It writes #cmd("HTTPD098I HTTPD SHUTTING DOWN") and #cmd("HTTPD099I HTTPD SHUTDOWN COMPLETE"),
and the message that names the cause comes before them. The code 8 is what
tells a refused start from a normal stop.

The next messages do not change the return code.

#tab(caption: [Messages with no effect on the return code])[
  #table(columns: (0.95in, 1fr),
    [Message], [Effect],
    [#cmd("HTTPD003W"), #cmd("HTTPD004W")], [The server runs with the inherited
      identity (@ref-stc-id).],
    [#cmd("HTTPD020W")], [The server runs on the built-in defaults.],
    [#cmd("HTTPD034W")], [The server runs without worker threads.],
    [#cmd("HTTPD039W")], [The server runs with the debug output off.],
    [#cmd("HTTPD044W")], [The server runs without a file system.],
    [#cmd("HTTPD047E")], [The server runs, and logins do not work.],
    [#cmd("HTTPD440W"), #cmd("HTTPD441W"), #cmd("HTTPD442W")], [The route works.
      The module is fetched for each request.],
    [#cmd("HTTPD912E")], [#cmd("TERMINATE FAILED WITH RC=........"). The stop
      sequence itself failed. The code of the step stays as it was.],
  )
]

The routine in front of the server (@ref-stc-std) ends with
#cmd("EXIT_FAILURE") for the messages #cmd("HTTPD014E") to #cmd("HTTPD019W").

A worker thread is protected by a recovery routine. These two messages concern
an abend while a worker serves a request:

- #cmd("HTTPD062E ABEND xxxxxxxx IN WORKER(aaaaaa) CLIENT(aaaaaaaa) SOCKET(n)")
  names the abend code in hexadecimal. The worker goes on with the next
  request. The server does not stop.
- #cmd("HTTPD063E NO RECOVERY IN WORKER(aaaaaa), CLIENT(aaaaaaaa) SOCKET(n) DROPPED")
  is written when the recovery routine of the worker could not be set up. The
  client is dropped without an answer.

The module ABEND0C1 abends on purpose to test this recovery (@ref-dm). It
writes #cmd("HTTPD921W ABEND0C1 ALLOCATED n KB IN n BLOCKS, ABENDING WITHOUT FREEING")
first. @apx-messages lists every message with its meaning.

== The Package <ref-stc-pkg>

#idx("package")
#idx("SMP", "package")
#idx("FMID")
#idx("THTP")
#idx("SYSMOD")
HTTPD comes as an SMP4 function. The command #cmd("mbt package") writes the
archive. This section is the map of what the archive contains and what the
install job does. The _HTTPD User's Guide_ explains how to install it, in
"Installing HTTPD".

=== The FMID <ref-stc-fmid>

The FMID of a release is #cmd("THTP") and three digits: the major version, the
minor version and the patch version. HTTPD 4.4.0 is #cmd("THTP440"), and
HTTPD 4.5.0 is #cmd("THTP450"). A release spends its FMID once, and the FMID is
never used again. No component of the version may exceed 9, because a FMID has
7 characters and no room for a second digit. A patch is a new function level and
not a PTF on the old one. The package build makes no PTF.

The build derives the FMID and the predecessor from the version in the project
file. Version 4.5.0 gives #cmd("THTP450") and deletes #cmd("THTP440"). A patch
release needs an explicit FMID.

Each release deletes its predecessor. The SYSMOD carries a #cmd("++VER")
statement with the operand #cmd("DELETE"), for example
#cmd("++VER(Z038) DELETE(THTP430)") in the package of 4.4.0. SMP keys the
ownership of an element on the name of the module and not on the library. A
SYSMOD that does not own the module #cmd("HTTPD") cannot install it, and the
install then copies nothing while every condition code reads success. The
#cmd("DELETE") passes the ownership on. SMP deletes the load modules of the old
FMID from the target library and copies the new ones in.

The deleted FMID has no backup entry in the SMP data set. For this reason the
APPLY of an upgrade ends with return code 4, and the install job lets the ACCEPT
run for a code of 4 or less. A return code does not show that the work was
done. List the members of #cmd("HTTPD.LINKLIB") after an install, and look for
the message #cmd("HMA2380") (copy successful).

The names of the data sets carry no version. There is one installation, and an
upgrade installs into the same libraries. The allocation job is run once.

=== The files in the archive <ref-stc-files>

The archive contains these files. The word #var("version") is the version of the
release, for example #cmd("4.4.0-dev") for a development build.

#tab(caption: [The files in the archive])[
  #table(columns: (2.3in, 1fr),
    [File], [Contents],
    [#cmd("README.md")], [The installation guide of the product.],
    [#cmd("httpd-version-load.xmit")], [The load modules, as an XMIT file.],
    [#cmd("httpd-version-samplib.xmit")], [The sample library, as an XMIT file.],
    [#cmd("httpd-version-alloc.jcl")], [The job that allocates the data sets of
      SMP.],
    [#cmd("httpd-version-inst.jcl")], [The job that installs the SYSMOD.],
    [#cmd("httpd-webroot.img")], [The document root as a UFS370 image. SMP does
      not know it.],
  )
]

The XMIT files are uploaded in binary, as RECFM=FB with LRECL=80, to data set
names of the operator's choice. The install job names them in the operands
#cmd("INDSN") of its two RECEIVE steps, and these are the only lines that the
operator changes in it.

=== The data sets <ref-stc-dsn>

#idx("HTTPD.LINKLIB")
#idx("HTTPD.SAMPLIB")
#idx("HTTPD.AHTTPLOD")
#idx("HTTPD.HTTPLOAD")
#idx("HTTPD.WEBROOT.UFS")
#idx("data sets", "package")
#tab(caption: [The data sets of the package])[
  #table(columns: (1.4in, 0.5in, 0.5in, 0.9in, 1fr),
    [Name], [DSORG], [RECFM], [Space], [Use],
    [#cmd("HTTPD.LINKLIB")], [PO], [U], [CYL (5,2,20)], [The target library of
      SMP, with BLKSIZE 15040. The STEPLIB of the procedure names it. Only the
      install job writes to it. The allocation job creates it.],
    [#cmd("HTTPD.AHTTPLOD")], [PO], [U], [CYL (5,2,20)], [The distribution
      library of SMP, with BLKSIZE 15040. The ACCEPT fills it. A RESTORE
      returns to it. The allocation job creates it.],
    [#cmd("HTTPD.HTTPLOAD")], [PO], [by RECEIVE], [by RECEIVE], [The staging
      library. The install job receives the load XMIT into it and deletes it at
      the end.],
    [#cmd("HTTPD.SAMPLIB")], [PO], [by RECEIVE], [by RECEIVE], [The sample
      library, created by TSO RECEIVE. SMP does not own it.],
    [#cmd("HTTPD.WEBROOT.UFS")], [PS], [U], [4096-byte blocks (256)], [The disk
      image of the document root, with BLKSIZE 4096. The job #cmd("HTTPWEBR")
      allocates it, and the operator uploads the image into it. SMP does not
      own it.],
  )
]

The allocation job sets no LRECL for the two load libraries. The two data sets
that TSO RECEIVE creates get their attributes from the XMIT files.

The allocation job has no DELETE step, on purpose. Once the FMID is accepted,
the distribution library holds the accepted copy of every element. A job that
scratched it again would leave the SMP inventory reporting "accepted" with an
empty library behind it. The space is in cylinders, because a space in tracks
reaches the limit of 16 extents long before the volume is full. The job uses
#cmd("UNIT=SYSDA"). Change the unit and the volume in the job if you want
others.

The install job runs these steps in this order.

#tab(caption: [The steps of the install job])[
  #table(columns: (0.9in, 1fr),
    [Step], [Action],
    [#cmd("DELOLD")], [IDCAMS deletes #cmd("HTTPD.HTTPLOAD") and
      #cmd("HTTPD.SAMPLIB"). #cmd("SET MAXCC=0") lets a first install pass,
      where neither exists. TSO RECEIVE does not merge into an existing data
      set, so the step makes the job run more than once.],
    [#cmd("RECV1")], [TSO RECEIVE of the load XMIT into #cmd("HTTPD.HTTPLOAD").],
    [#cmd("RECV2")], [TSO RECEIVE of the sample XMIT into #cmd("HTTPD.SAMPLIB").],
    [#cmd("RECV")], [SMP RECEIVE of the SYSMOD. The SYSMOD is part of the job as
      in-stream data and contains the JCLIN.],
    [#cmd("APPLYCHK")], [SMP APPLY with the operand #cmd("CHECK"). It changes
      nothing. The next step runs only if this step ends with code 0.],
    [#cmd("APPLY")], [SMP APPLY. It copies the modules into
      #cmd("HTTPD.LINKLIB").],
    [#cmd("ACCEPT")], [SMP ACCEPT. It runs when the APPLY ended with a code of 4
      or less. It makes this level the base of a later RESTORE.],
    [#cmd("CLEANUP")], [IDCAMS deletes #cmd("HTTPD.HTTPLOAD"). It runs only when
      the ACCEPT ended with code 0. Comment the step out to keep the library.],
  )
]

The JCLIN of the SYSMOD copies the members with IEBCOPY from
#cmd("HTTPD.AHTTPLOD") to #cmd("HTTPD.LINKLIB") and lists five modules. SMP does
not link them again, so every attribute stays as the linkage editor of the
build set it, including the authorization code of the server.

The sample library is not part of SMP. The configuration pattern has comment
lines that reach column 80, and an MCS card ends at column 72. The product
owns the patterns in the library, and the site owns the copies that it makes.

=== The members <ref-stc-members>

#idx("load modules")
#idx("HTTPDSRV")
#idx("HTTPDM")
#idx("HTTPDMTT")
#idx("ABEND0C1")
#idx("HTTPPRM0")
#idx("HTTPWEBR")
The load library #cmd("HTTPD.LINKLIB") has five members. All five are linked
reentrant and serially reusable.

#tab(caption: [The members of HTTPD.LINKLIB])[
  #table(columns: (1.1in, 1fr),
    [Member], [Contents],
    [#cmd("HTTPD")], [The server. It is the only member that a started task
      names. It is linked with the authorization code 1 and contains the entry
      point #cmd("CTHREAD").],
    [#cmd("HTTPDSRV")], [The display of the control blocks of the server
      (@ref-dm).],
    [#cmd("HTTPDM")], [The display of storage (@ref-dm).],
    [#cmd("HTTPDMTT")], [The display of the master trace table (@ref-dm).],
    [#cmd("ABEND0C1")], [The abend test (@ref-dm).],
  )
]

All five modules ship, because the package build packages every module and has
no way to exclude one. A module is inert unless a route of the Parmlib member
names it. The line for ABEND0C1 in the sample member is a comment, and so are the
lines for the three display modules.

The sample library #cmd("HTTPD.SAMPLIB") has three members.

#tab(caption: [The members of HTTPD.SAMPLIB])[
  #table(columns: (1.1in, 1fr),
    [Member], [Contents],
    [#cmd("HTTPD")], [The procedure of this chapter. The operator copies it to the
      procedure library.],
    [#cmd("HTTPPRM0")], [The pattern for the Parmlib member (@ref-prm).],
    [#cmd("HTTPWEBR")], [The job that allocates #cmd("HTTPD.WEBROOT.UFS")
      (@ref-stc-web).],
  )
]

=== The document root disk <ref-stc-web>

#idx("webroot image")
#idx("httpd-webroot.img")
#idx("UFS370", "webroot")
The file #cmd("httpd-webroot.img") is a formatted UFS370 file system with the
pages that HTTPD serves from its document root. The operator uploads it with
IND\$FILE into the data set that the job #cmd("HTTPWEBR") allocated, and mounts
it with UFSD.

The job allocates the data set with the attributes that the image needs.

- The data set exists before the upload, because IND\$FILE takes the DCB from the
  label of an existing data set and invents one only for a new data set. The job
  fixes #cmd("BLKSIZE=4096"), the block size that the image was formatted with.
  UFSD compares the two at MOUNT and refuses a mismatch with
  #cmd("UFSD062E SUPERBLOCK VALIDATION FAILED FOR dsn").
- The space is in the primary extent only. The file system is addressed by block
  number inside the primary extent, so a block in a secondary extent would never
  be reached.
- The space is 256 blocks of 4096 bytes, which is the 1 MB of the shipped
  image. MVS rounds the request up to a track. The superblock and not the data
  set says how large the file system is.
- The job card has no NOTIFY operand. The value #cmd("NOTIFY=&SYSUID") is
  replaced only by TSO SUBMIT and mvsMF, and any other way to submit gets
  IEF627I and a JCL error.

An image that is larger than the shipped one needs a larger space. Count it in
blocks of 4096 bytes. The _HTTPD User's Guide_ shows the whole procedure, in
"Installing HTTPD".
