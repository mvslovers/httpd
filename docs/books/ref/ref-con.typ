#import "../bookmaster/bookmaster.typ": *

= Console Commands <ref-con>

#idx("console commands", "reference")
#idx("MODIFY command")
#idx("operator commands")
The operator controls HTTPD from the console with the MODIFY command and the
STOP command. The _HTTPD User's Guide_ shows how to use them, in "Operating
HTTPD". This chapter lists the syntax of every command and every message that
a command writes.

== Syntax <ref-con-syntax>

```
/F HTTPD,command
/P HTTPD
```

The name #cmd("HTTPD") is the name of the started task. HTTPD splits the text
of a MODIFY command into words. A blank and a comma both separate words, so
#cmd("/F HTTPD,D,C") and #cmd("/F HTTPD,D C") are the same command.

- *The echo.* HTTPD first writes the text it received, as
  #cmd("HTTPD100I CONS(n) \"text\""). The number is the id of the console. The
  line is written for every MODIFY command, also for one that HTTPD does not
  know.
- *Case.* Commands are not case sensitive.
- *Abbreviations.* A word matches a keyword when it is the beginning of the
  keyword. #cmd("D"), #cmd("DIS") and #cmd("DISPLAY") are the same word. HTTPD
  tries the keywords in the order of the tables below, and the first match
  wins. This makes some short forms mean something else than they seem to.
  #cmd("D T") is #cmd("DISPLAY THREADS"), not #cmd("DISPLAY TIME"). Use
  #cmd("D TI") for the time. Likewise #cmd("D C") is #cmd("CONFIG"), #cmd("D L")
  is #cmd("LOGIN"), #cmd("D M") is #cmd("MEMORY"), #cmd("D P") is #cmd("PORTS"),
  #cmd("D S") is #cmd("STATS") and #cmd("D V") is #cmd("VERSION"). For
  #cmd("SET"), #cmd("S MA") is #cmd("MAXTASK"), #cmd("S MI") is
  #cmd("MINTASK") and #cmd("S S") is #cmd("STATS"). A plain #cmd("S M") is
  #cmd("MAXTASK"). The help text shows the shortest form in capitals, for
  example #cmd("DISPLAY TIme").
- *No operand.* #cmd("DISPLAY") or #cmd("SET") alone, with nothing after it,
  gets no answer except the echo.
- *The answers.* All answers are messages to the console. None of them goes to
  a client or to a DD name. Each message is one line.
- *Queue.* HTTPD accepts at most 5 MODIFY commands that wait for it at one
  time.
- *Case of the output.* HTTPD writes its fixed text in upper case. Values keep
  their case, for example the document root or the name of the codepage.
- The command #cmd("/P HTTPD") (STOP) ends the server (@ref-con-stop).
  There is no MODIFY command that stops it.

== The Commands <ref-con-cmds>

#tab(caption: [The MODIFY commands])[
  #table(columns: (1.9in, 1fr),
    [Command], [Action],
    [#cmd("DISPLAY CONFIG")\ #cmd("D C")], [Shows the configuration that the
      server runs with (@ref-con-config).],
    [#cmd("DISPLAY LOGIN")\ #cmd("D L")], [Shows the users who are logged in
      (@ref-con-login).],
    [#cmd("DISPLAY MEMORY xxxxxx[,nnn]")\ #cmd("D M xxxxxx")], [Dumps storage
      (@ref-con-memory).],
    [#cmd("DISPLAY PORTS")\ #cmd("D P")], [Shows the port the server listens
      on: #cmd("HTTPD102I HTTPD IS LISTENING ON PORT n").],
    [#cmd("DISPLAY STATS")\ #cmd("D S")], [Shows the SMF level and the
      counters (@ref-con-stats).],
    [#cmd("DISPLAY THREADS")\ #cmd("D T")], [Shows every thread of the server
      (@ref-con-threads).],
    [#cmd("DISPLAY TIME [-|+][minutes]")\ #cmd("D TI")], [Shows the time
      (@ref-con-time).],
    [#cmd("DISPLAY VERSION")\ #cmd("D V")], [Shows the version of the server
      (@ref-con-version).],
    [#cmd("SET MAXTASK n")\ #cmd("S MA n")], [Sets the largest number of
      worker threads (@ref-con-set).],
    [#cmd("SET MINTASK n")\ #cmd("S MI n")], [Sets the smallest number of
      worker threads (@ref-con-set).],
    [#cmd("SET STATS level [RESET]")\ #cmd("S S")], [Sets the SMF level, and
      resets the counters (@ref-con-setstats).],
    [#cmd("HELP")\ #cmd("?")], [Lists the commands (@ref-con-help).],
  )
]

No console command lists the routes of the server. The display module
#cmd("HTTPDSRV") shows them. @ref-dm describes it.

== DISPLAY CONFIG <ref-con-config>

#idx("DISPLAY CONFIG")
#idx("configuration", "display")
The command shows the values that the server holds now, after it has read the
Parmlib member. Use it to find out what HTTPD made of the member. The messages
come in this order.

#tab(caption: [The messages of DISPLAY CONFIG])[
  #table(columns: (0.85in, 1fr),
    [Message], [Content],
    [#cmd("HTTPD128I")], [#cmd("PORT n  MINTASK n  MAXTASK n"). The two task
      counts are the values from the Parmlib member. #cmd("SET MINTASK") and
      #cmd("SET MAXTASK") do not change them.],
    [#cmd("HTTPD129I")], [#cmd("CLIENT_TIMEOUT n  KEEPALIVE t/m"). #var("n") is
      the client timeout in seconds, #var("t") the keep-alive timeout in
      seconds and #var("m") the number of requests allowed on one connection.],
    [#cmd("HTTPD130I")], [#cmd("SESSION_TIMEOUT n MIN"). The text
      #cmd("(REAPER DISABLED)") follows when the value is 0.],
    [#cmd("HTTPD135I")], [#cmd("SESSION_MAXAGE n MIN"). The text
      #cmd("(NO MAX-AGE)") follows when the value is 0.],
    [#cmd("HTTPD131I")], [#cmd("DOCROOT path"), or #cmd("DOCROOT (NONE)") when
      no document root is set.],
    [#cmd("HTTPD134I")], [#cmd("CODEPAGE name"). The name is #cmd("CP037")
      when the Parmlib member does not set one.],
    [#cmd("HTTPD136I")], [#cmd("REALM text"), the realm for Basic
      authentication.],
    [#cmd("HTTPD132I")], [#cmd("SMF TYPE n LEVEL level"). The level is
      #cmd("NONE"), #cmd("ERROR"), #cmd("AUTH") or #cmd("ALL"). It is the level
      in effect now, so a #cmd("SET STATS") shows here.],
    [#cmd("HTTPD133I")], [#cmd("CONFIG FROM name"), the Parmlib member that
      HTTPD read, for example #cmd("SYS2.PARMLIB(HTTPPRM0)").],
    [#cmd("HTTPD137I")], [#cmd("RESIDENT YES MODULES HELD n") or
      #cmd("RESIDENT NO MODULES HELD n"). The count is the number of modules
      that the server holds in storage.],
  )
]

#screen(raw(read("../ex/ug-ops/d-config.txt")))

The command does not show the routes, #cmd("BIND_TRIES"), #cmd("BIND_SLEEP"),
#cmd("LISTEN_QUEUE") or the debug switch. @ref-prm lists every keyword.

== DISPLAY LOGIN <ref-con-login>

#idx("DISPLAY LOGIN")
#idx("logged in users")
The first message is #cmd("HTTPD121I USERS LOGGED IN: n"). One message follows
for each login that is held:

#screen(raw(read("../ex/ug-ops/d-login.txt")))

The user id is shown in up to 8 characters. The address is the address of the
client. The number in brackets is the address of the security environment
(ACEE) that belongs to the login, in hex.

== DISPLAY STATS <ref-con-stats>

#idx("DISPLAY STATS")
Two messages. #cmd("HTTPD123I SMF: level (TYPE n)") shows the SMF level and the
SMF record type. #cmd("HTTPD124I REQUESTS: n  ERRORS: n  BYTES: n  ACTIVE: n")
shows the number of requests, the number of errors, the bytes sent and the
number of connections that are open now. The counters start at zero when the
server starts, and #cmd("SET STATS ... RESET") sets the first three to zero.
#cmd("HTTPD416I") shows the same three counters when the server stops. The
levels are described in @ref-smf.

#screen(raw(read("../ex/ug-ops/d-stats.txt")))

== DISPLAY THREADS <ref-con-threads>

#idx("DISPLAY THREADS")
#idx("threads", "display")
The command writes one block for each thread. A line of dashes
(#cmd("HTTPD199I")) ends each block. The order of the blocks is: the main
thread, the socket thread, the dispatcher (the manager of the worker pool)
and then the worker threads. A block exists only when the thread exists, with
one exception: if there is no socket thread, the command still writes the
#cmd("HTTPD103I") line of the socket thread, with the address 00000000 and no
other line. Without a worker pool there are no dispatcher and no worker blocks.

#tab(caption: [The messages of DISPLAY THREADS])[
  #table(columns: (0.85in, 1fr),
    [Message], [Content],
    [#cmd("HTTPD103I")], [Starts a block: #cmd("..THREAD aaaaaaaa role"). The
      address is the control block of the thread. The role is
      #cmd("MAIN SERVER THREAD"), #cmd("SOCKET THREAD"),
      #cmd("DISPATCHER THREAD") or #cmd("WORKER THREAD").],
    [#cmd("HTTPD104I")], [#cmd("EYE"), #cmd("TCB") and #cmd("OWNTCB") of the
      thread.],
    [#cmd("HTTPD105I")], [#cmd("TERMECB"), #cmd("RC") and #cmd("STACKSZE").
      The stack size is in hex.],
    [#cmd("HTTPD106I")], [#cmd("FUNC") (the entry point of the thread),
      #cmd("ARG1") and #cmd("ARG2").],
    [#cmd("HTTPD120I")], [Main thread only: #cmd("...RNAME name"). The name is
      #cmd("HTTPD.") and the address space id as 4 hex digits.],
    [#cmd("HTTPD119I")], [Socket thread only:
      #cmd("..SOCKET n ....PORT n  (HTTP PROTOCOL)"), the socket number of the
      listener and the port.],
    [#cmd("HTTPD107I")\ to\ #cmd("HTTPD111I")], [Dispatcher only. 107: eye,
      task, wait ECB. 108: function, user data, stack size. 109: worker,
      queue and the state of the dispatcher as a number and a name
      (#cmd("INIT"), #cmd("RUNNING"), #cmd("QUIESCE"), #cmd("STOPPED") or
      #cmd("WAITING")). 110: the minimum and the maximum number of workers now
      in effect. 111: the number of workers and #cmd(".DISPCNT"), the number
      of dispatches.],
    [#cmd("HTTPD112I")\ to\ #cmd("HTTPD117I")], [One set for each worker.
      112: worker, eye, wait ECB. 113: manager, task, queue. 114: the state of
      the worker as a number and a name (#cmd("INIT"), #cmd("RUNNING"),
      #cmd("WAITING"), #cmd("DISPATCHED"), #cmd("SHUTDOWN") or
      #cmd("STOPPED")). 115, 116 and 117: #cmd("START"), #cmd("WAIT") and
      #cmd("DISP"), each a 64-bit time value in hex followed by the date and
      time as text.],
    [#cmd("HTTPD141I")], [#cmd(".DISPCNT n"), how often this worker was
      dispatched.],
    [#cmd("HTTPD118I")], [A worker in state #cmd("RUNNING") that serves a
      client: #cmd("PROTOCOL HTTP ....USER name ...GROUP name"). The user is
      #cmd("ANONYMOUS") when the client has not logged in.],
    [#cmd("HTTPD119I")], [The same worker:
      #cmd("..REMOTE CLIENT ....PORT n ......IP a.b.c.d"), the port and the
      address of the client.],
    [#cmd("HTTPD120I")], [The same worker:
      #cmd("..SOCKET n ....PORT n ......IP a.b.c.d"), the socket of the
      connection and the port and the address of the server side.],
    [#cmd("HTTPD199I")], [A line of dashes after each block.],
  )
]

The next screen shows a server with three idle workers.

#screen(raw(read("../ex/ug-ops/d-threads.txt")))

== DISPLAY MEMORY <ref-con-memory>

#idx("DISPLAY MEMORY")
#idx("storage dump")

```
/F HTTPD,D M xxxxxx[,nnn]
```

#var("xxxxxx") is the address in hex. #var("nnn") is the length in bytes. A
length with the prefix #cmd("0x") is hex, and a length with a leading 0 is
octal. Any other length is decimal. One separator character stands between the address and the length. The comma is
the one to use. The default is 256 bytes. A length that is 0 or below becomes
256. A length above 4096 becomes 4096.

#tab(caption: [The messages of DISPLAY MEMORY])[
  #table(columns: (0.85in, 1fr),
    [Message], [Content],
    [#cmd("HTTPD148I")], [#cmd("USAGE: DISPLAY MEMORY XXXXXX[,NNN] (D M XXXXXX)").
      There is no address.],
    [#cmd("HTTPD144E")], [#cmd("INVALID MEMORY ADDRESS 0x........"). The
      address is above the 16 MB line (above hex 00FFFFFF). Nothing is read.],
    [(dump)], [The storage as a dump, written with the console message
      service.],
    [#cmd("HTTPD145I")], [#cmd("END OF MEMORY DUMP FOR 0x......"). The dump is
      complete.],
    [#cmd("HTTPD146E")], [#cmd("ESTAE CREATE FAILURE, RC=0x........"). The
      command could not set up its recovery and did not read the storage.],
    [#cmd("HTTPD147E")], [#cmd("ABEND Sxxx FOR DISPLAY MEMORY 0x......") or
      #cmd("ABEND Uxxxx FOR DISPLAY MEMORY 0x......"). Reading the storage
      abended, for example because the address is not valid. The recovery
      catches the abend.],
  )
]

#screen(raw(read("../ex/ug-ops/d-memory.txt")))

== DISPLAY TIME <ref-con-time>

#idx("DISPLAY TIME")
#idx("time zone offset")

```
/F HTTPD,D TI [-|+][minutes]
```

The command writes two messages. The first is
#cmd("HTTPD142I TIME yyyy/mm/dd hh:mm:ss GMT"). The second is
#cmd("HTTPD143I TIME yyyy/mm/dd hh:mm:ss LOCAL OFFSET=+m") with the offset from
GMT in minutes. Without an argument, or with 0, the offset is the one of the
system. With another number, the command uses that number of minutes as the
offset for this display only. The number can have a sign. HTTPD keeps it within
720 minutes (12 hours) in both directions.

#screen(raw(read("../ex/ug-ops/d-time.txt")))

== DISPLAY VERSION <ref-con-version>

#idx("DISPLAY VERSION")
One message: #cmd("HTTPD140I HTTPD version (commit)"). The text is in upper
case. The version is the version of the product. The commit identifies the
source that was built.

#screen(raw(read("../ex/ug-ops/d-version.txt")))

== SET MAXTASK and SET MINTASK <ref-con-set>

#idx("SET MAXTASK")
#idx("SET MINTASK")
#idx("worker threads", "set")

```
/F HTTPD,S MA n
/F HTTPD,S MI n
```

The number is decimal and is required. The server does not check for a missing
number, so never enter the command without one. The server reads the number as
an unsigned number. A value of 0 is 1, and a value above 100 is 100. A word that is not a number is 1. A
negative number is read as a very large unsigned number, so #cmd("S MA -5")
sets 100, not 1. The command sets the new limit and
wakes the dispatcher, which creates or ends workers to fit. The answer is the
dispatcher block of #cmd("DISPLAY THREADS") (messages #cmd("HTTPD107I") to
#cmd("HTTPD111I")), followed by a line of dashes. The block shows the limits
that are in effect now.

- #cmd("SET MAXTASK") with a value below the current minimum lowers the minimum
  to that value.
- #cmd("SET MINTASK") with a value above the current maximum raises the maximum
  to that value.
- The new values hold until the server stops. The Parmlib member is not
  changed, and #cmd("DISPLAY CONFIG") still shows the values it read.
- When the server runs without a worker pool, the commands write nothing.

A value that is too large is cut to 100. The dispatcher block shows the cut
value:

#screen(raw(read("../ex/ug-ops/set-maxtask-bad.txt")))

When #cmd("SET MINTASK") raises the minimum, the dispatcher starts the missing
workers at once and writes #cmd("HTTPD061I") for each one:

#screen(raw(read("../ex/ug-ops/set-mintask.txt")))

== SET STATS <ref-con-setstats>

#idx("SET STATS")
#idx("SMF level", "set")

```
/F HTTPD,S S NONE|ERROR|AUTH|ALL [RESET]
/F HTTPD,S S RESET
```

The level is one of #cmd("NONE"), #cmd("ERROR"), #cmd("AUTH") and #cmd("ALL"),
spelled in full. @ref-smf describes the levels.

#tab(caption: [The messages of SET STATS])[
  #table(columns: (0.85in, 1fr),
    [Message], [Content],
    [#cmd("HTTPD126I")], [#cmd("SMF LEVEL SET TO level"). The level changes
      at once.],
    [#cmd("HTTPD125I")], [#cmd("STATISTICS COUNTERS RESET"). The word
      #cmd("RESET") follows the level, where the beginning of the word is enough,
      or it is the only operand, where it must be spelled in full. The three counters requests, errors and bytes are set to
      zero. The count of open connections is not changed. With only
      #cmd("RESET"), the level is not changed.],
    [#cmd("HTTPD126E")], [#cmd("INVALID SET STATS VALUE \"text\""). The first
      operand is not a level and not #cmd("RESET").],
    [#cmd("HTTPD127W")], [#cmd("MISSING STATS NONE|ERROR|AUTH|ALL [RESET]").
      There is no operand.],
  )
]

#screen(raw(read("../ex/ug-ops/set-stats-reset.txt")))

#screen(raw(read("../ex/ug-ops/set-stats-bad.txt")))

== HELP and unknown commands <ref-con-help>

#idx("HELP command")
#idx("unknown command")
#cmd("HELP") and #cmd("?") write #cmd("HTTPD150I MODIFY COMMANDS ARE:") and then
one #cmd("HTTPD151I") line for each line of the help text. The text lists each
command with a line of explanation below it, and ends with the example
#cmd("/F HTTPD,D C"). An unknown first word gets the error and then the same
text. An unknown second word gets only the error:

- *First word not known.* #cmd("HTTPD101E UNKNOWN CONSOLE COMMAND \"word\"")
  with the word as typed, followed by the help text.
- *Second word not known.* #cmd("HTTPD101E UNKNOWN CONSOLE COMMAND
  \"DISPLAY word\"") or #cmd("\"SET word\""), without the help text.

This is the help text:

#screen(raw(read("../ex/ug-ops/help.txt")))

An unknown second word gets only the error:

#screen(raw(read("../ex/ug-ops/d-unknown.txt")))

== Stopping the server <ref-con-stop>

#idx("STOP command")
#idx("shutdown")
#cmd("/P HTTPD") ends the server. HTTPD writes
#cmd("HTTPD098I HTTPD SHUTTING DOWN") and then works through these steps.

+ The server stops accepting connections.
+ HTTPD waits for the socket thread, up to 10 seconds. From the second second
  on it writes #cmd("HTTPD040I WAITING FOR SOCKET THREAD TO TERMINATE (n)").
  When the thread has not ended after 10 seconds,
  #cmd("HTTPD041I FORCE DETACHING SOCKET THREAD") follows.
+ The listener is closed, the worker pool is stopped and the modules that the
  server held are released.
+ The clients and the routes are freed.
+ #cmd("HTTPD416I STATS: n REQUESTS, n ERRORS, n BYTES") shows the counters.
+ #cmd("HTTPD099I HTTPD SHUTDOWN COMPLETE") closes the sequence. The security
  identity of the task and the authorization are given back after it.

#screen(raw(read("../ex/ug-ops/stop-normal.txt")))

The job step ends with return code 0. If the shutdown itself fails, the server
writes #cmd("HTTPD912E TERMINATE FAILED WITH RC=........"). @apx-messages lists
every message. The _HTTPD User's Guide_ shows a stop from the console in
"Operating HTTPD".

#note[The procedure that ships with HTTPD lists the line
#cmd("/F HTTPD,SHUTDOWN") in its comments. The server has no such command. A
command with this word gets #cmd("HTTPD101E") and the help text. Use
#cmd("/P HTTPD").]
