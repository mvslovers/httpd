#import "../bookmaster/bookmaster.typ": *

= A First Request <ug-first>

#idx("first request")
This chapter sends a few requests to a running HTTPD with #cmd("curl") and
reads what comes back: the welcome page, a plain text file, a file that is not
there, and two requests on one connection. The requests ask for static files
from the document root. Routes, server modules and logins come in later
chapters.

You need a running HTTPD (@ug-install, @ug-ops), the host name of the MVS
system and the port of HTTPD. The examples run #cmd("curl") on a machine that
reaches HTTPD as #cmd("localhost") on port 8080, which is the default of
#cmd("PORT"). Replace #cmd("localhost:8080") with the host name and port of
your system. The web root image that ships with HTTPD holds the welcome page
and nothing else. The other files in the examples (#cmd("hello.txt"),
#cmd("data.json") and a few more) are small test files. To repeat an example,
put a file of that name into your web root (@ug-static).

== What the Console Shows <ug-first-console>

#idx("start", "console messages")
#idx("HTTPD001I")
Before the first request, look at the console. A healthy start ends with
message #cmd("HTTPD001I"). The lines before it show what HTTPD has set up:

#screen(raw(read("../ex/ug-ops/start-healthy.txt")))

The lines have this meaning:

#deflist(width: 1.0in,
  [#cmd("HTTPD000I")], [The start. It names the version and the build of the
    program.],
  [#cmd("HTTPD005I")], [The C runtime library that HTTPD is linked with.],
  [#cmd("HTTPD002I")], [How the started task got its authorization: by the
    library it was fetched from, or by an SVC (the case in the capture).],
  [#cmd("HTTPD004I")], [The started task has given up the default identity
    that RAKF gives to every started task. It runs under the user ID and the
    group from the #cmd("STCUSER") and #cmd("STCGRP") values of the procedure
    (here #cmd("HTTPD") and #cmd("USER")).],
  [#cmd("HTTPD036I")], [One line for each #cmd("MOD=") route: the server
    module and the path it is registered for. A configuration without
    #cmd("MOD=") routes has none of these lines.],
  [#cmd("HTTPD054I")], [HTTPD listens on the port. #cmd("ANY") means all
    interfaces.],
  [#cmd("HTTPD061I")], [A thread has started: the socket thread first, then
    the workers. The default is three workers.],
  [#cmd("HTTPD001I")], [HTTPD is ready. The text after #cmd("SERVING") is the
    document root.],
)

Do not send requests before #cmd("HTTPD001I") has been written. How to start,
stop and watch HTTPD is the subject of @ug-ops. A message that is not in this
list is described under "Messages" in the _HTTPD Reference_.

== Choosing a Client <ug-first-client>

#idx("client")
#idx("curl")
Any HTTP client can talk to HTTPD. A browser shows the page. #cmd("curl")
shows more, and four forms of it are used in this book:

- *#cmd("curl url")* prints the body of the response and nothing else. Use it
  to see what a page contains.
- *#cmd("curl -i url")* prints the status line and the headers first, then
  the body. Use it for most of the sessions in this book.
- *#cmd("curl -v url")* prints the request that #cmd("curl") sends too, each
  line with the prefix #cmd(">"), and the response, each line with the prefix
  #cmd("<"), and some lines of its own that start with #cmd("*"). Use it
  when it matters what the client sent, for example for the connection that
  is reused in "Two Requests on One Connection".
- *#cmd("curl -I url")* sends a HEAD request and prints the status line and
  the headers, and nothing else. HTTPD answers a HEAD for a static file with
  the headers that a GET gets, and with no body:

#screen(raw(read("../ex/ug-static/head-request.txt")))

The #cmd("Content-Length") is the size of the file. HTTPD 4.4.0 and earlier
answer this request with #cmd("501 Not Implemented"). A HEAD request for a path
that belongs to a server module goes to the module, and the module decides
(@ug-routes).

== Getting the Welcome Page <ug-first-welcome>

#idx("welcome page")
#idx("index.html")
A request for #cmd("/") asks for a directory. HTTPD looks in that directory
for #cmd("index.html") and then for #cmd("default.html") (see
@ug-static-dirs). The web root that ships with HTTPD has an #cmd("index.html"),
the welcome page. Fetch it:

#screen(```
curl http://localhost:8080/
```)

The command prints the HTML text of the page. The page has the title
"Welcome to HTTPD", the line "It works." and the text "You are connected to
HTTPD, an HTTP/1.1 server running on MVS 3.8j." A browser at the same URL
shows it as a page. The headers are of more interest. This command prints
them and nothing else:

#screen(raw(read("../ex/ug-first/welcome-headers.txt")))

The option #cmd("-s") keeps #cmd("curl") quiet about its progress, #cmd("-D -")
writes the response headers to the standard output, and #cmd("-o /dev/null")
throws the body away.

== Reading the Response <ug-first-read>

#idx("status line")
#idx("response headers")
The first line is the status line: the HTTP version, the status code and a
short text. HTTPD answers in the version of the request. A request in
HTTP/1.1 gets #cmd("HTTP/1.1"), and a request in HTTP/1.0 gets
#cmd("HTTP/1.0"). The headers follow, one on each line, up to an empty line.
The body comes after it.

Every response that HTTPD writes starts with the same four headers:

#deflist(width: 1.3in,
  [#cmd("Date")], [The date and time at which HTTPD built the response, in
    the form of RFC 1123.],
  [#cmd("Server")], [Always #cmd("HTTPD Server"). It does not name the
    version.],
  [#cmd("X-Powered-By")], [Always #cmd("MVSLOVERS").],
  [#cmd("Connection")], [#cmd("keep-alive") if HTTPD takes another request on
    this connection, #cmd("close") if it closes the connection after this
    response.],
)

A response for a static file adds more headers. The examples show them in the
order in which HTTPD writes them:

#deflist(width: 1.3in,
  [#cmd("Cache-Control")], [#cmd("no-store"), sent for HTML documents. It
    keeps a browser from keeping a copy of the page. A text file has no such
    header.],
  [#cmd("Content-Type")], [The type of the file, chosen from the end of its
    name (@ug-static-mime).],
  [#cmd("X-Content-Type-Options")], [#cmd("nosniff"). It tells a browser to
    believe the #cmd("Content-Type").],
  [#cmd("Content-Length")], [The size of the file in bytes. A response without
    it uses #cmd("Transfer-Encoding: chunked") instead.],
)

The welcome page above has #cmd("Transfer-Encoding: chunked") and no
#cmd("Content-Length"). HTTPD scans an HTML document for server side includes
while it sends it, so it does not know the length in advance
(@ug-static-ssi). A text file has a #cmd("Content-Length"):

#screen(raw(read("../ex/ug-first/hello-file.txt")))

The file is 61 bytes long. The body follows the empty line. To see the whole
conversation and not only the response, use #cmd("curl -v"):

#screen(raw(read("../ex/ug-static/file-verbose.txt")))

The lines with #cmd(">") are what #cmd("curl") sent. A request in HTTP/1.1
must have a #cmd("Host") header. HTTPD refuses one without it with a
#cmd("400") response.

== A File That Is Not There <ug-first-404>

#idx("404")
#idx("not found")
A request for a path that is not in the document root is answered with
#cmd("404 Not Found"):

#screen(raw(read("../ex/ug-static/not-found.txt")))

The body is one line of plain text. It repeats the path as HTTPD decoded it
(@ug-static-path). The same answer comes for a directory without an index
file and for a path with two dots in it. The answer does not say which of
these it was. The 404 response has #cmd("Cache-Control: no-store") and a body
of plain text. The status codes of HTTPD are in "HTTP Behaviour and Limits" in
the _HTTPD Reference_.

== Two Requests on One Connection <ug-first-keepalive>

#idx("keep-alive")
#idx("persistent connection")
A client that wants two files can send both requests on one connection. Give
#cmd("curl") two URLs:

#screen(raw(read("../ex/ug-static/keepalive-two-urls.txt")))

The line "Re-using existing http: connection" shows it. The first response has
#cmd("Connection: keep-alive"), so HTTPD waits for the next request. It waits
for #cmd("KEEPALIVE_TIMEOUT") seconds (5 by default), and
#cmd("KEEPALIVE_MAX") limits the number of requests on one connection (100 by
default, plus the one that is answered with #cmd("Connection: close")). Both are described in "The HTTPPRM Keywords" in the _HTTPD
Reference_. When the limit is reached, HTTPD answers with
#cmd("Connection: close") and closes the connection after the response.

An HTTP/1.0 client gets a different answer. In HTTP/1.0 a connection ends
after one response, unless the client asks for #cmd("Connection: keep-alive"):

#screen(raw(read("../ex/ug-static/http10-request.txt")))

The status line says #cmd("HTTP/1.0") and the response has
#cmd("Connection: close"). HTTPD closes the connection after the body.

A request in HTTP/1.1 can end the connection too. The client can send
#cmd("Connection: close"), and a request that has a body (a
#cmd("Content-Length") or a #cmd("Transfer-Encoding") header) is always the
last one on its connection. @ug-static-methods shows examples.

== What Can Go Wrong <ug-first-wrong>

#idx("troubleshooting", "first request")
- *The connection is refused or times out.* HTTPD is not running, the port
  is not the value of #cmd("PORT"), or a firewall is in the way. Look for
  #cmd("HTTPD001I") and #cmd("HTTPD054I") on the console. @ug-trouble covers
  a start that fails.
- *Every request for a file gets a 404, also the one for #cmd("/").* The
  document root has no files, or HTTPD cannot reach it. @ug-static describes
  the document root and the web root image.
- *A request for the path of a server module gets a 401.* The route needs a
  login (@ug-security).
- *The page is old.* A browser shows what it kept in its cache. HTML
  documents carry #cmd("Cache-Control: no-store"), other files do not.
