#import "../bookmaster/bookmaster.typ": *

= The Server Module Interface <ref-api>

#idx("server module")
#idx("module", "interface")
#idx("HTTPX")
#idx("HTTPC")
#idx("HTTPD", "control block")
#idx("httpcgi.h")
A server module is a load module that HTTPD runs for a route. The route names
the module with the keyword #cmd("MOD="), as described in @ref-route. The
module receives the request, writes the response and returns. HTTPD itself
needs no more from it.

This chapter is the contract between a module and the server. It describes how
HTTPD starts a module and what the module finds when it runs, the control
blocks that it may read, every entry of the function vector (HTTPX), the
variables of a request, the storage rules, what happens when a module abends,
and what the started task needs for a module to work. The header
#cmd("httpcgi.h") declares everything in this chapter. A module includes it and
nothing else of HTTPD.

The _HTTPD User's Guide_ shows how to build and install a module, in "Writing a
Server Module". The four modules that come with HTTPD are in @ref-dm.

== How a Module Is Run <ref-api-run>

#idx("server module", "LINK")
#idx("server module", "entry")
#idx("cgistart")
#idx("argv", "module")
HTTPD runs a module with the LINK SVC, in the task of the worker that serves
the request. It runs the module once per request, whatever the method is: GET,
HEAD, PUT, POST and DELETE all arrive at the module in the same way. The
module has to complete the response before it returns. When it returns, HTTPD
ends the request, whatever the return code is.

The LINK is made through the C library routine #cmd("__linkdsx"). The routine
sets an ESTAE first, so that an abend of the module comes back to the server
as a result and does not end the worker (see @ref-api-abend). The module name
is the name in the route. If the server holds the module in storage, the LINK
finds that copy (see @ref-api-resident).

Before the module runs, HTTPD has read the request, parsed it into variables
(see @ref-api-env), looked for a credential and found the route. It then applies
the authentication rule of the route. A request that fails the rule is answered
with 401 or 403 by the server and never reaches the module. HTTPD itself
answers the paths #cmd("/login") and #cmd("/logout"). Only after these steps
does the server run the module of a route that has one (#cmd("MOD=")). A route
without a module (#cmd("LOC=")) serves static files.

=== The Parameter List <ref-api-plist>

HTTPD builds a parameter list of three words for the module.

#tab(caption: [The parameter list of a module])[#table(
  columns: (0.8in, 1.6in, 1fr),
  [*Word*], [*Content*], [*Meaning*],
  [0], [address of a parameter], [A halfword length and the text of the parameter: the request path in double quotes, for example #cmd("\"/zosmf/info\""). The text has at most 254 characters, the quotes included; a longer path is cut.],
  [1], [address of HTTPD], [The server control block, with the eye catcher #cmd("*HTTPD*").],
  [2], [address of HTTPC], [The control block of the client, with the eye catcher #cmd("*HTTPC*"). The high bit is set to mark the end of the list.],
)]

A module does not read this list. The startup code that every module is linked
with, #cmd("cgistart"), reads it.

=== What the Startup Code Does <ref-api-start>

#cmd("cgistart") supplies the entry point #cmd("__start") of the module. The
module is linked with it; it comes from the CGI library of HTTPD. In this
order it does the following.

+ It scans the list for the two eye catchers and stores the address of HTTPD
  and the address of HTTPC in the global runtime table (GRT) of the task, in
  the fields #cmd("grtapp1") and #cmd("grtapp2").
+ If the module is run outside HTTPD, it stops here. It writes "is an HTTPD
  server module and cannot run on its own" and ends with the exit status
  #cmd("EXIT_FAILURE"). The display modules of @ref-dm write the message
  #cmd("HTTPD920E") and return 12 instead.
+ It opens #cmd("stdout") on the DD #cmd("HTTPDOUT"), #cmd("stderr") on the DD
  #cmd("HTTPDERR") and #cmd("stdin") on the DD #cmd("HTTPDIN"). If #cmd("HTTPDIN") is
  missing, it opens #cmd("'NULLFILE'") for #cmd("stdin"). The started task
  procedure has the three DDs (see @ref-api-proc).
+ It loads environment variables from the DD #cmd("SYSENV"). If there is no such
  DD, it tries the DD #cmd("ENVIRON").
+ It calls #cmd("tzset()").
+ It builds #cmd("argv") from the parameter of word 0 and calls
  #cmd("main(argc, argv)"). Around the call it switches the heap of the module
  to the request subpool (see @ref-api-storage).

The module finds in #cmd("main"):

#deflist(width: 1.35in,
  [#cmd("argc")], [2.],
  [#cmd("argv[0]")], [The name of the module, up to 8 characters.],
  [#cmd("argv[1]")], [The decoded #cmd("REQUEST_PATH"), without the quotes.],
  [HTTPD, HTTPC], [Read #cmd("grt->grtapp1") and #cmd("grt->grtapp2"). The server has the functions #cmd("cgihttpd()") and #cmd("cgihttpc()") for this lookup, but they are not part of the client library, and a module cannot link them.],
  [HTTPX], [Call #cmd("http_get_httpx(httpd)").],
)

#cmd("printf") needs a note. The startup code contains its own #cmd("printf"),
and it takes the place of the library function in a module. When the module
runs under HTTPD, #cmd("printf") sends its output as part of the response, like
#cmd("http_printv"). It does not write to #cmd("stdout"). A call of
#cmd("fprintf(stdout, ...)") writes to #cmd("HTTPDOUT").

The module must have a C stack that is smaller than the default. The default
of the runtime asks for 256 KB of contiguous storage in subpool 0 for every
request. The modules of HTTPD set a smaller one in their own source file:

#screen(```c
unsigned __stklen = 64*1024;
```)

Put this variable in the source file of the module and not in a library.

=== The Return Code <ref-api-rc>

#idx("server module", "return code")
When #cmd("main") returns, the startup code clamps a negative value to 0 and
exits with the value. HTTPD takes the return code in this way:

- A value of 0 or more is not an error. The server does not look at it.
- An abend, a module that cannot be found or loaded, and a few other
  failures are errors. The server answers them as described in
  @ref-api-abend.

After the module has returned, the server sets the state of the client to
#cmd("CSTATE_DONE"). It closes the files, writes the statistics (see
@ref-smf) and then resets the client or closes the connection. The module does
not call any of these steps.

=== A Minimal Module <ref-api-skel>

This module answers every request with a line of text.

#screen(```c
#include <stdio.h>
#include <mvs/crt.h>
#include "httpcgi.h"

unsigned __stklen = 64*1024;

int main(int argc, char **argv)
{
    CLIBGRT *grt   = __grtget();
    HTTPD   *httpd = grt->grtapp1;
    HTTPC   *httpc = grt->grtapp2;
    HTTPX   *httpx = http_get_httpx(httpd);

    http_resp(httpc, 200);
    http_printf(httpc, "Content-Type: text/plain\r\n");
    http_printf(httpc, "\r\n");
    http_printf(httpc, "Hello from %s\n", argv[1]);
    return 0;
}
```)

The macros of #cmd("httpcgi.h") expand into a call through the local variable
#cmd("httpx"). The variable must be in scope where a macro is used. The
modules in the HTTPD source that include #cmd("httpd.h") instead define
#cmd("httpx") as #cmd("(httpd->httpx)").

== The Structures <ref-api-struct>

#idx("HTTPC", "fields")
#idx("HTTPV")
#idx("HTTPM")
#idx("CSTATE")
#idx("RDW")
The header #cmd("httpcgi.h") defines the structures that a module may look
into: HTTPC, HTTPV and HTTPM, and the enumerations CSTATE and RDW. It leaves
the others opaque: HTTPD, CRED, UFS, UFSFILE, CIB and HTTPROUTE. A module holds
pointers to them and passes them to the vector functions. It does not look
inside them. The two accessors that read HTTPD are in @ref-api-direct.

=== HTTPC, the Client <ref-api-httpc>

HTTPC is 4096 bytes long. HTTPD allocates it when it accepts a connection and
frees it when the connection ends. The structure belongs to the server. A
module reads the fields and lets the vector functions change them.

#tab(caption: [The fields of HTTPC])[#table(
  columns: (0.55in, 1.45in, 1.25in, 1fr),
  [*Offset*], [*Name*], [*Type*], [*Meaning*],
  [00], [#cmd("eye")], [char\[8\]], [#cmd("*HTTPC*").],
  [08], [#cmd("httpd")], [#cmd("HTTPD *")], [The server.],
  [0C], [#cmd("env")], [#cmd("HTTPV **")], [The array of request variables. Use the variable functions of @ref-api-envfn and do not walk the array.],
  [10], [#cmd("state")], [short], [The state of the request (see below).],
  [12], [#cmd("subtype")], [UCHAR], [A state of the server's own file transfer. Do not use it.],
  [13], [#cmd("substate")], [UCHAR], [As #cmd("subtype").],
  [14], [#cmd("socket")], [int], [The socket of the client.],
  [18], [#cmd("addr")], [unsigned], [The IP address of the client. #cmd("http_ntoa") converts it.],
  [1C], [#cmd("port")], [int], [The port of the client.],
  [20], [#cmd("fp")], [#cmd("FILE *")], [The file of the response, if the server opened one. #cmd("http_done") closes it.],
  [24], [#cmd("len")], [unsigned], [The length of data in #cmd("buf").],
  [28], [#cmd("pos")], [unsigned], [The position of the next unsent byte in #cmd("buf").],
  [2C], [#cmd("sent")], [unsigned], [The bytes sent to the client for this request. It counts every raw byte, the status line, the headers and the chunk framing included.],
  [30], [#cmd("start")], [double], [The start time of the request in seconds.],
  [38], [#cmd("end")], [double], [The end time of the request in seconds.],
  [40], [#cmd("rdw")], [UCHAR], [A value of RDW. #cmd("http_printf") also uses this byte as the flag "the headers have ended" (see @ref-api-out).],
  [41], [#cmd("chunked")], [UCHAR], [1 if the response is sent in chunks.],
  [42], [#cmd("resp")], [short], [The status code. It is 0 until #cmd("http_resp") has been called.],
  [44], [#cmd("cred")], [#cmd("CRED *")], [The credential of the client, or NULL. A module does not use it directly. Use the functions of @ref-api-auth.],
  [48], [#cmd("ufs")], [#cmd("UFS *")], [The UFS session of the client, or NULL. #cmd("http_get_ufs") creates it.],
  [4C], [#cmd("ufp")], [#cmd("UFSFILE *")], [The UFS file that #cmd("http_open") opened, or NULL.],
  [50], [#cmd("ssi")], [UCHAR], [1 if the file is processed for server side includes.],
  [51], [#cmd("ssilevel")], [UCHAR], [The nesting level of the includes. The maximum is 10.],
  [52], [#cmd("content_length_set")], [UCHAR], [1 if a #cmd("Content-Length") header was sent.],
  [53], [#cmd("keepalive")], [UCHAR], [1 if the connection stays open after this response.],
  [54], [#cmd("connect_time")], [unsigned], [The time of the connect in 1/100 s (SMF time).],
  [58], [#cmd("total_bytes_sent")], [unsigned], [The bytes sent on this connection, over all requests.],
  [5C], [#cmd("request_count")], [unsigned], [The requests answered on this connection.],
  [60], [#cmd("buf")], [UCHAR\[4000\]], [The work buffer of the server for input and file transfer. A module does not keep data in it.],
)]

The header comments for #cmd("subtype") and #cmd("substate") give the offsets
11 and 12. The structure puts them at 12 and 13, because #cmd("state") is two
bytes long.

The field #cmd("state") holds a value of CSTATE:

#tab(caption: [The values of CSTATE])[#table(
  columns: (1.5in, 0.5in, 1fr),
  [*Name*], [*Value*], [*Meaning*],
  [#cmd("CSTATE_IN")], [0], [HTTPD reads the request.],
  [#cmd("CSTATE_PARSE")], [1], [HTTPD parses the request.],
  [#cmd("CSTATE_GET")], [2], [The method is GET.],
  [#cmd("CSTATE_HEAD")], [3], [The method is HEAD.],
  [#cmd("CSTATE_PUT")], [4], [The method is PUT.],
  [#cmd("CSTATE_POST")], [5], [The method is POST.],
  [#cmd("CSTATE_DELETE")], [6], [The method is DELETE.],
  [#cmd("CSTATE_DONE")], [10], [The response is complete. No more output is accepted.],
  [#cmd("CSTATE_REPORT")], [11], [HTTPD writes the statistics.],
  [#cmd("CSTATE_RESET")], [12], [HTTPD resets the client.],
  [#cmd("CSTATE_CLOSE")], [13], [HTTPD closes the connection.],
)]

A module runs in one of the states 2 to 6. It reads the state to see the
method. The state becomes #cmd("CSTATE_DONE") when the module returns, and
also when a send fails because the client has gone. After that
#cmd("http_printf"), #cmd("http_printv") and #cmd("http_send") return -1. A
module that loops over a long response tests the state or the return codes
and stops.

The values of RDW are #cmd("RDW_NONE") (0), #cmd("RDW_B2"), #cmd("RDW_L2"),
#cmd("RDW_B4") and #cmd("RDW_L4"). The server sets #cmd("rdw") to 1 itself
when the headers end (see @ref-api-out).

=== HTTPV, a Variable <ref-api-httpv>

A request variable is a block of this form.

#tab(caption: [The fields of HTTPV])[#table(
  columns: (0.55in, 1.0in, 1.1in, 1fr),
  [*Offset*], [*Name*], [*Type*], [*Meaning*],
  [00], [#cmd("eye")], [char\[8\]], [#cmd("*HTTPV*").],
  [08], [#cmd("value")], [#cmd("char *")], [The value. It points into the same block.],
  [0C], [#cmd("name")], [char\[2\]], [The name, a zero byte, the value and a zero byte, one after the other.],
)]

=== HTTPM, a MIME Type <ref-api-httpm>

#cmd("http_mime") returns a pointer to this structure of 12 bytes.

#tab(caption: [The fields of HTTPM])[#table(
  columns: (0.55in, 1.0in, 1.1in, 1fr),
  [*Offset*], [*Name*], [*Type*], [*Meaning*],
  [00], [#cmd("ext")], [#cmd("UCHAR *")], [The file extension.],
  [04], [#cmd("type")], [#cmd("UCHAR *")], [The MIME type, for example #cmd("text/html").],
  [08], [#cmd("binary")], [int], [1 if the file is sent as binary, 0 if it is text.],
)]

== The Request Variables <ref-api-env>

#idx("request variables")
#idx("environment variables", "request")
#idx("QUERY_STRING")
#idx("REQUEST_PATH")
#idx("POST_STRING")
Before the module runs, HTTPD has read the request line and the headers and has
stored them as variables of the client. A module reads them with
#cmd("http_get_env"). The names are not case sensitive. All values are in
EBCDIC.

#tab(caption: [The request variables])[#table(
  columns: (1.9in, 1fr),
  [*Name*], [*Value*],
  [#cmd("HTTP_REQUEST")], [The request line as it was sent.],
  [#cmd("REQUEST_METHOD")], [The method.],
  [#cmd("REQUEST_URI")], [The URI as it was sent, with the query.],
  [#cmd("REQUEST_VERSION")], [The version of the request: #cmd("HTTP/1.0") or #cmd("HTTP/1.1"). HTTPD rejects any other with the status 505.],
  [#cmd("REQUEST_PATH")], [The path of the URI, decoded, without the query.],
  [#cmd("QUERY_STRING")], [The text after the question mark of the URI, not decoded. The variable exists only if the URI has a query.],
  [#cmd("QUERY_name")], [One variable for each parameter of the query. The value is decoded, the name is not. A parameter without an equals sign has the value "".],
  [#cmd("HTTP_name")], [One variable for each header of the request, for example #cmd("HTTP_Host") or #cmd("HTTP_Content-Type"). The name is as the client sent it. The value follows the colon, without leading blanks. The header #cmd("Cookie") makes no #cmd("HTTP_Cookie"); see the next row.],
  [#cmd("HTTP_Cookie-name")], [One variable for each cookie. A cookie without an equals sign has the value "".],
  [#cmd("POST_STRING")], [The body of a POST request (see below).],
  [#cmd("POST_name")], [One variable for each field of a form body (see below).],
  [#cmd("SERVER_ADDR")], [The address of the server end of the connection.],
  [#cmd("SERVER_PORT")], [The port of the server end of the connection.],
  [#cmd("SERVER_PROTOCOL")], [Always #cmd("HTTP/1.1"), also for an HTTP/1.0 request.],
  [#cmd("SERVER_SOFTWARE")], [#cmd("HTTPD/") and the version of the server.],
  [#cmd("SCRIPT_FILENAME")], [The document root and the request path. HTTPD sets it only for a route whose path begins with #cmd("*."), for example #cmd("*.lua"). If it cannot set the variable, the request fails with the status 500 and the module does not run.],
)]

A request with more than 256 parameters in the query or in a form body gets
the status 400 and the connection is closed. The request line must fit in the
buffer, else the status is 414. A header with a name that has a character other
than a token character, or a value with a control character, gets 400. So does
a request with two #cmd("Host") headers. A request with a malformed
#cmd("Content-Length") gets 400. HTTP/1.1 requests without a #cmd("Host")
header get 400 too. A method other than GET, HEAD, PUT, POST and DELETE gets
405. A request with #cmd("Expect: 100-continue") is answered by the server
(HTTP/1.1 only). None of these requests reaches the module.

The body of a request is treated by the method.

#deflist(width: 1.35in,
  [POST], [HTTPD reads the body with one #cmd("recv") if the header #cmd("Content-Length") is present and the length is between 1 and 3999. It converts the body to EBCDIC with #cmd("http_atoe") and stores it in #cmd("POST_STRING"). If the header #cmd("Content-Type") is exactly #cmd("application/x-www-form-urlencoded"), in any case, it also splits the body into the fields #cmd("POST_name") and decodes the values. A body of 4000 bytes or more is not read: the status is 413 and the connection is closed. A request without a body, or with a chunked body, has no #cmd("POST_STRING"). A request with a body and no #cmd("Content-Type") is not read either.],
  [PUT], [HTTPD does not read the body. The module reads it from the socket in #cmd("httpc->socket").],
  [DELETE, GET, HEAD], [HTTPD does not read a body.],
)

Every request that has a #cmd("Content-Length") or a #cmd("Transfer-Encoding")
header ends the connection after the response. HTTPD turns keep-alive off
because it cannot be sure that it has read the whole body.

== The Function Vector <ref-api-vector>

#idx("HTTPX", "function vector")
#idx("function vector")
#idx("append-only rule")
A module calls the server through the function vector HTTPX. The vector is a
structure of function pointers. HTTPD fills it once, when it starts. The
address of the structure is in HTTPD at offset 8, and
#cmd("http_get_httpx(httpd)") returns it. The vector is 328 bytes (X'148')
long.

=== The Rules <ref-api-vrules>

#idx("HTTPX", "append-only")
#idx("HTTPX", "levels")
The vector is append-only. An entry never moves and never changes its meaning.
A new function is added at the end. Two consequences follow for a module.

- A module that was built against an older header runs on a newer server.
- A module that was built against a newer header can run on an older server,
  but an entry that the older server does not have holds 0 there, or lies
  beyond the end of the vector. There is no function that tells a module which
  entries exist. The only entry that a module can test before it calls is
  #cmd("http_cgi_subpool"): it is 0 on a server that is too old to have the
  request subpool. Name the lowest HTTPD level of the module in its own
  documentation.

The tables below give the level in which each entry first appears. The level
"4.0" means that the entry is in HTTPD 4.0.0 and every later release. The
layouts of earlier releases, HTTPD 3.3 included, are not covered here.

The header declares a macro for every entry. The macro has the name of the
entry and expands into a call through the variable #cmd("httpx"). The names
#cmd("http_find_cgi"), #cmd("http_add_cgi") and #cmd("http_process_cgi") are the
earlier names of #cmd("http_find_route"), #cmd("http_add_route") and
#cmd("http_process_route"). The header keeps them. The entries did not move.

The entries 0x10C and #cmd("http_read") (0xD4) hold 0 in every release. Do not
call them.

=== The Entries <ref-api-ventries>

#tab(caption: [The entries of the function vector])[#table(
  columns: (0.6in, 2.3in, 0.55in, 1fr),
  [*Offset*], [*Entry*], [*Level*], [*Described in*],
  [00], [#cmd("eye")], [4.0], [@ref-api-direct],
  [08], [#cmd("httpd")], [4.0], [@ref-api-direct],
  [0C], [#cmd("http_cgi_subpool")], [4.0], [@ref-api-storage],
  [10], [#cmd("http_in")], [4.0], [@ref-api-internal],
  [14], [#cmd("http_parse")], [4.0], [@ref-api-internal],
  [18], [#cmd("http_get")], [4.0], [@ref-api-internal],
  [1C], [#cmd("http_head")], [4.0], [@ref-api-internal],
  [20], [#cmd("http_put")], [4.0], [@ref-api-internal],
  [24], [#cmd("http_post")], [4.0], [@ref-api-internal],
  [28], [#cmd("http_done")], [4.0], [@ref-api-internal],
  [2C], [#cmd("http_report")], [4.0], [@ref-api-internal],
  [30], [#cmd("http_reset")], [4.0], [@ref-api-internal],
  [34], [#cmd("http_close")], [4.0], [@ref-api-internal],
  [38], [#cmd("http_send")], [4.0], [@ref-api-out],
  [3C], [#cmd("http_decode")], [4.0], [@ref-api-envfn],
  [40], [#cmd("http_del_env")], [4.0], [@ref-api-envfn],
  [44], [#cmd("http_find_env")], [4.0], [@ref-api-envfn],
  [48], [#cmd("http_set_env")], [4.0], [@ref-api-envfn],
  [4C], [#cmd("http_new_env")], [4.0], [@ref-api-envfn],
  [50], [#cmd("http_set_http_env")], [4.0], [@ref-api-envfn],
  [54], [#cmd("http_set_query_env")], [4.0], [@ref-api-envfn],
  [58], [#cmd("http_get_env")], [4.0], [@ref-api-envfn],
  [5C], [#cmd("http_date_rfc1123")], [4.0], [@ref-api-misc],
  [60], [#cmd("http_printv")], [4.0], [@ref-api-out],
  [64], [#cmd("http_printf")], [4.0], [@ref-api-out],
  [68], [#cmd("http_resp")], [4.0], [@ref-api-out],
  [6C], [#cmd("http_server_name")], [4.0], [@ref-api-out],
  [70], [#cmd("http_resp_not_implemented")], [4.0], [@ref-api-err],
  [74], [#cmd("http_etoa")], [4.0], [@ref-api-xlate],
  [78], [#cmd("http_atoe")], [4.0], [@ref-api-xlate],
  [7C], [#cmd("array_new")], [4.0], [@ref-api-array],
  [80], [#cmd("array_add")], [4.0], [@ref-api-array],
  [84], [#cmd("array_addf")], [4.0], [@ref-api-array],
  [88], [#cmd("array_count")], [4.0], [@ref-api-array],
  [8C], [#cmd("array_free")], [4.0], [@ref-api-array],
  [90], [#cmd("array_get")], [4.0], [@ref-api-array],
  [94], [#cmd("array_size")], [4.0], [@ref-api-array],
  [98], [#cmd("array_del")], [4.0], [@ref-api-array],
  [9C], [#cmd("http_cmp")], [4.0], [@ref-api-cmp],
  [A0], [#cmd("http_cmpn")], [4.0], [@ref-api-cmp],
  [A4], [#cmd("http_dbgw")], [4.0], [@ref-api-dbg],
  [A8], [#cmd("http_dbgs")], [4.0], [@ref-api-dbg],
  [AC], [#cmd("http_dbgf")], [4.0], [@ref-api-dbg],
  [B0], [#cmd("http_dump")], [4.0], [@ref-api-dbg],
  [B4], [#cmd("http_enter")], [4.0], [@ref-api-dbg],
  [B8], [#cmd("http_exit")], [4.0], [@ref-api-dbg],
  [BC], [#cmd("http_secs")], [4.0], [@ref-api-misc],
  [C0], [#cmd("http_mime")], [4.0], [@ref-api-misc],
  [C4], [#cmd("http_resp_internal_error")], [4.0], [@ref-api-err],
  [C8], [#cmd("http_resp_not_found")], [4.0], [@ref-api-err],
  [CC], [#cmd("http_open")], [4.0], [@ref-api-file],
  [D0], [#cmd("http_send_binary")], [4.0], [@ref-api-file],
  [D4], [#cmd("http_read") (always 0)], [-], [@ref-api-vrules],
  [D8], [#cmd("http_send_file")], [4.0], [@ref-api-file],
  [DC], [#cmd("http_send_text")], [4.0], [@ref-api-file],
  [E0], [#cmd("http_is_busy")], [4.0], [@ref-api-internal],
  [E4], [#cmd("http_set_busy")], [4.0], [@ref-api-internal],
  [E8], [#cmd("http_reset_busy")], [4.0], [@ref-api-internal],
  [EC], [#cmd("http_process_clients")], [4.0], [@ref-api-internal],
  [F0], [#cmd("http_ntoa")], [4.0], [@ref-api-misc],
  [F4], [#cmd("http_console")], [4.0], [@ref-api-internal],
  [F8], [#cmd("http_process_client")], [4.0], [@ref-api-internal],
  [FC], [#cmd("http_link")], [4.0], [@ref-api-internal],
  [100], [#cmd("http_find_route")], [4.0], [@ref-api-routefn],
  [104], [#cmd("http_add_route")], [4.0], [@ref-api-routefn],
  [108], [#cmd("http_process_route")], [4.0], [@ref-api-internal],
  [10C], [unused (always 0)], [-], [@ref-api-vrules],
  [110], [#cmd("http_xlate")], [4.0], [@ref-api-xlate],
  [114], [#cmd("xlate_cp037")], [4.0], [@ref-api-xlate],
  [118], [#cmd("xlate_1047")], [4.0], [@ref-api-xlate],
  [11C], [#cmd("xlate_legacy")], [4.0], [@ref-api-xlate],
  [120], [#cmd("http_get_ufs")], [4.0], [@ref-api-file],
  [124], [#cmd("http_cgictx_get")], [4.0], [@ref-api-ctxfn],
  [128], [#cmd("http_get_userid")], [4.0], [@ref-api-auth],
  [12C], [#cmd("http_get_acee")], [4.0], [@ref-api-auth],
  [130], [#cmd("http_get_token")], [4.0], [@ref-api-auth],
  [134], [#cmd("http_check_auth")], [4.0], [@ref-api-auth],
  [138], [#cmd("http_logout")], [4.0], [@ref-api-auth],
  [13C], [#cmd("http_get_password")], [4.0], [@ref-api-auth],
  [140], [#cmd("http_realm")], [4.0], [@ref-api-auth],
  [144], [#cmd("http_new_session")], [4.2], [@ref-api-session],
)]

The following sections describe the entries by group. A group begins with the
declarations of its functions as the macros take them. The sections list
return values and constraints for each function.

=== Output <ref-api-out>

#idx("http_resp")
#idx("http_printf")
#idx("http_send")
#idx("response", "headers")
#idx("response", "chunked")
#screen(```c
int http_resp(HTTPC *httpc, int code);
int http_printf(HTTPC *httpc, const char *fmt, ...);
int http_printv(HTTPC *httpc, const char *fmt, va_list args);
int http_send(HTTPC *httpc, const UCHAR *buf, int len);
UCHAR *http_server_name(HTTPD *httpd);
```)

A response has four parts, in this order:

+ #cmd("http_resp(httpc, code)") for the status line and the standard headers.
+ Zero or more header lines, each with #cmd("http_printf").
+ One call with the text #cmd("\"\\r\\n\"") and nothing else. It ends the
  headers.
+ The body, with #cmd("http_printf") or #cmd("http_send").

#deflist(width: 1.35in,
  [#cmd("http_resp")], [Sets #cmd("httpc->resp") to the code and sends the status line. The line has the version of the request, HTTP/1.1 or HTTP/1.0. The function also sends the headers #cmd("Date"), #cmd("Server: HTTPD Server"), #cmd("X-Powered-By: MVSLOVERS") and #cmd("Connection"), which is #cmd("keep-alive") or #cmd("close"). It does not end the headers. It returns 0, or a value other than 0 if the send fails. The codes that HTTPD knows are 200, 201, 202, 204, 206, 301, 302, 303, 304, 400, 401, 403, 404, 405, 409, 410, 412, 413, 414, 429, 500, 501, 502, 503, 505 and 507. For any other code it writes the console message #cmd("HTTPD909E") and sends 500.],
  [#cmd("http_printf"), #cmd("http_printv")], [Format the text in a buffer of 5120 bytes, convert it to ASCII and send it. Text that is longer than 5119 bytes is cut. They return 0 on success and -1 on failure. They do not return a count of bytes. They return -1 at once if the state of the client is #cmd("CSTATE_DONE") or later. The two functions are the same: #cmd("http_printv") takes a #cmd("va_list").],
  [#cmd("http_send"), raw bytes], [Sends the bytes of the buffer as they are. The caller provides ASCII or binary data. The function does not look at the data. If the response is not chunked, it returns the number of bytes sent, which can be less than #cmd("len"). It returns 0 if the socket is not ready and -1 if the connection is dead. In the second case it sets the state to #cmd("CSTATE_DONE"). If the response is chunked, it sends the buffer as one chunk and returns #cmd("len"), or -1 after a failure. A failure turns chunking and keep-alive off. A call with #cmd("len") 0 or less returns 0.],
  [#cmd("http_server_name")], [Returns the string #cmd("HTTPD Server"). The argument is not used.],
)

*The end of the headers.* #cmd("http_printf") watches for the call that ends the
headers. This call has the text #cmd("\"\\r\\n\"") alone, exactly two
characters. When it sees the call, it sets #cmd("httpc->rdw") to 1. If the
request is HTTP/1.1, the status is not 1xx, 204 or 304, and no
#cmd("Content-Length") header has been sent, it sends
#cmd("Transfer-Encoding: chunked") first and turns chunking on. From then on
every send is a chunk, and the server sends the final empty chunk when the
request is done.

Two consequences follow.

- Send the end of the headers as a call of its own. Do not write
  #cmd("\"...\\r\\n\\r\\n\""). HTTPD does not recognize the end of the headers in
  such a call. The response is then not chunked, and the client does not know
  where the body ends. A later call with exactly #cmd("\"\\r\\n\"") in the body
  would be taken for the end of the headers and would add a header line to the
  body.
- A header line that goes out with #cmd("http_send") is not examined. HTTPD
  does not see a #cmd("Content-Length") in it.

A line that begins with #cmd("Content-Length:") in a call of
#cmd("http_printf") sets #cmd("content_length_set"). Chunking is for HTTP/1.1
only. For an HTTP/1.0 request send a #cmd("Content-Length").

If a send makes no progress, the server retries it, with a short pause between
the tries. It gives up when the server is quiescing or shutting down, or when
the retries have used their budget. It then writes a message to the console
(throttled) and sets the state to #cmd("CSTATE_DONE").

=== Standard Error Responses <ref-api-err>

#idx("http_resp_not_found")
#idx("http_resp_internal_error")
#idx("http_resp_not_implemented")
#screen(```c
int http_resp_not_found(HTTPC *httpc, const UCHAR *path);
int http_resp_internal_error(HTTPC *httpc);
int http_resp_not_implemented(HTTPC *httpc);
```)

Each of the three functions sends a complete response, with the end of the
headers, and returns 0, or a value other than 0 if the send fails. They do not
set the state to #cmd("CSTATE_DONE"). Return from the module afterwards.

#tab(caption: [The standard error responses])[#table(
  columns: (2.0in, 0.5in, 1fr),
  [*Function*], [*Status*], [*Body*],
  [#cmd("http_resp_not_found")], [404], [#cmd("404 document \"path\" not found"). The headers are #cmd("Cache-Control: no-store") and #cmd("Content-Type: text/plain"). If #cmd("path") is NULL, the body has no text.],
  [#cmd("http_resp_internal_error")], [500], [#cmd("500 Internal Server Error"), as #cmd("text/plain").],
  [#cmd("http_resp_not_implemented")], [501], [#cmd("501 Not Implemented"), as #cmd("text/plain"), with #cmd("Cache-Control: no-store").],
)]

=== Request Variables <ref-api-envfn>

#idx("http_get_env")
#idx("http_set_env")
#idx("http_find_env")
#idx("http_decode")
#screen(```c
UCHAR    *http_get_env(HTTPC *httpc, const UCHAR *name);
int       http_set_env(HTTPC *httpc, const UCHAR *name, const UCHAR *value);
unsigned  http_find_env(HTTPC *httpc, const UCHAR *name);
int       http_del_env(HTTPC *httpc, const UCHAR *name);
HTTPV    *http_new_env(const UCHAR *name, const UCHAR *value);
int       http_set_http_env(HTTPC *httpc, const UCHAR *name, const UCHAR *value);
int       http_set_query_env(HTTPC *httpc, const UCHAR *name, const UCHAR *value);
UCHAR    *http_decode(UCHAR *str);
```)

The names are compared without regard to case.

#deflist(width: 1.35in,
  [#cmd("http_get_env")], [Returns the value of the variable, or NULL if there is none. The pointer is valid until the variable is changed or deleted, or until the request ends.],
  [#cmd("http_set_env")], [Creates the variable, or replaces the value of an existing one. Returns 0, or -1 on failure. A failure also writes the message #cmd("HTTPD904E") or #cmd("HTTPD905E").],
  [#cmd("http_find_env")], [Returns the index of the variable plus one, or 0 if there is none.],
  [#cmd("http_del_env")], [Deletes the variable and always returns 0.],
  [#cmd("http_new_env")], [Builds a variable block and returns it, or NULL. The sum of name and value must not exceed 8192 bytes, else the function fails with #cmd("errno") E2BIG. It also returns NULL when it has no storage. The block comes from subpool 0. The function does not add it to the client. The other functions do that.],
  [#cmd("http_set_http_env")], [Stores the value as #cmd("HTTP_name"). The server uses it for the headers. A header called #cmd("Cookie") is split into the variables #cmd("HTTP_Cookie-name"). The name must fit in 256 bytes.],
  [#cmd("http_set_query_env")], [Stores the value as #cmd("QUERY_name"). The name must fit in 256 bytes.],
  [#cmd("http_decode")], [Decodes a URL-encoded string in place and returns it. A sequence #cmd("%xx") becomes the character of the server code page (see @ref-api-xlate). A #cmd("+") becomes a blank. An incomplete #cmd("%") at the end stays as it is.],
)

A module that creates its own variables chooses names that do not collide with
the names of @ref-api-env. The variables live until the request is reset.

=== Translation <ref-api-xlate>

#idx("http_etoa")
#idx("http_atoe")
#idx("http_xlate")
#idx("code page")
#idx("translation", "EBCDIC and ASCII")
#screen(```c
UCHAR *http_etoa(UCHAR *buf, int len);
UCHAR *http_atoe(UCHAR *buf, int len);
unsigned char *http_xlate(unsigned char *buf, int len, const unsigned char *tbl);

typedef struct { const unsigned char *atoe; const unsigned char *etoa; } HTTPCP;
/* in the vector: HTTPCP *xlate_cp037, *xlate_1047, *xlate_legacy */
```)

HTTPD works in EBCDIC and talks ASCII. The conversion tables depend on the
code page of the server (see @ref-prm for the keyword).

#deflist(width: 1.35in,
  [#cmd("http_etoa"), #cmd("http_atoe")], [Translate the buffer in place with the table of the server code page and return the buffer. A #cmd("len") of 0 means the length of the string. If the server has no context, the code page is CP037. Never pass a string literal: the literal may sit in storage that the module cannot write, and the call abends with S0C4. Copy the text to an array first.],
  [#cmd("http_xlate")], [Translates in place with a table of 256 bytes that the caller gives. A table of NULL means the table EBCDIC to ASCII of the server. Returns the buffer.],
  [#cmd("xlate_cp037"), #cmd("xlate_1047"), #cmd("xlate_legacy")], [Entries of the vector that point to the pairs of tables for the code pages CP037, IBM-1047 and LEGACY. Use the member #cmd("etoa") or #cmd("atoe") as the table of #cmd("http_xlate").],
)

The names of the code pages that the server accepts are CP037, IBM1047 and
LEGACY. A UFS file is stored in IBM-1047. #cmd("http_send_file") uses that
table for it.

=== Time, MIME Types and Addresses <ref-api-misc>

#idx("http_date_rfc1123")
#idx("http_secs")
#idx("http_mime")
#idx("http_ntoa")
#screen(```c
UCHAR        *http_date_rfc1123(time64_t t, UCHAR *buf, size_t size);
double       *http_secs(double *psecs);
const HTTPM  *http_mime(const UCHAR *path);
char         *http_ntoa(struct in_addr addr);
```)

#deflist(width: 1.35in,
  [#cmd("http_date_rfc1123")], [Writes the time as a date of the form #cmd("Thu, 01 Jan 2026 12:00:00 GMT") (the format of RFC 1123) into the buffer and returns the buffer.],
  [#cmd("http_secs")], [Writes the time since the epoch, with microseconds, into the double that the argument points to. Read the double that you gave and ignore the return value. The declaration says that the function returns a pointer, but the function returns a double by value. Only the lower 32 bits of the seconds are used.],
  [#cmd("http_mime")], [Returns the MIME type of the path. The type comes from the extension of the last part of the path. The search does not depend on case. For a name of the form #cmd("dd:name(member)") the part in the parentheses is dropped. An unknown extension gives the type #cmd("text/plain") as text. The function never returns NULL. The extensions #cmd("html") and #cmd("htm") give #cmd("text/html"). The extensions #cmd("shtm") and #cmd("shtml") give #cmd("text/x-server-parsed-html").],
  [#cmd("http_ntoa")], [Writes the address as #cmd("a.b.c.d") into a buffer of the C runtime of the task and returns the buffer. The next call in the same task overwrites it.],
)

=== Files <ref-api-file>

#idx("http_open")
#idx("http_send_file")
#idx("http_send_text")
#idx("http_send_binary")
#idx("http_get_ufs")
#screen(```c
FILE *http_open(HTTPC *httpc, const UCHAR *path, const HTTPM *mime);
int   http_send_text(HTTPC *httpc);
int   http_send_binary(HTTPC *httpc);
int   http_send_file(HTTPC *httpc, int binary);
UFS  *http_get_ufs(HTTPC *httpc);
```)

#deflist(width: 1.35in,
  [#cmd("http_open")], [Opens a file of the document root. *It always returns NULL.* Look at #cmd("httpc->ufp") to see whether the open worked: it is not NULL if it did. The function refuses a path that contains #cmd("..") and cuts a path to 255 bytes. If #cmd("mime") is NULL, it calls #cmd("http_mime") itself. It opens the file as binary if the type is binary and as text if not. It sets #cmd("httpc->ssi") to 1 for the types #cmd("text/html") and #cmd("text/x-server-parsed-html") and to 0 for the others. The file name is the document root with the path appended.],
  [#cmd("http_send_text"), #cmd("http_send_binary"), #cmd("http_send_file")], [Send the open file in steps. One call reads a piece of the file into #cmd("httpc->buf"), sends what it can and returns 0. Call it again and again until #cmd("httpc->state") is #cmd("CSTATE_DONE"). The state becomes #cmd("CSTATE_DONE") at the end of the file, and also when the send fails or the UFS read fails. The text form translates the data from IBM-1047 to ASCII, the binary form sends it as it is. #cmd("http_send_text") is #cmd("http_send_file(httpc, 0)") and #cmd("http_send_binary") is #cmd("http_send_file(httpc, 1)"). The function processes server side includes if #cmd("httpc->ssi") is 1: #cmd("<!--#echo var=\"X\" -->"), #cmd("<!--#include file=\"x\" -->"), the form with #cmd("virtual") and #cmd("<!--#printenv -->"). Send the headers before the first call. Do not close the file. #cmd("http_done") closes it. If a UFS read fails, the response ends: chunking and keep-alive go off and the server writes a console message.],
  [#cmd("http_get_ufs")], [Returns the UFS session of the client. It creates the session the first time. It returns NULL if the server has no UFS.],
)

The server uses these functions for its own static GET. Its steps are a
model for a module that serves files: get the type with #cmd("http_mime"), open
the file with #cmd("http_open"), send the status, #cmd("Content-Type") and the
end of the headers, then send the file.

=== Dynamic Arrays <ref-api-array>

#idx("array_new")
#idx("array_add")
#idx("dynamic array")
#screen(```c
void     *array_new(unsigned count);
int       array_add(void *varray, void *item);
int       array_addf(void *varray, const char *fmt, ...);
unsigned  array_count(void *varray);
unsigned  array_size(void *varray);
void     *array_get(void *varray, unsigned index);
void     *array_del(void *varray, unsigned index);
int       array_free(void *varray);
```)

The array entries give a module the dynamic arrays of the C library, so that
the module does not need to be linked with them. An array is a pointer to the
items. The first argument of the functions is the address of that pointer, not
the pointer itself. The indexes run from 1 to the count. #cmd("array_free")
frees the array and does not free the items. HTTPD itself tests the result of
#cmd("array_add") against 0 for success. The C library documents the rest.

=== Comparison <ref-api-cmp>

#idx("http_cmp")
#idx("http_cmpn")
#screen(```c
int http_cmp(const UCHAR *s1, const UCHAR *s2);
int http_cmpn(const UCHAR *s1, const UCHAR *s2, int n);
```)

#deflist(width: 1.35in,
  [#cmd("http_cmp")], [Compares two strings without regard to case. It returns the difference of the first characters that differ, and 0 if the strings are the same.],
  [#cmd("http_cmpn")], [The same, for at most #cmd("n") characters.],
)

=== Debug Output <ref-api-dbg>

#idx("DEBUG", "module output")
#idx("http_dbgf")
#idx("HTTPDBG")
#screen(```c
int http_dbgw(const char *buf, int len);
int http_dbgs(const char *str);
int http_dbgf(const char *fmt, ...);
int http_dump(void *addr, int len, const char *title, ...);
int http_enter(const char *fmt, ...);
int http_exit(const char *fmt, ...);
```)

The six functions write to the debug file of the server, the DD #cmd("HTTPDBG").
The server opens the file only if the keyword #cmd("DEBUG") is on (see
@ref-prm). If it is off, the functions write nothing. If the file cannot be
opened, the server writes #cmd("HTTPD020E") and the output goes to
#cmd("stderr").

#deflist(width: 1.35in,
  [#cmd("http_dbgw")], [Writes #cmd("len") bytes.],
  [#cmd("http_dbgs")], [Writes a string.],
  [#cmd("http_dbgf")], [Writes formatted text.],
  [#cmd("http_dump")], [Writes a dump of #cmd("len") bytes in lines of 16 bytes with a title. The title is formatted into a buffer of 80 bytes. Keep it short.],
  [#cmd("http_enter"), #cmd("http_exit")], [Write the text with the prefix "Enter " or "Exit " to mark the entry to a function and the exit from it.],
)

=== Authentication <ref-api-auth>

#idx("authentication", "module")
#idx("http_get_userid")
#idx("http_check_auth")
#idx("http_get_token")
#idx("http_get_password")
#idx("http_logout")
#idx("http_realm")
#screen(```c
UCHAR        *http_get_userid(HTTPC *httpc, UCHAR *out, unsigned outlen);
struct acee  *http_get_acee(HTTPC *httpc);
int           http_get_token(HTTPC *httpc, UCHAR *out, unsigned outlen);
int           http_check_auth(HTTPC *httpc, const char *class,
                              const char *resource, int attr);
int           http_logout(HTTPC *httpc);
UCHAR        *http_get_password(HTTPC *httpc, UCHAR *out, unsigned outlen);
const char   *http_realm(HTTPD *httpd);
```)

The server has already identified the client when the module runs. It reads
the credential from these sources in this order: the cookie #cmd("Sec-Token"),
the cookie #cmd("LtpaToken2"), a Bearer token and then Basic authentication.
The mode of the route (#cmd("AUTH="), see @ref-route) changes only the response
to a client that has no credential. A route with #cmd("AUTH=NONE") passes the
request without a credential. All functions in this group accept a client
without a credential.

#deflist(width: 1.35in,
  [#cmd("http_get_userid")], [Copies the userid of the client into #cmd("out") and returns #cmd("out"). A userid has at most 8 characters, so a buffer of 9 bytes holds it; in a smaller buffer the userid is cut. The function returns NULL, with an empty string in #cmd("out"), if the client has no credential. It returns NULL if #cmd("out") is NULL or #cmd("outlen") is 0.],
  [#cmd("http_get_acee")], [Returns the ACEE of the client, or NULL. A module that calls the RACF routines itself with this ACEE sees the raw SAF code. An unprotected resource then answers 4, not 0. Use #cmd("http_check_auth") unless you need the difference.],
  [#cmd("http_get_token")], [Copies the token of the session of the client, 32 bytes, into #cmd("out") and returns 32. It returns 0 if there is no credential or #cmd("outlen") is less than 32. The token is binary. The caller encodes it with base64 to send it.],
  [#cmd("http_check_auth")], [Asks RACF whether the client may use the resource under the class, with the attribute #cmd("attr"): #cmd("RACF_ATTR_READ"), #cmd("RACF_ATTR_UPDATE"), #cmd("RACF_ATTR_CONTROL") or #cmd("RACF_ATTR_ALTER"). 0 means READ. It returns 0 if the access is permitted, 8 or more (the return code of SAF) if it is refused, and -1 if the client is not authenticated. The case "no profile" is also returned as 0. Test for 0. Never test for a value of 4 or less: that would let an unauthenticated client (-1) through.],
  [#cmd("http_logout")], [Removes the session of the client and clears the credential of the request. It returns 0, or -1 if there was no session.],
  [#cmd("http_get_password")], [Decrypts the password of the client and writes it into #cmd("out"). The password is in upper case. It returns #cmd("out"), or NULL if there is no credential or if the credential has no password (a session that is known only by token). In a buffer that is too small the password is cut. Overwrite the buffer when you are done with it.],
  [#cmd("http_realm")], [Returns the realm of the server: the value of the keyword #cmd("REALM"), or the SMF identifier of the system if there is none (see @ref-prm). It is never NULL. The string stays valid for the life of the address space. A module that sends its own #cmd("WWW-Authenticate") challenge uses it as the realm, so that the challenge of the module and that of the server agree.],
)

=== Sessions <ref-api-session>

#idx("http_new_session")
#idx("session", "login")
#screen(```c
int http_new_session(HTTPC *httpc);
```)

#cmd("http_new_session") creates a new login session for the client of the
request. The session has the same userid, password and address as the
credential of the request. It has a token of its own and an ACEE of its own.
The function is for a login endpoint of a module. Each call costs a RACF logon
and a place in the credential store. The entry exists from HTTPD 4.2.

The function returns 0 on success. Then #cmd("http_get_token") returns the token
of the new session. If the module cannot send the token to the client, it
calls #cmd("http_logout") so that the session does not stay.

From HTTPD 4.5 the function reports why it failed:

#tab(caption: [The results of http_new_session])[#table(
  columns: (2.1in, 0.5in, 1fr),
  [*Constant*], [*Value*], [*Meaning*],
  [#cmd("HTTP_SESSION_NONE")], [-1], [The request is not authenticated, or it carries no password to log on again with. A credential that came from a token alone has no password.],
  [#cmd("HTTP_SESSION_REFUSED")], [-2], [RACF refused the logon (return code 4, 8, 12 or 28 of RACINIT). The server marks the credential as stale. It does not answer by password or token from it any more and frees it 600 seconds later. The current request still runs on it. The module answers 401.],
  [#cmd("HTTP_SESSION_BUSY")], [-3], [The client has 16 sessions, all in use, or the credential store is full. A session is replaced only if it has been idle for 60 seconds or more.],
  [#cmd("HTTP_SESSION_NOMEM")], [-4], [The server is out of storage, or RACF could not be asked: RACF is not active, an installation exit or a terminal or time restriction stopped the logon. The server then writes the message #cmd("HTTPD009W"); a refusal of the password is not written.],
)]

Before HTTPD 4.5 every failure returns -1. Test the result for #cmd("!= 0") or
#cmd("< 0"), not for #cmd("== -1"). Treat any other negative value as an
unknown failure.

=== The Context Block <ref-api-ctxfn>

#idx("http_cgictx_get")
#screen(```c
void *http_cgictx_get(HTTPD *httpd, const char *eye, unsigned size);
```)

The function returns a block of storage that lives as long as the server and
is the same for every request. See @ref-api-cgictx for the details.

=== Routes <ref-api-routefn>

#idx("http_find_route")
#idx("http_add_route")
#screen(```c
HTTPROUTE *http_find_route(HTTPD *httpd, const char *path);
HTTPROUTE *http_add_route(HTTPD *httpd, const char *pgm, const char *path,
                          int login);
```)

#deflist(width: 1.35in,
  [#cmd("http_find_route")], [Searches the route table in the order of the Parmlib and returns the first route that matches the path, or NULL. A route with a wildcard (#cmd("*") or #cmd("?")) matches by pattern, any other route by equality without case.],
  [#cmd("http_add_route")], [Adds a route. It copies #cmd("pgm") and #cmd("path"). A #cmd("pgm") of NULL makes a route for static files (as the keyword #cmd("LOC=")). The argument #cmd("login") is not used. The new route is public, as with #cmd("AUTH=NONE"). It returns NULL on failure.],
)

=== Entries for the Server Only <ref-api-internal>

The following entries are the steps of the request cycle and the control of the
worker pool. The server calls them. A module does not call them. The table
says what each one does.

#tab(caption: [The entries that belong to the server])[#table(
  columns: (2.1in, 1fr),
  [*Entry*], [*What it does*],
  [#cmd("http_in"), #cmd("http_parse")], [Read the request and parse it into variables.],
  [#cmd("http_get")], [Serves a static file for GET. See @ref-http.],
  [#cmd("http_head"), #cmd("http_put"), #cmd("http_post")], [Stubs. Each answers 501 and sets the state to #cmd("CSTATE_DONE"). POST is routed to #cmd("http_put").],
  [#cmd("http_done")], [Closes #cmd("fp") and #cmd("ufp"), sends the final chunk of a chunked response, sets the end time and the state #cmd("CSTATE_REPORT").],
  [#cmd("http_report")], [Adds to the counters and writes the SMF record (see @ref-smf). The state becomes #cmd("CSTATE_RESET").],
  [#cmd("http_reset")], [Frees the variables and clears the request fields. With keep-alive the state becomes #cmd("CSTATE_IN"), else #cmd("CSTATE_CLOSE").],
  [#cmd("http_close")], [Removes the client from the table, closes the files, the UFS session and the socket, and frees HTTPC.],
  [#cmd("http_is_busy"), #cmd("http_set_busy"), #cmd("http_reset_busy")], [Manage the table of the clients that a worker is serving.],
  [#cmd("http_process_clients"), #cmd("http_process_client")], [The loop of the worker.],
  [#cmd("http_console")], [Handles an operator command.],
  [#cmd("http_link"), #cmd("http_process_route")], [Run a module (see @ref-api-run). #cmd("http_link") returns the result codes of @ref-api-linkrc.],
)]

== The Direct Accessors <ref-api-direct>

#idx("http_get_httpx")
#idx("http_get_flag")
#idx("HTTPD_FLAG_QUIESCE")
#idx("HTTPD_FLAG_SHUTDOWN")
HTTPD is opaque. Two macros read a field of it. They are not entries of the
vector. They read the storage at a fixed offset, and the server checks the
offsets when it is built.

#screen(```c
#define http_get_httpx(h)  (*(HTTPX **)((char *)(h) + 8))
#define http_get_flag(h)   (*(volatile unsigned char *)((char *)(h) + 0x2C))

#define HTTPD_FLAG_QUIESCE  0x40
#define HTTPD_FLAG_SHUTDOWN 0x80
```)

#deflist(width: 1.35in,
  [#cmd("http_get_httpx")], [Returns the function vector.],
  [#cmd("http_get_flag")], [Returns the flag byte of the server. The bit #cmd("HTTPD_FLAG_QUIESCE") means that the server accepts no new requests. The bit #cmd("HTTPD_FLAG_SHUTDOWN") means that the server is shutting down.],
)

Both macros exist from HTTPD 4.0. See @ref-api-drain for the use of the flags.

== Result Codes of a Link <ref-api-linkrc>

#idx("http_link", "result codes")
#idx("HTTP_LINK_ENOMOD")
#idx("HTTP_LINK_EPGMRC")
#cmd("http_link") returns the outcome of the LINK as one integer. The server
uses it to build the answer that is described in @ref-api-abend. The values:

#tab(caption: [The result of http_link])[#table(
  columns: (2.0in, 1.2in, 1fr),
  [*Value*], [*Number*], [*Meaning*],
  [0 or more], [], [The module ran and returned this code.],
  [#cmd("HTTP_LINK_ENOLOAD")], [-0x01000001], [The LINK failed and the cause is not S806.],
  [#cmd("HTTP_LINK_ENOPGM")], [-0x01000002], [The route has no program name.],
  [#cmd("HTTP_LINK_EESTAE")], [-0x01000003], [The ESTAE could not be set. The module did not run.],
  [#cmd("HTTP_LINK_ENOMOD")], [-0x01000004], [The module is not in the STEPLIB or the link list (S806).],
  [#cmd("HTTP_LINK_EPGMRC(rc)")], [-0x02000000 + rc], [The module ran and returned a negative code #cmd("rc"). The macros #cmd("HTTP_LINK_IS_PGMRC(v)") and #cmd("HTTP_LINK_PGMRC(v)") test the value and take the code out. The startup code clamps a negative code to 0, so only a module with its own start code returns one.],
  [any other negative value], [], [The module abended. The value is the negated completion code in the form X'00sssuuu' (system code sss, user code uuu).],
)]

Every negative value means that the module did not finish its work. For the
values that say that the module never ran, there is no dump to look for.

== Storage Rules <ref-api-storage>

#idx("module", "storage")
#idx("subpool 5")
#idx("static data", "module")
#idx("__wsaget")
#idx("__getmsp")
A module runs in the task of a worker of HTTPD. Its storage follows from that.

#deflist(width: 1.5in,
  [Heap], [The startup code switches the heap of the module to the request subpool, subpool 5, for the time of #cmd("main"). Everything that the module gets with #cmd("malloc") belongs to the request. A module frees what it allocates, on every path. If the module abends, the server frees the whole subpool with one FREEMAIN. When the worker task ends, MVS frees what is left.],
  [#cmd("http_cgi_subpool")], [#cmd("UCHAR http_cgi_subpool(HTTPC *httpc)") returns the number of the request subpool, 5, also available as the constant #cmd("HTTP_CGI_SUBPOOL"). It returns 0 for a client that the server does not own, and the entry holds 0 in a server that is too old to have the subpool. In that case the startup code sets no subpool and the heap is as it was. A module does not need to call it.],
  [Storage that must live longer], [Get it with #cmd("__getmsp(size, 0)") (declared in #cmd("mvs/storage.h")), or ask for a block with #cmd("http_cgictx_get") (see @ref-api-cgictx). Storage in any other subpool does not survive the request.],
  [No writable static data], [A module must not write to a file-scope variable or to a static variable of a function. A module that is fetched from an APF-authorized library or from the link list is in storage of key 0, while its code runs in key 8. The first write to the module storage then abends with S0C4. The library decides this, not the attribute AC(1) and not RENT. A switch to key 0 with #cmd("__super()") does not help, because a module can be in key 0 without the right to switch.],
  [Where to keep state], [Keep the state of a request in local variables, on the heap, or in the memory that #cmd("__wsaget()") returns. Keep state of the server in the block of #cmd("http_cgictx_get"). Variables that the module only reads are fine in the static data: tables, and constant strings.],
  [Strings that are translated], [Declare #cmd("UCHAR text[] = \"...\"") and not #cmd("UCHAR *text = \"...\""), if you translate the text in place. The array is a copy on the stack. The pointer points to the constant itself.],
  [Server allocations], [The functions #cmd("http_new_env"), #cmd("http_open"), #cmd("http_get_ufs") and #cmd("http_add_route") get their storage from subpool 0 inside the server. A reclaim of subpool 5 does not touch it.],
  [Stack], [See @ref-api-start for #cmd("__stklen").],
)

== Long-Running Handlers and Shutdown <ref-api-drain>

#idx("shutdown", "module")
#idx("quiesce", "module")
#idx("drain")
A request that takes long holds a worker. When the server stops, it sets the flags
#cmd("HTTPD_FLAG_QUIESCE") and #cmd("HTTPD_FLAG_SHUTDOWN") in the flag byte,
and it waits up to 10 seconds, in
steps of 1 second, for the socket thread to end (it writes the messages
#cmd("HTTPD040I") and #cmd("HTTPD041I") while it waits). The sends of the
server honor the flags: they stop and set the state to #cmd("CSTATE_DONE").

A module that loops, waits or sends in pieces looks at the flags in each round
and returns when a flag is set:

#screen(```c
while (more_to_send) {
    if (http_get_flag(httpd) & (HTTPD_FLAG_QUIESCE | HTTPD_FLAG_SHUTDOWN))
        break;
    if (httpc->state == CSTATE_DONE)
        break;
    /* ... send the next piece ... */
}
```)

== Abends <ref-api-abend>

#idx("abend", "module")
#idx("ESTAE")
#idx("HTTPD908E")
#idx("HTTPD062E")
#idx("HTTPD063E")
Two layers of recovery protect a worker.

*The module.* The LINK of the module is made through #cmd("__linkdsx"), which
sets an ESTAE. If the module abends, the ESTAE retries the call of the server.
The worker lives and serves its next request. The server then does this in
order:

+ It frees the subpool 5. If the FREEMAIN fails, it writes
  #cmd("HTTPD906W STORAGE RECLAIM FOR pgm FAILED, RC=n").
+ It builds the text of the answer from the result of @ref-api-linkrc.
+ If no response was started (#cmd("httpc->resp") is 0), it sends the status
  503 with #cmd("Content-Type: text/plain") and the end of the headers. If the
  module had begun the response, the text goes after what the module sent.
+ It writes the body #cmd("External program pgm text"), and the console message
  #cmd("HTTPD908E EXTERNAL PROGRAM pgm text").
+ It sets the state to #cmd("CSTATE_DONE").

The text is one of these:

#tab(caption: [The texts of a failed module])[#table(
  columns: (1.9in, 1fr),
  [*Text*], [*Cause*],
  [was not found], [The module is in no library of the STEPLIB or link list (S806).],
  [could not be loaded], [The LINK failed for another reason, for example a fetch error or too little storage.],
  [was dispatched without a program name], [The route has no program.],
  [could not be started, ESTAE create failed], [The ESTAE could not be set.],
  [returned rc n], [The module returned the negative code n.],
  [failed with Snnn ABEND], [A system abend with the code Snnn.],
  [failed with Unnnn ABEND], [A user abend with the code Unnnn, in decimal.],
)]

An abend of a module does not end the worker. A reclaim of subpool 5 releases
what the module allocated with #cmd("malloc"). It does not release what the
module holds outside subpool 5: files that the module opened itself, a UFS
session that it created, or storage from other subpools. A module that needs
to clean these up after an abend must do so with its own recovery.

*The worker.* Each worker runs under an ESTAE of its own, with a retry. If the
abend happens in the code of the server and not in a module, the server writes
#cmd("HTTPD062E ABEND code IN WORKER(id) CLIENT(addr) SOCKET(n)"), clears the
busy mark of the client, closes the client and the worker serves on. If the
worker cannot recover, it writes the message #cmd("HTTPD063E"), which says that there is no recovery in the worker and that the client is dropped. The messages are in @apx-messages.

== The Started Task Procedure <ref-api-proc>

#idx("started task", "procedure")
#idx("PROC", "module requirements")
#idx("HASPCKPT")
#idx("HASPACE1")
#idx("jesopen")
A module runs in the address space of the server. It has no allocations of its
own. When it opens a ddname, the system looks for it in the procedure of the
server. A module is therefore a part of the contract of the procedure: if the
procedure lacks a DD that a module needs, the module fails.

The procedure that comes with HTTPD has these DDs. It also has the DDs
#cmd("SYSUDUMP"), #cmd("SNAP") and #cmd("HTTPSTAT"), all with
#cmd("SYSOUT=*").

#tab(caption: [The DDs of the HTTPD procedure])[#table(
  columns: (1.2in, 1fr),
  [*DD*], [*Use*],
  [#cmd("STEPLIB")], [The load library of the server and of the modules.],
  [#cmd("HTTPDERR")], [#cmd("stderr") of a module.],
  [#cmd("HTTPDOUT")], [#cmd("stdout") of a module.],
  [#cmd("HTTPDIN")], [#cmd("stdin") of a module. It is DUMMY.],
  [#cmd("HTTPDBG")], [The debug file (see @ref-api-dbg).],
  [#cmd("HTTPPRM")], [The Parmlib member (#cmd("FREE=CLOSE")).],
  [#cmd("HASPCKPT"), #cmd("HASPACE1")], [The JES2 checkpoint data set and the spool volume. HTTPD never opens them. See below.],
)]

The two JES2 DDs are for the modules. A module that calls the C library
routine #cmd("jesopen()") needs both. The jobs interface of mvsMF is such a
module. If they are missing, every job list, every lookup of a job and every
read of spool output answers 500, and two lines go to the console. A job that is submitted does not need them, because the
submit goes through the internal reader, which the module allocates itself.

#screen(```
//HASPCKPT DD DSN=SYS1.HASPCKPT,DISP=SHR,UNIT=3350,VOL=SER=MVS000
//HASPACE1 DD DSN=SYS1.HASPACE,DISP=SHR,UNIT=3350,VOL=SER=SPOOL1
```)

The unit and the volume serial must be those of the JES2 procedure of the
site. The values above are those of the sample procedure.
Before you remove a DD from the procedure, find out which modules of other
products use it.

== Resident Modules <ref-api-resident>

#idx("RESIDENT")
#idx("resident modules")
#idx("HTTPD440W")
#idx("HTTPD441W")
#idx("HTTPD442W")
#idx("HTTPD443W")
With the keyword #cmd("RESIDENT=YES"), which is the default, the server loads
each module of its routes once, when it starts. It does a BLDL for every
different name in a #cmd("MOD=") keyword and then a LOAD in the main task. A
LINK of the worker then finds the module in storage and does not fetch it from
disk again. The keyword is described in @ref-prm.

The server holds a module only if it is reentrant or reusable (RENT or REUS).
For any other case it writes a message and leaves the module to be fetched at
each request:

#tab(caption: [The messages of the resident load])[#table(
  columns: (0.9in, 1fr),
  [*Message*], [*Meaning*],
  [#cmd("HTTPD440W")], [The module was not found when the server started. It is not held.],
  [#cmd("HTTPD441W")], [The module is not reusable. It is fetched for each request.],
  [#cmd("HTTPD442W")], [The load failed with the abend code shown. The module is fetched for each request.],
  [#cmd("HTTPD443W")], [The value of #cmd("RESIDENT") is not YES or NO.],
)]

The server holds every module that it could load. Its count of the held modules, which #cmd("D C") and the server block show, stops at 255. The server deletes the modules when it ends. Two
consequences follow for the module author:

- A new level of a resident module does not take effect until the server is
  restarted. With #cmd("RESIDENT=NO") the next request fetches the new level.
- A module that is not RENT or REUS works, but it is fetched anew for every
  request.

The display module HTTPDSRV (see @ref-dm) shows the setting and the number of
held modules.

== Context Pointers <ref-api-cgictx>

#idx("CGI_CONTEXT_POINTERS")
#idx("http_cgictx_get", "context block")
#idx("context pointers")
Storage that a module allocates in a request does not outlive it. A module that
needs data across requests, such as a cache or a table of open resources, asks
for a block of the server:

#screen(```c
void *http_cgictx_get(HTTPD *httpd, const char *eye, unsigned size);
```)

The server keeps a table of blocks. The block is identified by an eye catcher
of 8 bytes. The first call with an eye catcher creates the block. It gets the
requested size from storage that lives as long as the address space, clears it
and writes the eye catcher into its first 8 bytes. Every later call with the
same eye catcher returns the same block. The size counts only when the block is
created, so a module always gives the same size. The server never frees the
block.

The function returns NULL if the table is full or if there is no storage. The
table is locked for the duration of the call, but the block itself is not. A
module that shares a block between requests, which can run at the same time in
different workers, serializes its own access.

The size of the table is set by the Parmlib keyword #cmd("CGI_CONTEXT_POINTERS").
The default is 255 and the maximum is 255. The server cuts a larger value to 255
and ignores a negative value. The table has one more place than the value. The
keyword is in @ref-prm. HTTPDSRV shows the value (see @ref-dm).

The function exists from HTTPD 4.0.
