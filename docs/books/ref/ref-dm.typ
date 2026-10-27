#import "../bookmaster/bookmaster.typ": *

= The Display Modules <ref-dm>

#idx("display modules")
#idx("HTTPDSRV")
#idx("HTTPDM")
#idx("HTTPDMTT")
#idx("ABEND0C1")
#idx("diagnostics", "display modules")
HTTPD comes with four small server modules for diagnosis. Three of them show
live data of the running server: HTTPDSRV shows the control blocks of the
server, HTTPDM shows any piece of storage as a dump, and HTTPDMTT shows the
master trace table of the system. The fourth, ABEND0C1, abends on purpose to
test the recovery of the server. All four are load modules that are built with
the server. They are server modules in the sense of @ref-api: HTTPD runs them
for the route that you give them in the Parmlib.

This chapter lists, for each module, the URL, the query parameters, what it
shows, the Parmlib line, the authentication it belongs behind, and its limits.
The _HTTPD User's Guide_ shows how an operator uses the modules, in "Operating
HTTPD".

== Setting Up the Modules <ref-dm-setup>

#idx("display modules", "route")
#idx("display modules", "authentication")
HTTPD registers no route for these modules by itself. The sample Parmlib
member has the four lines, commented out. They are the recommended routes:

#screen(```
MOD=HTTPDSRV /.dsrv     AUTH=FORM
MOD=HTTPDM   /.dm       AUTH=BASIC
MOD=HTTPDMTT /.dmtt     AUTH=BASIC
MOD=ABEND0C1 /.abend    AUTH=BASIC
```)

The keywords of a route line are in @ref-prm and @ref-route. Observe these
rules.

- A display module reads storage of the server address space and, for
  HTTPDMTT, the console log. A route without an #cmd("AUTH=") is public in
  HTTPD, so never register one of these modules without it. The sample member
  says the same: none of the lines may be uncommented without an #cmd("AUTH=")
  of its own.
- The modules do not look at the user. Every user who passes the route gate
  sees everything. To restrict the route to some users, add a #cmd("RES=")
  with a RACF profile, for example #cmd("RES=FACILITY:HTTPD.ADMIN"). The
  keyword is described in @ref-route.
- HTTPDSRV writes links to the path #cmd("/.dm"). The route for HTTPDM must
  therefore be exactly #cmd("/.dm") if the links in the pages of HTTPDSRV are
  to work. HTTPDSRV is a browser tool, so #cmd("AUTH=FORM") (a login page)
  suits it. HTTPDM and HTTPDMTT also work with #cmd("curl"), so
  #cmd("AUTH=BASIC") suits them.
- Use the modules on a test system or for a short time. They are diagnostic
  tools, not production endpoints.

HTTPDSRV, HTTPDM and HTTPDMTT send the response header
#cmd("Cache-Control: no-store"). None of them sends
#cmd("Access-Control-Allow-Origin"), so a page of another origin cannot read an
answer in a browser that holds a session with the server. The #cmd("text/plain")
answers of HTTPDM and HTTPDMTT (#cmd("d") set to a number other than 0) and the
pages of HTTPDSRV also send #cmd("X-Content-Type-Options: nosniff"), so a
browser does not take the text for HTML.

A display module that is started in another way than by HTTPD (for example from
JCL) does nothing. It writes the message #cmd("HTTPD920E") to the console and a
line "This program ... must be called by the HTTPD web server" to its standard
output, and it ends with the return code 12.

=== Reading Storage <ref-dm-read>

HTTPDSRV and HTTPDM read storage at addresses that come from the URL. They can
read what the server task may read. They do not check an address before they
read it. Four things follow from this.

- The memory dump of both modules runs under an abend handler (ESTAE). An
  address that the task cannot read ends the dump with a line that starts with
  #cmd("ABEND S0C4"). The server keeps working.
- The tables that HTTPDSRV prints after a dump are not under the handler. If an
  address in the URL of the targets #cmd("FILE"), #cmd("TASK"), #cmd("MOD") or
  #cmd("WORKERS") cannot be read, the module abends. HTTPD answers with the
  status 503 and the text "External program HTTPDSRV failed with S0C4 ABEND",
  and writes the message #cmd("HTTPD908E") to the console. The server keeps
  working.
- The modules show storage as it is at the moment of the request. The values
  can change while the page is built.
- An address is only good while the server runs. After a restart all addresses
  are new.

== HTTPDSRV, the Server Display <ref-dm-dsrv>

#idx("HTTPDSRV", "targets")
#idx("/.dsrv")
HTTPDSRV shows the control blocks of the server as HTML tables. The path in
the recommended route is #cmd("/.dsrv"). The module answers every request
method in the same way. It ignores the request body.

=== Parameters <ref-dm-dsrv-par>

#tab(caption: [The query parameters of HTTPDSRV])[
  #table(columns: (1.35in, 0.8in, 1fr),
    [Parameter], [Default], [Meaning],
    [#cmd("target"), #cmd("t")], [#cmd("HTTPD")], [What to display. The value is
      matched against the names in @ref-dm-dsrv-tgt. If both names are given,
      #cmd("target") counts.],
    [#cmd("m"), #cmd("mem"), #cmd("memory")], [none], [The address of the block to
      display, as hexadecimal digits. The first of the three names that is
      present counts. The targets that need an address stop with a message when
      it is missing.],
  )
]

The comparison of the target is not case sensitive, and the value only has to
be the start of a name. HTTPDSRV tests the names in a fixed order and takes the
first one that fits. The order is #cmd("FILE"), #cmd("FS"), #cmd("MGR"),
#cmd("MOD"), #cmd("HTTPD"), #cmd("TASK"), #cmd("TRACE"), #cmd("WORKERS"). So
#cmd("target=M") is #cmd("MGR"), #cmd("target=MO") is #cmd("MOD"),
#cmd("target=F") is #cmd("FILE"), and #cmd("target=T") is #cmd("TASK"). The
target #cmd("TRACE") needs at least the two letters #cmd("TR"). A value that fits none of the names gives the help page
(@ref-dm-dsrv-help). The help page lists only the targets of this table.

=== Targets <ref-dm-dsrv-tgt>

#tab(caption: [The targets of HTTPDSRV])[
  #table(columns: (1.0in, 1.1in, 1fr),
    [Target], [Needs an address], [Shows],
    [#cmd("HTTPD")], [no], [The HTTPD control block of the server. This is the
      default.],
    [#cmd("MOD")], [yes], [The route array and every route in it.],
    [#cmd("MGR")], [no (ignored)], [The manager of the worker pool.],
    [#cmd("WORKERS")], [yes], [The array of workers, with the request that
      each one is processing.],
    [#cmd("TASK")], [yes], [A thread (task) handle.],
    [#cmd("TRACE")], [no], [The request trace, as plain text (@ref-dm-dsrv-trace).],
    [#cmd("FILE")], [yes], [A C file handle (the debug file is an example).],
    [#cmd("FS")], [no], [The UFS system block of the server.],
  )
]

Every page except the help page has a heading with the address of the block, a
dump of the block with links, and a table of the fields. The table has four
columns: the offset of the field in the block (in hexadecimal, with a plus
sign), the name of the field, a description, and the contents.

The pages use the status 200 and the type #cmd("text/html"). A problem such as
a missing address is reported in the text of the page, still with the status
200.

==== Target HTTPD <ref-dm-dsrv-httpd>

The page shows the heading "HTTPD Handle" with the address of the block. The
table that follows has one row for every field of the control block, in order of
offset. The fields are these.

#tab(caption: [The fields shown for target HTTPD])[
  #table(columns: (1.5in, 1fr),
    [Field], [Contents],
    [#cmd("eye")], [The eye catcher of the block.],
    [#cmd("httpx")], [The address of the function vector (@ref-api-vector). A
      link opens the vector in HTTPDM.],
    [#cmd("httpc")], [The address of the client array, with the number of
      clients.],
    [#cmd("addr"), #cmd("port"), #cmd("listen")], [The address and the port that
      the server listens on, and the number of the listen socket.],
    [#cmd("cfg_realm")], [The Basic realm (the server name).],
    [#cmd("dbg")], [The debug file handle. When the debug file is open, a link
      opens it with the target #cmd("FILE").],
    [#cmd("tzoffset")], [The time zone offset in seconds.],
    [#cmd("busy")], [The array of busy clients, with its count.],
    [#cmd("flag")], [The processing flag in hexadecimal, followed by the names
      of the bits that are set: #cmd("INIT"), #cmd("LISTENER"),
      #cmd("READY"), #cmd("CFGERR"), #cmd("QUIESCE"), #cmd("SHUTDOWN").],
    [#cmd("client")], [The client options in hexadecimal, followed by the names
      of the bits that are set: #cmd("TIMEOUT"), #cmd("DUMP"), #cmd("STATS").],
    [#cmd("sockstate")], [The state of the socket thread: 0 starting, 1 running,
      2 gone.],
    [#cmd("socket_thread")], [The socket thread handle, with a link to the
      target #cmd("TASK").],
    [#cmd("mgr")], [The thread manager handle, with a link to the target
      #cmd("MGR").],
    [#cmd("rname")], [The resource name of the server.],
    [#cmd("route")], [The address of the route array, with a link to the target
      #cmd("MOD").],
    [#cmd("uptime")], [The start time of the server, as text.],
    [#cmd("ufssys")], [The UFS system handle, with a link to the target
      #cmd("FS").],
    [#cmd("readerr_last")], [The time of the last console message about a read
      error of the file system (#cmd("HTTPD045E")), 0 if there was none.],
    [#cmd("version")], [The version string of the server.],
    [#cmd("readerr_held")], [The number of such read errors not yet reported.],
    [#cmd("cfg_maxtask"), #cmd("cfg_mintask")], [The configured maximum and
      minimum number of workers.],
    [#cmd("cfg_client_timeout")], [The client timeout in seconds.],
    [#cmd("smf_level"), #cmd("smf_type")], [The SMF level (number and name) and
      the SMF record type (@ref-smf).],
    [#cmd("cfg_resident")], [Whether the server holds the modules of its routes
      in storage (@ref-api-resident).],
    [#cmd("resident_held")], [The number of modules held.],
    [#cmd("cfg_cgictx")], [The configured number of context pointers
      (@ref-api-cgictx).],
    [#cmd("ufs_enabled"), #cmd("dbg_enabled")], [Whether UFS is enabled and
      whether debug output is on.],
    [#cmd("bind_tries"), #cmd("bind_sleep")], [The retry count and the delay in
      seconds for the bind of the listen port.],
    [#cmd("total_requests"), #cmd("total_errors"), #cmd("total_bytes_sent")], [The
      counters of the server. An error is a response with a status of 400 or
      more.],
    [#cmd("active_connections")], [The number of open client connections.],
    [#cmd("xlate")], [The address of the code page pair in effect.],
    [#cmd("giveup_last")], [The time of the last console message about a client
      that was given up.],
    [#cmd("stc_prev_acee")], [The address of the ACEE that the server restores
      at shutdown.],
    [#cmd("giveup_held")], [The number of such cases not yet reported.],
    [#cmd("ufs")], [The UFS session handle of the server.],
    [#cmd("cfg_session_maxage")], [The hard maximum age of a credential in
      minutes, 0 for off.],
    [#cmd("self")], [The main thread handle, with a link to the target
      #cmd("TASK").],
    [#cmd("cgictx")], [The address of the context pointer array. A link opens it
      in HTTPDM.],
    [#cmd("docroot")], [The UFS document root.],
    [#cmd("listen_queue")], [The listen backlog.],
    [#cmd("cfg_res_noprof")], [The value of #cmd("RES_NOPROFILE"): the page shows
      #cmd("ALLOW") or #cmd("DENY").],
    [#cmd("codepage")], [The code page name from the #cmd("CODEPAGE") keyword.
      When the keyword is not set, the page shows "CP037 (default, no CODEPAGE
      keyword)".],
    [#cmd("cfg_keepalive_timeout"), #cmd("cfg_keepalive_max")], [The keep-alive
      idle timeout in seconds, and the maximum number of requests per
      connection.],
    [#cmd("cfg_session_timeout")], [The idle time of a credential in minutes,
      0 for off.],
    [#cmd("credkey"), #cmd("credarr")], [The addresses of the credential key and
      of the credential array. The page shows the addresses only.],
    [#cmd("cfg_realm_val")], [The text of the realm.],
    [#cmd("trace")], [The address of the request trace, 0 when it is off
      (@ref-dm-dsrv-trace).],
  )
]

The page shows fields that the server no longer uses as reserved, so that the
table accounts for every byte of the block. It also has a small form that posts
to #cmd("/.dm") and shows an arbitrary memory area (@ref-dm-dm).

==== Target MOD <ref-dm-dsrv-mod>

The target needs the address of the route array in #cmd("m"). The link on the
field #cmd("route") of the HTTPD page supplies it. Without #cmd("m") the page
says #cmd("Missing memory address (&m=nnnnnnnn)").

The page shows a dump of the array. For every route it shows a heading
"Route \#n" with #cmd("MOD") for a route that has a program and #cmd("LOC")
for a static prefix, a dump of the route block, and a table:

#tab(caption: [The fields shown for a route])[
  #table(columns: (1.2in, 1fr),
    [Field], [Contents],
    [#cmd("eye")], [The eye catcher of the route block.],
    [#cmd("wild")], [Whether the path contains a wildcard.],
    [#cmd("len")], [The length of the path.],
    [#cmd("path")], [The path of the route, as written in the Parmlib.],
    [#cmd("pgm")], [The name of the program. A #cmd("LOC=") route has none.],
    [#cmd("auth")], [The policy: #cmd("NONE") (public), #cmd("FORM") (an HTML
      login form), #cmd("BASIC") (a 401 with a #cmd("WWW-Authenticate")
      header), or #cmd("TOKEN") (a bare 401, for an API route).],
    [#cmd("resattr")], [The access level that #cmd("RES=") asks for: READ,
      UPDATE, CONTROL or ALTER. The value 0 means that no level was set, and
      READ is assumed.],
    [#cmd("resclass"), #cmd("resname")], [The RACF class and the resource name
      of the #cmd("RES=") keyword.],
  )
]

This is the page to use to find out which policy a running server applies to
a path (@ref-route).

==== Target MGR <ref-dm-dsrv-mgr>

The target shows the thread manager of the worker pool. It does not take an
address: it ignores #cmd("m") and uses the manager of the server. The page
prints #cmd("Invalid Eye Catcher") if the block is not a manager. The help page
says the same: the target takes no address.

The fields are the task of the manager (with a link to the target #cmd("TASK")),
the wait flag, the function that the workers run, the user data pointer, the
stack size, the worker array (with a link to the target #cmd("WORKERS")), the
queue, the state (#cmd("INIT"), #cmd("RUNNING"), #cmd("QUIESCE"),
#cmd("STOPPED") or #cmd("WAITING")), the minimum and maximum number of tasks,
the number of requests dispatched since the start, the start time and the
resource name.

==== Target WORKERS <ref-dm-dsrv-workers>

The target needs the address of the worker array in #cmd("m"). The link on the
field #cmd("worker") of the MGR page supplies it. The page skips an entry whose
eye catcher is not a worker.

For every worker the page shows a dump and a table with the eye catcher, the
wait flag, the manager, the task (with a link to the target #cmd("TASK")), the
queue, the state (#cmd("INIT"), #cmd("RUNNING"), #cmd("WAITING"),
#cmd("DISPATCH"), #cmd("SHUTDOWN") or #cmd("STOPPED")), the options
(#cmd("TIMER"), #cmd("NOWORK")), the start time, the wait time and the dispatch
time, and the number of requests that the worker dispatched.

When a worker is in the state #cmd("RUNNING") and has a queue entry, the page
adds a block that tells what the worker is doing: the protocol (HTTP), the user
(the user ID of the credential, or "ANONYMOUS" if there is none), the group,
the socket number, and the local and the remote address with the port.

==== Target TRACE <ref-dm-dsrv-trace>

#idx("TRACE", "target")
#idx("request trace")
The target shows the request trace that the Parmlib keyword #cmd("TRACE")
switches on (@ref-prm-diag). It needs no address. The answer is not HTML. It has
the type #cmd("text/plain") and the header #cmd("Cache-Control: no-store"). If
the keyword is not set, or its value was refused, the answer is the single line

#screen(```
#HTTPTRC off
```)

The query parameter #cmd("min") is a number of milliseconds. With it, the answer
leaves out each finished record whose time from the start to the last mark is
shorter. A record of a request that is still running is always shown. A value
that is not a number counts as 0.

The first line of the answer starts with #cmd("#HTTPTRC v1") and has these
words.

#tab(caption: [The words of the first line])[
  #table(columns: (1.4in, 1fr),
    [Word], [Meaning],
    [#cmd("now")], [The clock at the readout, in ticks of 1.024 ms (see below).],
    [#cmd("wall")], [The time of the readout in seconds since 1 January 1970.],
    [#cmd("unit")], [The length of a tick, always #cmd("1.024ms").],
    [#cmd("slots")], [The number of records, the value of #cmd("TRACE").],
    [#cmd("seq")], [The number of requests that got a record since the start.
      It is also the number of the newest record.],
    [#cmd("lost")], [The number of requests that got no record because all
      records belonged to requests that were still running.],
    [#cmd("miss")], [How often a phase mark found no record for its request.
      A request that was lost causes a miss at each of its marks.],
    [#cmd("workers")], [The number of workers now.],
  )
]

One line for each record follows. The lines are in the order of the table, not
of time: sort by #cmd("seq"). A line is a row of #cmd("name=value") words
without a space inside a value. The line of the request that reads the trace is
among them, and the request is still running.

#tab(caption: [The words of a trace line])[
  #table(columns: (1.4in, 1fr),
    [Word], [Meaning],
    [#cmd("seq")], [The number of the request since the start.],
    [#cmd("c")], [The address of the client block (HTTPC), in hex.],
    [#cmd("w"), #cmd("tcb")], [The address of the worker (the same as
      #cmd("WORKER(xxxxxx)") on the WORKERS page and in #cmd("HTTPD061I")) and
      of its task block, in hex. Both are 0 until a worker has taken the
      connection.],
    [#cmd("ip"), #cmd("port")], [The address and the port of the client.],
    [#cmd("n")], [The number of the request on its connection, from 0. A
      connection that is kept alive has one record for each request.],
    [#cmd("resp")], [The status code at the last mark. It is 0 as long as no
      response line was written.],
    [#cmd("ago")], [The milliseconds from the start of the record to the
      readout.],
    [#cmd("live")], [1 while the request is running, 0 when it is finished.],
    [#cmd("last"), #cmd("st")], [The last phase that the request reached, and
      the state of the client at that mark (#cmd("IN"), #cmd("PARSE"),
      #cmd("GET"), #cmd("HEAD"), #cmd("PUT"), #cmd("POST"), #cmd("DELETE"),
      #cmd("DONE"), #cmd("REPORT"), #cmd("RESET") or #cmd("CLOSE")). #cmd("last")
      is #cmd("q") if no phase was reached.],
    [#cmd("age")], [The milliseconds from the start to the last mark, or to the
      readout for a request that is still running.],
    [#cmd("run") to #cmd("close")], [The milliseconds from the start to each
      phase, or #cmd("-") if the request never got there (the next table).],
    [#cmd("polls")], [How often the server waited half a second for the next
      data of the request line or of a header line.],
    [#cmd("txn")], [The number of calls of #cmd("send") for the request,
      including calls that sent nothing.],
    [#cmd("txst")], [How often the server paused for a tenth of a second
      because TCP/IP took no data. Only the send of a chunked response counts
      these pauses.],
    [#cmd("txms")], [The milliseconds spent inside #cmd("send").],
    [#cmd("sent")], [The bytes sent for the request. The server fills the word
      when the request ends, so it is 0 for a request that is still running.],
    [#cmd("path")], [The first 23 characters of the path of the request, without
      the query. A character that is not printable shows as #cmd("?"). The word
      is empty until the request is parsed. It is always the last word.],
  )
]

The start of a record is the accept of the connection for the first request of
a connection. For a later request on the same connection it is the end of the
request before, so the time that the client needs to send its next request is
part of the times of that record. These are the phases, in the order a request
normally reaches them.

#tab(caption: [The phases of a request])[
  #table(columns: (0.9in, 1fr),
    [Word], [The mark is set when ...],
    [#cmd("run")], [a worker took the connection. For a later request of a
      connection it is 0, because the worker still holds the connection.],
    [#cmd("line1")], [the request line was read.],
    [#cmd("req")], [the server stopped reading the request line and the header
      lines. The mark is also set when the read failed or timed out, or when the
      client closed the connection. Then #cmd("line1") is #cmd("-") or the
      later phases are.],
    [#cmd("parse")], [the request was parsed.],
    [#cmd("cred")], [the credential of the request was resolved, with or
      without a result.],
    [#cmd("route")], [the request passed the check of the policy of its
      route. A request that the check refuses, and the paths #cmd("/login")
      and #cmd("/logout"), do not get the mark.],
    [#cmd("lnk0"), #cmd("lnk1")], [just before the module of a #cmd("MOD=")
      route is linked, and when it returns. A static file has neither. The time
      between the two is the run of the module, including its load when the
      server does not hold it in storage
      (@ref-api-resident).],
    [#cmd("snd1"), #cmd("sndl")], [the first and the last call of #cmd("send")
      that handed data to TCP/IP. The headers count as data.],
    [#cmd("done")], [the response is complete.],
    [#cmd("end")], [the request was reported (SMF and the counters) and the
      server is about to reset the connection.],
    [#cmd("close")], [the connection was closed while the record was still
      open. It is the mark of a connection that ends without another request.],
  )
]

This is a trace of a connection that stays open, from the lab server. The client
sent two requests on one connection and then closed it (the readout is cut to
the words of interest):

#screen(raw(read("../ex/ug-trouble/trace-keepalive.txt")))

The first record is a request that a module answered. The server waited 4 ms
for the request line, the module ran from 14 ms to 44 ms, and 532 bytes went to
TCP/IP in 15 calls. The second record has the same worker, and its #cmd("run")
is 0. The third record is the connection after its last request: the server
waited for a next request that did not come, and the client closed the
connection after 503 ms.

The times come from the TOD clock. The server shifts the clock value right by 22
bits and keeps 32 bits, so one tick is 1.024 milliseconds and the figures in a
line are rounded down to a whole millisecond of that scale. The tick count wraps
after about 51 days. The trace only uses the difference of two ticks, so the
wrap does not matter, but the #cmd("now") of one readout cannot be compared with
the #cmd("now") of a much later one.

The table does not keep the records in time order. A new record takes the next
free place, counted from a position that moves by one for each request. A record
of a running request is never taken. A record of a finished request that took
977 ticks (about one second) or more is taken only when no other record is left,
so a slow request stays in the table longer than a fast one. The time of a
request counts from the accept if it is the first request of its connection, and
from its request line if it is a later one, so the time that a client takes to
send its next request does not make a request slow. When all records belong to
running requests, the next request gets none and #cmd("lost") grows.

#note[The record holds the address and the port of the client and the first
23 characters of the path of each request. Anyone who may call the HTTPDSRV
route can read them. Give the route an #cmd("AUTH=") (see the start of this
chapter), and switch #cmd("TRACE") on only while you look for a problem.]

==== Target TASK <ref-dm-dsrv-task>

The target needs the address of a thread handle in #cmd("m"). The fields are
the eye catcher, the task control block address (#cmd("tcb")), #cmd("owntcb"),
the termination ECB (#cmd("termecb")), the return code, the stack size, the
function, its two arguments, and the address of the stack. A link on the stack
opens it in HTTPDM with the stack size as the length.

==== Target FILE <ref-dm-dsrv-file>

The target needs the address of a C file handle in #cmd("m"). The fields are
the eye catcher; the DCB address (a link opens it in HTTPDM); the assembler
buffer; the logical record length and the block size; the file position; the
buffer address (a link opens it) with the pointers into it; the flags, which
are decoded as #cmd("DYNAMIC"), #cmd("OPEN"), #cmd("READ"), #cmd("WRITE"),
#cmd("APPEND"), #cmd("BINARY"), #cmd("RECORD"), #cmd("BSAM"), #cmd("TERM"),
#cmd("ERROR") and #cmd("EOF"); the record format, which is decoded (fixed,
variable or undefined, and the attributes blocked, spanned, ASA, machine
control, key length); the DD name, the member, the data set name and the mode.

==== Target FS <ref-dm-dsrv-fs>

Without #cmd("m") the page shows the UFS system block of the server: a dump
of the block and the eye catcher. A line tells that the state of the disks
and inodes belongs to the UFSD started task. HTTPD holds only the client side
of UFS, so the page cannot show more.

Do not use #cmd("m") with this target. With #cmd("m"), the module does not
display the block at that address. If the storage at the address does not start
with one of the two block names that the module looks for, the module sends no
response.

=== The Help Page <ref-dm-dsrv-help>

A value of #cmd("target") that matches no name gives the page "HTTPDSRV Help".
It has the status 200, shows the usage line with the host and the path of the
request, and lists the targets. The host and the path are escaped for HTML.

== HTTPDM, the Memory Display <ref-dm-dm>

#idx("HTTPDM", "parameters")
#idx("/.dm")
#idx("memory dump")
HTTPDM shows a block of storage as a hexadecimal dump with a character column.
It is the module behind the links in the pages of HTTPDSRV. It answers every
request method in the same way.

=== Parameters <ref-dm-dm-par>

HTTPDM takes the first name that is present from each row of the table.

#tab(caption: [The query parameters of HTTPDM])[
  #table(columns: (1.5in, 0.8in, 1fr),
    [Parameter], [Default], [Meaning],
    [#cmd("title"), #cmd("t")], [none], [A title that is printed in the first
      line of the dump. In the HTML form the module escapes #cmd("<"),
      #cmd(">"), #cmd("&"), #cmd("\"") and #cmd("'"), and the escaped title has at
      most 255 characters, a replacement is never cut in two. With #cmd("d") other than 0 it is written as it is.],
    [#cmd("memory"), #cmd("mem"), #cmd("m")], [0], [The address to dump, in
      hexadecimal. HTTPDM keeps the low 24 bits of the value.],
    [#cmd("length"), #cmd("len"), #cmd("l")], [256], [The number of bytes to
      dump. The module converts the number as decimal, or as hexadecimal with a
      leading 0x. A value of 0 or less gives 256, a value above 4096 gives
      4096.],
    [#cmd("chunk"), #cmd("size"), #cmd("c"), #cmd("s")], [16], [The number of
      bytes in a line. See the rule below.],
    [#cmd("data"), #cmd("d")], [0], [The output form. 0 gives an HTML page, any
      other number gives plain text. The module converts the number as decimal,
      or as hexadecimal with a leading 0x.],
  )
]

The rule for #cmd("chunk"): a number of 0 gives 16. A number above 64 gives 64.
Any other number is rounded up to a multiple of 4. So 1 to 4 give 4, 5 to 8
give 8, and 61 to 64 give 64.

=== Output <ref-dm-dm-out>

With #cmd("d=0") (the default) the response is #cmd("text/html"). It has a form
with the fields Memory Address, Data Length and Chunk Size, and a preformatted
block with the dump. With #cmd("d") set to another number, the response is
#cmd("text/plain") and holds the dump only, without the form and without any
report of an abend.

The dump starts with the line

#screen(```
Dump of [title ]AAAAAAAA (n byte chunks) (m bytes total)
```)

in which #cmd("AAAAAAAA") is the address in hexadecimal, and #cmd("n") and
#cmd("m") are the chunk size and the length. One line follows for every chunk.
A line has the offset from the start (a plus sign and five hexadecimal
digits), then the bytes in hexadecimal in groups of four, and then the
characters of the bytes between colons. The character column shows the bytes as
EBCDIC characters. A byte that is not a printable character shows as a period.

In the HTML form every group of four bytes is a link. The link dumps the
storage to which the four bytes point, with the same length, chunk size and
data parameters. The link has the form
#cmd("?m=address&l=length&c=chunk&d=data"). So you follow a chain of pointers
with a click. In the character column the characters #cmd("<"), #cmd(">") and
#cmd("&") are written as HTML entities.

An address that the task cannot read gives, in the HTML form, the line
#cmd("ABEND S0C4 occured for DISPLAY MEMORY 0x...") in place of the rest of the
dump. A user abend shows as #cmd("ABEND Unnnn occured..."). If the abend handler
cannot be created, the line is #cmd("ESTAE CREATE failure, RC=0x...") with the
code. With #cmd("d") other than 0 these lines are not written and the output
simply stops.

=== Limits <ref-dm-dm-lim>

- The largest dump is 4096 bytes per request.
- HTTPDM keeps the low 24 bits of the address, because the system uses 24-bit
  addresses. The form always shows the address that the module used.
- The route belongs behind a login (@ref-dm-setup), because the dump shows
  storage of the server.
- The dump can reach any storage of the server address space that the task may
  read. That may include data that the server holds for its users, such as
  cached credentials.

== HTTPDMTT, the Master Trace Table <ref-dm-dmtt>

#idx("HTTPDMTT", "parameters")
#idx("/.dmtt")
#idx("master trace table")
HTTPDMTT shows the master trace table of the system. The table holds the
recent messages of the system console. The module makes a copy of the table
and prints the entries in order.

=== Parameters <ref-dm-dmtt-par>

#tab(caption: [The query parameter of HTTPDMTT])[
  #table(columns: (1.5in, 0.8in, 1fr),
    [Parameter], [Default], [Meaning],
    [#cmd("data"), #cmd("d")], [0], [The output form. The module converts the
      number as decimal. 0 gives an HTML page, any other number gives plain
      text.],
  )
]

=== Output <ref-dm-dmtt-out>

With #cmd("d=0") the response is #cmd("text/html"). The page has the heading
"Master Trace Table", then one preformatted block with one line for every entry
of the table. With #cmd("d") set to another number, the response is
#cmd("text/plain") and has the lines only. The module prints the text of an
entry as text. It does not read the text as a format, so a percent sign in a
message does no harm.

In the HTML page the module escapes #cmd("<"), #cmd(">"), #cmd("&"),
#cmd("\"") and #cmd("'") in every entry, so a message such as
#cmd("<b>") shows as that text and is not read as markup. The table holds the
messages of every job, not only those of HTTPD. In the plain form the entries
are as the table holds them, without escaping. The module reads an entry up to
its length in the table, or up to a null character if that comes first.

If the module cannot get storage for the copy, it prints "Unable to allocate
Master Trace Table." and ends. The module frees the copy at the end.

The module sends an entry in pieces of at most 255 characters, after any
escaping. It checks the sends of the response header, of the start of the HTML
page up to the body tag, and of the entries. At the first of these that fails,
for example because the client has closed the connection, it stops and sends
nothing more: not the rest of the entry, not the next entries and not the
closing lines of the HTML page. An entry longer than one piece can therefore end
in the middle of the line. The module frees the copy in this case too. It does
not check the sends of the heading, of the opening line of the preformatted
block, of the closing lines of the page and of the allocation message; a
failure there does not stop it.

=== Limits <ref-dm-dmtt-lim>

- The table is the master trace table of the system, so it shows every message
  that went to the console. It exposes the console log to everyone who may use
  the route. It needs strong authentication.
- The module does not select, filter or search. Take the plain form
  (#cmd("d=1")) and use a tool of your own for that.

== ABEND0C1, the Abend Test <ref-dm-abend>

#idx("ABEND0C1")
#idx("/.abend")
#idx("abend", "test module")
ABEND0C1 takes storage and abends with a program check (S0C1). It exists to
prove that HTTPD gives back the storage of a failed module and keeps its
workers (@ref-api-abend). Call it many times and check that the other modules
still load and run as fast as before.

The module looks for the text #cmd("kb=") anywhere in the query string, not case
sensitive, and reads the digits after it. An example is
#cmd("/.abend?kb=512").

#tab(caption: [The query parameter of ABEND0C1])[
  #table(columns: (1.0in, 0.8in, 1fr),
    [Parameter], [Default], [Meaning],
    [#cmd("kb")], [128], [The amount of storage to take, in kilobytes. A value
      above 1024 gives 1024. A value of 0, a missing value and a value that
      is not a number give 128.],
  )
]

What the module does:

+ It takes the storage in blocks of 16 KB with #cmd("malloc") and writes the
  byte hex C1 into every block. It stops when no storage is left.
+ It writes the console message #cmd("HTTPD921W"). The message names the amount
  taken in KB and the number of blocks.
+ It abends with S0C1, without freeing the blocks. It sends no response itself.

The client then gets the answer of HTTPD for a failed module: the status 503
with the text "External program ABEND0C1 failed with S0C1 ABEND". HTTPD
releases the storage that the module took (@ref-api-abend).

The default amount does not endanger the server. The module is a probe that you
may point at a running system.

#note[Register this module only for a test, and with #cmd("AUTH=BASIC") or
stronger. A route to it makes a failure on request.]
