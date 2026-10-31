#import "../bookmaster/bookmaster.typ": *

// A message text is set word by word: the fixed words as literals, the parts
// in braces as variable text. The blanks between the words stay breakable.
#let msg(s) = {
  let words = s.split(" ")
  for (i, w) in words.enumerate() {
    if i > 0 { [ ] }
    let pos = 0
    for m in w.matches(regex("\{[^}]*\}")) {
      if m.start > pos { cmd(w.slice(pos, m.start)) }
      var(m.text.slice(1, -1))
      pos = m.end
    }
    if pos < w.len() { cmd(w.slice(pos)) }
  }
}

// The first cell of a row: the index entry of the ID and the text.
#let mi(s) = {
  idx(s.split(" ").first())
  msg(s)
}

// One table row: the message, the explanation, and optionally the system
// action and the response.
#let r(m, e, a: none, o: none) = (
  mi(m),
  {
    e
    if a != none {
      parbreak()
      [*System action.* ]
      a
    }
    if o != none {
      parbreak()
      [*Response.* ]
      o
    }
  },
)

// The explanation of a message that the header defines and no code writes.
#let unused = [Not issued in this release. The text is defined, and no code
  that is built writes it.]

= Messages <apx-messages>

#idx("messages", "list")
#idx("messages", "format")
This appendix lists every numbered message of HTTPD in the order of the
numbers. The header file of the product has one text for each message, and this
appendix has one entry for each of those texts: 178 texts, which carry 170
distinct combinations of number and severity letter. Where two texts share a
number, both are listed.

A message has the form #cmd("HTTPDnnnx"). The letter at the end is the
severity: #cmd("I") information, #cmd("W") warning, #cmd("E") error. There is
no other letter. The letter does not tell what the server does next. Some
messages with #cmd("E") only warn, and some with #cmd("W") end the start.
Every entry names the action, and it names the return code where the server
ends.

In the texts, the parts in _italic_ vary: a number, a name, a path. The rest is
fixed. HTTPD writes the fixed text in upper case. The values that it shows
as they are, such as paths and names that a user typed, keep their case. A run
of blanks in a text is shown as one blank. The messages have no trailing new
line.

#idx("messages", "console")
#idx("messages", "master trace table")
#idx("messages", "length")
Every numbered message is written with a single-line WTO. It goes to the
console, to the master trace table (MTT) and to the hardcopy log. A line
longer than about 70 to 90 characters is cut by the system. HTTPD writes no
numbered message to a data set. The display module for the trace table
(#cmd("HTTPDMTT"), @ref-dm) shows the table from inside the server.

#idx("messages", "unnumbered lines")
A few console lines have no number. They are listed at the end of this
appendix (@apx-messages-plain). What a client causes (a bad request, an
unknown route, a file that is not there) is answered in the HTTP response and
does not appear here.

The ranges are:

#deflist(width: 1.35in,
  [#cmd("000") to #cmd("019")], [Identity, login and the standard files
    (@apx-messages-start).],
  [#cmd("020") to #cmd("048")], [Configuration and the start of the listener
    (@apx-messages-cfg).],
  [#cmd("050") to #cmd("099")], [Listener, workers and the stop
    (@apx-messages-run).],
  [#cmd("100") to #cmd("199")], [Console commands (@apx-messages-con).],
  [#cmd("410") to #cmd("444")], [Routes, SMF and resident modules
    (@apx-messages-route).],
  [#cmd("901") to #cmd("999")], [Diagnostics (@apx-messages-diag).],
)

The numbers 020 to 048 belong to the configuration although they come from the
time before the 4xx block. To find a message, search by the number and do not
rely on the range.

== A Healthy Start and Stop <apx-messages-order>

#idx("messages", "start order")
#idx("messages", "stop order")
A server that starts without a problem writes these messages in this order.
A message with a condition appears only under that condition.

+ #cmd("HTTPD000I"), the banner with the version and the commit.
+ #cmd("HTTPD005I"), the version of the C library.
+ #cmd("HTTPD006W"), only for a build from a modified working tree.
+ #cmd("HTTPD002I"), how the server got its authorization. The message is
  written only if the authorization worked.
+ #cmd("HTTPD004I"), the identity of the started task (or #cmd("HTTPD003W")
  or #cmd("HTTPD004W") if it could not be set).
+ #cmd("HTTPD042W"), only if the security product accepts any user ID. In a
  healthy start RAKF writes #cmd("RAKF0004") for the user ID #cmd("HTTPPRB") at
  this place.
+ The messages of the configuration (@apx-messages-cfg and
  @apx-messages-route): the warnings about the parameters, and one
  #cmd("HTTPD036I") or #cmd("HTTPD417I") for each route.
+ #cmd("HTTPD054I"), the listener is bound to the port. A bind that has to
  wait for the port adds #cmd("HTTPD030E") and #cmd("HTTPD030I") first.
+ The warnings that follow the bind, such as #cmd("HTTPD430W"),
  #cmd("HTTPD425W") or #cmd("HTTPD426W"), and #cmd("HTTPD440W") to
  #cmd("HTTPD442W").
+ #cmd("HTTPD066I"), only if #cmd("TRACE=") is set: the request trace is on.
  (#cmd("HTTPD067W") instead if the storage for it was not available.)
+ #cmd("HTTPD061I") for the socket thread and for each worker thread. The
  threads write these lines themselves, so their order is not fixed.
+ #cmd("HTTPD001I"), the last message of the start. After it the server
  answers the clients and the console commands.

The stop by #cmd("P HTTPD") writes:

+ #cmd("HTTPD098I"), the stop begins.
+ #cmd("HTTPD040I"), while the socket thread still runs. The message is
  repeated up to nine times, one second apart. If the thread does not end,
  #cmd("HTTPD041I") follows.
+ #cmd("HTTPD060I") for the socket thread and for each worker thread, written
  by the threads.
+ #cmd("HTTPD416I"), the statistics of the run.
+ #cmd("HTTPD099I"), the last message of the stop. The return code is 0.

== A Start That Ends <apx-messages-fail>

#idx("messages", "start refused")
#idx("messages", "return code")
Some messages end the start. The return code of the job tells which kind.

#tab(caption: [Messages that end the start])[
  #table(columns: (2.2in, 0.8in, 1fr),
    [Message], [Return code], [What happens afterwards],
    [#cmd("HTTPD037E"), #cmd("HTTPD028E"), #cmd("HTTPD030E"),
      #cmd("HTTPD031E"), #cmd("HTTPD420E"), #cmd("HTTPD038E"),
      #cmd("HTTPD033E"), #cmd("HTTPD064E")], [8], [The server cleans up and
      stops. #cmd("HTTPD098I"), #cmd("HTTPD416I") and #cmd("HTTPD099I")
      follow. The listener number in the statistics is 0.],
    [#cmd("HTTPD090E")], [8], [The server stops at once. No other stop
      message follows.],
    [#cmd("HTTPD014E"), #cmd("HTTPD015E"), #cmd("HTTPD016E"),
      #cmd("HTTPD017W"), #cmd("HTTPD018W"), #cmd("HTTPD019W")], [12], [The
      program ends before the banner.],
    [#cmd("HTTPD012E")], [the return code of the authorization call], [The
      banner follows, without #cmd("HTTPD002I"), and the program ends.],
  )
]

#note[The messages #cmd("HTTPD048E") and #cmd("HTTPD427E") set an error flag in
the configuration. #cmd("HTTPD420E") follows at the end of the configuration and
the start ends with the return code 8.]

== Lifecycle and Identity (000 to 019) <apx-messages-start>

#tab(caption: [Messages 000 to 019])[
  #table(columns: (2.9in, 1fr),
    [Message], [Meaning and action],
    ..r("HTTPD000I HTTPD {version} ({commit}) STARTING",
      [The first message of the start. It names the version and the commit of
      the build, in upper case. HTTPD writes it also when the authorization
      failed (#cmd("HTTPD012E")).]),
    ..r("HTTPD001I HTTPD {version} READY - SERVING {docroot}",
      [The last message of a healthy start. The server accepts clients. The
      document root keeps its case.]),
    ..r("HTTPD001I HTTPD {version} READY - NO DOCUMENT ROOT",
      [The same message for a server whose document root is empty.]),
    ..r("HTTPD002I AUTHORIZED BY LIBRARY (MODULE KEY 0)",
      [The program was authorized when it started: the load library is APF
      authorized. HTTPD infers this from the state of the program before its
      own authorization call. The message is written only if the setup
      worked.]),
    ..r("HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)",
      [The program was not authorized when it started. The authorization call
      of the C runtime made it so. The message is written only if the setup
      worked.]),
    ..r("HTTPD003W RACINIT SKIPPED, CANNOT ENTER SUPERVISOR STATE",
      [The server cannot enter supervisor state, so it does not set the
      identity of the started task.],
      a: [The identity is not changed. The start goes on with the identity that
      the task inherited.]),
    ..r("HTTPD004I STC IDENTITY SET TO {user}/{group} VIA RACINIT",
      [The started task has the identity that is shown. The default is
      #cmd("HTTPD")/#cmd("USER"). The environment variables
      #cmd("HTTPD_STC_USER") and #cmd("HTTPD_STC_GROUP") change it. The
      #cmd("PARM") of the JCL changes it with #cmd("STCUSER=") and
      #cmd("STCGROUP=").]),
    ..r("HTTPD004W RACINIT ENVIR=CREATE FAILED FOR {user}/{group} RC={rc}",
      [The security product refused the identity. #var("rc") is the return
      code of the call.],
      a: [The start goes on with the inherited identity.]),
    ..r("HTTPD005I {text}",
      [The version of the C library that is linked into the server, in upper
      case.]),
    ..r("HTTPD006W BUILT FROM A MODIFIED WORKING TREE",
      [The program was built from sources with changes that are not committed.
      The commit in #cmd("HTTPD000I") does not describe the code.]),
    ..r("HTTPD007I USER {userid} IP {a.b.c.d} LOGIN SUCCESSFUL ACEE({acee})",
      [A client logged in with the login form. #var("userid") is the user (up
      to 8 characters), then the address of the client and the address of the
      security environment (6 hex digits).]),
    ..r("HTTPD008I USER {userid} IP {a.b.c.d} LOGOUT SUCCESSFUL",
      [A request to #cmd("/logout") released the credential of the client.]),
    ..r("HTTPD008W USER {userid} IP {a.b.c.d} LOGOUT FAILED",
      [The client has a credential, but the release of it failed. The client
      gets the answer "Logout failed.".]),
    ..r("HTTPD009W LOGIN SESSION NOT CREATED, RACINIT FAILED RC={rc}",
      [From HTTPD 4.5. The security product failed the logon with a return
      code that is not a plain refusal. A refusal (the return codes 4, 8, 12
      and 28) is silent. The source names three other cases as examples: the
      product is not active (32), an installation exit refuses the user (24),
      and the terminal or the time of the user is restricted (hex 30). The
      client sees "Login failed, no session available".],
      a: [The login creates no session. The server goes on.]),
    ..r("HTTPD010I {name} IS APF AUTHORIZED", unused),
    ..r("HTTPD011I {name} WAS APF AUTHORIZED VIA SVC 244", unused),
    ..r("HTTPD012E {name} UNABLE TO DYNAMICALLY OBTAIN APF AUTHORIZATION",
      [The authorization call failed. #var("name") is the name that the
      program was started with.],
      a: [The banner (#cmd("HTTPD000I")) and #cmd("HTTPD005I") follow, without
      #cmd("HTTPD002I"). The program ends with the return code of the
      call.]),
    ..r("HTTPD013I STEPLIB IS NOW APF AUTHORIZED", unused),
    ..r("HTTPD014E SYSPRINT DD NOT ALLOWED, HTTPD USES HTTPDOUT FOR STDOUT",
      [The JCL has a DD statement #cmd("SYSPRINT"). HTTPD refuses the name.],
      a: [The program ends with the return code 12.],
      o: [Remove the DD statement. HTTPD uses the private DD name
      #cmd("HTTPDOUT").]),
    ..r("HTTPD015E SYSTERM DD NOT ALLOWED, HTTPD USES HTTPDERR FOR STDERR",
      [The JCL has a DD statement #cmd("SYSTERM"). HTTPD refuses the name.],
      a: [The program ends with the return code 12.],
      o: [Remove the DD statement. HTTPD uses the private DD name
      #cmd("HTTPDERR").]),
    ..r("HTTPD016E SYSIN DD NOT ALLOWED, HTTPD USES HTTPDIN FOR STDIN",
      [The JCL has a DD statement #cmd("SYSIN"). HTTPD refuses the name.],
      a: [The program ends with the return code 12.],
      o: [Remove the DD statement. HTTPD uses the private DD name
      #cmd("HTTPDIN").]),
    ..r("HTTPD017W HTTPDOUT DD NOT DEFINED, STDOUT IS NOT AVAILABLE",
      [HTTPD could not open the DD name #cmd("HTTPDOUT") for output.],
      a: [Although the letter is W, the start ends. The program closes the
      files that it opened and ends with the return code 12.],
      o: [Add the DD statement to the JCL of the started task.]),
    ..r("HTTPD018W HTTPDERR DD NOT DEFINED, STDERR IS NOT AVAILABLE",
      [HTTPD could not open the DD name #cmd("HTTPDERR") for output.],
      a: [Although the letter is W, the start ends with the return code 12.],
      o: [Add the DD statement to the JCL of the started task.]),
    ..r("HTTPD019W HTTPDIN DD NOT DEFINED, STDIN IS NOT AVAILABLE",
      [HTTPD could not open the DD name #cmd("HTTPDIN") for input, and it could
      not open the data set #cmd("NULLFILE") as a replacement.],
      a: [Although the letter is W, the start ends with the return code 12.],
      o: [Add the DD statement to the JCL of the started task.]),
  )
]

== Configuration and Start (020 to 048) <apx-messages-cfg>

The configuration is read from the DD name #cmd("HTTPPRM") (@ref-prm). The
rest of this range reports the start of the listener.

#tab(caption: [Messages 020 to 048])[
  #table(columns: (2.9in, 1fr),
    [Message], [Meaning and action],
    ..r("HTTPD020W CANNOT OPEN DD:{ddname} -- USING DEFAULTS",
      [The DD name of the configuration (#cmd("HTTPPRM")) cannot be opened.],
      a: [The server starts with its defaults: port 8080, 3 to 9 workers, the
      document root #cmd("/www"), no routes and no SMF records (@ref-prm
      lists the defaults).],
      o: [Add the DD statement if the defaults are not what you want.]),
    ..r("HTTPD020E FOPEN FOR DD:HTTPDBG FAILED, ERRNO={errno}",
      [The keyword #cmd("DEBUG=") is greater than 0, and the DD name
      #cmd("HTTPDBG") cannot be opened. #cmd("HTTPD021I") follows.],
      a: [The server goes on. The debug output goes to the standard error
      file.]),
    ..r("HTTPD021I DEBUG/TRACE OUTPUT WILL BE TO DD:SYSTERM",
      [The debug output goes to the standard error file. The text names
      #cmd("SYSTERM"), but HTTPD allocates the standard error file as
      #cmd("HTTPDERR").]),
    ..r("HTTPD023E INVALID PORT VALUE ({port})",
      [The value of #cmd("PORT=") is not a number from 1 to 65535.],
      a: [The server keeps the earlier value or the default and goes on.]),
    ..r("HTTPD024W CONFIG={value} IS IGNORED, CONFIGURATION COMES FROM THE {ddname} DD",
      [The #cmd("PARM") of the JCL has #cmd("CONFIG="). HTTPD ignores it and
      reads the DD name. #var("value") is cut after 32 characters.],
      a: [The server goes on. #cmd("HTTPD024I") follows.]),
    ..r("HTTPD024I USE S HTTPD,M=MEMBER TO SELECT A DIFFERENT MEMBER",
      [The hint that follows #cmd("HTTPD024W").]),
    ..r("HTTPD025W TZOFFSET IS NO LONGER USED, THE SYSTEM OFFSET GMT {sign}{hh}:{mm} APPLIES",
      [The configuration has the retired keyword #cmd("TZOFFSET="). The server
      uses the time zone offset of the system.],
      a: [The server goes on. #cmd("HTTPD025I") follows.]),
    ..r("HTTPD025I SET TZ IN THE SYSENV OR ENVIRON DD TO OVERRIDE IT FOR ALL TASKS",
      [The hint that follows #cmd("HTTPD025W").]),
    ..r("HTTPD026W UNRECOGNIZED CONFIGURATION LINE: {line}",
      [A line of the configuration is not a comment and has no equal sign.
      The form is #cmd("KEYWORD=VALUE"). A comment starts with #cmd("\#") or
      #cmd("*"). The text shows the first 40 characters of the line.],
      a: [The line is skipped. The server goes on.]),
    ..r("HTTPD026E SESSION_TIMEOUT ({minutes} MIN) <= CLIENT_TIMEOUT ({seconds} SEC), RAISE IT",
      [The session timeout (in minutes) is not longer than the client timeout
      (in seconds). The message is written after the listener is bound, and
      only if the session timeout is not 0.],
      a: [Although the letter is E, this is a warning. The server goes on with
      the values as they are.],
      o: [Raise #cmd("SESSION_TIMEOUT=") in the configuration.]),
    ..r("HTTPD027I CLOSING STALE SOCKET {socket} ON PORT {port}",
      [The bind failed because the address is in use. The server closes each
      other socket of the address space that is bound to the port, then waits
      2 seconds and binds once more.]),
    ..r("HTTPD028E SOCKET() FAILED, RC={rc} ERRNO={errno}",
      [The server could not create the listener socket.],
      a: [The start ends with the return code 8.]),
    ..r("HTTPD029W UNKNOWN CONFIGURATION KEYWORD: {keyword}",
      [A line of the configuration has a keyword that HTTPD does not know. The
      keyword is shown in upper case.],
      a: [The line is skipped. The server goes on.]),
    ..r("HTTPD030E BIND() FAILED FOR HTTP PORT, RC={rc} ERRNO={errno}",
      [The bind of the listener to the port failed. If the cause is an address
      in use, the server waits and tries again (#cmd("HTTPD030I")), up to
      #cmd("BIND_TRIES=") times, #cmd("BIND_SLEEP=") seconds apart. The
      message is written for the first failure. It is written a second time if
      the last try also fails. For any other cause, it is written twice in
      a row.],
      a: [A try that works within the limit lets the start go on. Otherwise the
      start ends with the return code 8.]),
    ..r("HTTPD030I EADDRINUSE, WAITING FOR TCPIP TO RELEASE HTTP PORT {port}",
      [The port is still in use, for example after a quick restart. The server
      waits #cmd("BIND_SLEEP=") seconds (default 10) and binds again. The
      default number of tries is 10 (#cmd("BIND_TRIES=")).]),
    ..r("HTTPD031E LISTEN() FAILED, RC={rc} ERRNO={errno}",
      [The server could not put the socket into the listen state.],
      a: [The start ends with the return code 8.]),
    ..r("HTTPD032E SESSION_MAXAGE ({minutes} MIN) <= CLIENT_TIMEOUT ({seconds} SEC), RAISE IT",
      [The maximum age of a session (in minutes) is not longer than the client
      timeout (in seconds). The message is written after the listener is bound,
      and only if the maximum age is not 0.],
      a: [Although the letter is E, this is a warning. The server goes on.],
      o: [Raise #cmd("SESSION_MAXAGE=") in the configuration.]),
    ..r("HTTPD033E UNABLE TO CREATE SOCKET THREAD",
      [The task for the listener could not be created.],
      a: [The server closes the listener. The start ends with the return code
      8.]),
    ..r("HTTPD034W UNABLE TO CREATE WORKER THREADS, DYNAMIC DOCUMENTS DISABLED",
      [The pool of workers could not be created. Dynamic documents (the
      routes that run programs) are disabled.],
      a: [The server comes up anyway.]),
    ..r("HTTPD035W UNABLE TO REGISTER MODULE {module} FOR {path}",
      [The server could not store a #cmd("MOD=") route. The path is served from
      the document root, with nothing that gates it.],
      a: [If the route asked for a gate, #cmd("HTTPD419E") and #cmd("HTTPD420E")
      follow and the server does not start.]),
    ..r("HTTPD036I MODULE {module} REGISTERED FOR {path}",
      [A #cmd("MOD=") route is registered. The module name is in upper case and
      has up to 8 characters. The path is as written in the configuration. If
      the line has no path, the path is #cmd("*.") and the program name in
      lower case.]),
    ..r("HTTPD037E HTTPD IS ALREADY ACTIVE ON PORT {port}, THIS INSTANCE ENDS",
      [Another HTTPD holds the lock for the port. The lock is the name
      #cmd("HTTPD.PORT.nnnnn") (the port with 5 digits). A second server on
      another port is allowed.],
      a: [This instance ends with the return code 8.],
      o: [Stop the other server, or use another port.]),
    ..r("HTTPD038E READ ERROR ON DD:{ddname} -- CONFIGURATION INCOMPLETE",
      [The server opened the configuration and could not read it to the end.],
      a: [The start ends with the return code 8.],
      o: [Check the data set of the DD name #cmd("HTTPPRM") and start again.]),
    ..r("HTTPD039W WRITE ERROR ON DD:HTTPDBG -- DEBUG OUTPUT SWITCHED OFF",
      [A write of the trace failed, for example because the data set of the DD
      name #cmd("HTTPDBG") is full. The server writes the message once in a
      run, for the first failed write, from whichever thread meets it. The text
      names #cmd("HTTPDBG") also when the trace goes to the standard error file
      (#cmd("HTTPD021I")). The keyword #cmd("DEBUG=") must be greater than 0
      for the message to appear.],
      a: [The server switches the trace off and goes on. It writes no more
      trace lines until the next start. The trace ends at the point of the
      failure.],
      o: [Give the DD name #cmd("HTTPDBG") a data set with more room, and
      restart the server to trace again.]),
    ..r("HTTPD040I WAITING FOR SOCKET THREAD TO TERMINATE ({n})",
      [Written during the stop while the socket thread still runs. #var("n")
      counts from 1 to 9. The server waits one second between the messages.]),
    ..r("HTTPD041I FORCE DETACHING SOCKET THREAD",
      [The socket thread did not end in time. The server detaches the task.]),
    ..r("HTTPD042W SECURITY SYSTEM ACCEPTS ANY USERID, ALL LOGINS REFUSED",
      [From HTTPD 4.5.1. At the start, after the identity of the started task
      is set, the server logs on once with the user ID #cmd("HTTPPRB") and a
      password that cannot be right. The security product returned an ACEE
      for it, so it accepts any user ID. RAKF 1.2.x does this when its user
      table is missing or invalid, and the console then also shows
      #cmd("RAKF0001") and #cmd("RAKF0002"), and #cmd("RAKF0003") and
      #cmd("RAKF0009") if the profile table is missing too. A product that
      checks gives no ACEE, and RAKF writes #cmd("RAKF0004") for the user ID
      #cmd("HTTPPRB") instead.],
      a: [The start goes on. Every Basic, form and token login is refused,
      with the same answer as for a wrong password, and the server writes no
      message for it. Routes that need no login are served as before. The
      server does not test again.],
      o: [Correct the tables of the security product and restart HTTPD. See
      'A Security System That Accepts Any User ID' in the _HTTPD User's
      Guide_. The server does not run the test if #cmd("HTTPD003W") skipped the
      logon of the task.]),
    ..r("HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM",
      [The file system is enabled and the server could not create its handle to
      it. The server has no handle for UFSD.],
      a: [The server goes on. The sessions of the clients to the file system
      are created only if the handle exists.]),
    ..r("HTTPD045E UFS READ ERROR RC={rc}, RESPONSE ABORTED: {path}, {count} SUPPRESSED",
      [A read of a file failed while the server sent it. #var("path") is the
      path of the request, or the path as written in the directive if the
      error was in a file that a server-side include pulled in. It is cut after
      40 characters. #var("count") is the number of read errors that the
      server did not report since the last message of this kind, 0 if none.],
      a: [The server drops the data that is not sent and closes the connection.
      It writes this message at most once in 60 seconds. The first error after
      a quiet minute is written at once, the others in the next 60 seconds are
      only counted, and the count stays unreported until the next error. The
      server goes on.]),
    ..r("HTTPD047E UNABLE TO INITIALIZE SERVER CREDENTIAL KEY, RC={rc}",
      [The server could not initialize its credential key. Login does not
      work.],
      a: [The server comes up anyway.]),
    ..r("HTTPD048W LOGIN IS RETIRED -- USE AUTH= ON EACH MOD=/LOC= ROUTE",
      [The configuration has the keyword #cmd("LOGIN=") with an empty value or
      with only #cmd("NONE") operands. The keyword has no effect.],
      a: [The server goes on.],
      o: [Remove the keyword and write #cmd("AUTH=") on each route (@ref-route).]),
    ..r("HTTPD048E LOGIN {operand} IS RETIRED -- ROUTES WITHOUT AUTH= WOULD BECOME PUBLIC",
      [The configuration has the keyword #cmd("LOGIN=") with an operand that
      asked for a login. #var("operand") is shown as written. A route without
      #cmd("AUTH=") is public.],
      a: [The server sets an error flag. #cmd("HTTPD420E") follows and the
      start ends with the return code 8.],
      o: [Remove the keyword and write #cmd("AUTH=") on each route (@ref-route).]),
  )
]

== Listener, Workers and Stop (050 to 099) <apx-messages-run>

#tab(caption: [Messages 050 to 099])[
  #table(columns: (2.9in, 1fr),
    [Message], [Meaning and action],
    ..r("HTTPD050E MAXSOCK IS ZERO, THE LISTENER IS IN AN INVALID STATE",
      [The loop of the socket thread found no socket to wait for.],
      a: [The socket thread ends. The server does not start it again. The main
      task still answers the console commands.]),
    ..r("HTTPD051E SELECTEX() FAILED, RC={rc} ERRNO={errno}",
      [The wait for work on the sockets failed. The message is not written
      while the server stops.],
      a: [The socket thread ends. The server does not accept clients any
      more.]),
    ..r("HTTPD052E ACCEPT() FAILED, RC={rc} ERRNO={errno}",
      [The server could not accept a connection. The message is not written
      while the server stops.],
      a: [The listener goes on.]),
    ..r("HTTPD053E UNABLE TO SET NON-BLOCKING I/O FOR SOCKET {socket}",
      [The server could not change the mode of an accepted socket.],
      a: [The server writes the message and goes on with the connection.]),
    ..r("HTTPD054I LISTENING ON {address} PORT {port}",
      [The listener is bound. The address in the text is always #cmd("ANY"):
      HTTPD listens on every address of the host.]),
    ..r("HTTPD060I SHUTDOWN {thread} TCB({tcb}) TASK({task}) STACKSIZE({size})",
      [A thread ends. The thread writes the message itself. #var("thread") is
      #cmd("SOCKET THREAD") or #cmd("WORKER(xxxxxx)"), where the number is the
      address of the work block. #var("tcb") and #var("task") are 6 hex digits
      each. #var("size") is the stack size.]),
    ..r("HTTPD061I STARTING {thread} TCB({tcb}) TASK({task}) STACKSIZE({size})",
      [A thread starts. The thread writes the message itself. The layout is
      the same as in #cmd("HTTPD060I").]),
    ..r("HTTPD062E ABEND {code} IN WORKER({work}) CLIENT({client}) SOCKET({socket})",
      [A worker ended a request with an abend. #var("code") is the abend word
      (8 hex digits). The other values are the work block, the control block
      of the client and the socket.],
      a: [The server closes the client. The worker goes on with the next work
      item.]),
    ..r("HTTPD063E NO RECOVERY IN WORKER({work}), CLIENT({client}) SOCKET({socket}) DROPPED",
      [The worker could not set up its recovery routine. It neither read nor
      answered the request.],
      a: [The server closes the connection. The worker goes on.]),
    ..r("HTTPD064E SOCKET THREAD IS NOT RUNNING, START ABANDONED",
      [The socket thread is not up after its creation.],
      a: [The start ends with the return code 8, after the clean up.]),
    ..r("HTTPD065W CLIENT {a.b.c.d} ABANDONED, NO SEND PROGRESS FOR {seconds} SEC, {bytes} BYTES SENT, {n} SUPPRESSED",
      [A client did not receive any data for the whole time of the limit (10
      seconds), so the server gave up. This happens on a chunked response and on
      the body of a static file.
      #var("bytes") is the number of bytes that were sent. The server writes
      the message at most once in 60 seconds. #var("n") is the number of
      messages that it held back in the meantime. A server that stops writes
      nothing.],
      a: [The server cuts the response and closes the connection.]),
    ..r("HTTPD066I REQUEST TRACE ON, {records} RECORDS, {bytes} BYTES",
      [The Parmlib keyword #cmd("TRACE=") asked for the request trace and the
      server allocated its table. #var("records") is the number of requests the
      table holds and #var("bytes") is the storage it uses. The table is read
      with the display module HTTPDSRV, target #cmd("TRACE").]),
    ..r("HTTPD067W NO STORAGE FOR REQUEST TRACE, TRACE OFF",
      [#cmd("TRACE=") asked for the request trace, but the server could not get
      the storage for the table.],
      a: [The server runs without the trace. Use a smaller value or a larger
      region.]),
    ..r("HTTPD070E UNKNOWN CODEPAGE \"{name}\", USING CP037",
      [The value of #cmd("CODEPAGE=") is not known. The known values are
      #cmd("CP037"), #cmd("IBM1047") and #cmd("LEGACY").],
      a: [The server uses #cmd("CP037") and goes on.]),
    ..r("HTTPD090E UNABLE TO INITIALIZE CONSOLE INTERFACE",
      [The server could not set up the interface for the console commands.],
      a: [The start ends with the return code 8. No stop message follows.]),
    ..r("HTTPD098I HTTPD SHUTTING DOWN",
      [The stop begins. It is the answer to #cmd("P HTTPD") and also the first
      step after a refused start.]),
    ..r("HTTPD099I HTTPD SHUTDOWN COMPLETE",
      [The last message of the stop.]),
  )
]

== Console Commands (100 to 199) <apx-messages-con>

The messages of this range are the output of the console commands (@ref-con).
Most of them are the answer of a command and have no response.

#tab(caption: [Messages 100 to 119])[
  #table(columns: (2.9in, 1fr),
    [Message], [Meaning and action],
    ..r("HTTPD100I CONS({console}) \"{command}\"",
      [The server echoes each command that it gets with #cmd("MODIFY").
      #var("console") is the identification of the console. #var("command") is
      the text as the operator typed it.]),
    ..r("HTTPD101E UNKNOWN CONSOLE COMMAND \"{word}\"",
      [The first word of the command is not known. The help (#cmd("HTTPD150I"),
      then #cmd("HTTPD151I")) follows. A word may be shortened to any leading
      part. The first match in the list of the help wins.],
      o: [Use one of the commands in the help.]),
    ..r("HTTPD101E UNKNOWN CONSOLE COMMAND \"DISPLAY {word}\"",
      [The word after #cmd("DISPLAY") is not known. There is no help.]),
    ..r("HTTPD101E UNKNOWN CONSOLE COMMAND \"SET {word}\"",
      [The word after #cmd("SET") is not known. There is no help.]),
    ..r("HTTPD102I HTTPD IS LISTENING ON PORT {port}",
      [The answer of #cmd("DISPLAY PORTS").]),
    ..r("HTTPD103I ..THREAD {address} {role}",
      [The first line of a thread in the answer of #cmd("DISPLAY THREADS").
      #var("role") is #cmd("MAIN SERVER THREAD"), #cmd("SOCKET THREAD"),
      #cmd("DISPATCHER THREAD") or #cmd("WORKER THREAD").]),
    ..r("HTTPD104I .....EYE {eye} .....TCB {tcb} ..OWNTCB {owntcb}",
      [The task block of a thread: the eye catcher, #var("tcb") and
      #var("owntcb"), in hex.]),
    ..r("HTTPD105I .TERMECB {ecb} ......RC {rc} STACKSZE {size}",
      [The task block, second line: #var("ecb"), the return code and the stack
      size, in hex.]),
    ..r("HTTPD106I ....FUNC {func} ....ARG1 {arg1} ....ARG2 {arg2}",
      [The task block, third line: the function that the thread runs and its
      two arguments, in hex.]),
    ..r("HTTPD107I .....EYE {eye} ....TASK {task} .WAITECB {ecb}",
      [The manager of the pool of workers: eye catcher, task and wait ECB. It
      is shown if the manager exists.]),
    ..r("HTTPD108I ....FUNC {func} ...UDATA {udata} STACKSZE {size}",
      [The manager, second line: the function, the user data and the stack
      size, in hex.]),
    ..r("HTTPD109I ..WORKER {worker} ...QUEUE {queue} ...STATE {state} {name}",
      [The manager, third line: the worker array and the queue, the state in
      hex, then the name of the state: #cmd("INIT"), #cmd("RUNNING"),
      #cmd("QUIESCE"), #cmd("STOPPED"), #cmd("WAITING") or #cmd("unknown").]),
    ..r("HTTPD110I .MINTASK {mintask} .MAXTASK {maxtask}",
      [The limits of the pool of workers, in decimal. #cmd("SET MINTASK") and
      #cmd("SET MAXTASK") answer with this line.]),
    ..r("HTTPD111I .WORKERS {workers} .DISPCNT {count}",
      [The number of workers now and the number of dispatches, in decimal.]),
    ..r("HTTPD112I ..WORKER {worker} .....EYE {eye} .WAITECB {ecb}",
      [A worker: the block, the eye catcher and the wait ECB.]),
    ..r("HTTPD113I .....MGR {mgr} ....TASK {task} ...QUEUE {queue}",
      [A worker, second line: its manager, its task and its queue.]),
    ..r("HTTPD114I ...STATE {state} {name}",
      [A worker, state: in hex and by name. The names are #cmd("INIT"),
      #cmd("RUNNING"), #cmd("WAITING"), #cmd("DISPATCHED"), #cmd("SHUTDOWN"),
      #cmd("STOPPED") and #cmd("unknown").]),
    ..r("HTTPD115I ...START {time} {text}",
      [The start time of the worker: 64 bits in hex, then the date and time
      as text. The text is empty if the time is 0.]),
    ..r("HTTPD116I ....WAIT {time} {text}",
      [The time of the last wait of the worker. The layout is the same as
      in #cmd("HTTPD115I").]),
    ..r("HTTPD117I ....DISP {time} {text}",
      [The time of the last dispatch of the worker. The layout is the same as
      in #cmd("HTTPD115I").]),
    ..r("HTTPD118I PROTOCOL HTTP ....USER {user} ...GROUP {group}",
      [The client of a running worker. #var("user") is #cmd("ANONYMOUS") and the
      group is empty if the client has no credential.]),
    ..r("HTTPD119I ..REMOTE CLIENT ....PORT {port} ......IP {address}",
      [The remote side of the client of a worker: port and address of the
      client.]),
    ..r("HTTPD119I ..SOCKET {socket} ....PORT {port} (HTTP PROTOCOL)",
      [The listener in the answer of #cmd("DISPLAY THREADS"): the socket and the
      port.]),
  )
]

#tab(caption: [Messages 120 to 149])[
  #table(columns: (2.9in, 1fr),
    [Message], [Meaning and action],
    ..r("HTTPD120I ..SOCKET {socket} ....PORT {port} ......IP {address}",
      [The socket of the client of a running worker, with the port and the
      address of the peer.]),
    ..r("HTTPD120I ...RNAME {rname}",
      [The resource name of the server: #cmd("HTTPD.") and the 4 hex digits of
      the address space number. It is shown for the main thread.]),
    ..r("HTTPD121I USERS LOGGED IN: {n}",
      [The first line of the answer of #cmd("DISPLAY LOGIN"). One
      #cmd("HTTPD122I") for each credential follows.]),
    ..r("HTTPD122I USER {userid} IP {a.b.c.d} ACEE({acee})",
      [A logged in user: the user, the address of the client and the address of
      the security environment.]),
    ..r("HTTPD123I SMF: {level} (TYPE {type})",
      [The first line of the answer of #cmd("DISPLAY STATS"): the level and the
      record type of SMF (@ref-smf).]),
    ..r("HTTPD124I REQUESTS: {requests}  ERRORS: {errors}  BYTES: {bytes}  ACTIVE: {active}",
      [The counters of the server: requests, errors, bytes and active
      clients.]),
    ..r("HTTPD125I STATISTICS COUNTERS RESET",
      [The counters of requests, errors and bytes are zero. It is the answer of
      #cmd("SET STATS RESET"), and also follows a level that has the word
      #cmd("RESET") after it.]),
    ..r("HTTPD126I SMF LEVEL SET TO {level}",
      [The answer of #cmd("SET STATS") with a level.]),
    ..r("HTTPD126E INVALID SET STATS VALUE \"{operand}\"",
      [The operand of #cmd("SET STATS") is not #cmd("NONE"), #cmd("ERROR"),
      #cmd("AUTH"), #cmd("ALL") or #cmd("RESET"). The level does not change.],
      o: [Give a valid operand.]),
    ..r("HTTPD127W MISSING STATS NONE|ERROR|AUTH|ALL [RESET]",
      [#cmd("SET STATS") has no operand.],
      o: [Give a level.]),
    ..r("HTTPD128I PORT {port}  MINTASK {mintask}  MAXTASK {maxtask}",
      [The first line of the answer of #cmd("DISPLAY CONFIG"): port and limits
      of the pool of workers.]),
    ..r("HTTPD129I CLIENT_TIMEOUT {seconds}  KEEPALIVE {timeout}/{max}",
      [The client timeout in seconds, the keep-alive timeout and the maximum
      number of requests on a connection.]),
    ..r("HTTPD130I SESSION_TIMEOUT {minutes} MIN{text}",
      [The session timeout in minutes. The text #cmd(" (REAPER DISABLED)")
      follows if the value is 0.]),
    ..r("HTTPD131I DOCROOT {path}",
      [The document root, or #cmd("(NONE)").]),
    ..r("HTTPD132I SMF TYPE {type} LEVEL {level}",
      [The record type and the level of SMF (@ref-smf). The level is
      #cmd("NONE"), #cmd("ERROR"), #cmd("AUTH") or #cmd("ALL").]),
    ..r("HTTPD133I CONFIG FROM {source}",
      [Where the configuration came from: the data set name with the member, or
      #cmd("DD:HTTPPRM").]),
    ..r("HTTPD134I CODEPAGE {name}",
      [The code page in use. It is #cmd("CP037") if none is set.]),
    ..r("HTTPD135I SESSION_MAXAGE {minutes} MIN{text}",
      [The maximum age of a session in minutes. The text #cmd(" (NO MAX-AGE)")
      follows if the value is 0.]),
    ..r("HTTPD136I REALM {realm}",
      [The realm of Basic authentication.]),
    ..r("HTTPD137I RESIDENT {yesno}  MODULES HELD {n}",
      [Whether the server keeps the modules in storage (#cmd("YES") or
      #cmd("NO")) and how many it holds now.]),
    ..r("HTTPD138I RES_NOPROFILE {value}",
      [The value of the keyword #cmd("RES_NOPROFILE"): #cmd("ALLOW") or
      #cmd("DENY"). It is the last line of #cmd("DISPLAY CONFIG").]),
    ..r("HTTPD140I HTTPD {version} ({commit})",
      [The answer of #cmd("DISPLAY VERSION"), in upper case.]),
    ..r("HTTPD141I .DISPCNT {count}",
      [The number of dispatches of a worker, in the answer of
      #cmd("DISPLAY THREADS").]),
    ..r("HTTPD142I TIME {yyyy/mm/dd} {hh:mm:ss} GMT",
      [The answer of #cmd("DISPLAY TIME"), the first line: the time in GMT.]),
    ..r("HTTPD143I TIME {yyyy/mm/dd} {hh:mm:ss} LOCAL",
      [The second line: the local time. The server adds the text
      #cmd(" OFFSET=") and the sign and the minutes of the offset. An argument
      in minutes sets the offset (from -720 to +720). Without an argument, the
      offset of the system applies.]),
    ..r("HTTPD144E INVALID MEMORY ADDRESS 0x{address}",
      [The address of #cmd("DISPLAY MEMORY") is above 16 MB.],
      o: [Give an address below 16 MB.]),
    ..r("HTTPD145I END OF MEMORY DUMP FOR 0x{address}",
      [The dump of #cmd("DISPLAY MEMORY") is complete. The lines of the dump
      have no number (@apx-messages-plain).]),
    ..r("HTTPD146E ESTAE CREATE FAILURE, RC=0x{rc}",
      [The server could not set up the recovery for #cmd("DISPLAY MEMORY").
      The dump is not made.]),
    ..r("HTTPD147E ABEND S{code} FOR DISPLAY MEMORY 0x{address}",
      [The read of the storage for #cmd("DISPLAY MEMORY") ended with a system
      abend. #var("code") has 3 hex digits.]),
    ..r("HTTPD147E ABEND U{code} FOR DISPLAY MEMORY 0x{address}",
      [The read of the storage for #cmd("DISPLAY MEMORY") ended with a user
      abend. #var("code") is in decimal.]),
    ..r("HTTPD148I USAGE: DISPLAY MEMORY XXXXXX[,NNN] (D M XXXXXX)",
      [#cmd("DISPLAY MEMORY") has no address. The default length of a dump is
      256 bytes and the maximum is 4096.]),
    ..r("HTTPD149I USAGE: SET MAXTASK|MINTASK N (S MA N, S MI N), N = 1 TO 100",
      [#cmd("SET MAXTASK") or #cmd("SET MINTASK") has no number, or a negative
      number. The limits do not change, and the dispatcher is not woken.],
      o: [Give a decimal number.]),
  )
]

#tab(caption: [Messages 150 to 199])[
  #table(columns: (2.9in, 1fr),
    [Message], [Meaning and action],
    ..r("HTTPD150I MODIFY COMMANDS ARE:",
      [The header of the help. The command #cmd("?") or #cmd("HELP") and an
      unknown command show it.]),
    ..r("HTTPD151I {line}",
      [A line of the help. The help lists #cmd("DISPLAY") with the operands
      #cmd("CONFIG"), #cmd("LOGIN"), #cmd("MEMORY"), #cmd("PORTS"),
      #cmd("STATS"), #cmd("THREADS"), #cmd("TIME") and #cmd("VERSION"), and
      #cmd("SET") with #cmd("MAXTASK"), #cmd("MINTASK") and #cmd("STATS"). Each
      command has a line with the command and its short form and a line with a
      description. The last line is the example #cmd("/F HTTPD,D C").]),
    ..r("HTTPD199I -------------------------------------------------------",
      [A separator line. It closes each block of the answer of
      #cmd("DISPLAY THREADS"), and the answer of #cmd("SET MAXTASK") and
      #cmd("SET MINTASK").]),
  )
]

== Routes, SMF and Resident Modules (410 to 444) <apx-messages-route>

The routes are described in @ref-route and the keywords in @ref-prm. Several
messages of this range say that a route could not be registered. A route that
is lost does not remove its path: the server serves the path from the document
root, with nothing that gates it. Therefore a lost route that asked for a gate
(#cmd("AUTH=") other than #cmd("NONE"), or #cmd("RES=")) is an error. The
server sets an error flag, writes #cmd("HTTPD419E") and refuses to start with
#cmd("HTTPD420E").

#tab(caption: [Messages 410 to 444])[
  #table(columns: (2.9in, 1fr),
    [Message], [Meaning and action],
    ..r("HTTPD410W CGI= IS DEPRECATED, USE MOD= INSTEAD",
      [The configuration has the old keyword #cmd("CGI="). The server still
      honors it.],
      o: [Write #cmd("MOD=") instead.]),
    ..r("HTTPD411E UNKNOWN AUTH MODE '{mode}' -- USE NONE, FORM, BASIC OR TOKEN",
      [A route has an #cmd("AUTH=") value that HTTPD does not know. The value
      is cut after 24 characters.],
      a: [The server refuses the route. #cmd("HTTPD419E") and #cmd("HTTPD420E")
      follow, and the start ends with the return code 8.],
      o: [Write #cmd("NONE"), #cmd("FORM"), #cmd("BASIC") or #cmd("TOKEN").]),
    ..r("HTTPD412W IGNORING MALFORMED RES= '{text}' (NEED CLASS:RESOURCE)",
      [The value of #cmd("RES=") of a route is not in the form
      #cmd("class:resource").],
      a: [The server ignores the option and goes on.]),
    ..r("HTTPD413W IGNORING UNKNOWN ROUTE OPTION '{token}'",
      [A route has a word that is not an option. The old option
      #cmd("RECLAIM=") gets #cmd("HTTPD422W") instead.],
      a: [The server ignores the word and goes on.]),
    ..r("HTTPD414W AUTH=NONE IGNORES RES={class}:{resource} (PUBLIC ROUTE)",
      [A route has #cmd("AUTH=NONE") and also #cmd("RES="). #cmd("NONE") wins and
      the route is public. A #cmd("RES=") without #cmd("AUTH=") means
      #cmd("BASIC").],
      a: [The server goes on.]),
    ..r("HTTPD415W LOC= REQUIRES A PATH (E.G. LOC /ADMIN/* AUTH=BASIC)",
      [A #cmd("LOC=") line is empty, or it has an option where the path
      belongs.],
      a: [The server drops the line. If the line asked for a gate,
      #cmd("HTTPD419E") and #cmd("HTTPD420E") follow.]),
    ..r("HTTPD416I STATS: {requests} REQUESTS, {errors} ERRORS, {bytes} BYTES",
      [The counters of the run, written at the stop before #cmd("HTTPD099I").]),
    ..r("HTTPD417I LOCATION {path} REGISTERED",
      [A #cmd("LOC=") route is registered.]),
    ..r("HTTPD418E NO STORAGE FOR RES={class}:{resource}",
      [The server had no storage for the #cmd("RES=") option of a route. The
      class is cut after 16 characters and the resource after 40.],
      a: [The server refuses the route. #cmd("HTTPD419E") and #cmd("HTTPD420E")
      follow, and the start ends with the return code 8.]),
    ..r("HTTPD419E {kind}={path} COULD NOT BE REGISTERED -- ITS AUTH POLICY IS LOST",
      [A route that asked for a gate could not be registered. #var("kind") is
      #cmd("MOD") or #cmd("LOC"). #var("path") is the path, or the line as
      written if the line could not be split, cut after 40 characters. The
      causes are no storage for the route or for #cmd("RES="), a line that
      cannot be split, a missing path or program name, and an unknown
      #cmd("AUTH=") value.],
      a: [The server sets an error flag. #cmd("HTTPD420E") follows.],
      o: [Correct the line in the configuration.]),
    ..r("HTTPD420E ROUTE AUTHORIZATION POLICY INCOMPLETE -- HTTPD WILL NOT START",
      [At least one route lost its policy (#cmd("HTTPD419E"), or
      #cmd("HTTPD048E")), or the value of #cmd("RES_NOPROFILE") is not valid
      (#cmd("HTTPD427E")). The message is written at the end of the
      configuration, before the bind. Its text is the same for every cause. The
      cause is in the message before it.],
      a: [The start ends with the return code 8.],
      o: [Correct the lines that the other messages name.]),
    ..r("HTTPD421W MOD= REQUIRES A PROGRAM NAME (E.G. MOD=MVSMF /ZOSMF/* AUTH=BASIC)",
      [A #cmd("MOD=") line is empty, or it has an option where the program name
      belongs.],
      a: [The server drops the line. If the line asked for a gate,
      #cmd("HTTPD419E") and #cmd("HTTPD420E") follow.]),
    ..r("HTTPD422W {keyword} IS RETIRED -- CGI STORAGE RECLAIM IS ALWAYS ON",
      [A route has the option #cmd("RECLAIM="). The keyword is shown as
      written. The option has no effect.],
      a: [The server goes on.]),
    ..r("HTTPD423W UNABLE TO REGISTER LOCATION {path}",
      [The server could not store a #cmd("LOC=") route. The prefix is served
      from the document root, with nothing that gates it.],
      a: [If the route asked for a gate, #cmd("HTTPD419E") and #cmd("HTTPD420E")
      follow and the server does not start.]),
    ..r("HTTPD424W INVALID REALM VALUE: {value}",
      [The value of #cmd("REALM=") is not valid. The value is cut after 40
      characters.],
      a: [The server keeps the default realm (the SMF identification) and goes
      on.]),
    ..r("HTTPD425W NO PROFILE FOR {class}:{resource} -- {path} NOT GATED",
      [A route with #cmd("RES=") (not #cmd("AUTH=NONE")) names a resource that
      the security product does not protect. The check at the start found no
      profile. The class is cut after 8 characters, the resource after 32 and
      the path after 24.],
      a: [The route is served, without a gate. The server goes on.],
      o: [Define the profile, or correct the name in the configuration. To
      refuse the requests instead, set #cmd("RES_NOPROFILE=DENY") (then
      #cmd("HTTPD426W") replaces this message).]),
    ..r("HTTPD426W NO PROFILE FOR {class}:{resource} -- {path} REFUSES ALL REQUESTS",
      [As #cmd("HTTPD425W"), for a server that runs with
      #cmd("RES_NOPROFILE=DENY"). The check at the start found no profile for
      the resource of a route with #cmd("RES=") (not #cmd("AUTH=NONE")). The
      class is cut after 8 characters, the resource after 32 and the path after
      24.],
      a: [The server goes on. The route answers every request that reaches the
      check with status 403, until a profile exists. A profile that is defined
      and loaded later takes effect without a restart, but this message is
      written at the start only.],
      o: [Define the profile, or correct the name in the configuration.]),
    ..r("HTTPD427E INVALID RES_NOPROFILE VALUE \"{value}\" (ALLOW|DENY)",
      [The value of #cmd("RES_NOPROFILE") is neither #cmd("ALLOW") nor
      #cmd("DENY"). The value is cut after 16 characters.],
      a: [The server sets an error flag. It does not fall back to
      #cmd("ALLOW"). #cmd("HTTPD420E") follows and the start ends with the
      return code 8.],
      o: [Write #cmd("ALLOW") or #cmd("DENY").]),
    ..r("HTTPD430I SMF TYPE {type} LEVEL {level}",
      [#unused #cmd("DISPLAY CONFIG") shows the same values in
      #cmd("HTTPD132I").]),
    ..r("HTTPD430W SMF TYPE {type} CONFIGURED BUT SMF INACTIVE",
      [The level of SMF is above #cmd("NONE"), but SMF is not active on the
      system.],
      a: [The server goes on.],
      o: [Start SMF, or set the level to #cmd("NONE").]),
    ..r("HTTPD431W INVALID SMF LEVEL \"{value}\"",
      [The value of #cmd("SMF=") is not a level.],
      a: [The server keeps the earlier level and goes on.]),
    ..r("HTTPD432W INVALID SMF TYPE={type} (128-255)",
      [The record type is not in the range from 128 to 255.],
      a: [The server keeps the earlier type and goes on.]),
    ..r("HTTPD440W MODULE {module} NOT FOUND AT STARTUP, NOT HELD",
      [With #cmd("RESIDENT=YES"), the server tried to load a #cmd("MOD=") module
      at the start and did not find it. The name is in upper case, up to 8
      characters.],
      a: [The server does not hold the module. It fetches it for each
      request.]),
    ..r("HTTPD441W MODULE {module} NOT REUSABLE, FETCHED PER REQUEST",
      [The module is not marked reusable, so the server cannot keep it.],
      a: [The server fetches it for each request. The route works.]),
    ..r("HTTPD442W MODULE {module} LOAD FAILED, ABEND CODE S{code} REASON {reason}, FETCHED PER REQUEST",
      [The load of the module at the start failed with a system abend. The
      codes in the source comment are 806 (not found), 106 (fetch error) and
      80A (no storage).],
      a: [The server fetches the module for each request. The route works.]),
    ..r("HTTPD443W INVALID RESIDENT VALUE \"{value}\" (YES|NO)",
      [The value of #cmd("RESIDENT=") is not #cmd("YES") or #cmd("NO").],
      a: [The default #cmd("YES") stays.]),
    ..r("HTTPD444W INVALID TRACE VALUE \"{value}\" (0 OR 16-1024), TRACE OFF",
      [The value of #cmd("TRACE=") is not a number, or it is not 0 and not in
      the range from 16 to 1024.],
      a: [The request trace stays off.]),
  )
]

== Diagnostics (901 to 999) <apx-messages-diag>

The header says that a message of this range means the server met a state that
its code did not expect, and that each one is worth a bug report.

#tab(caption: [Messages 901 to 999])[
  #table(columns: (2.9in, 1fr),
    [Message], [Meaning and action],
    ..r("HTTPD901E SPIN IN SERVE_CLIENT: CLIENT({client}) STATE({state}) SOCKET({socket}) REQ({count}) WORKER({work}) -- FORCING CLOSE",
      [The processing of a client went around 1000 times without a change of
      state.],
      a: [The server clears the busy entry and closes the client.]),
    ..r("HTTPD904E NO STORAGE FOR ENVIRONMENT VARIABLE {name} CLIENT({client})",
      [The server could not store an environment variable for a request. The
      name is cut after 32 characters.],
      a: [The set of the variable fails.]),
    ..r("HTTPD905E ENVIRONMENT VARIABLE {name} TOO LARGE CLIENT({client})",
      [The value of an environment variable does not fit.],
      a: [The set of the variable fails.]),
    ..r("HTTPD906W STORAGE RECLAIM FOR {module} FAILED, RC={rc}",
      [After a module ran, the server could not free the storage of the module.
      The storage stays allocated until the server restarts.],
      a: [The server goes on.]),
    ..r("HTTPD907W STALE BUSY ENTRY FOR CLIENT({client}) STATE({state}) SOCKET({socket}) REQ({count}) -- CLEARED",
      [A client was in the list of busy clients when the processing found no
      progress.],
      a: [The server clears the entry and goes on.]),
    ..r("HTTPD908E EXTERNAL PROGRAM {module} {reason}",
      [A program that a route runs failed. #var("reason") is one of: "was not
      found", "could not be loaded", "was dispatched without a program name",
      "could not be started, ESTAE create failed", "returned rc n", "failed
      with Sxxx ABEND" or "failed with Unnnn ABEND". The name is "(none)" if
      there is none. "failed with U0801 ABEND", with
      #cmd("@@CRT0 - No storage for C stack") before it on the console, means
      that the region had no free block for the stack of the module (see
      @ref-stc-region).],
      a: [The server frees the storage of the program (#cmd("HTTPD906W") if
      that fails). If no response header went out, the client gets the status
      503 with the same text. The server goes on.]),
    ..r("HTTPD909E UNSUPPORTED HTTP STATUS {code} REQUESTED, SENT 500",
      [A module asked for a status that HTTPD does not know.],
      a: [The server sends the status 500.]),
    ..r("HTTPD910E ABEND S{code} DETECTED", unused),
    ..r("HTTPD910E ABEND U{code} DETECTED", unused),
    ..r("HTTPD911E {function} LOCK FAILURE",
      [The lock of the server block failed. #var("function") is the name of the
      function, in lower case.],
      a: [The function skips its work. The socket thread goes on.]),
    ..r("HTTPD912E TERMINATE FAILED WITH RC={rc}",
      [The stop routine ended with an abend or an error. #var("rc") is the
      value that it returned, in 8 hex digits.],
      a: [The server goes on to end. #cmd("HTTPD416I") and #cmd("HTTPD099I") may
      be missing.]),
    ..r("HTTPD913E UNEXPECTED SUBTYPE {n} IN HTTP_GET()",
      [The processing of a GET found a type of request that it does not
      handle.],
      a: [The client gets the status 500.]),
    ..r("HTTPD914E HTTP_SEND() POSITION UNDERFLOW {pos}",
      [The send loop found a position before the start of the buffer.],
      a: [The loop stops.]),
    ..r("HTTPD915E {function} LENGTH INCONSISTENCY, LEN={len} BUFLEN={buflen}",
      [The formatting of text for the response failed. #var("function") is
      #cmd("ssi_printv").],
      a: [The function fails.]),
    ..r("HTTPD916E HTTP_SET_BUSY() FAILED",
      [The server could not mark a client as busy.],
      a: [The server does not serve the client in this pass.]),
    ..r("HTTPD920E {name} MUST BE CALLED BY THE HTTPD SERVER",
      [A display module (#cmd("HTTPDM"), #cmd("HTTPDMTT") or #cmd("HTTPDSRV"))
      was run from TSO or from a batch job (@ref-dm). The same sentence goes to
      the standard output.],
      a: [The module ends with the return code 12.],
      o: [Call the module through its route of the server.]),
    ..r("HTTPD921W ABEND0C1 ALLOCATED {kb} KB IN {blocks} BLOCKS, ABENDING WITHOUT FREEING",
      [The test module #cmd("ABEND0C1") allocated storage and now ends with an
      S0C1 abend on purpose. #cmd("HTTPD906W") and #cmd("HTTPD908E") follow.]),
    ..r("HTTPD922E HTTPD CONTROL BLOCK NOT FOUND BY {name}",
      [A module could not find the control block of the server. A trace back
      follows.]),
    ..r("HTTPD923E HTTPC CONTROL BLOCK NOT FOUND BY {name}",
      [#unused Its only call is compiled out.]),
    ..r("HTTPD999E OUT OF MEMORY",
      [The server had no storage for the control block of a new client.],
      a: [The server closes the accepted socket. The listener goes on.]),
  )
]

== Lines Without a Number <apx-messages-plain>

#idx("messages", "unnumbered")
A few console lines have no message number.

#deflist(width: 1.35in,
  [Client timeout], [A client sent part of a request and then went silent.
    The line is #cmd("client xxxxxxxx on socket n timed out after n seconds"),
    where the first value is the address of the control block of the client
    and the last is the keep-alive timeout if the connection already served a
    request, or else the client timeout. A second output is a dump of the
    buffer with the title #cmd("Receive Buffer"). Both are on by default. The
    keywords #cmd("CLIENT_TIMEOUT_MSG=0") and #cmd("CLIENT_TIMEOUT_DUMP=0")
    switch them off (@ref-prm). The connection is closed.],
  [Memory dump], [The lines of #cmd("DISPLAY MEMORY") between the command and
    #cmd("HTTPD145I"). The title is #cmd("DISPLAY MEMORY").],
  [Offset], [The text #cmd(" OFFSET=") at the end of #cmd("HTTPD143I").],
)

== Messages That Are Gone <apx-messages-gone>

#idx("messages", "retired")
The numbers #cmd("HTTPD900D"), #cmd("HTTPD902D") and #cmd("HTTPD903D") were
debug counters and are no longer defined. HTTPD uses no message letter other
than I, W and E.

The texts of #cmd("HTTPD010I"), #cmd("HTTPD011I"), #cmd("HTTPD013I"),
#cmd("HTTPD430I"), both texts of #cmd("HTTPD910E") and #cmd("HTTPD923E") are
defined in the header and no code that is built writes them. The function that
would write the first three is never called. The text of #cmd("HTTPD012E") is
written, but from a different place.
