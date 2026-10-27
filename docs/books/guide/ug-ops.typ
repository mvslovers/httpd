#import "../bookmaster/bookmaster.typ": *

= Operating HTTPD <ug-ops>

#idx("operation")
#idx("operator")
This chapter is for the operator. It shows how to start and stop HTTPD, what
a healthy start and a normal stop look like on the console, and how to look at
a running server and change a few of its settings without a restart. It also
shows how to read the server from a browser or from #cmd("curl") with the
display modules, and what to watch on a server that runs for weeks. The
complete list of commands and messages is in the _HTTPD Reference_, in the
chapters "Console Commands" and "Messages".

The screens in this chapter were taken from a test system. The numbers in them,
such as the control block addresses and the times, are those of that system and
will differ on yours.

== Starting <ug-ops-start>

#idx("start")
#idx("started task", "start")
Start HTTPD with the console command #cmd("S"). The installation put the
procedure into your procedure library (@ug-install).

#screen(```
S HTTPD
S HTTPD,M=HTTPPRM1
S HTTPD,D='MY.PARMLIB'
```)

The first form reads the member that the procedure names, the second reads
another member of the same library, and the third reads the member from
another library. The two symbolic parameters of the procedure are #cmd("M")
(the member) and #cmd("D") (the data set). The members, the keywords and the
route lines are the subject of @ug-config and @ug-routes.

The server reads the Parmlib member once, at the start. There is no command
that reads it again. A change of the member takes effect at the next start
(@ug-ops-restart).

=== A healthy start <ug-ops-start-healthy>

#idx("start", "healthy")
A good start looks like this on the console. The Parmlib member of this server
has four routes for the module MVSMF:

#screen(raw(read("../ex/ug-ops/start-healthy.txt")))

The first four lines come from the system. The messages of HTTPD come in the
order of the table. A message in the table that does not appear above
appears only in some cases.

#tab(caption: [The messages of a start, in order])[
  #table(columns: (1.4in, 1fr),
    [Message], [What it tells],
    [#cmd("HTTPD000I")], [The version and the commit of the build.],
    [#cmd("HTTPD005I")], [The version of LIBC/370 that HTTPD was linked with.],
    [#cmd("HTTPD006W")], [Only in a build from a changed working tree. A
      released build never writes it.],
    [#cmd("HTTPD002I")], [How HTTPD got its authority: from the library
      (module key 0) or by the SVC (key 8). Both are good. @ug-install explains
      the two ways.],
    [#cmd("HTTPD003W"), #cmd("HTTPD004I"), #cmd("HTTPD004W")], [The security
      identity of the task. #cmd("HTTPD004I") says that the task took the user
      ID and the group of the procedure parameters #cmd("STCUSER") and
      #cmd("STCGRP"). The two warnings say that the identity was not changed
      and the task keeps the one it was started with (@ug-security).],
    [#cmd("HTTPD020W")], [Only if the Parmlib member could not be opened
      (@ug-ops-refused-member).],
    [#cmd("HTTPD036I")], [One line for each route with a program (#cmd("MOD=")).
      The line names the module and the path.],
    [#cmd("HTTPD417I")], [One line for each static location (#cmd("LOC=")).],
    [#cmd("HTTPD430W")], [Only if the SMF level is above NONE and SMF is not
      active in the system (@ug-ops-smf).],
    [#cmd("HTTPD054I")], [HTTPD listens on the port.],
    [#cmd("HTTPD061I")], [One line for the socket thread and one for each
      worker that starts. The number of workers is the value of #cmd("MINTASK").],
    [#cmd("HTTPD001I")], [HTTPD is ready. The text is #cmd("READY - SERVING")
      and the document root, or #cmd("READY - NO DOCUMENT ROOT") on a server
      that has none.],
  )
]

HTTPD is ready for clients only when #cmd("HTTPD001I") has appeared. A start
that is stopped before that point ends with a message that says what failed.
@ug-trouble lists the common ones.

The next start used the lab configuration of this book. It adds the display
modules and some static locations:

#screen(raw(read("../ex/ug-ops/httpprm-lab-prm.txt")))

The routes of this member register seven programs and six locations. The
server writes one #cmd("HTTPD036I") for each program route and one
#cmd("HTTPD417I") for each location:

#screen(raw(read("../ex/ug-ops/start-lab-config.txt")))

Count the routes in the messages and compare the count with your Parmlib
member. A route that is missing from the messages is a route that the server
does not have.

=== A start that is refused <ug-ops-refused>

#idx("start", "refused")
#idx("HTTPD037E")
A second HTTPD must not run on the port of a first one. HTTPD takes an
exclusive lock with the name of the port when it starts. When the lock is held
by another HTTPD, the new one writes #cmd("HTTPD037E"), stops, and the job step
ends with return code 8. The member in this example has the port 8081, and
another HTTPD was already running on that port:

#screen(raw(read("../ex/ug-ops/refuse-port-in-use-prm.txt")))

#screen(raw(read("../ex/ug-ops/refuse-port-in-use.txt")))

The three lines #cmd("HTTPD098I"), #cmd("HTTPD416I") and #cmd("HTTPD099I") are
the lines of a normal stop (@ug-ops-stop). The new server has read the member
and registered the route (#cmd("HTTPD036I")) before it found the port taken, but
it never listens. The first server is not touched. Look at the running one with
#cmd("D P") (@ug-ops-commands) and decide: stop it, or give the new member
another #cmd("PORT").

The lock stops only a second HTTPD. A different program that holds the port is
found later, when HTTPD binds the port. @ug-ops-restart shows what the server
does in that case.

=== A missing Parmlib member is not a refusal <ug-ops-refused-member>

#idx("HTTPD020W")
The most dangerous start is the one that looks like a good one. A member name
that does not exist, as in #cmd("S HTTPD,M=NOSUCH"), does not stop the server.
HTTPD writes a warning, runs on its built-in defaults, and writes
#cmd("HTTPD001I READY"):

#screen(raw(read("../ex/ug-ops/refuse-member-missing.txt")))

Look at the capture. There is no #cmd("HTTPD036I") and no #cmd("HTTPD417I"):
the server has the port 8080 and no routes. It runs, but it is not the server
that the member would have given you. Treat #cmd("HTTPD020W") as a failed
start: stop the server, correct the member name or the library, and start
again. A line in the member that is not of the form #cmd("KEYWORD=VALUE") gives
#cmd("HTTPD026W") in the same way and the server goes on (@ug-config).

== Stopping <ug-ops-stop>

#idx("stop")
#idx("P HTTPD")
Stop HTTPD with the console command #cmd("P"):

#screen(```
P HTTPD
```)

HTTPD stops accepting connections, waits for its socket thread, ends the
workers and gives back what it holds. On the console this is:

#screen(raw(read("../ex/ug-ops/stop-normal.txt")))

The steps are these.

+ #cmd("HTTPD098I") starts the stop.
+ HTTPD waits up to 10 seconds for the socket thread to end. From the second
  second on it writes #cmd("HTTPD040I WAITING FOR SOCKET THREAD TO TERMINATE (n)").
  If the thread is still there after 10 seconds, #cmd("HTTPD041I") says that
  HTTPD detaches it by force.
+ The listener is closed and every worker ends. Each thread writes one
  #cmd("HTTPD060I"). The count is the socket thread and the workers that were
  running.
+ The modules that the server held in storage are released, and the clients and
  the routes are freed.
+ #cmd("HTTPD416I") shows the requests, errors and bytes of the run.
  #cmd("HTTPD099I") ends the sequence, and the job ends.

A normal stop ends with return code 0. If the shutdown itself fails, the
server writes #cmd("HTTPD912E") and the code of the failure. There is no
second way to stop HTTPD from the console, and no #cmd("MODIFY") command stops
it. The comment lines of the procedure that ships with HTTPD name a command
#cmd("F HTTPD,SHUTDOWN"). The server does not have it. This is what it
answers:

#screen(raw(read("../ex/ug-ops/shutdown-unknown.txt")))

A request that is in progress when the server stops is not guaranteed to
complete. The sends of the server look at the stop flags, and a module that
runs for a long time has to look at them itself (@ug-module-drain). Before you
stop the server, check with #cmd("D T") that no worker serves a client you care
about (@ug-ops-d-threads).

== Restarting <ug-ops-restart>

#idx("restart")
There is no restart command. Restart is a stop and a start:

#screen(```
P HTTPD
S HTTPD
```)

Wait for #cmd("HTTPD099I") before you start. Restart the server after

- a change of the Parmlib member, because it is read only at the start;
- a new level of a server module in the load library, because HTTPD holds the
  modules in storage (@ug-module-deploy);
- a new level of HTTPD itself.

Do not mix this up with the three commands that change a setting while the
server runs (@ug-ops-set). They cannot do the work of a restart.

After a quick restart the TCP/IP stack may not have released the port yet. The
new server then cannot bind it. It writes #cmd("HTTPD030E") with the codes of
the failure and, when the reason is that the address is in use, it does not give
up at once. It writes

#screen(```
HTTPD030I EADDRINUSE, WAITING FOR TCPIP TO RELEASE HTTP PORT 8080
```)

and tries again after a pause. The number of tries and the pause are the
Parmlib keywords #cmd("BIND_TRIES") (default 10) and #cmd("BIND_SLEEP")
(default 10 seconds). If the bind still fails after the last try, the server
writes #cmd("HTTPD030E") again and the start ends. The Reference chapter
"Messages" lists the codes. Wait for the line #cmd("HTTPD001I") before you
tell the users that the server is back.

== The Operator Commands <ug-ops-commands>

#idx("MODIFY command")
#idx("console commands")
A running HTTPD takes commands with the #cmd("MODIFY") command of the system:

#screen(```
F HTTPD,D C
```)

The answers are messages on the console, with the prefix #cmd("HTTPD"). The
answers never go to a web client. The commands are these.

#tab(caption: [The commands for the operator])[
  #table(columns: (1.9in, 1fr),
    [Command], [Task],
    [#cmd("HELP"), #cmd("?")], [List the commands.],
    [#cmd("D C")], [Which configuration is the server running with?],
    [#cmd("D P")], [Which port does it listen on?],
    [#cmd("D V")], [Which version and build?],
    [#cmd("D TI [-|+][min]")], [What time does the server see?],
    [#cmd("D L")], [Who is logged in?],
    [#cmd("D T")], [What are the threads doing?],
    [#cmd("D M address[,len]")], [Dump storage of the server.],
    [#cmd("D S")], [How many requests, errors and bytes? How many connections
      are open?],
    [#cmd("SET MA n")], [Change the largest number of workers.],
    [#cmd("SET MI n")], [Change the smallest number of workers.],
    [#cmd("SET STATS level [RESET]")], [Change the SMF level, or set the
      counters to zero.],
  )
]

Some rules hold for all commands:

- HTTPD writes every command back as #cmd("HTTPD100I CONS(n) \"text\""), also a
  command it does not know. The screens below show this line.
- The words are not case sensitive. A comma and a blank both separate words.
- A word matches a keyword when it is the beginning of the keyword, and the
  first match wins. #cmd("D T") is #cmd("DISPLAY THREADS"). The time is
  #cmd("D TI"). After the #cmd("F HTTPD,"), the #cmd("S") is #cmd("SET"), not
  the start command of the system, and #cmd("S MA") is #cmd("SET MAXTASK").
  A bare #cmd("S M") is also #cmd("MAXTASK"). Use the forms of the table.
- The server queues at most 5 commands. Do not send a command faster than
  the answer arrives.

=== Help <ug-ops-help>

#idx("HELP command")
#cmd("F HTTPD,HELP") and #cmd("F HTTPD,?") list the commands. The shortest
form of a keyword shows in capitals. The brackets that mark an optional part
show as tilde characters on the console, as in the capture below.

#screen(raw(read("../ex/ug-ops/help-question.txt")))

=== The configuration, the port, the version and the time <ug-ops-d-config>

#idx("DISPLAY CONFIG")
#cmd("D C") shows the values that the server holds after it has read the
member. Use it to see what the server made of the member, not what you meant
it to do:

#screen(raw(read("../ex/ug-ops/d-config.txt")))

Read the lines in this way.

#tab(caption: [The lines of D C])[
  #table(columns: (1.0in, 1fr),
    [Message], [What it tells],
    [#cmd("HTTPD128I")], [The port and the numbers of workers. These are the
      values from the Parmlib member. #cmd("SET MA") and #cmd("SET MI") do not
      change them (@ug-ops-set).],
    [#cmd("HTTPD129I")], [The client timeout in seconds, and the keep-alive
      timeout in seconds and the number of requests on one connection.],
    [#cmd("HTTPD130I"), #cmd("HTTPD135I")], [The idle time and the maximum age
      of a login, in minutes (@ug-security).],
    [#cmd("HTTPD131I")], [The document root.],
    [#cmd("HTTPD134I")], [The code page.],
    [#cmd("HTTPD136I")], [The realm of the Basic authentication.],
    [#cmd("HTTPD132I")], [The SMF record type and the level in effect now.],
    [#cmd("HTTPD133I")], [Where the configuration came from. #cmd("DD:HTTPPRM")
      means the member of the #cmd("HTTPPRM") DD of the procedure.],
    [#cmd("HTTPD137I")], [Whether the modules are held in storage, and how many
      different modules are held. The four modules in the screen above are MVSMF, HTTPDSRV,
      HTTPDM and HTTPDMTT; they have seven routes.],
    [#cmd("HTTPD138I")], [The value of #cmd("RES_NOPROFILE"), #cmd("ALLOW") or
      #cmd("DENY") (@ug-security-noprofile).],
  )
]

The command does not list the routes. The display module HTTPDSRV does
(@ug-ops-dm).

#cmd("D P") shows the port, #cmd("D V") the version and the build, and
#cmd("D TI") the time of the server in GMT and in local time. The offset is in
minutes. With a number, as in #cmd("D TI -60"), the command shows the time for
that offset. This changes only the display.

#screen(raw(read("../ex/ug-ops/d-ports.txt")))

#screen(raw(read("../ex/ug-ops/d-version.txt")))

#screen(raw(read("../ex/ug-ops/d-time.txt")))

=== Who is logged in <ug-ops-d-login>

#idx("DISPLAY LOGIN")
The server holds a login for a user until it expires (@ug-security). The login
has the security environment (the ACEE) of the user. #cmd("D L") shows how many
logins there are, and for each one the user ID, the address of the client and
the address of the ACEE:

#screen(raw(read("../ex/ug-ops/d-login.txt")))

=== The threads <ug-ops-d-threads>

#idx("DISPLAY THREADS")
#cmd("D T") shows each thread of the server in a block of lines that ends with a
line of dashes (#cmd("HTTPD199I")). The blocks come in this order: the main
thread, the socket thread, the dispatcher and the workers. The screen shows an
idle server with three workers:

#screen(raw(read("../ex/ug-ops/d-threads.txt")))

In the dispatcher block, #cmd("HTTPD110I") shows the smallest and the largest
number of workers that are in effect now, and #cmd("HTTPD111I") the number of
workers and the number of requests the dispatcher has handed out
(#cmd(".DISPCNT")). A worker block shows the state of the worker
(#cmd("WAITING") when it has nothing to do) and three times: when the worker
started, when it last waited, and when it last got a request. A worker that
serves a client has three more lines: the user, the address of the client and
the socket of the connection (#cmd("HTTPD118I") to #cmd("HTTPD120I")). That is
how you find a request that does not end (@ug-ops-monitor).

=== Storage <ug-ops-d-memory>

#idx("DISPLAY MEMORY")
#cmd("D M address,length") dumps storage of the server. The address is in hex
and must be below 16 MB (hex 00FFFFFF), else the server answers with
#cmd("HTTPD144E"). Without an address the answer is #cmd("HTTPD148I"), a line
that shows the syntax. The length is a decimal number, or a hex number with the
prefix #cmd("0x"). A number with a leading 0 is octal. The default is 256 bytes, and the largest dump is 4096. An
address that the task cannot read ends the dump with #cmd("HTTPD147E"), and the
server goes on.

#screen(raw(read("../ex/ug-ops/d-memory.txt")))

The dump itself is not a numbered message. #cmd("HTTPD145I") ends it. The
command is a tool for a person who knows the control blocks. The same dump is
easier to read, and has links that follow the pointers, in the display
modules (@ug-ops-dm).

=== The statistics <ug-ops-d-stats>

#idx("DISPLAY STATS")
#cmd("D S") shows the SMF level and four counters:

#screen(raw(read("../ex/ug-ops/d-stats.txt")))

#cmd("REQUESTS") is the number of requests that the server has handled since
the start, #cmd("ERRORS") the number of responses with a status of 400 or more,
and #cmd("BYTES") the number of bytes it has sent. #cmd("ACTIVE") is the number
of connections that are open now. The three counters count at every SMF level,
also at NONE. The same three counters are in the line #cmd("HTTPD416I") at the
stop.

=== Changing the number of workers and the SMF level <ug-ops-set>

#idx("SET MAXTASK")
#idx("SET MINTASK")
#idx("SET STATS")
Three settings can change while the server runs.

*The number of workers.* #cmd("SET MA n") sets the largest number of worker
threads, #cmd("SET MI n") the smallest. Give the number: without one, and with
a negative number, HTTPD answers #cmd("HTTPD149I") and changes nothing. HTTPD
takes the value 0 as 1 and a value above 100 as 100, and a word that is not a
number as 1. The new value holds until the server stops. The Parmlib member
does not change, and #cmd("D C") still shows the values from the member.

The answer is the dispatcher block of #cmd("D T"). It shows the new limits.
The count of workers in it is the count from before the server reacted. The
server then starts or ends workers, and each one writes #cmd("HTTPD061I") or
#cmd("HTTPD060I"):

#screen(raw(read("../ex/ug-ops/set-maxtask.txt")))

#screen(raw(read("../ex/ug-ops/set-mintask.txt")))

The second screen shows that a larger #cmd("MINTASK") starts the missing
worker at once. The next screen shows the cut to 100. When MAXTASK is set below
MINTASK, MINTASK follows it down, and when MINTASK is set above MAXTASK,
MAXTASK follows it up. Every worker has a stack of 64 KB (the value
#cmd("STACKSIZE(65536)") in #cmd("HTTPD061I")) and the region of the server is
fixed in the procedure, so do not raise the limits without need.

#screen(raw(read("../ex/ug-ops/set-maxtask-bad.txt")))

*The SMF level.* #cmd("SET STATS") with #cmd("NONE"), #cmd("ERROR"),
#cmd("AUTH") or #cmd("ALL") sets the level of recording at once. Write the
name in full. The word #cmd("RESET") after the level, or alone, sets the three
counters to zero. The number of active connections is not set to zero.

#screen(raw(read("../ex/ug-ops/set-stats-all.txt")))

#screen(raw(read("../ex/ug-ops/set-stats-reset.txt")))

A level that the server does not know is an error, and it keeps the old level:

#screen(raw(read("../ex/ug-ops/set-stats-bad.txt")))

=== Commands that are not accepted <ug-ops-rejected>

#idx("HTTPD101E")
A word that HTTPD does not know as the first word gives #cmd("HTTPD101E") and
the help text. Look at the first lines of the answer, the rest is the help:

#screen(raw(read("../ex/ug-ops/unknown-command.txt")))

A known first word with an unknown second word gives only the error:

#screen(raw(read("../ex/ug-ops/d-unknown.txt")))

#screen(raw(read("../ex/ug-ops/set-unknown.txt")))

#cmd("DISPLAY") or #cmd("SET") alone gets no answer except the echo. #cmd("SET STATS") without a level gives #cmd("HTTPD127W"), and #cmd("SET MA") or #cmd("SET MI") without a number gives #cmd("HTTPD149I"). Check that the echo
#cmd("HTTPD100I") shows the command that you meant to send, and then that the
answer is what you expect. A command that reaches the server and is not
understood is not an abend, it is a mistyped command.

== The Display Modules over HTTP <ug-ops-dm>

#idx("display modules")
#idx("HTTPDSRV")
#idx("HTTPDM")
#idx("HTTPDMTT")
HTTPD comes with three small server modules that show the running server
through HTTP. HTTPDSRV shows the control blocks of the server, such as the
server block, the worker pool and the routes. HTTPDM shows any piece of storage
as a dump. HTTPDMTT shows the Master Trace Table of the system. A fourth module,
ABEND0C1, abends on purpose for a test of the recovery. The _HTTPD Reference_
describes each one in the chapter "The Display Modules". This section shows
how an operator uses them.

=== Setting up the routes <ug-ops-dm-setup>

The server registers no route for the modules by itself. The recommended
lines are the ones that the lab member above has:

#screen(```
MOD=HTTPDSRV /.dsrv     AUTH=FORM
MOD=HTTPDM   /.dm       AUTH=BASIC
MOD=HTTPDMTT /.dmtt     AUTH=BASIC
```)

Three rules apply, and they are not optional.

- *Always write the AUTH=.* A route without it is public. A public
  #cmd("/.dm") is a window into the storage of the server, and a public
  #cmd("/.dmtt") is the console log of the system for everybody.
- *The modules do not look at the user.* Everyone who passes the gate of the
  route sees everything, including the console log and storage that may hold
  data of other users. To limit the route to some users, add a RACF resource to
  the line, for example #cmd("RES=FACILITY:HTTPD.ADMIN"). The line is described
  in @ug-routes and @ug-security.
- *Use the modules on a test system or for a short time.* They are tools for
  diagnosis, not endpoints for production. Take the routes out of the member,
  and restart, when the work is done.

The path of the route for HTTPDM must be exactly #cmd("/.dm"), because the
pages of HTTPDSRV have links to that path.

=== What authentication they need <ug-ops-dm-auth>

#idx("display modules", "authentication")
Every route takes every kind of credential that HTTPD knows: the cookie of a
login, a bearer token, and a user ID and password in Basic form. The mode in
the line, #cmd("FORM") or #cmd("BASIC"), says only what a client without a
credential gets. #cmd("FORM") sends a browser to a login page. #cmd("BASIC")
answers with the status 401 and the request to log in. For a tool such as
#cmd("curl") this means that #cmd("curl -u") works against all three routes,
also against the route of HTTPDSRV, which has #cmd("AUTH=FORM"). @ug-security
explains the modes.

A request without a credential is answered with the status 401 and the realm
of the server (the #cmd("REALM") of the member, shown in #cmd("D C")):

#screen(raw(read("../ex/ug-ops/dm-401.txt")))

A user ID with a wrong password gets the same answer. The server does not say
which of the two is wrong:

#screen(raw(read("../ex/ug-ops/dm-badauth.txt")))

The user ID must be known to the security product. The password is in the
command line of #cmd("curl") and so in the process list and in the history of
the shell. Give the user ID alone (#cmd("-u IBMUSER")) and #cmd("curl") asks
for the password. The Basic form sends the user ID and the password with every
request, encoded but not encrypted.

=== HTTPDM, a dump of storage <ug-ops-dm-dm>

#idx("/.dm")
The parameters of HTTPDM are the address (#cmd("m")), the length (#cmd("l")),
the size of a line (#cmd("c")) and the output form (#cmd("d")). The address is
in hex, the length is decimal, and the longest dump is 4096 bytes. The default
output is an HTML page where every group of four bytes is a link to the storage
that the four bytes point to. With #cmd("d=1") the answer is plain text. The
parameter #cmd("t") is a title that goes unchanged into the first line, so do
not put text from other people into it.

The first example reads 16 bytes at the address 10 (hex), which is 16 in
decimal. The first word there is the pointer to the CVT:

#screen(raw(read("../ex/ug-ops/dm-cvt.txt")))

The second example follows the pointer. The address 0001D048 is the first word
of the first dump. It reads 64 bytes in lines of 16 bytes, and the parameter
#cmd("t") puts the name CVT in the first line:

#screen(raw(read("../ex/ug-ops/dm-cvt-map.txt")))

The two commands in the captures cut the answer down to the dump with
#cmd("sed"), because the page has a form around it. In a browser you get the
whole page and follow the links.

=== HTTPDSRV, the control blocks <ug-ops-dm-dsrv>

#idx("/.dsrv")
HTTPDSRV is for the browser. The parameter #cmd("target") says what to show:
#cmd("HTTPD") (the default), #cmd("MGR"), #cmd("MOD"), #cmd("WORKERS"),
#cmd("TASK"), #cmd("FILE") and #cmd("FS"). Some of them need an address in the
parameter #cmd("m"). The pages give the offset, the name, the meaning and the
value of each field. The _HTTPD Reference_ lists the targets. The help page of
HTTPDSRV lists more targets than the module has, and says that some need an
address when they do not. The Reference has the correct list, and this guide
uses only the targets of the Reference.

The server block shows the state and the counters of the whole server. The
flag byte (offset +002C) is the state, the counters of requests, errors and
bytes sent and the number of active connections are the same as in #cmd("D S"),
and the up time is the time of the start. The screen is cut after the counters:

#screen(raw(read("../ex/ug-ops/dsrv-httpd.txt").split("\n").slice(0, 43).join("\n")))

The block of the manager of the worker pool shows the limits that are in effect
now and the number of requests that were handed out:

#screen(raw(read("../ex/ug-ops/dsrv-mgr.txt")))

The route array of the server block (offset +0044) is the address for the
target #cmd("MOD"). The page for it shows every route with its path, its
program, its authentication mode and its RACF resource. This is the place to
check that the server holds the routes that you wrote in the member. The
screen is cut after the third route. The lab member has thirteen:

#screen(raw(read("../ex/ug-ops/dsrv-route.txt").split("\n").slice(0, 40).join("\n")))

=== HTTPDMTT, the Master Trace Table <ug-ops-mtt>

#idx("master trace table")
#idx("/.dmtt")
Every message that HTTPD writes to the console also goes into the Master Trace
Table of the system. The table keeps the most recent messages of the console,
and HTTPD writes only what an operator can act on. What went wrong with a
client is in the response, not here. So the table is the diagnostic channel of
the server, and HTTPDMTT shows it in a browser or with #cmd("curl"), without a
TSO session. The module takes one parameter, #cmd("d"). With #cmd("d=1") the
answer is plain text. This is the end of the table after the start of the lab
server (the capture shows the last four lines of the answer):

#screen(raw(read("../ex/ug-ops/dmtt-tail.txt")))

Each line has some fields of the table entry, the time of the entry and the
text of the message. The module does not select or search. Use the plain form
with a tool of your own, for example #cmd("grep"). The table holds only the
recent messages, so a server that has run for a long time may have lost its
start messages: read the table soon after a failure. It holds the messages of
the whole system. It is as sensitive as the console, and the route needs the strongest
authentication that you have (@ug-security).

The module ABEND0C1 abends on purpose, to show that the recovery of the server
works. Register it only on a test system (@ug-ops-abend).

== Worker Abends <ug-ops-abend>

#idx("abend", "worker")
#idx("ESTAE")
#idx("HTTPD908E")
#idx("HTTPD062E")
HTTPD has two layers of recovery, and an abend in a request does not stop the
server.

*A module abends.* A module runs in the task of a worker. If it abends,
HTTPD frees the storage the module took from its request area, writes
#cmd("HTTPD908E EXTERNAL PROGRAM name failed with Sxxx ABEND") to the console,
and, if the module has not started a response, sends the client the status 503
with a text body. The worker goes on with the next request. If the free of the
storage fails, #cmd("HTTPD906W") tells you. The module chapter says more about
what a module has to do for itself (@ug-module-abend).

*The server code abends.* If the abend is in the code of the server and not in
a module, the worker writes #cmd("HTTPD062E ABEND code IN WORKER(..)
CLIENT(..) SOCKET(n)"), closes the client, and goes on with the next request.
If the worker cannot recover at all, the message is #cmd("HTTPD063E"), which
says that there is no recovery and the client is dropped.

What to do: write down the message, in particular the name of the module and
the code. A module that abends with every request is a defect of the module,
and a module that abends now and then points to its input. The Master Trace
Table keeps the message (@ug-ops-mtt). Do not restart the server for one
#cmd("HTTPD908E"): the worker lives on. A repeated #cmd("HTTPD062E") with the
same code is different. It points to the server itself, and @ug-trouble
is the place to go on.

On a test system you can check the recovery. Register ABEND0C1 with
#cmd("AUTH=BASIC") and call it a few times. Every call must give the status 503
and the text "External program ABEND0C1 failed with S0C1 ABEND", and the other
routes must go on working.

== Recording with SMF <ug-ops-smf>

#idx("SMF", "operator")
HTTPD can write an SMF record for each request, and for each connection at
the highest level. This chapter gives only what the operator needs. The record
layouts and the keywords are in the _HTTPD Reference_, in the chapter "SMF
Records".

The level is one of NONE (the default), ERROR (a record for each request with a
status of 400 or more), AUTH (the requests with a status of 401 or 403, and the
other errors) and ALL (every request and every connection). The default type is
243. The Parmlib keyword #cmd("SMF") sets the level at the start, and
#cmd("SET STATS") sets it while the server runs. A level that you set with the
command lasts until the server stops. The member does not change, so the next
start uses the level of the member.

You can see the level in two places: #cmd("HTTPD132I") in #cmd("D C") and
#cmd("HTTPD123I") in #cmd("D S"). The counters of #cmd("D S") run at every
level. They do not tell you whether records are written.

If the member asks for a level above NONE and SMF is not active in the system,
the server writes #cmd("HTTPD430W") at the start and runs, but it writes no
record. It checks for every record whether SMF is active, so the records
start when SMF becomes active. An SMF that does not record the type is a
matter of the SMF parameters of your installation, which this guide does not
cover.

== Things to Know <ug-ops-know>

- *Stop with P.* There is no #cmd("F HTTPD,SHUTDOWN"). The system answers it
  with #cmd("HTTPD101E").
- *A good start ends with HTTPD001I.* A start without #cmd("HTTPD036I") lines
  that match the member is not a good start, even with #cmd("HTTPD001I")
  (@ug-ops-refused-member).
- *The member is read once.* The only changes at run time are #cmd("SET MA"),
  #cmd("SET MI") and #cmd("SET STATS"). They last until the stop.
- *A new module needs a restart.* HTTPD holds the modules in storage
  (@ug-module-deploy).
- *The abbreviations take the first match.* Use #cmd("D TI") for the time and
  give the number to #cmd("SET MA") and #cmd("SET MI").
- *Never register a display module without AUTH=.*

== Watching a Long-Running Server <ug-ops-monitor>

#idx("monitoring")
A server that runs for weeks needs few commands. The commands show what they
show, and no more. There is no command that shows the free storage of the
region, and none that keeps a history. Write the readings into your own log if
you need one.

*Is it up, and busy?* #cmd("D S") shows the requests, the errors, the bytes and
the active connections. Look at it at the same time each day, and keep the
numbers. The difference between two readings is the work of the period. The
number of #cmd("ERRORS") is the number of responses with a status of 400 or
more. A client that sends a wrong password adds to it, so it is not the same as
a fault of the server. After #cmd("SET STATS ... RESET") the counters start
again from zero. The count of active connections is not reset.

*Are the workers free?* #cmd("D T") shows the state of each worker. At rest all
workers are #cmd("WAITING"). A worker that has served the same client for a
long time shows the client in the lines #cmd("HTTPD118I") to
#cmd("HTTPD120I"). Compare the time of the last start of work
(#cmd("HTTPD117I")) with the time now (#cmd("D TI")). If all workers up to
MAXTASK are busy and the clients wait, raise #cmd("MAXTASK") with
#cmd("SET MA") and look at the dispatcher block again. If one request is stuck
in a module, the module is the cause, and more workers do not cure it
(@ug-trouble).

*Memory.* The region of the server is fixed in the procedure. The commands show
the sizes of the thread stacks, and #cmd("D C") the number of modules that the
server holds. They do not show how much storage is left. Abends of workers and
modules (#cmd("HTTPD062E"), #cmd("HTTPD908E")) are in the Master Trace Table
(@ug-ops-mtt). Do not raise
#cmd("MAXTASK") far above what the load needs.

*Logins.* #cmd("D L") shows the users who are logged in. A login ends after the
idle time or the maximum age that #cmd("D C") shows in #cmd("HTTPD130I") and
#cmd("HTTPD135I"). Many logins of one user from one address may mean a client that logs in again
for every request (@ug-security).

*Time.* #cmd("D TI") shows the GMT and the local time of the server. The
offset is the one of the system. Compare the time with a clock you trust
after a change of the time zone.

*The long view.* With SMF at ERROR, and SMF active and set to keep the record type, the failed
requests can be read later from the SMF data (@ug-ops-smf). The Master Trace Table
shows the messages of the last hours, no more.
