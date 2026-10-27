#import "../bookmaster/bookmaster.typ": *

= Routes and Server Modules <ug-routes>

#idx("routes")#idx("server module")#idx("MOD")#idx("LOC")
A route ties a path of a request to what HTTPD does with it. Without a route,
HTTPD serves every path as a file. With a route, a path can go to a program
(a server module), or it can stay a file and ask for a login first. This
chapter shows how to write the routes in the Parmlib member, in which order,
and what happens when a module is not there. The login that a route asks for
is the subject of @ug-security. The full syntax and the full list of messages
are in 'Routes and Authentication Policy' in the _HTTPD Reference_.

== The Two Kinds of Route <ug-routes-kinds>

#idx("MOD", "route")#idx("LOC", "route")
A route is one line in the member. There are two forms.

#tab(caption: [The route lines])[
  #table(columns: (0.7in, 1fr),
    [Line], [Form],
    [#cmd("MOD")], [#cmd("MOD=program [pattern] [AUTH=mode] [RES=class:resource]")],
    [#cmd("LOC")], [#cmd("LOC=pattern [AUTH=mode] [RES=class:resource]")],
  )
]

A #cmd("MOD") route names a program. HTTPD links to the program, and the
program answers the request. A #cmd("LOC") route has no program. HTTPD serves
the path as a file from the document root, as it does without a route. The
only difference is the policy: a #cmd("LOC") route can ask for a login or a
RACF permission, and a path without a route cannot.

Both forms have the same options. #cmd("AUTH=") says how HTTPD asks a client
for a login. #cmd("RES=") names a RACF resource that the user must be allowed
to read. Both are explained in @ug-security. A route without any option is
public.

== MOD= Routes <ug-routes-mod>

#idx("MOD=")#idx("program name")#idx("extension route")#idx("SCRIPT_FILENAME")
The first word after #cmd("MOD=") is the name of the load module. HTTPD changes
it to upper case and keeps at most 8 characters. A longer name is cut without a
message. The second word is the pattern: the path that the route claims.

```
MOD=MVSMF /zosmf/* AUTH=TOKEN
```

This line sends every path that starts with #cmd("/zosmf/") to the module
#cmd("MVSMF"). The patterns are described in @ug-routes-order.

If you leave out the pattern, HTTPD builds one from the program name: #cmd("*.")
and the name in lower case. The line #cmd("MOD=REXX") becomes the pattern
#cmd("*.rexx"). A pattern that starts with #cmd("*.") makes an extension route.
For a request on an extension route HTTPD sets the variable
#cmd("SCRIPT_FILENAME") to the document root followed by the path of the
request, so that the module knows which file the client asked for. No other
route gets this variable.

Each #cmd("MOD") line adds one route. Many routes can use the same module, as
the examples in this chapter show. HTTPD reads at most 8 words of a line.
Words after the eighth are ignored without a message.

== LOC= Routes <ug-routes-loc>

#idx("LOC=")#idx("static files", "protect")
A #cmd("LOC") route is the way to protect part of the document tree. The
route does not change what HTTPD serves. It decides who may get it.

```
LOC=/private/*   AUTH=BASIC
LOC=/pub/*       AUTH=NONE
```

The first line asks for a user and a password for every path below
#cmd("/private/"). The second line makes every path below #cmd("/pub/")
public explicitly.

Two details prevent surprises:

- A pattern that ends in #cmd("/*") does not claim the directory itself. A
  route for #cmd("/private/*") does not cover the path #cmd("/private").
  Add a plain-path route when you need that.
- A line that is only options, such as
  #cmd("LOC=AUTH=BASIC RES=FACILITY:HTTPD.ADMIN"), has no path. HTTPD does not
  start with such a line, because the path would be public
  (@ug-routes-bad).

The serving of the files is described in @ug-static.

== The Order of the Routes and the First Match <ug-routes-order>

#idx("first match")#idx("pattern", "route")#idx("wildcard")
HTTPD keeps all routes, #cmd("MOD") and #cmd("LOC") together, in one table in
the order of the lines. For a request it walks the table from the top and uses
the first route that matches. It never looks for the best match. The order of
the lines is the only thing that sets the priority. The policy of the first
route is the whole policy of the request. A later route that also matches is
not asked.

Write the specific routes first and the general ones last.

=== What a Pattern Matches

HTTPD matches the path of the request, without the query string. A pattern is
of one of two kinds.

#tab(caption: [The two kinds of pattern])[
  #table(columns: (1.2in, 1fr),
    [Kind], [Rules],
    [Plain path], [The path and the pattern must be equal. The case of the
      letters does not count. #cmd("/status") matches #cmd("/STATUS") and does
      not match #cmd("/status/").],
    [With #cmd("*") or #cmd("?")], [The whole path must match. The case of the
      letters counts. #cmd("*") stands for any characters, also a slash.
      #cmd("?") stands for one character.],
  )
]

The captures below use this member. It has two plain-path routes, both
protected by Basic authentication:

#screen(raw(read("../ex/ug-routes/pattern-exact-prm.txt")))

A plain path claims exactly that path and ignores the query string. The
request for #cmd("/hello.txt") asks for a login, and so does the request with a
query:

#screen(raw(read("../ex/ug-routes/pattern-exact-file.txt")))
#screen(raw(read("../ex/ug-routes/pattern-exact-query.txt")))

The route #cmd("LOC=/pub") does not claim a path below it. The file
#cmd("/pub/notice.txt") is public:

#screen(raw(read("../ex/ug-routes/pattern-exact-prefix.txt")))

A path that no route claims is public and is served as a file:

#screen(raw(read("../ex/ug-routes/pattern-exact-other.txt")))

If you want to protect everything below a directory, write
#cmd("LOC=/pub/*"). A route for a directory and for everything below it needs
two lines, one for the directory and one with the trailing #cmd("/*").

=== The First Line Wins

Two routes for the same path show the rule. In the first member the Basic route
stands first:

#screen(raw(read("../ex/ug-routes/order-basic-first-prm.txt")))

The first route claims the path. The second route is never used, and the request
needs a login:

#screen(raw(read("../ex/ug-routes/order-basic-first.txt")))

In the second member the public route stands first:

#screen(raw(read("../ex/ug-routes/order-public-first-prm.txt")))

Now the same request is public. The route that asks for the login is dead, and
HTTPD gives no message:

#screen(raw(read("../ex/ug-routes/order-public-first.txt")))

#note[*A general route above a specific one hides it.* A line such as
#cmd("LOC=/* AUTH=NONE") makes all routes below it unreachable. If it
stands above a protected route, that route protects nothing. HTTPD does not
warn about this. After a change of the routes, request the protected path
without a login and check that you get the 401.]

=== The Fixed Paths

#idx("/login")#idx("/logout")
The paths #cmd("/login") and #cmd("/logout") belong to HTTPD. A request for one
of them goes to the login code of HTTPD whatever the routes say. Do not write
a route for them. The paths that match #cmd("/login.*") and
#cmd("/favicon.*") are exempt from the login for the methods GET and HEAD, so
that the login page can load its parts (@ug-security).

== The Standard Module: mvsMF <ug-routes-mvsmf>

#idx("mvsMF")#idx("MVSMF")#idx("/zosmf")#idx("z/OSMF")
The module that HTTPD is mostly used for is mvsMF. It answers the REST API of
z/OSMF below the path #cmd("/zosmf/"). mvsMF is a separate product. Its load
module is called #cmd("MVSMF"). This is the member that the captures of this
guide use. It has the routes for mvsMF, and it routes the display modules
(@ug-ops).

#screen(raw(read("../ex/ug-routes/httpprm-lab-prm.txt")))

Read the four #cmd("MVSMF") routes from top to bottom.

- #cmd("/zosmf/info") and #cmd("/zosmf/services/authenticate") are
  #cmd("AUTH=NONE"). HTTPD lets the request through, and mvsMF asks for the
  password itself. A request to #cmd("/zosmf/info") without a password gets its
  401 from mvsMF:

#screen(raw(read("../ex/ug-security/mvsmf-info-open.txt")))

- #cmd("/zosmf/restconsoles/*") is #cmd("AUTH=TOKEN") and has a
  #cmd("RES="). The user needs READ on the profile #cmd("MVSMF.CONSOLE") in the
  class #cmd("FACILITY").
- #cmd("/zosmf/*") is #cmd("AUTH=TOKEN"). It takes everything else.

The order matters. The route for the console stands before #cmd("/zosmf/*"),
because the general route also matches the console path. If you swap the two
lines, the console path falls to the general route, and the check of the
resource is lost.

The meaning of #cmd("TOKEN") and of #cmd("RES=") is in @ug-security.

== Where the Module Must Be <ug-routes-lib>

#idx("STEPLIB")#idx("APF authorization")#idx("link list")
HTTPD links to a module by name, in the task of the server. The system finds
the module in the #cmd("STEPLIB") of the procedure or in the link list. A
module in another library is not found. Put the module into the library
that the #cmd("STEPLIB") names, or add the library to the #cmd("STEPLIB").

Whether the library is APF-authorized decides the message #cmd("HTTPD002I")
and the protection key in which the system fetches the module. This is
described in 'The Started Task and the Package' in the _HTTPD Reference_.
Because of that key, a module must not change its own static storage
(@ug-module).

== Modules Held in Storage: RESIDENT <ug-routes-resident>

#idx("RESIDENT")#idx("HTTPD440W")#idx("HTTPD441W")#idx("HTTPD442W")
#idx("module", "resident")
By default HTTPD loads each module of a #cmd("MOD") route once, when it has
read the member and before it serves a request. It holds the copy until it
stops. A request then does not fetch the module from the library. HTTPD
does this once for each program name, however many routes use the name. The
member keyword is #cmd("RESIDENT"), with the values #cmd("YES") (the default)
and #cmd("NO").

#cmd("D C") shows the number of modules held in the line #cmd("HTTPD137I").
The member above uses four programs, and the line shows #cmd("MODULES HELD 4").

A module that HTTPD cannot hold still works. Each request fetches it, as with
#cmd("RESIDENT=NO"). The start tells you why:

#tab(caption: [The messages for resident modules])[
  #table(columns: (0.95in, 1fr),
    [Message], [Meaning],
    [#cmd("HTTPD440W")], [HTTPD did not find the module at the start. It does
      not hold it. If the module is in the library later, the route works.],
    [#cmd("HTTPD441W")], [The module is not reentrant and not serially
      reusable. HTTPD does not hold it.],
    [#cmd("HTTPD442W")], [The load failed. The message gives the abend code
      and the reason code.],
  )
]

With #cmd("RESIDENT=NO") HTTPD does not look at the modules at the start. A
missing module is then silent until the first request (see
@ug-routes-missing).

== Context Blocks for Modules <ug-routes-ctx>

#idx("CGI_CONTEXT_POINTERS")#idx("context block")#idx("http_cgictx_get")
A server module must not keep data in writable static storage. When a module
needs data that lives from one request to the next, it asks HTTPD for a
context block. The module calls the function #cmd("http_cgictx_get") with an
8-byte name (an eyecatcher) and a size. The first call for a name creates
the block. Every later call with the same name, from any worker, returns the
same block. The block stays until the server stops. HTTPD never frees it.

The keyword #cmd("CGI_CONTEXT_POINTERS") is the size of the table in which
HTTPD records these blocks. Each name takes one entry. The table has one entry
more than the number. The default is 255, which is also the largest value.
If the table is full, the call returns no block (NULL) and the module has to
deal with that. A module does not need the keyword to run. Leave it at the
default unless the documentation of a module tells you to change it. How a
module uses the block is described in @ug-module and in 'The Server Module
Interface' in the _HTTPD Reference_.

== A Route to a Missing Module <ug-routes-missing>

#idx("module", "missing")#idx("HTTPD908E")#idx("503")#idx("IEA703I")
A route to a module that is not in the library is not an error of the
member. HTTPD registers the route and tells you at the start:

#screen(raw(read("../ex/ug-routes/route-nosuch-module-prm.txt")))
#screen(raw(read("../ex/ug-routes/route-nosuch-module.txt")))

The message #cmd("HTTPD036I") says that the route exists. The message
#cmd("HTTPD440W") says that the module is not found and not held. The server
starts. The failure comes with the first request:

#screen(raw(read("../ex/ug-routes/nosuch-module-request.txt")))

The client gets the status 503 and the text #cmd("External program NOSUCH was
not found"). The console shows the system message #cmd("IEA703I 806- 4") and,
after it, #cmd("HTTPD908E") (the last two lines of the start capture above).
The completion code S806 means that the system found the module neither in the
#cmd("STEPLIB") nor in the link list. If you see this, check the name in the
route, then the library (@ug-routes-lib). The other reasons for a failure are
listed in 'Routes and Authentication Policy' in the _HTTPD Reference_.

With #cmd("RESIDENT=NO") the start is silent about the module:

#screen(raw(read("../ex/ug-routes/resident-no-prm.txt")))
#screen(raw(read("../ex/ug-routes/resident-no.txt")))

== Warnings and Refusals While HTTPD Reads the Routes <ug-routes-bad>

#idx("HTTPD413W")#idx("HTTPD412W")#idx("HTTPD414W")#idx("HTTPD425W")#idx("route errors")
HTTPD reads the whole member before it decides whether to start. Most
mistakes in a route give a warning, and the route is registered. One example
shows the common ones. The member has a bad #cmd("RESIDENT") value and four
routes with options that HTTPD does not accept:

#screen(raw(read("../ex/ug-routes/warn-route-options-prm.txt")))
#screen(raw(read("../ex/ug-routes/warn-route-options.txt")))

Read the console lines one after the other.

- #cmd("HTTPD443W") is the wrong #cmd("RESIDENT") value. The earlier value
  stays.
- #cmd("HTTPD414W") is for #cmd("/a/*"). The route is public
  (#cmd("AUTH=NONE")), so HTTPD ignores the #cmd("RES=").
- #cmd("HTTPD412W") is for #cmd("/b/*"). The text #cmd("NOCOLON") has no
  colon. HTTPD ignores the #cmd("RES="), and the route has #cmd("AUTH=BASIC").
- #cmd("HTTPD413W") is for #cmd("/c/*"). HTTPD does not know the word
  #cmd("BOGUS=1") and ignores it.
- #cmd("HTTPD425W") is for #cmd("/d/*"). The route has a #cmd("RES=") and no
  #cmd("AUTH="), so it gets Basic authentication. But the security product has
  no profile for #cmd("FACILITY:HTTPD.NOPROFILE"), so any user who is logged in
  passes. The text says the route is #cmd("NOT GATED"). A mistake in the class
  or in the resource name gives the same message (@ug-security).
- #cmd("HTTPD440W") is the missing module of the first route.

HTTPD checks only the words it knows. A line such as
#cmd("MOD=ADMIN /admin/* AUTH BASIC") has the words #cmd("AUTH") and
#cmd("BASIC") separately. HTTPD gives #cmd("HTTPD413W") twice and registers a
public route. Look for #cmd("HTTPD413W") in the log after every change.

In some cases HTTPD refuses to start, because a route that asked for a login
would otherwise turn public. An #cmd("AUTH=") value that does not exist gives
#cmd("HTTPD411E"), #cmd("HTTPD419E") and #cmd("HTTPD420E"), and the return code
is 8. The capture for that is in @ug-config. A line in which an option
stands where the program name or the path belongs also refuses the start, if
it asks for a login or a resource. A line with only #cmd("AUTH=NONE"), or with
no option, gives #cmd("HTTPD421W") for #cmd("MOD") or #cmd("HTTPD415W") for
#cmd("LOC"), and the start goes on.

A change in a route takes effect when you stop and start HTTPD. To look at the
routes that run now, use the display module #cmd("HTTPDSRV") (@ug-ops).
