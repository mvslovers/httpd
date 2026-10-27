#import "../bookmaster/bookmaster.typ": *

= Configuring HTTPD <ug-config>

#idx("configuration")#idx("Parmlib member")#idx("HTTPPRM", "DD name")
HTTPD takes its whole configuration from one member of a Parmlib data set.
There is no other file and no command that writes the configuration. This
chapter is organised by what you want to do: select the member, write it, read
back what HTTPD understood, and change the port, the worker tasks, the time
limits, the document root and the recording of SMF records. It ends with what
HTTPD does with a bad line. The routes (`MOD=` and `LOC=`) have their own
chapter, @ug-routes, and the authentication has @ug-security. Every keyword,
with its default and its limits, is listed in 'The HTTPPRM Keywords' in the
_HTTPD Reference_.

== Which Member HTTPD Reads <ug-config-where>

#idx("HTTPPRM", "member")#idx("M, procedure symbol")#idx("D, procedure symbol")
#idx("S HTTPD,M=")#idx("DD:HTTPPRM")
HTTPD reads the DD name #cmd("HTTPPRM"). The started task procedure points the
DD name at one member of a Parmlib data set:

```
//HTTPPRM  DD  DSN=&D(&M),DISP=SHR,FREE=CLOSE
```

The procedure has two symbolic parameters for this line. #cmd("M") is the
member, with the default #cmd("HTTPPRM0"). #cmd("D") is the data set, with the
default #cmd("SYS2.PARMLIB"). You choose another member or another data set
when you start the task. The member is the only difference between two
configurations.

#screen(```
S HTTPD
S HTTPD,M=HTTPPRM1
S HTTPD,D='MY.PARMLIB'
```)

The procedure and its other symbols are described in 'The Started Task and the
Package' in the _HTTPD Reference_. The installation is in @ug-install.

#note[*Two servers.* To run a second HTTPD on the same system, give it its own
#cmd("PORT") and so its own member. A second server on another port starts. A
second server on a port that the first one holds ends with the return code 8
(@ug-config-port).]

Three things need a word:

- #cmd("STCUSER") and #cmd("STCGROUP") come from the PARM string of the
  procedure, not from the member. A line with one of them in the member gives
  #cmd("HTTPD029W"). @ug-security describes the identity of the task.
- A PARM word #cmd("CONFIG=name") has no effect. HTTPD writes #cmd("HTTPD024W")
  and #cmd("HTTPD024I") and tells you to select the member with
  #cmd("S HTTPD,M=").
- A change in the member takes effect when you stop and start HTTPD.

When the DD name is missing, HTTPD writes #cmd("HTTPD020W") and starts with its
defaults: port 8080 and no routes. A read error gives #cmd("HTTPD038E") and the
return code 8. The effect of the missing routes shows in this capture. The path
of a module is now a request for a file, and there is no such file.

#screen(raw(read("../ex/ug-config/defaults-no-routes.txt")))

== The Format of the Member <ug-config-format>

#idx("Parmlib member", "format")#idx("comment")#idx("KEYWORD=VALUE")
The member has one setting on each line, in the form #cmd("KEYWORD=VALUE").

- Blanks around the keyword and around the value are removed. HTTPD changes the
  keyword to upper case. The value keeps its case. That matters for
  #cmd("REALM"), for paths and for module names in routes.
- A line that starts with #cmd("#") or #cmd("*") is a comment. A comment is a
  whole line. HTTPD has no comment at the end of a line, so text after a value
  is part of the value.
- HTTPD reads at most 255 characters of a line at once. There is no
  continuation line.
- When a keyword stands twice, the last line wins. The exceptions are
  #cmd("MOD") and #cmd("LOC"), where each line adds a route.

A line without an equals sign gives #cmd("HTTPD026W"). A keyword that HTTPD
does not know gives #cmd("HTTPD029W"). @ug-config-bad shows both.

== A Minimal Member <ug-config-min>

#idx("Parmlib member", "minimal")
A server that serves files needs very little. Everything else has a default.
This member names the port and the directory of the files:

#screen(raw(read("../ex/ug-config/docroot-subdir-prm.txt")))

With this member HTTPD listens on port 8080, serves the files below
#cmd("/www/docs") and has no route. Every path is a request for a file.
A route is needed for a module, or for a path that must ask for a
password (@ug-routes, @ug-security).

== A Production Member <ug-config-prod>

#idx("Parmlib member", "production")
A member for a server that has users writes down the time limits, the realm and
the recording explicitly, so that the values are in the member and not only in
the defaults. Every keyword in this example is described in the sections that
follow. Most values are the defaults. The routes are examples. They are
explained in @ug-routes.

```
# HTTPD production configuration
PORT=8080
MINTASK=3
MAXTASK=9
CLIENT_TIMEOUT=10
KEEPALIVE_TIMEOUT=5
KEEPALIVE_MAX=100
DOCROOT=/www
CODEPAGE=CP037
REALM=MVS.EXAMPLE
SESSION_TIMEOUT=30
SESSION_MAXAGE=480
SMF=ERROR
# routes: the first match wins
MOD=MVSMF /zosmf/info                    AUTH=NONE
MOD=MVSMF /zosmf/services/authenticate   AUTH=NONE
MOD=MVSMF /zosmf/*                       AUTH=TOKEN
LOC=/pub/*                               AUTH=NONE
LOC=/private/*                           AUTH=BASIC
```

The member that the captures of this guide use has more routes. @ug-routes
shows it.

== Reading Back What HTTPD Parsed <ug-config-dc>

#idx("D CONFIG")#idx("HTTPD128I")#idx("HTTPD133I")#idx("verify configuration")
After the start, ask HTTPD what it holds. The command is #cmd("F HTTPD,D C").

#screen(raw(read("../ex/ug-config/defaults-display-config.txt")))

The lines show the values that are in effect, after all the limits and
corrections. Each line has a message of its own:

#tab(caption: [The lines of D C])[
  #table(columns: (0.95in, 1fr),
    [Message], [Shows],
    [#cmd("HTTPD128I")], [The port and the least and the greatest number of
      worker tasks.],
    [#cmd("HTTPD129I")], [The client time limit, then the keep-alive time
      and count.],
    [#cmd("HTTPD130I")], [#cmd("SESSION_TIMEOUT") in minutes.],
    [#cmd("HTTPD135I")], [#cmd("SESSION_MAXAGE") in minutes.],
    [#cmd("HTTPD131I")], [The document root.],
    [#cmd("HTTPD134I")], [The code page.],
    [#cmd("HTTPD136I")], [The realm.],
    [#cmd("HTTPD132I")], [The SMF type and level.],
    [#cmd("HTTPD133I")], [The source of the configuration.],
    [#cmd("HTTPD137I")], [Whether modules are resident, and how many are
      held.],
    [#cmd("HTTPD138I")], [The value of #cmd("RES_NOPROFILE")
      (@ug-security-noprofile).],
  )
]

If a line shows another value than the one you wrote, HTTPD did not read the
member the way you thought. Look for a warning at the start (@ug-config-bad).
The routes are not part of this display. The display module #cmd("HTTPDSRV")
shows them. @ug-ops describes the console commands and the display modules.

The worker task counts that #cmd("D C") shows are the configured values.
A change with #cmd("SET MI") or #cmd("SET MA") does not appear here
(@ug-config-tasks).

== Changing the Port <ug-config-port>

#idx("PORT")#idx("port", "change")#idx("HTTPD054I")#idx("D PORTS")
The default port is 8080. To change it, edit the member, stop the server and
start it again. There is no command that moves the listener.

#screen(raw(read("../ex/ug-config/port-change-prm.txt")))

After the start, ask for the port:

#screen(raw(read("../ex/ug-config/port-change-d-ports.txt")))

HTTPD also writes the port at start, in the message #cmd("HTTPD054I").
Before it listens, HTTPD takes an ENQ on the port. A second HTTPD that asks for
the same port ends with #cmd("HTTPD037E") and the return code 8. When the
bind itself fails, HTTPD tries again: #cmd("BIND_TRIES") (default 10) times,
with #cmd("BIND_SLEEP") (default 10) seconds between the tries. The messages
are #cmd("HTTPD030E") and #cmd("HTTPD030I"). A failure of the socket call is
#cmd("HTTPD028E"), a failure of the listen call is #cmd("HTTPD031E").
#cmd("LISTEN_QUEUE") (default 5) is the length of the queue of connections that
wait for HTTPD to accept them.

#note[*A wrong port does not stop the server.* A port value that is not valid
gives #cmd("HTTPD023E"), but HTTPD goes on and listens on 8080. The member
below has #cmd("PORT=0"). The server starts, and the message #cmd("HTTPD054I")
names the port that it really uses. Check #cmd("HTTPD054I") or
#cmd("D PORTS") after every change of the port.]

#screen(raw(read("../ex/ug-config/refuse-bad-port-prm.txt")))
#screen(raw(read("../ex/ug-config/refuse-bad-port.txt")))

== Worker Threads <ug-config-tasks>

#idx("MINTASK")#idx("MAXTASK")#idx("worker threads")#idx("SET MI")#idx("SET MA")
HTTPD serves each request on a worker task. The pool starts with
#cmd("MINTASK") tasks (default 3) and can grow to #cmd("MAXTASK") (default 9).
You see the workers at start as lines #cmd("HTTPD061I STARTING WORKER"). The
captures in @ug-config-bad show three.

```
MINTASK=3
MAXTASK=9
```

In the member, both keywords accept 1 to 255. A value of 0 or less, or a
text that is not a number, is ignored and the earlier value stays. A value
above 255 is limited to 255.

The operator can change both numbers while the server runs, without a stop:

#screen(```
F HTTPD,SET MA 12
F HTTPD,SET MI 4
```)

#screen(raw(read("../ex/ug-ops/set-maxtask.txt")))

The command answers with the lines of the worker manager. The line
#cmd("HTTPD110I") shows the new #cmd("MINTASK") and #cmd("MAXTASK"). A raised
#cmd("MINTASK") starts the missing workers at once. After #cmd("SET MI 4"),
a line #cmd("HTTPD061I STARTING WORKER") follows.

The rules of the command differ from the rules of the member:

- The console accepts 1 to 100, not 255. A larger value is set to 100. In the
  capture below, 99999 gives a #cmd("MAXTASK") of 100.
- The console needs a number. Without one, and with a negative number, HTTPD
  answers #cmd("HTTPD149I") and changes nothing.
- A #cmd("MINTASK") above the #cmd("MAXTASK") raises the #cmd("MAXTASK"). A
  #cmd("MAXTASK") below the #cmd("MINTASK") lowers the #cmd("MINTASK").
- The change belongs to the running worker manager only. HTTPD does not write
  it to the member, and #cmd("D C") keeps showing the values of the member. At
  the next start the member counts again.

#screen(raw(read("../ex/ug-ops/set-maxtask-bad.txt")))

The commands and their messages are described in 'Console Commands' in the
_HTTPD Reference_, and their daily use in @ug-ops.

== Keep-Alive and Client Time Limits <ug-config-time>

#idx("CLIENT_TIMEOUT")#idx("KEEPALIVE_TIMEOUT")#idx("KEEPALIVE_MAX")
#idx("keep-alive")#idx("time-out", "client")
Three keywords set how long HTTPD waits for a client and how often a client
may reuse a connection.

#tab(caption: [Keywords for time and for connections])[
  #table(columns: (1.65in, 0.6in, 1fr),
    [Keyword], [Default], [Meaning],
    [#cmd("CLIENT_TIMEOUT")], [10], [Seconds that HTTPD waits for the next line
      of a request. The largest value is 255.],
    [#cmd("KEEPALIVE_TIMEOUT")], [5], [Seconds that HTTPD waits for the next
      request on a connection that has already carried a request.],
    [#cmd("KEEPALIVE_MAX")], [100], [How many requests a connection carries
      before HTTPD closes it (one more is answered with
      #cmd("Connection: close")). The values are 1 to 255.],
  )
]

#cmd("CLIENT_TIMEOUT") counts for each line of the request, not for the whole
request. A new connection waits for its first request line as long as
#cmd("CLIENT_TIMEOUT"). #cmd("KEEPALIVE_TIMEOUT") applies from the second
request on.

=== The Request Count of a Connection

#idx("KEEPALIVE_MAX", "count")
A connection carries #cmd("KEEPALIVE_MAX") plus one request. HTTPD answers the
last request of the connection with #cmd("Connection: close"). With the member

#screen(raw(read("../ex/ug-config/keepalive-max-prm.txt")))

the third response closes the connection:

#screen(raw(read("../ex/ug-config/keepalive-max-three.txt")))

The curl command uses one connection three times. The first two responses
say #cmd("Connection: keep-alive"), the third says #cmd("Connection: close").
Plan with one request more than the number that you write.

=== The Diagnostic of a Client Time-out

#idx("CLIENT_TIMEOUT_MSG")#idx("CLIENT_TIMEOUT_DUMP")#idx("CLIENT_STATS")
#idx("time-out", "diagnostic")
When a client is too slow, HTTPD writes a line to the console. Two keywords
control it. Both default to 1 (on). #cmd("CLIENT_TIMEOUT_MSG") writes the
line, and #cmd("CLIENT_TIMEOUT_DUMP") adds a dump of the characters that
arrived. HTTPD writes them only when the client has sent at least one
character of the line. A client that sends nothing leaves nothing on the
console.

The test below sets a limit of 3 seconds, sends half of a request line and
waits.

#screen(raw(read("../ex/ug-config/client-timeout-prm.txt")))
#screen(raw(read("../ex/ug-config/client-timeout-client.txt")))

The console shows:

#screen(raw(read("../ex/ug-config/client-timeout-console.txt")))

The first line names the address of the client block, the socket and the
seconds. It is in lower case and has no message ID. The dump shows what the
server received: 13 bytes, #cmd("GET /hello.tx"). It tells you how far the
request line came before the client stopped.

Raise #cmd("CLIENT_TIMEOUT") for clients on slow lines. Keep
#cmd("SESSION_TIMEOUT") and #cmd("SESSION_MAXAGE") longer than it
(@ug-config-sess).

#note[#cmd("CLIENT_STATS") is a third keyword of this group. It defaults to 1.
HTTPD stores the flag and the display module #cmd("HTTPDSRV") shows it. It
changes nothing else.]

== Sessions and the Client Time Limit <ug-config-sess>

#idx("SESSION_TIMEOUT")#idx("SESSION_MAXAGE")#idx("HTTPD026E")#idx("HTTPD032E")
The keywords #cmd("SESSION_TIMEOUT") (idle minutes, default 30) and
#cmd("SESSION_MAXAGE") (total minutes, default 480) belong to the logins that
@ug-security describes. This chapter shows only the check that HTTPD makes at
the end of the start. HTTPD changes both values to seconds and compares them
with #cmd("CLIENT_TIMEOUT"). If one is not larger, HTTPD writes
#cmd("HTTPD026E") for #cmd("SESSION_TIMEOUT") or #cmd("HTTPD032E") for
#cmd("SESSION_MAXAGE") and goes on. A limit of 0 is off and is not compared.

#screen(raw(read("../ex/ug-config/warn-session-unsafe-prm.txt")))
#screen(raw(read("../ex/ug-config/warn-session-unsafe.txt")))

== The Document Root, the Code Page, the Time Zone and the Trace <ug-config-misc>

=== The Document Root

#idx("DOCROOT")#idx("document root")
#cmd("DOCROOT") is the UFS directory that HTTPD puts in front of the path of
a request. The default is #cmd("/www"). The value must begin with a slash and
HTTPD keeps at most 127 characters. HTTPD removes one slash at the end. A
value without the first slash is ignored without a message, and the earlier
value stays. Look at #cmd("D C") after a change.

With #cmd("DOCROOT=/www/docs") the request for #cmd("/manual.txt") reads
#cmd("/www/docs/manual.txt"), and the request for the root gives the page
#cmd("index.html") of that directory:

#screen(raw(read("../ex/ug-config/docroot-subdir-file.txt")))
#screen(raw(read("../ex/ug-config/docroot-subdir-root.txt")))

A file in the old root is no longer reachable:

#screen(raw(read("../ex/ug-config/docroot-subdir-outside.txt")))

The identity of the task needs READ access to the document root
(@ug-security). The serving of files is described in @ug-static. The
keyword #cmd("UFS") (default 1) tells HTTPD to create its UFS system at start.
Leave it at the default.

=== The Code Page

#idx("CODEPAGE")#idx("code page")#idx("CP037")#idx("IBM1047")#idx("HTTPD070E")
#cmd("CODEPAGE") selects the table that translates between the ASCII on the
wire and the EBCDIC in HTTPD. The names are #cmd("CP037") (the default),
#cmd("IBM1047") and #cmd("LEGACY"), in any case. #cmd("LEGACY") is the table
of HTTPD 3.3.x. An empty value is #cmd("CP037"). Another name gives
#cmd("HTTPD070E"), and HTTPD uses #cmd("CP037"). #cmd("D C") shows the name as
the member has it (#cmd("HTTPD134I")), also when it is not one of the three.

=== The Time Zone

#idx("time zone")#idx("TZOFFSET")#idx("D TIME")#idx("HTTPD025I")
There is no Parmlib keyword for the time zone. HTTPD takes the offset from
the system. The command #cmd("F HTTPD,D TI") shows the time in GMT and the
local time with the offset in minutes:

#screen(raw(read("../ex/ug-ops/d-time.txt")))

The #cmd("Date") header of every response is in GMT. The old keyword
#cmd("TZOFFSET") is retired (@ug-config-retired). The message that it gives
ends with the text #cmd("SET TZ IN THE SYSENV OR ENVIRON DD TO OVERRIDE IT FOR ALL TASKS").

=== The Trace

#idx("DEBUG")#idx("HTTPDBG")#idx("trace")
#cmd("DEBUG=1") switches the trace on. It has one level: a value above 0
means on and 5 means the same as 1. With the trace on, HTTPD writes its trace
lines to the DD name #cmd("HTTPDBG") of the procedure. If HTTPD cannot open the
DD name, it writes #cmd("HTTPD020E") and #cmd("HTTPD021I"), and the trace goes
to #cmd("SYSTERM"). If a write to the trace fails, for example because the
data set is full, HTTPD writes #cmd("HTTPD039W") once, switches the trace off
and goes on. The trace stays off until the next start, and it ends at the point
of the failure. Give the DD name more room and restart the server to trace
again. Use the trace for a problem and switch it off after it.
@ug-trouble helps with problems.

== Recording SMF Records <ug-config-smf>

#idx("SMF")#idx("SMF", "level")#idx("HTTPD132I")#idx("HTTPD430W")#idx("SET STATS")
HTTPD can write one SMF record for a request, and one for a connection.
The default is that it writes none. To switch the recording on, choose a level.

#tab(caption: [The SMF levels])[
  #table(columns: (0.9in, 1fr),
    [Level], [HTTPD writes],
    [#cmd("NONE")], [No record. This is the default.],
    [#cmd("ERROR")], [A request record for each request with the status 400
      or more.],
    [#cmd("AUTH")], [The same requests as #cmd("ERROR"). The level is named
      for the statuses 401 and 403.],
    [#cmd("ALL")], [A request record for every request, and a session record
      for every connection.],
  )
]

There are two ways to set the level.

+ Write #cmd("SMF=ALL") in the member. The level is in effect from the start.
  With #cmd("SMF=ALL TYPE=244") you also choose the record type. The default
  type is 243, and the type must be in the range 128 to 255.
+ Use the command #cmd("F HTTPD,SET STATS ALL") while the server runs. The
  change lasts until the server stops. It does not change the member.

#screen(raw(read("../ex/ug-ops/set-stats-all.txt")))

Check the level with #cmd("D C"). The line #cmd("HTTPD132I") shows the type and
the level (the capture in @ug-config-dc). In the member, HTTPD compares the
level with the four names for as many characters as you write. #cmd("ERR") is
#cmd("ERROR"). #cmd("A") is #cmd("AUTH"), because #cmd("AUTH") comes before
#cmd("ALL"). Write the whole name. The console command #cmd("SET STATS")
accepts only the whole name. A word that is no level gives
#cmd("HTTPD431W") in the member and #cmd("HTTPD126E") at the console. A type
outside the range gives #cmd("HTTPD432W").

#screen(raw(read("../ex/ug-ops/set-stats-bad.txt")))

If the level is above #cmd("NONE") at the start and SMF is not active in the
system, HTTPD writes #cmd("HTTPD430W"). The server runs and writes no
records. It tests for each record whether SMF is active, so the records start
when SMF becomes active.

To find the records, select the record type (243 unless you chose another) with
the SMF dump program of your site. The SMF parameters of the installation
decide whether the system keeps that type. The layout of both records, field
by field, is in 'SMF Records' in the _HTTPD Reference_. The counters that the
display commands show (requests, errors, bytes) count at every level, also at
#cmd("NONE").

== What a Bad Line or a Bad Value Does <ug-config-bad>

#idx("HTTPD026W")#idx("HTTPD029W")#idx("configuration", "errors")
HTTPD does not stop for most typing errors in the member. It writes a message,
skips the line, and starts. After each change of the member, read the console
lines between #cmd("HTTPD000I") and #cmd("HTTPD001I").

=== A Line Without an Equals Sign

#idx("HTTPD026W")
A line that is not a comment and has no equals sign gives #cmd("HTTPD026W")
with the text of the line.

#screen(raw(read("../ex/ug-config/warn-unrecognized-line-prm.txt")))
#screen(raw(read("../ex/ug-config/warn-unrecognized-line.txt")))

=== An Unknown Keyword

#idx("HTTPD029W")
A keyword that HTTPD does not know gives #cmd("HTTPD029W") with the keyword.
A misspelling of a real keyword looks the same. The setting is not applied,
so the default stays.

#screen(raw(read("../ex/ug-config/warn-unknown-keyword-prm.txt")))
#screen(raw(read("../ex/ug-config/warn-unknown-keyword.txt")))

=== A Bad Value

What happens to a value that is not valid depends on the keyword. There are
four cases:

#tab(caption: [The reactions to a bad value])[
  #table(columns: (1.1in, 1fr),
    [Reaction], [Keywords and example],
    [Kept], [The earlier value stays and HTTPD writes nothing. A
      #cmd("MINTASK") of 0 or a text, and a #cmd("DOCROOT") that does not
      start with a slash, are of this kind.],
    [Limited], [HTTPD changes the value to the nearest allowed one. A
      #cmd("MAXTASK") of 300 becomes 255. #cmd("KEEPALIVE_MAX") is
      limited to 1 to 255.],
    [Read as 0], [A text that is not a number counts as 0. For the flags such
      as #cmd("DEBUG") and #cmd("UFS") this means off.],
    [Message], [A named value has its own warning: #cmd("HTTPD070E") for the
      code page, #cmd("HTTPD424W") for the realm, #cmd("HTTPD431W") and
      #cmd("HTTPD432W") for SMF, #cmd("HTTPD443W") for #cmd("RESIDENT"),
      #cmd("HTTPD023E") for the port (the server goes on with 8080).],
  )
]

Because the first case is silent, check the result with #cmd("D C")
(@ug-config-dc).

=== Refusals

#idx("refuse to start")#idx("HTTPD420E")
In some cases HTTPD does not start. The message #cmd("HTTPD420E") says that the
authorization policy of the routes is incomplete. The server ends with the
return code 8. Two causes are the common ones.

- A route has an #cmd("AUTH=") value that does not exist. The message
  #cmd("HTTPD419E") says that the policy of that route is lost.
- The keyword #cmd("LOGIN") has a value other than #cmd("NONE").

#screen(raw(read("../ex/ug-config/refuse-bad-auth-prm.txt")))
#screen(raw(read("../ex/ug-config/refuse-bad-auth.txt")))

@ug-security explains the values of #cmd("AUTH="). The messages are in
'Messages' in the _HTTPD Reference_.

== Retired Keywords <ug-config-retired>

#idx("retired keywords")#idx("LOGIN")#idx("TZOFFSET")#idx("RECLAIM")#idx("CGI=")
Older members may contain keywords that HTTPD no longer uses. Each one gets a
message so that nobody believes it still works.

#tab(caption: [The retired keywords])[
  #table(columns: (1.1in, 1.5in, 1fr),
    [Keyword], [Message], [What to do],
    [#cmd("LOGIN=")], [#cmd("HTTPD048W") for an empty value or #cmd("NONE"),
      otherwise #cmd("HTTPD048E") and #cmd("HTTPD420E")], [Delete it and write
      #cmd("AUTH=") on each route. Any value other than #cmd("NONE") refuses
      the start, because the routes without #cmd("AUTH=") would turn public.],
    [#cmd("TZOFFSET=")], [#cmd("HTTPD025W"), then #cmd("HTTPD025I")], [Delete
      it. The offset of the system applies (@ug-config-misc).],
    [#cmd("RECLAIM=")], [#cmd("HTTPD422W")], [An option of a #cmd("MOD") route,
      not a keyword. Delete it. The message says that the reclaim of the
      storage of modules is always on.],
    [#cmd("CGI=")], [#cmd("HTTPD410W")], [Write #cmd("MOD=") instead. HTTPD
      reads the line as a #cmd("MOD=") route.],
  )
]

With this member

#screen(raw(read("../ex/ug-config/warn-login-retired-prm.txt")))

HTTPD starts, and the console shows:

#screen(raw(read("../ex/ug-config/warn-login-retired.txt")))

With #cmd("LOGIN=BASIC") the start is refused:

#screen(raw(read("../ex/ug-config/refuse-login-basic-prm.txt")))
#screen(raw(read("../ex/ug-config/refuse-login-basic.txt")))
