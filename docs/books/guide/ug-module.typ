#import "../bookmaster/bookmaster.typ": *

= Writing a Server Module <ug-module>

#idx("server module")
#idx("module", "writing")
A server module is a program that HTTPD runs to answer the requests for a path.
HTTPD itself reads the request, checks the login, and finds the route. The
module writes the response. mvsMF, HTTPLUA and HTTPREXX are server modules, and
so are the display modules of @ug-ops. This chapter takes you from nothing to a
module that builds, runs under HTTPD and answers a request, and then it shows
the rules that a module must keep. The exact list of functions, variables and
messages is in the _HTTPD Reference_, in the chapter "The Server Module
Interface". This chapter points to it where the detail is.

The program in this chapter is complete. It was built with MBT, deployed to a
test system and called with #cmd("curl"). The screens are the real output.

== What a Server Module Is <ug-module-what>

#idx("server module", "LINK")
A server module is a load module in a library that the HTTPD started task can
read. A route of the Parmlib names it with the keyword #cmd("MOD=")
(@ug-routes). For every request to the route, the worker task that serves the
request runs the module with the LINK service of MVS. The module runs in the
address space of HTTPD, in the task of the worker, once for each request. The
method does not matter: GET, POST, PUT, DELETE and HEAD all arrive at the same
module, and the module has to look at the method itself.

Four facts follow from this, and the rest of the chapter depends on them.

- The server has done its work before the module starts. The request is read
  and parsed, and the authentication rule of the route has been applied. A
  request that fails the rule gets its 401 or 403 from the server and never
  reaches the module.
- The module must complete the response before it returns. When it returns,
  HTTPD ends the request, whatever the return code is.
- The module has no allocations of its own. A file that the module opens by
  ddname is looked for in the procedure of the started task, so the procedure
  is part of the contract of the module (see @ug-module-deploy).
- A module that fails does not take the server down (@ug-module-abend), but it
  can hold a worker for as long as it runs (@ug-module-drain).

== The Smallest Complete Module <ug-module-hello>

#idx("HELLO", "example module")
The module HELLO answers three kinds of request. A GET of #cmd("/hello") answers
with a greeting, and the query parameter #cmd("name") says whom to greet. A
POST answers with the body it received. Any other method gets the status 405.

#code(read("../ex/ug-module/hello.c"), numbers: true)

The program is the whole module. The lines in it do the following.

#tab(caption: [HELLO, line by line])[
  #table(columns: (0.9in, 1fr),
    [*Lines*], [*What they do*],
    [7 and 8], [#cmd("mvs/crt.h") of the C library declares #cmd("__grtget()")
      and the type #cmd("CLIBGRT"). #cmd("httpcgi.h") is the header of HTTPD for
      server modules.],
    [11], [Every function of the server is called through a vector of function
      pointers, the HTTPX. The macros of #cmd("httpcgi.h") expect a name
      #cmd("httpx") in scope. The line defines it as the expression that finds
      the vector in the server block, so the variable #cmd("httpd") must be in
      scope where a macro is used.],
    [13 and 14], [The entry point is #cmd("main(argc, argv)") (see
      @ug-module-entry).],
    [16 to 18], [The module finds the server block and the block of the client
      in the global runtime table: #cmd("grtapp1") holds HTTPD and
      #cmd("grtapp2") holds HTTPC.],
    [19], [#cmd("http_get_env") reads a variable of the request
      (@ug-module-request).],
    [21 and 22], [The parameters are not used. The build turns every warning
      into an error (@ug-module-build), so the module casts them to void.],
    [24 to 30], [GET. #cmd("http_resp(httpc, 200)") sends the status line and
      the standard headers. Line 27 sends one header. Line 28 ends the headers.
      Line 29 is the body.],
    [31 to 37], [POST. The body of the request is the variable
      #cmd("POST_STRING"). The module sends it back.],
    [38 to 44], [Any other method. The status 405 and the header
      #cmd("Allow") say what the module does accept.],
    [45], [The return code 0.],
  )
]

The listing shows the rules that every module must keep. They have their own
sections: how the module is entered, how the headers end, and what the module
does with storage.

#note[The listing is complete as a working program, but it leaves out one line
that a module you keep should have: the size of the stack. HELLO does not set
it, so each request that it serves asks for the default stack of 256 KB. The
captures in this chapter show that it works. See @ug-module-storage for the
line to add.]

== The Entry Point <ug-module-entry>

#idx("cgistart")
#idx("main", "server module")
#idx("argv", "module")
The entry point of a server module is the C function #cmd("main(argc, argv)").
It is not a function with the server blocks as parameters. The module is linked
with the startup code #cmd("cgistart") of the HTTPD library, and the startup
code, not the module, reads the parameter list that HTTPD builds for the LINK.
It stores the addresses of HTTPD and HTTPC in the global runtime table, opens
#cmd("stdout"), #cmd("stderr") and #cmd("stdin") on the DDs #cmd("HTTPDOUT"),
#cmd("HTTPDERR") and #cmd("HTTPDIN"), and calls #cmd("main").

#note[Older notes on server modules describe an entry
#cmd("cgimain(HTTPD *, HTTPC *)"). There is no such entry. The program in this
chapter is the form that works: a plain #cmd("main"), and the two blocks
fetched from the runtime table.]

In #cmd("main") the module finds:

#deflist(width: 1.35in,
  [#cmd("argc")], [2.],
  [#cmd("argv[0]")], [The name of the module, up to 8 characters.],
  [#cmd("argv[1]")], [The decoded request path, without quotes.],
  [HTTPD, HTTPC], [Read #cmd("grt->grtapp1") and #cmd("grt->grtapp2"), as the
    program does. Compare the eye catcher of each block (#cmd("HTTPD_EYE") and
    #cmd("HTTPC_EYE")) before you use it.],
  [The vector], [#cmd("http_get_httpx(httpd)") returns the HTTPX. In the
    program the macro #cmd("httpx") is defined with it.],
)

The return code of #cmd("main") does not decide the answer. A value of 0 or
more is not an error, and the startup code treats a negative value as 0.

If somebody runs the module outside HTTPD, the startup code writes "is an HTTPD
server module and cannot run on its own" and ends.

#cmd("printf") is not what you expect in a server module. The startup code has
a #cmd("printf") of its own that replaces the library function, and under HTTPD
it writes into the response, as #cmd("http_printv") does. If you need a line in
the output of the started task, write it with #cmd("fprintf(stdout, ...)")
or #cmd("fprintf(stderr, ...)"). The first goes to the DD
#cmd("HTTPDOUT"), the second to #cmd("HTTPDERR").

== Reading the Request <ug-module-request>

#idx("request variables")
#idx("QUERY_STRING")
#idx("POST_STRING")
HTTPD stores the request in variables of the client, and the module reads them
with #cmd("http_get_env"). The value comes back as an EBCDIC string, or NULL if
there is no such variable. The names are not case sensitive. These are the
variables a module uses most. The _HTTPD Reference_ has the full list in the
chapter "The Server Module Interface".

#tab(caption: [The request variables of most use])[
  #table(columns: (1.5in, 1fr),
    [*Variable*], [*Value*],
    [#cmd("REQUEST_METHOD")], [The method: GET, HEAD, PUT, POST or DELETE.],
    [#cmd("REQUEST_PATH")], [The path of the URI, decoded, without the query.],
    [#cmd("QUERY_STRING")], [The text after the question mark, not decoded. It
      exists only if the URI has a query.],
    [#cmd("QUERY_name")], [One variable for each query parameter. The value is
      decoded, the name is not. HELLO reads #cmd("QUERY_name") for the
      parameter #cmd("name").],
    [#cmd("HTTP_name")], [One variable for each header of the request, for
      example #cmd("HTTP_Content-Type").],
    [#cmd("POST_STRING")], [The body of a POST request.],
    [#cmd("POST_name")], [One variable for each field of a form body, decoded.],
  )
]

Test the result for NULL before you use it. HELLO does this for the
parameter #cmd("name") and for the body.

HTTPD reads the body of a POST only under conditions. The header
#cmd("Content-Length") must be present with a length from 1 to 3999, and there
must be a #cmd("Content-Type") header. Then the whole body is in
#cmd("POST_STRING"), raw and not decoded. If the type is exactly
#cmd("application/x-www-form-urlencoded"), the server also splits the body into
the #cmd("POST_name") variables and decodes their values. A body of 4000 bytes
or more is refused with the status 413. A chunked body, or a body without a
#cmd("Content-Type"), gives no #cmd("POST_STRING"). The server does not read the
body of a PUT: the module reads it from the socket in #cmd("httpc->socket").
A request with more than 256 parameters gets the status 400.

HELLO shows the difference between the variables. The first POST is a form, as
#cmd("curl -d") sends it. #cmd("POST_STRING") holds the raw text, with the
percent codes:

#screen(raw(read("../ex/ug-module/post-echo.txt")))

The second POST has the type #cmd("text/plain"). The body arrives in the same
variable:

#screen(raw(read("../ex/ug-module/post-text.txt")))

== Sending the Response <ug-module-response>

#idx("response", "module")
#idx("http_resp")
#idx("http_printf")
A response has four parts, and a module sends them in this order.

+ #cmd("http_resp(httpc, code)") sends the status line and the standard
  headers: #cmd("Date"), #cmd("Server"), #cmd("X-Powered-By") and
  #cmd("Connection"). The line has the version of the request.
+ Zero or more header lines, each with #cmd("http_printf").
+ One call that ends the headers. The text of the call is the two characters
  carriage return and line feed, and nothing else.
+ The body, with #cmd("http_printf") or, for bytes that are not text,
  #cmd("http_send").

#cmd("http_resp") knows a list of status codes: 200, 201, 202, 204, 206, 301,
302, 303, 304, 400, 401, 403, 404, 405, 409, 410, 412, 413, 414, 429, 500, 501,
502, 503, 505 and 507. For any other code it writes #cmd("HTTPD909E") and sends
500. #cmd("http_printf") formats its text in a buffer of 5120 bytes, converts it
to ASCII and sends it. Text that is longer than 5119 bytes is cut, so send a
long body in pieces. It returns 0, or -1 if the send failed.

Here is what HELLO sends for a GET. The header lines are those of the module
and of #cmd("http_resp"). The answer is chunked, because the module ended the
headers in the right way:

#screen(raw(read("../ex/ug-module/get-default.txt")))

The parameter reaches the module decoded:

#screen(raw(read("../ex/ug-module/get-name.txt")))

A method that the module does not take gets the status 405 and the header that
says what is allowed. The answer to a PUT:

#screen(raw(read("../ex/ug-module/put-405.txt")))

=== The End of the Headers <ug-module-blank>

#idx("response", "chunked")
#idx("Transfer-Encoding")
This is the rule that most often goes wrong. Line 28 of HELLO is a call of its
own:

#screen(```c
http_printf(httpc, "Content-Type: text/plain\r\n");
http_printf(httpc, "\r\n");     /* the blank line is a call of its own */
```)

HTTPD watches for the call whose text is exactly the two characters
carriage return and line feed. That call ends the headers. When the server sees
it, and the request is HTTP/1.1, and the status allows a body, and no
#cmd("Content-Length") has been sent, it sends the header
#cmd("Transfer-Encoding: chunked") and turns chunking on. From then on every
send is a chunk, and the server sends the last, empty chunk when the request
ends. The client knows where the body ends.

The version that looks shorter does not work:

#screen(```c
http_printf(httpc, "Content-Type: text/plain\r\n\r\n");   /* WRONG */
```)

HTTPD does not recognize the end of the headers in such a call. The headers go
out, the blank line goes out, but the response has neither a chunk framing nor
a length. The example directory has the file #cmd("hello-pitfall.c"), which is
HELLO with that one change. Its answer has no #cmd("Transfer-Encoding"), and the
last line of the headers is empty:

#screen(raw(read("../ex/ug-module/pitfall-blank-line-headers.txt")))

With no length and no chunk framing, the client has no way to know where the
body ends, so it waits for the connection to close. The measured times show what
that costs. With the correct code, one request takes:

#screen(raw(read("../ex/ug-module/timing.txt")))

The same request to the module with the wrong terminator takes:

#screen(raw(read("../ex/ug-module/pitfall-blank-line.txt")))

The measured time is close to the keep-alive timeout of the server, which is 5
seconds in the default setting (#cmd("D C") shows it as #cmd("KEEPALIVE 5/100"),
see @ug-ops-d-config). The capture does not prove why the connection ends when it
does. What it shows is the symptom: the answer is right, the call is slow, and
the delay is a number of seconds. If you see an answer that arrives after a
fixed delay, look at the end of the headers first.

The correct form lets two requests share one connection. The second request of
this capture uses the connection of the first:

#screen(raw(read("../ex/ug-module/keepalive-two.txt")))

A request that has a body does not keep the connection. The POST and PUT
captures above show #cmd("Connection: close"). The Reference says why: HTTPD
turns keep-alive off for a request with a #cmd("Content-Length") or a
#cmd("Transfer-Encoding") header, because it cannot be sure that it has read
the whole body. This is not a fault of the module.

Three more rules go with the end of the headers.

- Do not send a line break alone as a call of its own before the headers have
  ended. The call with exactly carriage return and line feed is taken for the
  end of the headers. After that, a line break alone in the body is data. Put
  the line break in a longer text anyway.
- If the module sends a header line with #cmd("Content-Length:") through
  #cmd("http_printf"), HTTPD sees it and does not chunk. The module then must
  send exactly that number of bytes. A header line that goes out with
  #cmd("http_send") is not examined.
- Chunking is for HTTP/1.1. A client that sends HTTP/1.0 needs a
  #cmd("Content-Length"), and a module that serves such clients sets one.

== Storage <ug-module-storage>

#idx("module", "storage")
#idx("static data", "module")
#idx("__wsaget")
#idx("subpool 5")
A module runs in the task of a worker of HTTPD, and the rules for its storage
follow from that.

#tab(caption: [The storage rules of a module])[
  #table(columns: (1.5in, 1fr),
    [*Subject*], [*Rule*],
    [Heap], [While #cmd("main") runs, the startup code points the heap of the
      module at the request subpool, subpool 5. What the module gets with
      #cmd("malloc") belongs to the request. The module still frees what it
      allocates, on every path. If the module abends, the server frees the
      whole subpool.],
    [Longer life], [Storage that must outlive the request comes from
      #cmd("__getmsp(size, 0)") or from the block of
      #cmd("http_cgictx_get"). Storage in any other subpool does not survive
      the request.],
    [No writable statics], [A module must not write to a variable at file scope
      or to a static variable of a function. A module that is fetched from an
      APF-authorized library or from the link list is in storage of key 0, while
      its code runs in key 8. The first write to the storage of the module then
      abends with S0C4. The library decides this, not the attribute of the
      module.],
    [Where to keep state], [In local variables, on the heap, or in the memory
      that #cmd("__wsaget()") returns. State of the server belongs in the block
      of #cmd("http_cgictx_get"). A table or a string that the module only
      reads may be static.],
    [Strings you change], [Declare #cmd("UCHAR text[] = \"...\"") and not
      #cmd("UCHAR *text = \"...\"") if the text is translated or changed in
      place. The array is a copy on the stack. The pointer points to the
      constant itself, and a write there is the same S0C4.],
    [Stack], [The default of the C runtime asks for 256 KB of contiguous storage
      in subpool 0 for the stack of every request. A module sets a smaller
      stack in its own source file, as shown below.],
    [Server allocations], [The functions #cmd("http_new_env"), #cmd("http_open"),
      #cmd("http_get_ufs") and #cmd("http_add_route") take their storage from
      subpool 0, inside the server. The reclaim of subpool 5 does not touch
      it.],
  )
]

A writable static is the commonest mistake. Keep it out from the start. HELLO
has no static at all.

The stack is the place where HELLO, as printed, is not what a module should be.
Put this line into the source file of the module, not into a library:

#screen(```c
unsigned __stklen = 64*1024;
```)

== Building <ug-module-build>

#idx("mbt", "server module")
#idx("mbt.toml")
A module is built on a workstation with the cc370 cross compiler and MBT. It is
not compiled on the MVS system. The project file of HELLO is this:

#code(read("../ex/ug-module/mbt.toml"))

The file says the following.

#tab(caption: [The keys of the project file])[
  #table(columns: (1.6in, 1fr),
    [*Key*], [*Meaning*],
    [#cmd("[toolchain]")], [The versions of MBT, of the compiler (cc370) and of
      the C library (libc370) that the project is built with.],
    [#cmd("[dependencies]")], [The product HTTPD, at 4.2.0 or later. MBT fetches
      the headers and the library of HTTPD from a release. The range names the
      HTTPD level that the module needs. Use the lowest level that has all the
      functions you call.],
    [#cmd("[module.HELLO]")], [A load module with the name HELLO. The name is the
      member name in the load library and the name in the #cmd("MOD=") keyword.],
    [#cmd("dep_startup = true")], [Link the module with the startup code
      #cmd("cgistart") of the HTTPD library, in place of the startup code of the
      C runtime. Without it the module has no #cmd("grtapp1") and no
      #cmd("grtapp2").],
    [#cmd("rent"), #cmd("reus")], [Mark the module reentrant and reusable. HTTPD
      holds a module in storage only if it has one of the two (see
      @ug-module-deploy), and several workers may run it at the same time.],
    [#cmd("sources")], [The source files.],
    [#cmd("[deploy]")], [The load library that #cmd("mbt deploy") writes to.],
  )
]

The headers come from the dependency. The command #cmd("mbt deps") resolves the
range, picks a release, and stages it into the directory
#cmd(".mbt/deps/httpd"), with the headers in #cmd("include") and the library in
#cmd("lib"). The following #cmd("mbt build") passes that directory to the
compiler with #cmd("-I") and links #cmd(".mbt/deps/httpd/lib/httpd.a"). The log
shows both steps:

#screen(raw(read("../ex/ug-module/build.txt")))

Note the options of the compiler: #cmd("-std=gnu99 -Wall -Wextra -Werror"). A
warning is an error. The result is the file #cmd("build/HELLO"), a load module
with the attributes RENT and REUS (the linker options #cmd("--rent --reus")).
The file #cmd("mbt.lock") records the release that was picked. Keep it in the
repository, so that the next build uses the same headers.

== Deploying and Routing <ug-module-deploy>

#idx("deploy", "server module")
#idx("STEPLIB", "server module")
#idx("RESIDENT")

=== The Library <ug-module-deploy-lib>

#cmd("mbt deploy") sends the load module to the system and writes it into the
library of the project. For HELLO this is the development library of HTTPD:

#screen(raw(read("../ex/ug-module/deploy.txt")))

The command replaces the member and does not delete the library. The library
must be in the #cmd("STEPLIB") of the HTTPD procedure, or the server does not
find the module. The development library is put first, so that a deployed level
is found before the one that SMP installed:

#screen(```
//STEPLIB  DD  DISP=SHR,DSN=HTTPD.DEV.LINKLIB
//         DD  DISP=SHR,DSN=HTTPD.LINKLIB
```)

Two rules go with it.

- Every library in an authorized #cmd("STEPLIB") must be APF authorized. A
  library that is not authorized takes the authority away from the whole task,
  and the failure that follows does not name the cause. Where HTTPD is
  authorized by its library (#cmd("HTTPD002I") says #cmd("AUTHORIZED BY
  LIBRARY")), the development library needs its own entry, beside the one of
  the SMP library. MVS 3.8j reads the list at the IPL and has no dynamic APF,
  so the entry needs an IPL (@ug-install).
- Use the development library only on a test system. It sits first in the
  concatenation, and it runs whatever was deployed last. The library
  #cmd("HTTPD.LINKLIB") belongs to SMP, and only the install job writes it.

=== The Route <ug-module-deploy-route>

The module runs only when a route names it. For HELLO the member has one route:

#screen(raw(read("../ex/ug-module/hello-route-prm.txt")))

The route has #cmd("AUTH=NONE"), so anybody who can reach the port can call it.
That suits a greeting on a test system. A module that does anything of value
gets #cmd("AUTH=BASIC") or stronger (@ug-security), and a route without the
keyword is public too. The syntax of the line is in @ug-routes.

=== The Restart <ug-module-deploy-restart>

HTTPD loads each module of its routes once, when it starts (the Parmlib keyword
#cmd("RESIDENT=YES"), the default). A new level of the module in the library has
no effect on a running server. Deploy, then restart:

#screen(```
P HTTPD
S HTTPD,M=HELLOPRM
```)

The start shows the route:

#screen(raw(read("../ex/ug-module/start-with-route.txt")))

#cmd("HTTPD036I MODULE HELLO REGISTERED FOR /hello") is the line to look for. If
the module is neither reentrant nor reusable, the server writes
#cmd("HTTPD441W") and fetches it for every request. With
#cmd("RESIDENT=NO") each request fetches the module, so a new level is used at
once. That is slower, and it is for the days when you write the module. A
module that is in no library of the #cmd("STEPLIB") makes every request to the
route answer with the status 503 and the text "External program HELLO was not
found" (@ug-module-abend).

=== What the Procedure Must Have <ug-module-deploy-proc>

A module opens files by ddname in the procedure of the started task, because it
has no allocations of its own. If your module needs a DD, the procedure must
have it, and every operator who runs HTTPD must keep it. The DDs that come with
the procedure are listed in the _HTTPD Reference_, in the chapter "The Server
Module Interface", section "The Started Task Procedure". Before you remove a DD
from the procedure, find out which modules of other products use it.

== Testing with curl <ug-module-test>

#idx("curl", "testing a module")
Test every module with #cmd("curl -i"), which shows the status line and the
headers. For each method that the module accepts, and for one that it does not,
check four things: the status, the #cmd("Content-Type"), the framing (a
#cmd("Transfer-Encoding: chunked") or a #cmd("Content-Length")), and the body.

The tests for HELLO are the screens of this chapter:

#tab(caption: [The tests of HELLO])[
  #table(columns: (2.0in, 1fr),
    [*Test*], [*What it shows*],
    [GET without a parameter], [200, chunked, the body "Hello, world!".],
    [GET with #cmd("?name=IBMUSER")], [The parameter reaches the module.],
    [POST of a form], [200, #cmd("Connection: close"), #cmd("POST_STRING") holds
      the raw body.],
    [POST of #cmd("text/plain")], [The same variable holds a text body.],
    [PUT], [405 and the header #cmd("Allow").],
    [Two GETs on one connection], [The second request re-uses the first
      connection.],
    [Time of one GET], [A fast answer. Seconds mean a fault in the end of the
      headers.],
  )
]

If the status is 503 and the text starts with "External program", the module
did not run or did not finish. The next section tells what the texts mean.

== When a Module Abends <ug-module-abend>

#idx("abend", "module")
#idx("HTTPD908E")
#idx("ESTAE")
HTTPD runs the module through a routine that sets an ESTAE. If the module
abends, the ESTAE returns control to the server, and the worker lives. It goes
on with its next request. The server then does this, in order.

+ It frees subpool 5. If the free fails, it writes #cmd("HTTPD906W").
+ It builds the text of the answer from the result of the LINK.
+ If the module has not started a response, it sends the status 503 with
  the type #cmd("text/plain"). If the module has begun the response, the text
  goes after what the module sent.
+ It writes the body "External program NAME text", and the console message
  #cmd("HTTPD908E EXTERNAL PROGRAM NAME text").
+ It ends the request.

The text says what happened.

#tab(caption: [The texts of a failed module])[
  #table(columns: (1.9in, 1fr),
    [*Text*], [*Cause*],
    [was not found], [The module is in no library of the STEPLIB or link list
      (S806).],
    [could not be loaded], [The LINK failed for another reason, for example a
      fetch error or too little storage.],
    [could not be started, ESTAE create failed], [The ESTAE could not be set.],
    [returned rc n], [The module returned the negative code n. Only a module
      with its own start routine can return it, because the start routine of
      the shipped library turns a negative code of #cmd("main") into 0.],
    [failed with Snnn ABEND], [A system abend with the code Snnn.],
    [failed with Unnnn ABEND], [A user abend with the code Unnnn, in decimal.],
  )
]

The reclaim of subpool 5 gives back what the module got with #cmd("malloc"). It
does not give back what the module holds outside subpool 5: a file that the
module opened, a UFS session that it created, or storage from other subpools. A
module that has such resources and must survive an abend needs a recovery of its
own. The chapter "The Server Module Interface" of the _HTTPD Reference_ has the
detail, and @ug-trouble covers the search for a fault.

== Long-Running Handlers <ug-module-drain>

#idx("shutdown", "module")
#idx("quiesce")
A request that takes long holds a worker. When the server stops, it sets two
flags in the flag byte of the server block, #cmd("HTTPD_FLAG_QUIESCE") and
#cmd("HTTPD_FLAG_SHUTDOWN"), and then waits up to 10 seconds, in steps of one
second, for its socket thread. The sends of the server look at the flags: they
stop and end the request. A module that loops, waits or sends in pieces must
look at the flags itself, in every round, and return when one is set:

#screen(```c
while (more_to_send) {
    if (http_get_flag(httpd) & (HTTPD_FLAG_QUIESCE | HTTPD_FLAG_SHUTDOWN))
        break;
    if (httpc->state == CSTATE_DONE)
        break;
    /* ... send the next piece ... */
}
```)

A module that does not look keeps its worker busy while the server tries to
stop (@ug-ops-stop). Both flag macros exist from HTTPD 4.0.

== HTTPD Levels and the Function Vector <ug-module-levels>

#idx("HTTPX", "append-only")
#idx("http_new_session")
#idx("HTTPD", "level")
A module calls every function of the server through the HTTPX vector. The
vector is append-only: an entry never moves and never changes its meaning, and a
new function is added at the end. This is the promise that lets a module that
was built a year ago run on a new HTTPD.

The other direction needs care. A module that was built against a newer header
and runs on an older server calls an entry that the older server does not have.
No function tells a module which entries exist, and there is no run-time test
for an entry. The one exception is #cmd("http_cgi_subpool"), which a module can
test because an older server holds 0 in that entry. The rule for the author is
therefore plain: name the lowest HTTPD level of the module in its own
documentation, and put the same level in the range of the dependency in
#cmd("mbt.toml").

The function #cmd("http_new_session") is a good example. It is the last entry of
the vector, at offset X'144', and it exists from HTTPD 4.2:

#screen(```c
int http_new_session(HTTPC *httpc);
```)

It turns the login of the current request into a session of its own, with its
own credential and its own token, which #cmd("http_get_token") returns from then
on. It is for an explicit login endpoint of a module. Each call costs a RACF
logon and a place in the credential store, and a client holds at most 16 such
sessions.

There is a second level. From HTTPD 4.5 the function tells why it failed. Before
that release every failure returned -1. The results are:

#tab(caption: [The results of http_new_session])[
  #table(columns: (2.1in, 0.5in, 1fr),
    [*Constant*], [*Value*], [*Meaning*],
    [], [0], [The session exists.],
    [#cmd("HTTP_SESSION_NONE")], [-1], [The request is not authenticated, or
      carries no password to log on again with.],
    [#cmd("HTTP_SESSION_REFUSED")], [-2], [RACF refused the logon. The module
      answers 401.],
    [#cmd("HTTP_SESSION_BUSY")], [-3], [The client holds 16 sessions that are in
      use, or the credential store is full. Retrying later may work. The module
      answers 503.],
    [#cmd("HTTP_SESSION_NOMEM")], [-4], [No storage, or RACF could not be asked
      for a reason that is not about the password. A fault of the server. The
      module answers 500.],
  )
]

A module that must run on both generations of server tests the result with
#cmd("!= 0") or #cmd("< 0"). It never tests with #cmd("== -1"): on a new server
that reads -2, -3 and -4 as success, and the module then goes on to
#cmd("http_get_token"), which hands out the token of the shared credential that
the call exists to avoid. It treats -1 as a failure of unknown reason, because
an old server returns -1 for every failure.

#screen(```c
rc = http_new_session(httpc);
if (rc == 0) {
    /* the session exists: http_get_token() returns its token.
       If the module cannot send the token, it calls http_logout(httpc)
       before it returns, or the session keeps its slot until it times out. */
}
else if (rc == HTTP_SESSION_REFUSED) {
    http_resp(httpc, 401);          /* wrong or revoked password */
}
else if (rc == HTTP_SESSION_BUSY) {
    http_resp(httpc, 503);          /* try again later */
}
else {
    http_resp(httpc, 500);          /* -1, -4, or a value not yet known */
}
```)

After -2 the credential of the request is marked unusable. The current request
still runs on it and must answer the refusal. The constants are in the header of
a release that has them: a module that mentions #cmd("HTTP_SESSION_REFUSED")
must be built with the headers of HTTPD 4.5 or later, and that belongs in the
documentation of the module too. The details are in the chapter "The Server
Module Interface" of the _HTTPD Reference_.

== Things to Know <ug-module-know>

- *The entry is main.* There is no #cmd("cgimain"). The blocks come from the
  global runtime table.
- *End the headers with a call of its own:* #cmd("http_printf(httpc, \"\\r\\n\")").
  A blank line inside a longer text is not recognized, and the client waits for
  the end of the connection.
- *No writable statics.* Put state in locals, on the heap, or in
  #cmd("__wsaget()"). Never change a string constant.
- *Set the stack:* #cmd("unsigned __stklen = 64*1024;") in the source file of the
  module.
- *A new level needs a restart,* unless the server runs with
  #cmd("RESIDENT=NO").
- *Every library of an authorized STEPLIB is APF authorized.* The development
  library is for a test system.
- *Look at the flags* in a handler that runs long.
- *Name the lowest HTTPD level* that the module needs.
