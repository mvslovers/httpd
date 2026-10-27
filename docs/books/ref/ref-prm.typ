#import "../bookmaster/bookmaster.typ": *

= The HTTPPRM Keywords <ref-prm>

#idx("HTTPPRM", "keywords")
#idx("Parmlib", "configuration member")
HTTPD reads its configuration once, at start, from the DD name
#cmd("HTTPPRM"). This chapter lists every keyword that HTTPD accepts, with its
default, its range and what HTTPD does with a value that is wrong. The route
keywords #cmd("MOD") and #cmd("LOC") have a chapter of their own
(@ref-route). The _HTTPD User's Guide_ explains how to write the member, in
"Configuring HTTPD".

HTTPD reads the file only at start. It has no command to read it again, so a
change takes effect at the next start. The command #cmd("F HTTPD,D CONFIG")
shows what HTTPD made of the file (@ref-con).

== The Rules of the File <ref-prm-rules>

#idx("HTTPPRM", "line format")
#idx("comments", "HTTPPRM")
#idx("duplicate keys")
#idx("HTTPD026W")
#idx("HTTPD029W")
#idx("HTTPD020W")
- The DD name is #cmd("HTTPPRM"). In the sample procedure it names one member
  of a PDS, and the procedure symbol #cmd("M") chooses the member
  (@ref-stc).
- A line has the form #cmd("KEY=value"). HTTPD splits the line at the first
  equal sign. Everything after it, including further equal signs, is the
  value.
- Blanks and tabs at both ends of the key and of the value are removed, so
  #cmd("PORT = 8080") is valid. A blank inside a value stays.
- The key may be in any case. HTTPD changes it to upper case. HTTPD does not
  change the case of a value. The tables below say where HTTPD compares a value
  without regard to case.
- An empty line is ignored. A line is a comment if its first character after
  the leading blanks is #cmd("\#") or #cmd("*"). A comment takes the whole
  line. HTTPD knows no comment at the end of a line: the text after a value
  belongs to the value.
- HTTPD reads at most 255 characters of a line at a time. It takes the rest of
  a longer line as the next line. There is no continuation line.
- A line without an equal sign gives the message #cmd("HTTPD026W") with the
  first 40 characters of the line. HTTPD goes on.
- A key that HTTPD does not know gives #cmd("HTTPD029W") with the key in upper
  case. HTTPD goes on.
- HTTPD writes these messages to the console.
- If a key is given twice, the last line counts. There are two exceptions.
  Each #cmd("MOD") line and each #cmd("LOC") line adds a route, and HTTPD does
  not look for a duplicate (@ref-route). A second #cmd("SMF") line without
  #cmd("TYPE=") keeps the type of the first.
- If the DD name is missing, HTTPD writes #cmd("HTTPD020W") and starts with the
  defaults. The defaults contain no route.
- If a read error ends the read before the end of the member, HTTPD writes
  #cmd("HTTPD038E") and stops with CC 0008. HTTPD does not start on half a
  member.

A value that is not right gets one of four treatments, and the tables below
say which one applies. HTTPD _keeps_ the value that was valid before, either
the default or an earlier line. HTTPD _limits_ the value to the nearest value
that is allowed. HTTPD _reads_ a text that is not a number as 0, because it
converts a number with the C function #cmd("atoi"): it takes the digits at the
start of the value and ignores the rest. HTTPD _refuses_ the value and the
start. Only some of these cases write a message.

== Listener and Connections <ref-prm-net>

#idx("PORT")
#idx("BIND_TRIES")
#idx("BIND_SLEEP")
#idx("LISTEN_QUEUE")
#idx("HTTPD023E")
#tab(caption: [Keywords of the listener])[
  #table(columns: (1.55in, 0.8in, 1fr),
    [Keyword], [Default], [Rules],
    [#cmd("PORT")], [8080], [The TCP port on which HTTPD listens. HTTPD has no
      keyword for an address: it listens on every address of the host. A number
      below 1 or above 65535, and a text that is not a number, give
      #cmd("HTTPD023E") with the number that HTTPD read (0 for a text). The
      earlier value stays.],
    [#cmd("BIND_TRIES")], [10], [How many times HTTPD tries again to bind the
      port when the bind fails because the port is in use. HTTPD limits the
      value to 1 to 100.],
    [#cmd("BIND_SLEEP")], [10], [The seconds that HTTPD waits before each of
      these tries. HTTPD limits the value to 1 to 100.],
    [#cmd("LISTEN_QUEUE")], [5], [The length of the queue of connections that
      wait to be accepted. HTTPD limits the value to 1 to 100.],
  )
]

Before HTTPD binds the port, it takes an ENQ with the major name #cmd("HTTPD")
and the minor name #cmd("HTTPD.PORT.nnnnn"), where #cmd("nnnnn") is the port
with five digits. If another address space holds the ENQ, or the ENQ fails,
HTTPD writes #cmd("HTTPD037E") and ends with CC 0008. A second HTTPD on another
port starts.

If the bind fails with "address in use", HTTPD closes every socket of the
stack that has this port as its local port (#cmd("HTTPD027I") for each one). If
it closed at least one, it waits 2 seconds and binds again. If the bind still
fails, HTTPD writes #cmd("HTTPD030E"). Then it writes #cmd("HTTPD030I") and
waits #cmd("BIND_SLEEP") seconds before each of at most #cmd("BIND_TRIES")
further tries. If all tries fail, HTTPD writes #cmd("HTTPD030E") again and ends
with CC 0008. Any other bind error ends the start at once. The same holds for
an error of #cmd("socket") (#cmd("HTTPD028E")) and of #cmd("listen")
(#cmd("HTTPD031E")). When the listener is ready, HTTPD writes #cmd("HTTPD054I").

== Worker Tasks and Time Limits <ref-prm-wrk>

#idx("MINTASK")
#idx("MAXTASK")
#idx("CLIENT_TIMEOUT")
#idx("KEEPALIVE_TIMEOUT")
#idx("KEEPALIVE_MAX")
#idx("CLIENT_TIMEOUT_MSG")
#idx("CLIENT_TIMEOUT_DUMP")
#idx("CLIENT_STATS")
#tab(caption: [Keywords of the worker tasks and of the time limits])[
  #table(columns: (1.55in, 0.8in, 1fr),
    [Keyword], [Default], [Rules],
    [#cmd("MINTASK")], [3], [The least number of worker tasks. A value of 0 or
      less, and a text that is not a number, are ignored without a message: the
      earlier value stays. HTTPD limits a larger value to 255. If the value is
      above #cmd("MAXTASK"), HTTPD sets #cmd("MAXTASK") to the same value.],
    [#cmd("MAXTASK")], [9], [The most worker tasks. The rules for a wrong value
      are those of #cmd("MINTASK"). At the end of the start HTTPD raises
      #cmd("MAXTASK") to the value of #cmd("MINTASK") if it is lower, whatever
      the order of the two lines.],
    [#cmd("CLIENT_TIMEOUT")], [10], [The seconds that HTTPD waits for the
      client while it reads one line of a request: the request line or a header
      line. The time starts again for each line. A line of a request that comes
      on a connection that has served a request before uses
      #cmd("KEEPALIVE_TIMEOUT") instead. A value of 0 or less, and a text that
      is not a number, are ignored without a message. HTTPD limits a larger
      value to 255. When the time is up, HTTPD closes the connection.],
    [#cmd("KEEPALIVE_TIMEOUT")], [5], [The seconds that HTTPD waits for the
      lines of the next request on a connection that stays open. HTTPD limits
      the value to 1 to 255.],
    [#cmd("KEEPALIVE_MAX")], [100], [The number of requests a connection
      carries before HTTPD closes it. HTTPD answers the request after that
      number with #cmd("Connection: close"), so a connection carries the value
      plus one request. HTTPD limits the value to 1 to 255.],
    [#cmd("CLIENT_TIMEOUT_MSG")], [1], [A number. A value above 0 means yes,
      any other value and a text that is not a number mean no. If yes, HTTPD
      writes one line to the console when a time limit ends while the client has
      sent at least one character of the line. The line has no message ID. It
      reads #cmd("client xxxxxxxx on socket n timed out after s seconds") and
      gives the address of the client block in hex, the socket and the seconds.],
    [#cmd("CLIENT_TIMEOUT_DUMP")], [1], [The same rules. If yes, HTTPD also
      writes a dump of the characters received so far, with the title
      #cmd("Receive Buffer"), at such a time-out.],
    [#cmd("CLIENT_STATS")], [1], [The same rules. HTTPD stores the flag and the
      display module #cmd("HTTPDSRV") shows it as #cmd("STATS"). No other code
      of HTTPD reads it, so it changes nothing that HTTPD does or writes.],
  )
]

The time limits count in whole seconds. #cmd("D CONFIG") shows the values of
#cmd("MINTASK"), #cmd("MAXTASK"), #cmd("CLIENT_TIMEOUT"),
#cmd("KEEPALIVE_TIMEOUT") and #cmd("KEEPALIVE_MAX") (@ref-con).

== Files and Character Set <ref-prm-file>

#idx("DOCROOT")
#idx("UFS")
#idx("CODEPAGE")
#idx("HTTPD044W")
#idx("HTTPD070E")
#tab(caption: [Keywords of the files and of the character set])[
  #table(columns: (1.55in, 0.8in, 1fr),
    [Keyword], [Default], [Rules],
    [#cmd("DOCROOT")], [/www], [The UFS directory that HTTPD puts in front of the
      path of a request when it serves a static file. The value must begin
      with a slash. Another value is ignored without a message, and the
      earlier value stays. HTTPD removes one slash at the end of a value that
      is longer than one character. HTTPD keeps at most 127 characters.],
    [#cmd("UFS")], [1], [A number. A value above 0 means yes, any other value
      and a text that is not a number mean no. If yes, HTTPD creates its UFS
      system at start. If that fails, HTTPD writes #cmd("HTTPD044W") and goes
      on without it. If no, HTTPD does not create the UFS system and serves no
      file from it. Leave it at the default.],
    [#cmd("CODEPAGE")], [CP037], [The table that translates between the ASCII on
      the wire and the EBCDIC in HTTPD. The names are #cmd("CP037"),
      #cmd("IBM1047") and #cmd("LEGACY"), in any case. #cmd("LEGACY") is the
      table of HTTPD 3.3.x. An empty value means #cmd("CP037"). Another name
      gives #cmd("HTTPD070E"), and HTTPD uses #cmd("CP037"). HTTPD keeps at most
      15 characters of the value as written; #cmd("D CONFIG") shows them.],
  )
]

== Authentication Values <ref-prm-auth>

#idx("REALM")
#idx("SESSION_TIMEOUT")
#idx("SESSION_MAXAGE")
#idx("HTTPD424W")
#idx("HTTPD026E")
#idx("HTTPD032E")
#idx("RES_NOPROFILE")
#idx("HTTPD426W")
#idx("HTTPD427E")
Which route needs a login is a property of the route, set with #cmd("AUTH=")
(@ref-route). There is no keyword that sets a login policy for all routes.
Three of the four keywords here set the name of the server and the life of a
login. The fourth, #cmd("RES_NOPROFILE"), sets what a route with #cmd("RES=")
does when the security product has no profile for its resource.

#tab(caption: [Keywords of the authentication values])[
  #table(columns: (1.55in, 0.8in, 1fr),
    [Keyword], [Default], [Rules],
    [#cmd("REALM")], [SMF ID], [The name of the server. It is the realm in the
      #cmd("WWW-Authenticate: Basic") header and part of the login form. The
      default is the SMF ID of the system: up to four characters, ending at the
      first blank. If the system has none, the default is #cmd("MVS"). A value
      has 1 to 64 characters. It must not contain a control character or one of
      the characters double quote, backslash, #cmd("<"), #cmd(">") and
      #cmd("&"). Another value gives #cmd("HTTPD424W") with the first 40
      characters, and the default stays (or the value of an earlier line).],
    [#cmd("SESSION_TIMEOUT")], [30], [The minutes after which HTTPD drops the
      login of a client that is idle. HTTPD reads the value as a number. A
      negative number and a text that is not a number give 0, a number above
      65535 gives 65535. A value of 0 switches this limit off.],
    [#cmd("SESSION_MAXAGE")], [480], [The minutes after which a login ends
      whatever the client does, counted from the login. Activity does not renew
      it. The rules for a wrong value are those of #cmd("SESSION_TIMEOUT"). A
      value of 0 switches this limit off.],
    [#cmd("RES_NOPROFILE")], [ALLOW], [#cmd("ALLOW") or #cmd("DENY"), in any
      case. The value ends at the first blank. It decides what a route with
      #cmd("RES=") does when no profile covers its resource. With #cmd("ALLOW")
      the request passes, and the start writes #cmd("HTTPD425W"). With
      #cmd("DENY") the request gets status 403, and the start writes
      #cmd("HTTPD426W"). Any other value is fatal: HTTPD writes
      #cmd("HTTPD427E") with the first 16 characters of the value and then
      #cmd("HTTPD420E"), and the start ends with CC 0008.],
  )
]

A login that a request is using must not be dropped. HTTPD therefore compares
the two limits with #cmd("CLIENT_TIMEOUT") at the end of the start. If
#cmd("SESSION_TIMEOUT") in seconds is not larger than #cmd("CLIENT_TIMEOUT"),
HTTPD writes #cmd("HTTPD026E"). If #cmd("SESSION_MAXAGE") in seconds is not
larger, HTTPD writes #cmd("HTTPD032E"). Both messages are warnings: HTTPD
starts. A limit of 0 is not compared. The server drops logins about once a
minute, if at least one of the two limits is above 0.

#note[The messages #cmd("HTTPD026W") and #cmd("HTTPD026E") have the same number
and different meanings. The first is a line that is not a keyword line. The
second is the check of #cmd("SESSION_TIMEOUT").]

#idx("RES_NOPROFILE", "rules")
#idx("RES=", "RES_NOPROFILE")
The rules of #cmd("RES_NOPROFILE"):

- *One value for the whole server.* The keyword is not an option of a route. It
  applies to every route with #cmd("RES="), except the routes with
  #cmd("AUTH=NONE"), which HTTPD does not check (@ref-route-auth).
- *The check is made for each request.* With #cmd("DENY") a request to a route
  whose resource has no profile gets status 403 and the body
  #cmd("403 Forbidden: no RACF profile protects this resource"). The class and
  the resource name are not sent. A request that a profile refuses gets the
  plain #cmd("403 Forbidden"). A request without a login gets the 401 of the
  first stage, as before.
- *A profile that is defined later needs no restart.* The check looks at the
  profile as the security product holds it at the time of the request. Under
  RAKF the product holds a new profile after the started task #cmd("RAKFPROF")
  has run ("RES=: Checking a Profile" in the _HTTPD User's Guide_). Until then
  the route answers 403 with the body above. #cmd("HTTPD426W") is written at
  the start only, so it stays in the log.
- *HTTPD sees only the return code 4.* The security product answers 4 when no
  profile covers the resource. HTTPD cannot tell why the product had none to
  apply, and with #cmd("DENY") every return code 4 refuses. This book does not
  state in which conditions, other than a missing profile, a product answers 4.
- *The default does not change anything.* Without the keyword, or with
  #cmd("ALLOW"), the check passes the return codes 0 and 4 and refuses every
  other one, as in the releases before 4.5.0.
- *A misspelled value is fatal on purpose.* HTTPD does not fall back to
  #cmd("ALLOW"). A keyword that exists to close a gate must not leave the gate
  open when it is misspelled.

== Routes and Modules <ref-prm-route>

#idx("MOD")
#idx("LOC")
#idx("RESIDENT")
#idx("CGI_CONTEXT_POINTERS")
#idx("HTTPD443W")
#tab(caption: [Keywords of the routes and of the modules])[
  #table(columns: (1.55in, 0.8in, 1fr),
    [Keyword], [Default], [Rules],
    [#cmd("MOD")], [none], [A route to a program: #cmd("MOD=program pattern
      AUTH=mode RES=class:resource"). Each line adds one route. The whole syntax
      is in @ref-route.],
    [#cmd("LOC")], [none], [A route without a program, for a path that HTTPD
      serves as a static file but with a login: #cmd("LOC=pattern AUTH=mode
      RES=class:resource"). See @ref-route.],
    [#cmd("RESIDENT")], [YES], [#cmd("YES") or #cmd("NO"), in any case. Text
      after the first blank is ignored. With #cmd("YES") HTTPD holds each module
      of a #cmd("MOD") route from the start to the end, so a request does not
      fetch it from the library. With #cmd("NO") each request fetches the
      module, so a new level of a module is used at once. Another value gives
      #cmd("HTTPD443W") and the earlier value stays. @ref-route describes what
      HTTPD does at start.],
    [#cmd("CGI_CONTEXT_POINTERS")], [255], [The size of the table in which modules
      register their context blocks: the table has this value plus one
      entries. HTTPD reads the value as a number. A negative number is ignored
      without a message. A number above 255 gives 255. A text that is not a
      number gives 0. A module gets a block through the function
      #cmd("http_cgictx_get") (@ref-api).],
    [#cmd("CGI")], [none], [The old name of #cmd("MOD"). HTTPD writes
      #cmd("HTTPD410W") for each line and then reads the line as a #cmd("MOD")
      line. Use #cmd("MOD").],
  )
]

== Diagnostics and Recording <ref-prm-diag>

#idx("DEBUG")
#idx("SMF", "keyword")
#idx("HTTPD431W")
#idx("HTTPD432W")
#tab(caption: [Keywords for diagnostics and for recording])[
  #table(columns: (1.55in, 0.8in, 1fr),
    [Keyword], [Default], [Rules],
    [#cmd("DEBUG")], [0], [A number. A value above 0 switches the trace on, any
      other value and a text that is not a number switch it off. There is only
      one level: 5 means the same as 1. With the trace on, HTTPD opens the DD
      name #cmd("HTTPDBG") for output and writes its trace lines to it. If the
      DD name cannot be opened, HTTPD writes #cmd("HTTPD020E") with the
      #cmd("errno") and #cmd("HTTPD021I"), and the trace goes to the DD name
      #cmd("SYSTERM"). If a write to the trace fails, HTTPD writes
      #cmd("HTTPD039W") once and switches the trace off until the next start.
      The server goes on.],
    [#cmd("SMF")], [NONE], [#cmd("SMF=level") or #cmd("SMF=level TYPE=nnn"). The
      level is #cmd("NONE"), #cmd("ERROR"), #cmd("AUTH") or #cmd("ALL"), in any
      case. HTTPD compares only as many characters as the word has, so
      #cmd("ERR") is read as #cmd("ERROR"). Another word, and an empty value,
      give #cmd("HTTPD431W") and the earlier level stays. #cmd("TYPE=") is the
      SMF record type, 128 to 255, default 243. HTTPD looks for it anywhere in
      the value, in upper case only. Another number gives #cmd("HTTPD432W") and
      the earlier type stays. @ref-smf describes the records.],
  )
]

== Keywords That Are No Longer Valid <ref-prm-retired>

#idx("retired keywords")
#idx("LOGIN", "retired")
#idx("TZOFFSET", "retired")
#idx("RECLAIM", "retired")
#idx("HTTPD048W")
#idx("HTTPD048E")
#idx("HTTPD025W")
#idx("HTTPD422W")
These keywords are known to HTTPD, and each gives a message. A member from an
earlier release starts, except in one case.

#tab(caption: [Retired keywords])[
  #table(columns: (1.3in, 1fr),
    [Keyword], [What HTTPD does],
    [#cmd("LOGIN")], [A global rule for a login has no place any more: the
      login is set per route with #cmd("AUTH="). An empty value, and a value
      that consists only of #cmd("NONE") (several words are separated by
      commas), give #cmd("HTTPD048W"). HTTPD ignores the line and the start
      goes on. Every other value (#cmd("ALL"), #cmd("CGI"), #cmd("GET"),
      #cmd("HEAD"), #cmd("POST"), and a word that HTTPD does not know) gives
      #cmd("HTTPD048E") with the value, and the start ends with
      #cmd("HTTPD420E") and CC 0008. Such a value required a login for every
      route without a policy of its own, and these routes would be open
      without the refusal. HTTPD does not turn the value into route policies:
      change the routes.],
    [#cmd("TZOFFSET")], [HTTPD ignores the value and writes #cmd("HTTPD025W")
      with the offset that applies, and #cmd("HTTPD025I"). There is no keyword
      for the time zone. The offset comes from the variable #cmd("TZ"), or from
      the time zone of the system if there is none. To choose another offset,
      set #cmd("TZ") in the DD name #cmd("SYSENV") or #cmd("ENVIRON") of the
      procedure.],
    [#cmd("RECLAIM=")], [This is an option of a #cmd("MOD") route, not a keyword.
      HTTPD ignores the value and writes #cmd("HTTPD422W") for each use. The
      storage of a module is always released after the request.],
  )
]

#note[A line #cmd("LOGIN ALL") without an equal sign is not a #cmd("LOGIN") line
for HTTPD. It is a line without an equal sign, and it gives #cmd("HTTPD026W")
only. The start goes on, and the routes keep the policy that they have.]

== Parameters That Are Not in the Member <ref-prm-parm>

#idx("STCUSER")
#idx("STCGROUP")
#idx("CONFIG", "parameter")
#idx("HTTPD024W")
Three names look like keywords but are not. HTTPD reads #cmd("STCUSER=") and
#cmd("STCGROUP=") from the #cmd("PARM") of the procedure, and not from the
member. @ref-stc describes them. A line #cmd("STCUSER=HTTPD") in the member
gives #cmd("HTTPD029W"), as for any unknown key.

The third name is #cmd("CONFIG="). It has no effect. HTTPD writes
#cmd("HTTPD024W") and #cmd("HTTPD024I"), which says to choose the member with
#cmd("S HTTPD,M=member").

== What Stops the Start <ref-prm-stop>

#idx("start refused")
#idx("CC 0008")
HTTPD ends with CC 0008 and without a listener in these cases. Every other
problem in the member gives a message, and the start goes on.

#tab(caption: [Causes of a refused start])[
  #table(columns: (1.1in, 1fr),
    [Message], [Cause],
    [#cmd("HTTPD038E")], [A read error before the end of the member.],
    [#cmd("HTTPD420E")], [A route lost its policy: #cmd("HTTPD419E") (a route
      that could not be registered), #cmd("HTTPD411E") (an unknown
      #cmd("AUTH=") value) or #cmd("HTTPD048E") (a retired #cmd("LOGIN") that
      required a login). Also #cmd("HTTPD427E"), a value of
      #cmd("RES_NOPROFILE") that is neither #cmd("ALLOW") nor #cmd("DENY").
      HTTPD reads the whole member first, so it writes all
      of these messages before it stops. It stops before it opens the port.
      @ref-route has the details.],
    [#cmd("HTTPD037E")], [Another address space already serves the port.],
    [#cmd("HTTPD028E"), #cmd("HTTPD030E"), #cmd("HTTPD031E")], [The stack
      refused the socket, the bind or the listen.],
  )
]

== Reading Back the Result <ref-prm-back>

#idx("D CONFIG")
The command #cmd("F HTTPD,D CONFIG") writes the values that HTTPD settled on,
and the name of the member that it read (@ref-con). It shows the port, the
tasks, the time limits, the two login limits, the document root, the code page,
the realm, the SMF type and level, the state of #cmd("RESIDENT") and the value
of #cmd("RES_NOPROFILE"). It does not show the other keywords. The display module #cmd("HTTPDSRV") shows the whole
control block of the server, including those (@ref-dm).
