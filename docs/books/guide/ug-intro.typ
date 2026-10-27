#import "../bookmaster/bookmaster.typ": *

= Introducing HTTPD <ug-intro>

#idx("HTTPD", "overview")
#idx("introduction")
HTTPD is an HTTP/1.1 server for MVS 3.8j. It runs as a started task and
listens on a TCP port. This chapter says what HTTPD does, what it is made of,
how it handles a request, what it needs on the system, and which chapter of
this book to read for which task.

== What HTTPD Does <ug-intro-what>

#idx("HTTPD", "purpose")
HTTPD has two jobs.

- *It hosts server modules.* A server module is a load module that HTTPD
  calls for the requests to a path. The main module is mvsMF, which gives
  MVS a REST interface in the style of z/OSMF. Clients such as scripts,
  command line tools and web front ends use it to work with data sets, jobs
  and the console. @ug-routes explains how a path is tied to a module, and
  @ug-module explains how to write one.
- *It serves static files.* HTML pages, style sheets, images and other files
  come from a file system that UFSD keeps (@ug-static). A request that no
  module owns is answered from there.

HTTPD answers a request in the version of HTTP that the client used, and it
keeps a connection open for more requests when the client allows it. The
check of a user against the security product and the SMF record of a request
are part of the server.

#idx("HTTPD", "limits")
HTTPD is not a general purpose web server. Some things it does not do:

- It speaks HTTP over plain TCP. It has no TLS support.
  @ug-security says what that means for a login.
- It does not start a program for each request. A server module is a load
  module that HTTPD calls in its own task.
- It does not take files. A POST, PUT or DELETE request for a static file is
  refused, and so is a HEAD request (@ug-static-methods).

== The Parts <ug-intro-parts>

#idx("started task")
#idx("Parmlib member")
#idx("route")
#idx("server module")
#idx("web root")
A running HTTPD is made of these parts:

#deflist(width: 1.35in,
  [Started task], [The program HTTPD runs as a started task from a procedure.
    The procedure names the load library, the Parmlib member and the identity
    under which HTTPD runs. You start it with the operator command
    #cmd("S HTTPD") and stop it with #cmd("P HTTPD") (@ug-ops). The procedure
    is described under "The Started Task and the Package" in the _HTTPD
    Reference_.],
  [Parmlib member], [The configuration. The #cmd("HTTPPRM") statement of the
    procedure points to a member of a Parmlib data set. The member holds lines
    of the form #cmd("KEYWORD=VALUE"): the port, the number of worker threads,
    the document root and the routes. Without a member HTTPD starts with its
    defaults and no routes (@ug-config).],
  [Routes], [A route ties a path to something that answers it. A
    #cmd("MOD=") line names a server module, and a #cmd("LOC=") line names a
    path that is served from the files. Each route says whether a client
    has to log in. The first route that matches the path is the one that is
    used, and a path that no route matches is public and is served from the
    files (@ug-routes).],
  [Server modules], [Programs that HTTPD loads when a request for their route
    arrives. A module is only active when a route names it. The sample
    Parmlib member names mvsMF.],
  [Web root], [The files that HTTPD serves. They live in a UFS file system
    that UFSD keeps in a data set, below the path of the #cmd("DOCROOT")
    keyword. HTTPD reaches them through the UFSD started task (@ug-static).],
  [RAKF], [The security product of the system. HTTPD asks it to check a user
    ID and a password, and to check whether a user may use a resource
    (@ug-security). RAKF also provides the SVC through which HTTPD can obtain
    its authorization (see "What HTTPD Needs").],
)

== How a Request Is Handled <ug-intro-flow>

#idx("request", "flow")
#idx("worker thread")
#idx("socket thread")
A request passes the same steps, whatever it asks for:

+ The socket thread of HTTPD waits for connections. When a client connects,
  the connection goes to a worker thread. The number of workers lies between
  #cmd("MINTASK") (3 by default) and #cmd("MAXTASK") (9 by default).
+ The worker reads the request line and the header lines from the client.
  A request that is too long or malformed is refused here.
+ The worker parses the request: the method, the path, the query string and,
  for a POST, the body.
+ The worker works out who the client is from the credential that came with
  the request (a session cookie, a bearer token or a user ID and a password),
  and looks for the route that matches the path.
+ The route decides whether the request may go on. A route that needs a login
  answers a client that has not logged in with a #cmd("401") response. A
  client that has logged in but may not use the route gets a #cmd("403").
  The paths #cmd("/login") and #cmd("/logout") belong to HTTPD itself and are
  answered before this check.
+ If the route names a server module, HTTPD calls the module, and the module
  writes the response. Otherwise HTTPD looks for a file in the web root and
  sends it.
+ HTTPD writes an SMF record for the request if the #cmd("SMF") keyword asks
  for one. The default is no record.
+ If the connection can carry another request, the worker resets itself and
  waits for the next request on the same connection. Otherwise it closes the
  connection.

The steps and their limits are listed under "HTTP Behaviour and Limits" in the
_HTTPD Reference_. @ug-first shows the steps at work on a request for a file.

== What HTTPD Needs <ug-intro-needs>

#idx("prerequisites")
#idx("requirements")
The installation steps are in @ug-install. In short, a system needs this:

#deflist(width: 1.35in,
  [MVS 3.8j], [The operating system that HTTPD is written for.],
  [APF authorization], [HTTPD has to be authorized when it runs. There are two
    ways. One is the SVC that RAKF provides (SVC 244), through which the
    program obtains the authorization itself. The other is an entry for the
    load library in the APF list. If HTTPD cannot obtain the authorization,
    it writes message #cmd("HTTPD012E"). @ug-trouble covers that case.],
  [RAKF], [Needed for logins. HTTPD checks a user ID and a password with
    RAKF.],
  [UFSD], [Needed for static files. HTTPD links the UFS library into its load
    module, so there is nothing more to install, but the UFSD started task has
    to run and hold a web root image. A site that uses HTTPD only for server
    modules can set #cmd("UFS=0") (@ug-static-noufsd). The build of HTTPD requires UFSD 1.5.2
    or later.],
  [mvsMF], [Needed only for the routes of the sample Parmlib member that name
    it. Without such a route HTTPD serves files only.],
)

HTTPD runs under a user ID of its own. The procedure sets it with the
#cmd("STCUSER") and #cmd("STCGRP") values, and @ug-install tells how to define
that user to RAKF.

== Version and Lineage <ug-intro-version>

#idx("history")
#idx("lineage")
HTTPD began as a web server by Mike Rayborn for MVS 3.8j, and the last version
of that line is 3.3. HTTPD 4.0 is a rewrite by Mike Großmann with HTTP/1.1,
persistent connections and a configuration in Parmlib, and this book
describes the 4.5 release of that line.

The version of a running server is in message #cmd("HTTPD000I") on the
console (@ug-first-console).

== Which Chapter for Which Task <ug-intro-map>

#idx("chapters", "overview")
Read the chapters in order the first time. After that, use the table to find
the one you need.

#tab(caption: [Chapters and tasks])[#table(
  columns: (3.2in, 1fr),
  align: left,
  [*I want to ...*], [*Chapter*],
  [Install HTTPD on a system], [@ug-install],
  [Send the first request and read the answer], [@ug-first],
  [Set the port, the worker threads, the time limits and the other
    keywords], [@ug-config],
  [Tie a path to mvsMF or another module, or to a static area], [@ug-routes],
  [Make a route need a login, or limit it to some users], [@ug-security],
  [Serve HTML pages and other files], [@ug-static],
  [Start and stop HTTPD, read its messages and look inside a running server],
    [@ug-ops],
  [Write a server module of my own], [@ug-module],
  [Move from one release to the next], [@ug-upgrade],
  [Remove HTTPD from a system], [@ug-remove],
  [Find out why something does not work], [@ug-trouble],
  [Look up a word], [@ug-glossary],
)]

Every keyword, command, message and limit is listed in full in the _HTTPD
Reference_ (ML07-0002). This book explains how to do things and points to the
Reference where a list is long.
