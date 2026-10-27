#import "../bookmaster/bookmaster.typ": *

= HTTP Behaviour and Limits <ref-http>

#idx("HTTP", "behaviour")
#idx("limits", "HTTP")
This chapter describes what HTTPD does with a request, which limits it
enforces, and what it sends back. It covers the methods, the limits on the
request, the decoding of the URL, the status codes and headers that the server
itself produces, keep-alive and time-outs, chunked transfer, static files and
their MIME types, character translation, and the limits of the worker model.

The chapter describes the server itself. A module that sends its own status
line and headers decides its own response. The server only adds what this
chapter names. The keywords that set the time-outs and task counts are in
"The HTTPPRM Keywords" (@ref-prm). The limits of a route line are in
"Routes and Modules" (@ref-route).

== How a Request Is Handled <ref-http-flow>

#idx("request", "processing order")
Every request passes the same stages, in this order.

+ The server reads the request line and the header lines from the socket.
  This stage rejects a request that is too long or malformed (@ref-http-limits).
+ It parses the method, the path, the query string and, for a POST, the body.
  This stage rejects an unknown method and a body that is too large.
+ It resolves the credential of the request and looks for the route that
  matches the path.
+ The requests for #cmd("/login") and #cmd("/logout") are answered by the server
  before the authentication check. A GET or HEAD for a path that starts with
  #cmd("/login.") or #cmd("/favicon.") skips the check, too.
+ The authentication check of the route gives a 401 or a 403, or lets the
  request go on.
+ If the route names a program, the server runs that module. If there is no
  route, or the route is a #cmd("LOC=") route, the static file handler answers.

== Methods <ref-http-methods>

#idx("methods")
#idx("GET")
#idx("HEAD")
#idx("POST")
#idx("PUT")
#idx("DELETE")
#idx("405 Method Not Allowed")
#idx("501 Not Implemented")
The server knows five methods: DELETE, GET, HEAD, POST and PUT. The comparison
is case sensitive, as the method of an HTTP request is, so #cmd("get") is not
the method #cmd("GET"). Any other method gets a 405. The 405 is sent before the authentication check and before
the route lookup, so it is the same for every path. It carries no #cmd("Allow")
header, the connection closes, and the body is #cmd("405 Method Not Allowed").

#tab(caption: [Dispatch by method])[
  #table(columns: (1in, 1.5in, 1fr),
    [Method], [Route with a program], [Static file or #cmd("LOC=") route],
    [GET], [The module runs.], [The server sends the file.],
    [HEAD], [The module runs.], [The server sends the status and the headers
      of a GET, and no body.],
    [POST], [The module runs.], [501 Not Implemented.],
    [PUT], [The module runs.], [501 Not Implemented.],
    [DELETE], [The module runs.], [501 Not Implemented.],
    [Any other], [405 Method Not Allowed.], [405 Method Not Allowed.],
  )
]

A route with a program has no restriction by method. The module decides which
methods it accepts and sends its own response to the others.

#idx("HEAD", "static file")
A HEAD for a static file takes the path of a GET as far as the empty line that
ends the headers, and stops there. The status and the headers are the ones that
a GET sends. #cmd("Content-Length") is the length of the file, not 0. The
response has no body. The client reads the headers and nothing else, so the
connection stays usable for the next request.

#screen(raw(read("../ex/ug-static/head-request.txt")))

These rules hold for a HEAD.

- The server opens the file as for a GET. A path with no file gets the 404 of a
  GET, and a directory is resolved to #cmd("index.html") or
  #cmd("default.html") as for a GET (@ref-http-path). The 404 has the headers of
  the GET, and no body.

  #screen(raw(read("../ex/ug-static/head-not-found.txt")))
- On a route with #cmd("AUTH=BASIC") or #cmd("AUTH=TOKEN"), a HEAD without a
  valid credential gets the 401 that a GET gets, and a HEAD that the resource
  check refuses gets the 403. Both have the headers of the GET and no body. A
  route with #cmd("AUTH=FORM") answers a HEAD in another way
  (@ref-http-form-methods).
- The server sends #cmd("Content-Length") only where a GET sends it
  (@ref-http-hdr). For a file with a server-side include type, which
  includes every #cmd("htm") and #cmd("html") file (@ref-http-ssi), and for a
  file of 0 bytes there is no length. The HEAD then has neither
  #cmd("Content-Length") nor #cmd("Transfer-Encoding"): a response to a HEAD is
  never chunked (@ref-http-chunk).

  #screen(raw(read("../ex/ug-static/head-html.txt")))
- On a route with a program the module runs and decides. @ref-api-out says
  what the server does with the output of a module for a HEAD.

#note[HTTPD 4.4.0 and earlier do not answer a HEAD for a static file with the
headers of a GET. They answer #cmd("501 Not Implemented") with no
#cmd("Content-Length"), chunked transfer and the text
#cmd("501 Not Implemented") as the body. A HEAD response must not have a body,
so the connection could not be used for the next request. The behaviour in this
section is that of HTTPD 4.5.0.]

An unknown method gets a 405 and the connection closes.

#screen(raw(read("../ex/ug-static/method-not-allowed.txt")))

== Limits on the Request <ref-http-limits>

#idx("request", "limits")
The server reads a request into a buffer of 4000 bytes. Several limits come
from that size.

=== The request line and the header lines <ref-http-lines>

#idx("request line", "length")
#idx("header", "length limit")
#idx("414 URI Too Long")
A line can hold at most 3998 characters. The CR in front of the LF is skipped
and is not counted. A line that is longer than that is rejected. For the
request line, the answer is 414 URI Too Long. For a header line, the answer is
400 Bad Request.

The server rejects a request in these cases, too. All of these responses close
the connection, because keep-alive is not yet decided when they are sent.

#tab(caption: [Requests that the server rejects while it reads the headers])[
  #table(columns: (2.4in, 0.55in, 1fr),
    [Condition], [Code], [Body],
    [The request line is longer than 3998 characters.], [414],
      [#cmd("414 URI Too Long")],
    [A line has a CR that is not followed by an LF.], [400],
      [#cmd("400 Bad Request")],
    [The request line has no version.], [400], [#cmd("400 Bad Request")],
    [The version is not HTTP/1.0 or HTTP/1.1.], [505],
      [#cmd("505 HTTP Version Not Supported")],
    [A header name has a character other than a letter, a digit or one of
      #cmd("!#$%&'*+-.^_`|~").], [400], [#cmd("400 Bad Request")],
    [A header value has a control character below the blank, other than the
      tab.], [400], [#cmd("400 Bad Request")],
    [The request has two #cmd("Host") headers.], [400],
      [#cmd("400 Bad Request")],
    [The #cmd("Content-Length") is empty or has a character that is not a
      digit.], [400], [#cmd("400 Bad Request")],
    [An HTTP/1.1 request has no #cmd("Host") header.], [400],
      [#cmd("400 Bad Request: Missing Host header")],
  )
]

All these bodies are #cmd("text/plain"). A header line without a colon is
skipped without a message.

If the request line is empty, or has a method but no URI, the server closes
the connection without sending a response.

=== The headers <ref-http-headers>

#idx("header", "request")
#idx("Host header")
#idx("Content-Length")
There is no limit on the number of header lines. If a header appears twice, the
later one replaces the earlier one. The #cmd("Host") header is the exception,
because two of them are an error.

The server turns the headers and some parts of the request into environment
variables for the request. A module reads them with the API (@ref-api). The
server builds the names this way.

- A header becomes #cmd("HTTP_") followed by the header name, for example
  #cmd("HTTP_Host").
- Each cookie of a #cmd("Cookie") header becomes #cmd("HTTP_Cookie-") followed
  by the cookie name. The header is split at the semicolons and the blanks.
- A query parameter becomes #cmd("QUERY_") followed by the name.
- A field of a form body becomes #cmd("POST_") followed by the name.

The server also sets #cmd("SERVER_ADDR"), #cmd("SERVER_PORT"),
#cmd("SERVER_PROTOCOL") and #cmd("SERVER_SOFTWARE"). The protocol is always
#cmd("HTTP/1.1"). The software is #cmd("HTTPD/") followed by the version.

#idx("environment variable", "size limit")
#idx("HTTPD905E")
A name and its value together can have at most 8192 bytes. If a module sets a
larger variable, the call fails and the server writes message HTTPD905E (see
@apx-messages). A request cannot reach this size by itself, because a line is
limited to 3998 characters. If setting a variable fails while the server
parses a request, the request fails. On the first request of a connection, the
server closes the connection without a response. On a later request, it closes
the connection, too.

=== Parameters <ref-http-params>

#idx("parameters", "limit")
#idx("query string")
A request can have at most 256 parameters. The query parameters and the form
fields of the body count together. A request with more gets a 400 with the
body #cmd("400 Bad Request").

=== The request body <ref-http-body>

#idx("request body", "limit")
#idx("413 Payload Too Large")
#idx("Expect: 100-continue")
The server reads a body only for a POST. It handles the body this way.

- If the request has a #cmd("Transfer-Encoding") header, the server reads
  nothing, and the body is ignored.
- If there is no #cmd("Content-Length"), or the length is 0, there is no body.
- If the length is more than 3999, the answer is 413 Payload Too Large with the
  body #cmd("413 Payload Too Large"). The body is not read, and the connection
  closes. This does not apply to a route with a program: the server does not
  read the body there either, and does not answer 413, but runs the module
  after the authentication check. The module reads the body from the socket
  itself, as it does for a PUT. The connection closes after the response.
- Otherwise the server reads the body with one non-blocking read of at most
  #cmd("Content-Length") bytes. If this read fails, the server closes the
  connection without a response.

The server translates the body from ASCII to EBCDIC. If the content type is
#cmd("application/x-www-form-urlencoded"), it sets #cmd("POST_STRING") to the
body, and it sets one #cmd("POST_") variable for each field. The values are
decoded as described in @ref-http-url. For any other content type, it sets
only #cmd("POST_STRING"). The comparison covers the whole value, in any case.
A value with a parameter, such as
#cmd("application/x-www-form-urlencoded; charset=UTF-8"), is therefore "any
other content type": the server sets #cmd("POST_STRING") and splits no fields.
If the request has a body but no #cmd("Content-Type") header, the server does
not read the body and sets nothing.

A PUT body is not read by the server. A module that handles PUT reads the body
from the socket itself.

The 400 and the 413 that the parser sends carry
#cmd("Cache-Control: no-store") and the type #cmd("text/plain").

The server turns off keep-alive for every request that has a
#cmd("Content-Length") or a #cmd("Transfer-Encoding") header. The response to a
POST or a PUT therefore says #cmd("Connection: close").

If the request is HTTP/1.1, has the header #cmd("Expect: 100-continue") and has
a #cmd("Content-Length") or a #cmd("Transfer-Encoding") header, the server
sends the interim response #cmd("HTTP/1.1 100 Continue") with an empty line
once it has read all the headers. This happens before the authentication
check.

== URL Decoding and Escaping <ref-http-url>

#idx("URL", "decoding")
#idx("percent encoding")
#idx("query string", "decoding")
The server splits the request target at the first question mark. The text in
front is the path. The text behind is the query string. The server stores the
raw query string in #cmd("QUERY_STRING").

*The path.* The server decodes the path and stores the result in
#cmd("REQUEST_PATH"). The decoding turns each #cmd("%xx") into the character
for that byte. A plus sign stays a plus sign in the path. In the value of a
query field it becomes a blank.

*The query.* The server splits the query string at each ampersand. It splits
each field at the first equal sign. The name is not decoded. The value is
decoded. A field without an equal sign gets an empty value.

*Rules of the decoding.*

- The hexadecimal value of an escape goes through the code page table of the
  server (@ref-http-xlate).
- A percent sign that is not followed by two hexadecimal digits stays as it is.
  So #cmd("%zz") and a percent sign at the end of the text are kept in the
  result.
- An escape that gives the byte 0 stays as it is, too: #cmd("%00") does not cut
  the path or the value, and no NUL character reaches #cmd("REQUEST_PATH") or
  a #cmd("POST_") variable.

*Escaping in output.* The server has one function that escapes text for HTML.
It replaces the characters #cmd("&"), #cmd("<"), #cmd(">"), #cmd("\"") and
#cmd("'") with #cmd("&amp;"), #cmd("&lt;"), #cmd("&gt;"), #cmd("&quot;") and
#cmd("&#39;"). It always ends the result with a NUL character, and it cuts
the result if the buffer is too small. The server uses it for the value of the
#cmd("Sec-Uri") cookie when it writes the login form. The text of a 404 is
#cmd("text/plain") and is not escaped.

== Status Codes <ref-http-status>

#idx("status codes")
#idx("100 Continue")
#idx("400 Bad Request")
#idx("401 Unauthorized")
#idx("403 Forbidden")
#idx("404 Not Found")
#idx("500 Internal Server Error")
#idx("503 Service Unavailable")
#idx("505 HTTP Version Not Supported")
The server itself sends these status codes. The body of every error response is
one line of #cmd("text/plain").

#tab(caption: [Status codes that the server sends])[
  #table(columns: (0.45in, 1.2in, 1fr),
    [Code], [Phrase], [When the server sends it],
    [100], [Continue], [The interim response to #cmd("Expect: 100-continue")
      (@ref-http-body).],
    [200], [OK], [A static file, the login form, the logout, and the pages of
      the display modules.],
    [302], [Moved Temporarily], [A GET on a route with form login and no
      session. The response has #cmd("Location: /login") and a #cmd("Set-Cookie")
      header for the cookie #cmd("Sec-Uri"). Only GET gets a 302
      (@ref-http-form-methods).],
    [303], [See Other], [A successful login by POST. The response has a
      #cmd("Location") header and a #cmd("Set-Cookie") header for the cookie
      #cmd("Sec-Token").],
    [400], [Bad Request], [A malformed request (@ref-http-lines,
      @ref-http-params).],
    [401], [Unauthorized], [The route needs a credential, and the request has
      none or a wrong one. The body is #cmd("401 Unauthorized").],
    [403], [Forbidden], [The user is known, but the resource check of the route
      denies access. The body is #cmd("403 Forbidden").],
    [404], [Not Found], [No file for the path. The body is
      #cmd("404 document \"path\" not found"), with the decoded path.],
    [405], [Method Not Allowed], [A method that the server does not know
      (@ref-http-methods).],
    [413], [Payload Too Large], [A body of more than 3999 bytes, for a route
      without a program (@ref-http-body).],
    [414], [URI Too Long], [A request line of more than 3998 characters.],
    [500], [Internal Server Error], [The server cannot build the response, for
      example because it cannot set the file name of a script, or a module asks
      for a status code that the server does not know. The body is
      #cmd("500 Internal Server Error").],
    [501], [Not Implemented], [POST, PUT or DELETE for a static file.],
    [503], [Service Unavailable], [A module does not run. The body is
      #cmd("External program") followed by the module name and the reason. If
      the module has already sent a status line, the text is appended to its
      output. See "When a Module Does Not Run" in @ref-route.],
    [505], [HTTP Version Not Supported], [A version other than HTTP/1.0 and
      HTTP/1.1.],
  )
]

The 401 adds a #cmd("WWW-Authenticate") header for a route with Basic
authentication. It does not add it for a route with token or form
authentication. It does not add it if the request has the header
#cmd("X-CSRF-ZOSMF-HEADER"), because the browser would open its own credential
dialog.

=== Other methods on a route with form login <ref-http-form-methods>

#idx("form login", "methods")
On a route with form login and no session, every method that the server knows
goes to the login step, not only GET. A GET gets the 302. A POST is treated as a
login attempt, whatever its path. A PUT or DELETE gets the status 200 and a
#cmd("text/plain") body #cmd("Invalid REQUEST_METHOD \"PUT\""), with the
method that the client sent. A HEAD gets the same status 200 and the header
#cmd("Content-Type: text/plain"), and no body. This holds for a HEAD for
#cmd("/login") and #cmd("/logout") too.

=== The phrase table <ref-http-phrases>

#idx("status codes", "phrase table")
#idx("HTTPD909E")
A module calls the server to write the status line. The server knows the
phrases of these 26 codes only.

#tab(caption: [The status codes that the server can write])[
  #table(columns: (0.5in, 1.7in, 0.5in, 1.7in),
    [Code], [Phrase], [Code], [Phrase],
    [200], [OK], [404], [Not Found],
    [201], [Created], [405], [Method Not Allowed],
    [202], [Accepted], [409], [Conflict],
    [204], [No Content], [410], [Gone],
    [206], [Partial Content], [412], [Precondition Failed],
    [301], [Moved Permanently], [413], [Payload Too Large],
    [302], [Moved Temporarily], [414], [URI Too Long],
    [303], [See Other], [429], [Too Many Requests],
    [304], [Not Modified], [500], [Internal Server Error],
    [400], [Bad Request], [501], [Not Implemented],
    [401], [Unauthorized], [502], [Bad Gateway],
    [403], [Forbidden], [503], [Service Unavailable],
    [], [], [505], [HTTP Version Not Supported],
    [], [], [507], [Insufficient Storage],
  )
]

For any other code, the server writes message HTTPD909E, #cmd("UNSUPPORTED HTTP
STATUS") followed by the code and #cmd("REQUESTED, SENT 500"), and it sends a
500 instead.

== Response Headers <ref-http-hdr>

#idx("response headers")
#idx("Date header")
#idx("Server header")
#idx("X-Powered-By")
#idx("Cache-Control")
#idx("Content-Type")
#idx("X-Content-Type-Options")
When a module or the server asks for a status line, the server writes these
lines first, in this order.

#tab(caption: [The lines the server writes with every status line])[
  #table(columns: (1.5in, 1fr),
    [Line], [Content],
    [Status line], [#cmd("HTTP/1.1") or #cmd("HTTP/1.0"), the same as in the
      request. A request without a version gets #cmd("HTTP/1.0").],
    [#cmd("Date")], [The current time in GMT, in the form
      #cmd("Tue, 27 Oct 2026 05:41:42 GMT").],
    [#cmd("Server")], [#cmd("HTTPD Server"). The text is a constant and has no
      version.],
    [#cmd("X-Powered-By")], [#cmd("MVSLOVERS").],
    [#cmd("Connection")], [#cmd("keep-alive") or #cmd("close")
      (@ref-http-ka).],
  )
]

The server adds more headers in these cases.

- *A static file.* The server adds #cmd("Cache-Control: no-store") if the MIME
  type contains the text #cmd("html") or the path ends in #cmd(".html"). Then it
  writes #cmd("Content-Type"), #cmd("X-Content-Type-Options: nosniff") and
  #cmd("Content-Length"). The length is written only if the server can read the
  size of the file, the size is not 0, and the file is not an SSI file
  (@ref-http-ssi).
- *An error that the server sends* (404, 405, 501, the 400 and the 413 of the
  parser, 401 and 403). The server adds #cmd("Cache-Control: no-store") and
  #cmd("Content-Type: text/plain"). The 500 of the server has a content type
  but no #cmd("Cache-Control"). The 414, the 505 and the 400 of the header reader
  have a content type but no #cmd("Cache-Control"), too.
- *A response with no length.* See @ref-http-chunk.

The server supports no range requests and no conditional requests. It ignores
the headers #cmd("Range"), #cmd("If-Modified-Since") and similar headers, and
it sends the whole file with a 200.

#screen(raw(read("../ex/ug-static/range-ignored.txt")))

#screen(raw(read("../ex/ug-static/conditional-ignored.txt")))

== Keep-Alive and Time-outs <ref-http-ka>

#idx("keep-alive")
#idx("Connection header")
#idx("time-out", "client")
#idx("CLIENT_TIMEOUT")
#idx("KEEPALIVE_TIMEOUT")
#idx("KEEPALIVE_MAX")
=== Keep-alive <ref-http-ka-rules>

The server decides after it has read the request headers.

- An HTTP/1.1 request is kept alive unless it has #cmd("Connection: close").
- An HTTP/1.0 request is closed unless it has #cmd("Connection: keep-alive").
- A request with a #cmd("Content-Length") or a #cmd("Transfer-Encoding")
  header is closed (@ref-http-body).
- If the connection has already served #cmd("KEEPALIVE_MAX") requests, the
  response to the next request says #cmd("Connection: close"), and the server
  closes the connection after it.

After a response, the server frees the request and waits for the next one on the
same connection. The response header #cmd("Connection") shows the decision.

The next screens are a run with #cmd("KEEPALIVE_MAX=2"). The first two
responses say #cmd("keep-alive"). The third says #cmd("close").

#screen(raw(read("../ex/ug-config/keepalive-max-prm.txt")))

#screen(raw(read("../ex/ug-config/keepalive-max-three.txt")))

An HTTP/1.0 request gets a closed connection and a #cmd("Content-Length").

#screen(raw(read("../ex/ug-static/http10-request.txt")))

=== Time-outs <ref-http-timeouts>

#idx("time-out", "keep-alive")
#idx("CLIENT_TIMEOUT_MSG")
#idx("CLIENT_TIMEOUT_DUMP")
The server waits for each line of a request for a limited time. The timer starts
again for each line. The limit is #cmd("KEEPALIVE_TIMEOUT") seconds for the
second and each later request of a connection. It is #cmd("CLIENT_TIMEOUT")
seconds for the first request.

When the time is up, the server closes the connection. It sends no HTTP
response. If the server has received at least one character of the request and
#cmd("CLIENT_TIMEOUT_MSG") is on, it writes a line to the console. If
#cmd("CLIENT_TIMEOUT_DUMP") is on, a dump of the received text follows. Both
keywords are on by default. The line has no message ID.

The next screens show a client that sends a part of a request line and then
waits. The Parmlib sets the time-out to 3 seconds.

#screen(raw(read("../ex/ug-config/client-timeout-prm.txt")))

#screen(raw(read("../ex/ug-config/client-timeout-client.txt")))

#screen(raw(read("../ex/ug-config/client-timeout-console.txt")))

The defaults and the allowed values are these. "The HTTPPRM Keywords" (@ref-prm)
has the keywords in full.

#tab(caption: [Defaults and ranges of the time-out keywords])[
  #table(columns: (1.8in, 0.9in, 1fr),
    [Keyword], [Default], [Range],
    [#cmd("CLIENT_TIMEOUT")], [10], [1 to 255. Zero, a negative value and text
      that is not a number are ignored, and the default stays.],
    [#cmd("KEEPALIVE_TIMEOUT")], [5], [1 to 255.],
    [#cmd("KEEPALIVE_MAX")], [100], [1 to 255.],
  )
]

=== A stalled send <ref-http-stall>

#idx("send stall")
#idx("HTTPD065W")
#idx("HTTPD045E")
When the client does not take data, the server tries again every 0.1 second. It
gives up after 100 tries without progress, that is, after 10 seconds. It then
abandons the client and writes message HTTPD065W. The server writes this
message at most once in 60 seconds. It counts the messages that it suppresses
and shows the count in the next one. A server that is stopping does not wait
for the 10 seconds.

If the server cannot read a file from the UFS in the middle of a response, it
writes message HTTPD045E, cuts the response, and closes the connection. The
server writes this message at most once in 60 seconds, too, and the next one
shows the count of the errors that it held back. For an error in a file that a
server-side include pulled in (@ref-http-ssi), the message names that file.

== Chunked Transfer <ref-http-chunk>

#idx("chunked transfer")
#idx("Transfer-Encoding")
The server uses chunked transfer when it does not know the length of a
response. When the server writes the empty line that ends the headers, it adds
the header #cmd("Transfer-Encoding: chunked") if all of these are true.

- The request is HTTP/1.1.
- The response has no #cmd("Content-Length") header. The server finds out by
  looking for a header line that starts with #cmd("Content-Length:").
- The status is not 1xx, not 204 and not 304.
- The request is not a HEAD. A response to a HEAD has no body, so it is not
  chunked and has no final chunk.

Each chunk is the length in hexadecimal digits, a CR LF, the data, and a
CR LF. The server ends the response with a chunk of length 0. If the server
cannot send a chunk, it turns off the chunk framing and keep-alive. An
HTTP/1.0 response is never chunked.

This applies to the responses of modules, too. A module only has to send a
#cmd("Content-Length") header if it wants to avoid chunking. The responses of
the server to errors, such as the 404 in the next screen, are sent chunked.

#screen(raw(read("../ex/ug-static/not-found.txt")))

== Static Files <ref-http-static>

#idx("static files")
#idx("document root")
#idx("DOCROOT")
The server reads static files from the UFS only. It does not read them from a
data set. If the server has no UFS, it finds no file, and every request for a
static path gets a 404. A static file is served for a GET, and a HEAD gets the
headers of the GET (@ref-http-methods). POST, PUT and DELETE get a 501.

=== The path <ref-http-path>

#idx("path traversal")
#idx("index.html")
#idx("default.html")
The server joins the document root and the decoded path. It applies these
rules.

- A path with two dots in a row, anywhere in it, is refused with a 404.
- The server cuts the path to 255 characters, and the joined name to 255
  characters. It does not tell the client.
- An empty path gets a 404.
- If the path ends in a slash, the server tries #cmd("index.html") and then
  #cmd("default.html") in that directory. If neither exists, or the name does not
  fit into the buffer of 300 bytes, the answer is a 404.
- If the path names a directory and does not end in a slash, the answer is a
  404.

#screen(raw(read("../ex/ug-static/dir-index.txt")))

#screen(raw(read("../ex/ug-static/dir-noslash.txt")))

#screen(raw(read("../ex/ug-static/dir-noindex.txt")))

=== MIME types <ref-http-mime>

#idx("MIME types")
#idx("Content-Type", "static files")
#idx("file extension")
The server takes the type from the extension of the file name. The extension
is the text after the last dot in the last part of the path. The comparison
ignores case. A file name with no extension, or with an extension that is not in
the table, gets #cmd("text/plain") and is sent as text. A file #cmd("data.json")
is an example.

#screen(raw(read("../ex/ug-static/json.txt")))

The table has a column "Body". *Text* means that the server opens the file as
text and translates it from EBCDIC to ASCII. *Binary* means that the server
opens the file in binary mode and sends the bytes as they are. The table
lists the types as they are in the server, also the ones where the setting is
not what the name of the type suggests (for example #cmd("sh") and
#cmd("tex") are binary).

#tab(caption: [The MIME types of the server])[
  #table(columns: (1.5in, 2.4in, 0.7in),
    [Extensions], [Content-Type], [Body],
    [#cmd("ai")], [#cmd("application/postscript")], [Binary],
    [#cmd("aif"), #cmd("aifc"), #cmd("aiff")], [#cmd("audio/x-aiff")], [Binary],
    [#cmd("au"), #cmd("snd")], [#cmd("audio/basic")], [Binary],
    [#cmd("avi")], [#cmd("video/x-msvideo")], [Binary],
    [#cmd("bcpio")], [#cmd("application/x-bcpio")], [Binary],
    [#cmd("bin")], [#cmd("application/octet-stream")], [Binary],
    [#cmd("c"), #cmd("h"), #cmd("lua"), #cmd("rex"), #cmd("rexx"), #cmd("txt")],
      [#cmd("text/plain")], [Text],
    [#cmd("cdf"), #cmd("nc")], [#cmd("application/x-netcdf")], [Binary],
    [#cmd("cpio")], [#cmd("application/x-cpio")], [Binary],
    [#cmd("csh")], [#cmd("application/x-csh")], [Binary],
    [#cmd("css")], [#cmd("text/css")], [Text],
    [#cmd("dvi")], [#cmd("application/x-dvi")], [Binary],
    [#cmd("eps"), #cmd("ps")], [#cmd("application/postscript")], [Text],
    [#cmd("etx")], [#cmd("text/x-setext")], [Text],
    [#cmd("gif")], [#cmd("image/gif")], [Binary],
    [#cmd("gtar")], [#cmd("application/x-gtar")], [Binary],
    [#cmd("hdf")], [#cmd("application/x-hdf")], [Binary],
    [#cmd("htm"), #cmd("html")], [#cmd("text/html")], [Text],
    [#cmd("ico"), #cmd("icon")], [#cmd("image/x-icon")], [Binary],
    [#cmd("ief")], [#cmd("image/ief")], [Binary],
    [#cmd("jpe"), #cmd("jpeg"), #cmd("jpg")], [#cmd("image/jpeg")], [Binary],
    [#cmd("js")], [#cmd("application/x-javascript")], [Text],
    [#cmd("latex")], [#cmd("application/x-latex")], [Binary],
    [#cmd("lod")], [#cmd("application/x-load")], [Binary],
    [#cmd("man")], [#cmd("application/x-troff-man")], [Text],
    [#cmd("me")], [#cmd("application/x-troff-me")], [Text],
    [#cmd("mif")], [#cmd("application/x-mif")], [Binary],
    [#cmd("mov"), #cmd("qt")], [#cmd("video/quicktime")], [Binary],
    [#cmd("movie")], [#cmd("video/x-sgi-movie")], [Binary],
    [#cmd("mpe"), #cmd("mpeg"), #cmd("mpg")], [#cmd("video/mpeg")], [Binary],
    [#cmd("ms")], [#cmd("application/x-troff-ms")], [Binary],
    [#cmd("oda")], [#cmd("application/oda")], [Binary],
    [#cmd("pbm")], [#cmd("image/x-portable-bitmap")], [Binary],
    [#cmd("pdf")], [#cmd("application/pdf")], [Binary],
    [#cmd("pgm")], [#cmd("image/x-portable-graymap")], [Binary],
    [#cmd("png")], [#cmd("image/png")], [Binary],
    [#cmd("pnm")], [#cmd("image/x-portable-anymap")], [Binary],
    [#cmd("ppm")], [#cmd("image/x-portable-pixmap")], [Binary],
    [#cmd("ras")], [#cmd("image/x-cmu-raster")], [Binary],
    [#cmd("rgb")], [#cmd("image/x-rgb")], [Binary],
    [#cmd("rng"), #cmd("xml")], [#cmd("text/xml")], [Text],
    [#cmd("roff"), #cmd("t"), #cmd("tr")], [#cmd("application/x-troff")], [Binary],
    [#cmd("rtf")], [#cmd("application/rtf")], [Binary],
    [#cmd("rtx")], [#cmd("text/richtext")], [Binary],
    [#cmd("sh")], [#cmd("application/x-sh")], [Binary],
    [#cmd("shar")], [#cmd("application/x-shar")], [Binary],
    [#cmd("shtm"), #cmd("shtml")], [#cmd("text/x-server-parsed-html")], [Text],
    [#cmd("src")], [#cmd("application/x-wais-source")], [Text],
    [#cmd("sv4cpio")], [#cmd("application/x-sv4cpio")], [Binary],
    [#cmd("sv4src")], [#cmd("application/x-sv4crc")], [Binary],
    [#cmd("svg")], [#cmd("image/svg+xml")], [Text],
    [#cmd("tar")], [#cmd("application/x-tar")], [Binary],
    [#cmd("tcl")], [#cmd("application/x-tcl")], [Binary],
    [#cmd("tex")], [#cmd("application/x-tex")], [Binary],
    [#cmd("texi"), #cmd("texinfo")], [#cmd("application/x-texinfo")], [Binary],
    [#cmd("tif"), #cmd("tiff")], [#cmd("image/tiff")], [Binary],
    [#cmd("tsv")], [#cmd("text/tab-separated-values")], [Text],
    [#cmd("ustar")], [#cmd("application/x-ustar")], [Binary],
    [#cmd("wav")], [#cmd("audio/x-wav")], [Binary],
    [#cmd("xbm")], [#cmd("image/x-xbitmap")], [Binary],
    [#cmd("xhtml")], [#cmd("application/xhtml+xml")], [Text],
    [#cmd("xpm")], [#cmd("image/x-xpixmap")], [Binary],
    [#cmd("xsl"), #cmd("xslt")], [#cmd("text/xsl")], [Text],
    [#cmd("xwd")], [#cmd("image/x-xwindowdump")], [Binary],
    [#cmd("zip")], [#cmd("application/zip")], [Binary],
  )
]

Two screens show the headers of a CSS file and of a PNG file.

#screen(raw(read("../ex/ug-static/css.txt")))

#screen(raw(read("../ex/ug-static/png.txt")))

=== Server-side includes <ref-http-ssi>

#idx("server-side includes")
#idx("SSI")
The server processes server-side includes in a file with the MIME type
#cmd("text/html") or #cmd("text/x-server-parsed-html"). These are the extensions
#cmd("htm"), #cmd("html"), #cmd("shtm") and #cmd("shtml"). The response is
chunked on HTTP/1.1, because it has no #cmd("Content-Length"). The directory
index in @ref-http-path shows this.

The server knows three directives. A directive starts with #cmd("<!--#") and
ends at the first blank-and-marker, #cmd(" -->"), so a blank must stand in front
of the closing marker. A line can hold several directives.

#tab(caption: [Server-side include directives])[
  #table(columns: (2.6in, 1fr),
    [Directive], [Effect],
    [#cmd("<!--#echo var=\"X\" -->")], [Inserts the value of the request
      variable X. If there is none, the server looks for an environment variable
      of its own, of that name (see below). If there is neither, it inserts the
      comment #cmd("<!-- Oops: we didn't find var=\"X\" -->").],
    [#cmd("<!--#include virtual=\"path\" -->")], [Inserts the file. The form
      with #cmd("file=") is the same.],
    [#cmd("<!--#printenv -->")], [Inserts one HTML table, headed
      #cmd("Server Variables"), with a row for each variable of the request
      (@ref-api-env). It lists nothing of the environment of the server.],
  )
]

#idx("server-side includes", "server environment")
The name in an #cmd("echo") directive is looked up in two places, in this order.
First the server looks at the request variables (@ref-api-env). If the request
has no variable of that name, the server looks at its own environment, the one
that the server task runs with, and inserts the variable of that name from it.
So a page can print a single variable of the server environment by name. The
name comes from the page, never from the request: a client cannot choose it and
cannot make the server read any other variable. The directive can therefore be
used only by someone who can put a page into the document root. A client cannot
do it. The directive #cmd("printenv") never reads the server environment.

#note[The server drops the second look in HTTPD 4.6.0 (httpd#421). From that
version #cmd("echo") reads the request variables only, and a variable that is
not among them gives the #cmd("Oops") comment. A page that prints a variable of
the server environment must not rely on this.]

#idx("server-side includes", "escaping")
The server writes every value that #cmd("echo") inserts, and every name and
value that #cmd("printenv") lists, as HTML text. The five characters
#cmd("&"), #cmd("<"), #cmd(">"), #cmd("\"") and #cmd("'") become the entities
#cmd("&amp;"), #cmd("&lt;"), #cmd("&gt;"), #cmd("&quot;") and #cmd("&#39;"). A
header value from the client therefore reaches the page as text and not as
markup. The server never cuts a value to do this. Text that comes from the
page itself, from the file or from a file that it includes, is sent as it is.

Text that the server does not recognize as a directive is sent as it is. That
covers an unknown directive name and a missing blank in front of the closing
marker. A directive that has a known name but a bad argument is not sent as it
is. The server writes a comment in its place:
#cmd("<!-- Unable to parse 'echo' request -->") or
#cmd("<!-- Unable to parse 'include' request -->").

Includes can be nested up to 10 levels. Below that level the server writes
#cmd("<!-- Oops: Maximum Server Side Include levels exceeded -->") in place of
the include. An include of a file that does not exist gets
#cmd("<!-- Oops: we didn't find \"path\" -->"), and an include of a file with a
binary MIME type gets a comment that starts with #cmd("<!-- Oops: we can't
include a binary file"). The comment that the server writes in place of a
directive is limited to 4095 characters. A value that #cmd("echo") or
#cmd("printenv") writes is not cut at that limit. A read error in an included
file ends the response as it does in the page itself (@ref-http-stall). The
includes that follow on the same line are not opened.

The server reads the file line by line and looks for directives in each line. A
line can be of any length. The server reads a long line in pieces and sends it
as one line. A directive that a read cuts in two is joined again, and the server
recognizes it, if it is at most 384 characters long. A longer directive that a
read cuts in two is not recognized in every case. The server then sends it as
text, like any other text that is not a directive. The same happens when the
output buffer of the response is still full after the server has tried to send
it, at the moment the rest of a long line is due: the part of the line that was
held back goes out as text, and a directive that spans that cut is not
recognized.

== Character Translation <ref-http-xlate>

#idx("code page")
#idx("CODEPAGE")
#idx("translation", "ASCII EBCDIC")
The network carries ASCII. The server works in EBCDIC. The Parmlib keyword
#cmd("CODEPAGE") selects the table: #cmd("CP037") (the default),
#cmd("IBM1047") or #cmd("LEGACY"). The name can be in any case. An unknown name
gives message HTTPD070E, and the server uses #cmd("CP037"). See "Files and
Character Set" in @ref-prm.

The server translates from ASCII to EBCDIC with that table in these places.

- The request line and the header lines.
- The escapes #cmd("%xx") of the path and of the query values.
- The body of a POST.
- The Basic credentials.

It translates from EBCDIC to ASCII with that table in these places.

- Everything that is written with the printf function of the server. This is
  the status line, the headers, the chunk lengths, the texts of the error
  responses, the login pages and the pages of the display modules.

Two kinds of data are not translated with the table of the server.

- *A static file.* A file of the type "Text" in the MIME table is translated
  with the code page #cmd("IBM-1047"), whatever the keyword says. A file of the
  type "Binary" is not translated.
- *Data a module sends with the send function.* The server sends these bytes as
  they are. The module has to translate them.

== Workers and Threads <ref-http-wrk>

#idx("worker tasks")
#idx("MAXTASK")
#idx("MINTASK")
#idx("socket thread")
#idx("threads")
The server has one socket thread and a pool of worker threads. The socket
thread has a stack of 32 KB. Each worker has a stack of 64 KB.

The socket thread waits for a connection. It accepts the connection, sets the
socket to non-blocking, allocates the control block of the client, and puts it
in the queue of the pool. A worker takes one connection from the queue and
serves it until the connection is closed. A connection with keep-alive
therefore holds one worker until the connection closes, also while it is idle.
It holds the worker for at most #cmd("KEEPALIVE_TIMEOUT") seconds between two
requests.

The pool has at least #cmd("MINTASK") and at most #cmd("MAXTASK") workers. The
defaults are 3 and 9. Both values can be 1 to 255 in the Parmlib. A value of 0
or below is ignored. The console commands #cmd("SET MAXTASK") and
#cmd("SET MINTASK") change the values while the server runs, in the range 1 to
100 (see "Console Commands" in @ref-con). The Parmlib keywords are in
@ref-prm.

There is no limit on the queue of connections that wait for a worker, except
the storage of the region. The keyword #cmd("LISTEN_QUEUE") sets the length of
the queue of the TCP/IP stack for the listener. If the pool is stopping, the
server closes the new connection without a response.

#tab(caption: [Events in the socket thread and the workers])[
  #table(columns: (0.9in, 1fr),
    [Message], [Cause],
    [HTTPD052E], [The accept call fails.],
    [HTTPD053E], [The server cannot set the socket to non-blocking.],
    [HTTPD999E], [The server cannot allocate the control block of the client.
      It closes the socket.],
    [HTTPD901E], [A worker has gone through its loop 1000 times for one
      connection without progress.],
    [HTTPD062E], [A worker abends. The recovery closes the connection, and the
      worker carries on.],
    [HTTPD063E], [The recovery for a worker could not be set up.],
  )
]

The texts of the messages are in @apx-messages.

== Other Hard Limits <ref-http-lim>

#idx("limits", "hard")
#idx("counters", "statistics")
#idx("D STATS")
#tab(caption: [Fixed limits of the server])[
  #table(columns: (2.2in, 1fr),
    [Item], [Limit],
    [A request line or header line], [3998 characters (@ref-http-lines)],
    [Header lines in a request], [No limit.],
    [Parameters of a request], [256 (@ref-http-params)],
    [Body of a request that the server reads], [3999 bytes (@ref-http-body)],
    [Name and value of an environment variable], [8192 bytes together],
    [Output of one call of the printf function of the server], [5119
      characters. The server cuts the rest.],
    [Path of a static file], [255 characters, and 255 for the joined name],
    [Nesting of server-side includes], [10 levels],
    [Routes], [See @ref-route.],
    [Statistics counters], [The counters for requests, errors, bytes sent and
      active connections are 32-bit unsigned numbers. They wrap at
      4294967295.],
  )
]

The counter of the errors counts each response with a status of 400 or more.
The counter of the bytes counts what the server hands to the send function. That
includes the headers and the chunk framing. A response that does not reach its
end, for example after a time-out, is not counted. The console command
#cmd("D STATS") shows the counters (see "Console Commands" in @ref-con).

=== The read of the request <ref-http-recv>

#idx("recv", "one byte")
The server reads the request line and the headers one byte at a time, because
the TCP/IP stack of MVS 3.8j corrupts data when a read of several bytes crosses
the wrap of its ring buffer.
