#import "../bookmaster/bookmaster.typ": *

= Routes and Authentication Policy <ref-route>

#idx("routes")
#idx("route table")
A route ties a path of a request to what HTTPD does with it. A route is written
in the HTTPPRM member (@ref-prm) as a #cmd("MOD") line or a #cmd("LOC") line. A
#cmd("MOD") route names a program, a server module that HTTPD links to. A
#cmd("LOC") route has no program: HTTPD serves the path as a static file. Both
can carry a policy, which says who may use the route. This chapter is the full
description of the syntax, of the matching and of the policy. The _HTTPD
User's Guide_ shows how to build routes for common cases, in "Routes and Server
Modules" and in "Authentication and Security".

== The Syntax of MOD and LOC <ref-route-syntax>

#idx("MOD", "syntax")
#idx("LOC", "syntax")
#idx("AUTH=")
#idx("RES=")
#tab(caption: [The route lines])[
  #table(columns: (1.0in, 1fr),
    [Line], [Form],
    [#cmd("MOD")], [#cmd("MOD=program [pattern] [AUTH=mode] [RES=class:resource]")],
    [#cmd("LOC")], [#cmd("LOC=pattern [AUTH=mode] [RES=class:resource]")],
  )
]

#tab(caption: [The parts of a route line])[
  #table(columns: (1.1in, 1fr),
    [Part], [Rules],
    [#var("program")], [The name of the load module. The first token after the
      equal sign. HTTPD changes it to upper case and keeps at most 8
      characters. A longer name is cut without a message. A module name is at
      most 8 characters on MVS, so the cut name is the one that HTTPD links to.
      Required for #cmd("MOD").],
    [#var("pattern")], [The path that the route claims. See @ref-route-pat. For
      #cmd("MOD") it is the second token, and it is optional. If it is left
      out, HTTPD derives it from the program name (@ref-route-ext). For
      #cmd("LOC") it is the first token and it is required. A token that starts
      with #cmd("AUTH="), #cmd("RES=") or #cmd("RECLAIM="), in any case, is
      an option and never a pattern.],
    [#cmd("AUTH=")#var("mode")], [The login that the route needs: #cmd("NONE"),
      #cmd("FORM"), #cmd("BASIC") or #cmd("TOKEN"), in any case. See
      @ref-route-auth.],
    [#cmd("RES=")#var("class:resource")], [A RACF resource that the user must be
      allowed to read. See @ref-route-res-gate.],
    [#cmd("RECLAIM=")#var("x")], [Retired. HTTPD ignores it and writes
      #cmd("HTTPD422W"). The storage of a module is always released after the
      request.],
  )
]

The rules for the line as a whole:

- The tokens are separated by blanks or tabs. The options may come in any
  order, but after the pattern.
- HTTPD reads at most 8 tokens. It ignores the rest of the line without a
  message.
- A token that is not a pattern and not one of the options above gives
  #cmd("HTTPD413W") with the token, and HTTPD ignores it. The route is still
  registered. See the warning in @ref-route-err.
- An option may stand twice. For #cmd("AUTH=") the last valid mode counts, but a
  mode that is not valid refuses the route whatever follows. For #cmd("RES=")
  the last well-formed value counts.
- Each line adds one route. HTTPD does not check for a route that is there
  already, and it sets no limit on the number of routes.
- #cmd("CGI=") is the old name of #cmd("MOD="). HTTPD writes #cmd("HTTPD410W")
  and reads the line as a #cmd("MOD") line.

The member may contain these lines:

#screen(```
MOD=MVSMF /zosmf/restconsoles/*   AUTH=TOKEN RES=FACILITY:MVSMF.CONSOLE
MOD=MVSMF /zosmf/*                AUTH=TOKEN
MOD=HTTPDSRV /.dsrv               AUTH=FORM
LOC=/admin/*                      AUTH=BASIC RES=FACILITY:HTTPD.ADMIN
LOC=/*                            AUTH=NONE
```)

== The Pattern Language <ref-route-pat>

#idx("pattern", "route")
#idx("wildcard")
#idx("first match")
HTTPD compares the path of the request with the pattern of each route. The path
is the part of the URI before the first question mark. HTTPD decodes it first:
each #cmd("%xx") becomes the character. A plus sign stays a plus sign
(@ref-http-url). The query string never takes part in the match.

A pattern is a wildcard pattern if it contains #cmd("*") or #cmd("?"). Any
other pattern is a plain path.

#tab(caption: [The two kinds of pattern])[
  #table(columns: (1.2in, 1fr),
    [Kind], [Rules],
    [Plain path], [The path of the request and the pattern must be equal. HTTPD
      ignores the case of the letters. #cmd("/status") matches #cmd("/STATUS").
      It does not match #cmd("/status/").],
    [Wildcard pattern], [HTTPD matches the whole path against the pattern with
      the library function #cmd("__patmat"). The case of the letters counts.
      #cmd("*") stands for any characters, also a slash. At the end of the
      pattern it also stands for no character. Anywhere else it stands for at
      least one character. #cmd("?") stands for one character. Any other
      character stands for itself.],
  )
]

Some examples:

#tab(caption: [Examples of patterns])[
  #table(columns: (1.5in, 1.6in, 1fr),
    [Pattern], [Matches], [Does not match],
    [#cmd("/zosmf/*")], [#cmd("/zosmf/info"), #cmd("/zosmf/a/b")], [#cmd("/zosmf"), #cmd("/ZOSMF/info")],
    [#cmd("/.dsrv")], [#cmd("/.dsrv"), #cmd("/.DSRV")], [#cmd("/.dsrv/x")],
    [#cmd("*.rexx")], [#cmd("/a/b.rexx")], [#cmd("/a/b.REXX"), #cmd(".rexx")],
    [#cmd("/a*b")], [#cmd("/axb"), #cmd("/a/b")], [#cmd("/ab")],
    [#cmd("/doc?.html")], [#cmd("/doc1.html")], [#cmd("/doc12.html")],
  )
]

A prefix therefore needs a trailing #cmd("*"). The pattern #cmd("/admin") claims
only that one path.

=== The First Match Wins

HTTPD keeps all routes, #cmd("MOD") and #cmd("LOC") together, in one table in
the order of the lines. For a request it walks the table from the top and uses
the first route that matches. It never looks for the best match, and it does not
test prefixes before extensions. The order of the lines is the only thing that
sets the priority.

Two consequences follow.

- A route with a general pattern hides every route below it that it also
  matches. The line #cmd("LOC=/*") before #cmd("MOD=MVSMF /zosmf/*") sends
  every request to the #cmd("LOC") route. HTTPD gives no message.
- The policy of the first route is the whole policy of the request. The policy
  of a later route that would match too does not apply.

Write the specific routes first and the general ones last.

A request that no route matches is public. HTTPD serves it as a static file
(@ref-route-loc).

=== The Fixed Paths

#idx("/login")
#idx("/logout")
The paths #cmd("/login") and #cmd("/logout") belong to HTTPD. A request for one
of them goes to the login code of HTTPD whatever the routes say and whatever
the method is, and no policy applies. The match ignores the case of the letters
and needs the whole path. Do not give a route to these two paths.

The paths that match #cmd("/login.*") and #cmd("/favicon.*") are exempt from
the policy for the methods GET and HEAD, so that the login page can load its
own parts. The case of the letters counts here. The route still applies: HTTPD
only skips the check of the login. A #cmd("MOD") route that claims such a path
runs its module without a login.

== Routes with an Extension <ref-route-ext>

#idx("extension routing")
#idx("SCRIPT_FILENAME")
If a #cmd("MOD") line has no pattern, HTTPD builds one from the program name:
#cmd("*.") followed by the program name in lower case. The line
#cmd("MOD=REXX") becomes the pattern #cmd("*.rexx").

A pattern that starts with #cmd("*.") makes an extension route, whether HTTPD
built it or the line states it. For a request on an extension route HTTPD sets
the variable #cmd("SCRIPT_FILENAME") to the document root (#cmd("DOCROOT"),
@ref-prm) followed by the path of the request. If HTTPD cannot set the
variable, it answers with an internal error (status 500) and does not link to
the module. No other route gets this variable from HTTPD.

The pattern is a wildcard pattern, so the match is case sensitive: #cmd("*.rexx")
does not match #cmd("/a/b.REXX"). The position in the table counts as for any
other route.

== The Login Mode of a Route <ref-route-auth>

#idx("AUTH=", "modes")
#idx("authentication", "route policy")
#idx("HTTPD411E")
Each route has one of four modes. The mode says whether the route needs a user,
and how HTTPD asks a client that has none.

#tab(caption: [The values of AUTH=])[
  #table(columns: (0.8in, 1fr),
    [Mode], [Rules],
    [#cmd("NONE")], [The route is public. HTTPD does not check a login. This is
      also the mode of a route with no #cmd("AUTH=") and no #cmd("RES=").],
    [#cmd("FORM")], [The route needs a login. A client without one gets the
      HTML login form.],
    [#cmd("BASIC")], [The route needs a login. A client without one gets status
      401 with a Basic challenge.],
    [#cmd("TOKEN")], [The route needs a login. A client without one gets status
      401 and no challenge.],
  )
]

The rules for a missing or a wrong mode:

- A line with #cmd("RES=") and without #cmd("AUTH=") gets the mode
  #cmd("BASIC"), because a resource check needs a user.
- A line with #cmd("AUTH=NONE") and with #cmd("RES=") gives
  #cmd("HTTPD414W"). The mode #cmd("NONE") wins, and HTTPD does not check the
  resource.
- A mode that is not one of the four gives #cmd("HTTPD411E") with the first 24
  characters of the value. HTTPD refuses the route, and the start ends
  (@ref-route-err).

The mode does not choose the way the client proves who it is. Before HTTPD looks
for the route, it tries these sources in this order and uses the first one that
gives a user.

+ The cookie #cmd("Sec-Token"), if it holds a valid session token.
+ The cookie #cmd("LtpaToken2"), if it holds a valid session token.
+ The header #cmd("Authorization: Bearer") with a session token.
+ The header #cmd("Authorization: Basic") with user and password.

If neither cookie gives a user, HTTPD reads the #cmd("Authorization") header.
A Bearer header ends the search, also when its token is not valid. A header
that is neither Bearer nor Basic gives no user.

The login is bound to the address of the client. A user that logged in from
another address does not count. Every route accepts every source, whatever its
mode. The mode only says what HTTPD sends to a client that has no login.

=== What Each Mode Answers

#idx("401")
#idx("403")
HTTPD runs the check in two stages, before it serves a file or links to a
module. The first stage asks for a user. The second stage applies only if the
route has #cmd("RES="). It asks RACF whether the user may read the resource.
The two stages apply to #cmd("MOD") routes and to #cmd("LOC") routes in the
same way, for the methods GET, HEAD, PUT, POST and DELETE.

#tab(caption: [What HTTPD answers to a request without a login])[
  #table(columns: (0.8in, 1fr),
    [Mode], [Answer],
    [#cmd("NONE")], [None. HTTPD serves the request.],
    [#cmd("FORM")], [A GET gets status 302 with #cmd("Location: /login"). The
      cookie #cmd("Sec-Uri") holds the URI of the request, encoded in base64,
      and the form sends the user back to it after the login. A POST is read as
      a login attempt: the fields #cmd("userid") and #cmd("password") of the
      body are the user. A request with another method gets status 200 and a
      text that says #cmd("Invalid REQUEST_METHOD").],
    [#cmd("BASIC")], [Status 401 with the headers #cmd("WWW-Authenticate: Basic
      realm=\"name\""), #cmd("Cache-Control: no-store") and
      #cmd("Content-Type: text/plain"). The body is the line #cmd("401
      Unauthorized"). The name is the value of #cmd("REALM") (@ref-prm). If the
      request has the header #cmd("X-CSRF-ZOSMF-HEADER"), whatever its value,
      HTTPD leaves out the #cmd("WWW-Authenticate") header.],
    [#cmd("TOKEN")], [The same as #cmd("BASIC"), but HTTPD never sends the
      #cmd("WWW-Authenticate") header, whatever the request holds.],
  )
]

A request with a login, to a route with #cmd("RES="), goes to the second stage.
If the user may not read the resource, HTTPD answers with status 403, the
headers #cmd("Cache-Control: no-store") and #cmd("Content-Type: text/plain"),
and the body #cmd("403 Forbidden"). Otherwise HTTPD goes on with the request.

== The Resource Check <ref-route-res-gate>

#idx("RES=", "resource check")
#idx("RACF", "resource check")
#idx("HTTPD412W")
#idx("HTTPD425W")
#idx("HTTPD426W")
#idx("HTTPD418E")
#tab(caption: [The value of RES=])[
  #table(columns: (1.5in, 1fr),
    [Rule], [Detail],
    [Form], [#cmd("RES=class:resource"). HTTPD splits the value at the first
      colon. The class is the text before it and the resource is the rest, so
      the resource may contain colons. HTTPD keeps the case as written.],
    [Access asked], [READ.],
    [A wrong value], [A value without a colon, and a value with nothing after
      the colon, give #cmd("HTTPD412W") with the value. HTTPD ignores the
      option and registers the route. Without an #cmd("AUTH=") the route is
      then public.],
    [Storage], [If HTTPD cannot get storage for the value, it writes
      #cmd("HTTPD418E"). The route is refused, and the start ends.],
    [Two #cmd("RES="), one line], [The last well-formed value counts.],
    [With #cmd("AUTH=NONE")], [HTTPD ignores the resource. See
      @ref-route-auth.],
    [Resource without a profile], [HTTPD counts the return code 4 of the
      check, "no profile covers the resource", as permitted, and the return
      code 0 as well. Every code of 8 and above is a refusal. If the security
      product answers a resource without a profile with 4, nothing is checked
      in fact. RAKF answers 4. Whether another product does depends on its
      settings, not on HTTPD; @ref-stc-rakf has the detail. If it answers
      with 8 or above, the user gets the 403, and HTTPD writes no
      #cmd("HTTPD425W"). RAKF 2.0.0 (MVS/CE) answers a class that it does not define with 8. A
      mistake in the resource name, and for RAKF 1.2.x (measured on TK5) a class
      that is not defined, looks like a missing profile, so HTTPD
      checks every route with a resource once at the start, and writes
      #cmd("HTTPD425W") for each one that has no profile. The message names the
      class, the resource and the pattern of the route. HTTPD does not check
      routes with the mode #cmd("NONE"). The keyword #cmd("RES_NOPROFILE=DENY")
      changes the first part: a request to a route whose resource has no
      profile then gets status 403, and the start writes #cmd("HTTPD426W")
      instead of #cmd("HTTPD425W") (@ref-prm-auth).],
    [Denied], [Status 403.],
  )
]

== Limits <ref-route-limits>

#idx("limits", "routes")
#tab(caption: [Limits of the route lines])[
  #table(columns: (1.9in, 1fr),
    [Limit], [Value],
    [Characters read from one line], [255 (@ref-prm-rules).],
    [Tokens read from one line], [8. HTTPD ignores the rest without a message.],
    [Characters of a program name], [8. HTTPD cuts a longer name without a
      message.],
    [Number of routes], [No limit from HTTPD. A route that cannot be added for
      lack of storage gives #cmd("HTTPD035W") or #cmd("HTTPD423W").],
    [Duplicates], [HTTPD does not look for them. The first of two equal routes
      hides the second.],
    [Reading the member], [Once, at the start. A change of a route needs a new
      start.],
  )
]

== Messages While HTTPD Reads the Routes <ref-route-err>

#idx("HTTPD419E")
#idx("HTTPD420E")
#idx("HTTPD421W")
#idx("route errors")
HTTPD reads the whole member before it decides whether to start. The messages
below come while it reads. They go to the console.

#tab(caption: [Messages of the route lines])[
  #table(columns: (1.1in, 0.7in, 1fr),
    [Message], [Start], [Meaning],
    [#cmd("HTTPD036I")], [goes on], [A #cmd("MOD") route is registered. The
      message gives the program and the pattern.],
    [#cmd("HTTPD417I")], [goes on], [A #cmd("LOC") route is registered.],
    [#cmd("HTTPD410W")], [goes on], [A line used the old name #cmd("CGI=").],
    [#cmd("HTTPD421W")], [see below], [A #cmd("MOD") line without a program
      name. An option stands where the name belongs, or the value is empty.],
    [#cmd("HTTPD415W")], [see below], [A #cmd("LOC") line without a path. An
      option stands where the path belongs, or the value is empty.],
    [#cmd("HTTPD411E")], [ends], [An #cmd("AUTH=") value that is not valid.
      HTTPD refuses the route, and writes #cmd("HTTPD419E").],
    [#cmd("HTTPD412W")], [goes on], [A #cmd("RES=") value without class and
      resource. HTTPD ignores it.],
    [#cmd("HTTPD413W")], [goes on], [A token that is not an option. HTTPD
      ignores it.],
    [#cmd("HTTPD414W")], [goes on], [#cmd("AUTH=NONE") with #cmd("RES="). The
      resource is not checked.],
    [#cmd("HTTPD418E")], [ends], [No storage for #cmd("RES="). HTTPD writes
      #cmd("HTTPD419E") too.],
    [#cmd("HTTPD422W")], [goes on], [A line used #cmd("RECLAIM="). HTTPD
      ignores it.],
    [#cmd("HTTPD035W")], [see below], [HTTPD could not register a #cmd("MOD")
      route. The message gives the program and the pattern.],
    [#cmd("HTTPD423W")], [see below], [HTTPD could not register a #cmd("LOC")
      route.],
    [#cmd("HTTPD419E")], [ends], [A route that asked for a login or a resource
      is lost. The message gives the kind and the pattern.],
    [#cmd("HTTPD420E")], [ends], [The end of the check, see below.],
  )
]

A route that asked for a login or for a resource, and that HTTPD cannot build,
is never dropped quietly. Without the route the path would be public. HTTPD
therefore writes #cmd("HTTPD419E") for each such line. When it has read the
whole member, it writes #cmd("HTTPD420E"), ends with CC 0008 and does not open
the port. These lines belong to this case:

- A line with an #cmd("AUTH=") value that is not valid.
- A line with #cmd("RES=") for which HTTPD gets no storage.
- A line in which an option stands where the program name or the path belongs.
  An example is #cmd("LOC=AUTH=BASIC RES=FACILITY:HTTPD.ADMIN"). For such a
  line #cmd("HTTPD419E") gives the whole line. HTTPD checks the options to
  decide: a line that asks for a login or a resource ends the start. A line
  with only #cmd("AUTH=NONE") or without options gives #cmd("HTTPD421W") or
  #cmd("HTTPD415W") and the start goes on.
- A line for which HTTPD gets no storage to split it into tokens.
- A route with a login or a resource that HTTPD cannot add to the table.

A route with the mode #cmd("NONE"), and a route without any policy, that HTTPD
cannot register give a warning (#cmd("HTTPD035W"), #cmd("HTTPD423W")) and the
start goes on. Losing them cannot make the path less protected than the line
asked.

#note[HTTPD checks only the words it knows. A line like
#cmd("MOD=ADMIN /admin/* AUTH BASIC") has the tokens #cmd("AUTH") and
#cmd("BASIC"). HTTPD writes #cmd("HTTPD413W") twice and registers a public
route. Look for #cmd("HTTPD413W") in the log after each change of the member.
The same holds for #cmd("RES=") with a wrong value and without #cmd("AUTH=").]

== Static Routes <ref-route-loc>

#idx("LOC", "static routes")
#idx("static files", "routes")
A #cmd("LOC") route has no program. The two stages of the check apply as
described above. If the request passes, HTTPD serves the path as a static file
from the document root (#cmd("DOCROOT"), @ref-prm), for the methods GET, HEAD,
PUT, POST and DELETE. A request that no route matches is served the same way,
without a check.

A #cmd("LOC") route is the way to protect a part of the document tree. The
route does not change what HTTPD serves. It decides who may get it.

Three rules help:

- A route with the mode #cmd("NONE") and a general pattern, as in
  #cmd("LOC=/* AUTH=NONE"), claims every path that is not claimed above it. It
  is a good last line. As a first line it hides all that follows.
- A pattern that ends in #cmd("/*") does not claim the directory itself. Add a
  plain-path route for it.
- The login page lives in the document tree. Its paths #cmd("/login.*") are
  exempt (@ref-route-pat), so a #cmd("LOC=/* AUTH=FORM") route still lets the
  page load.

== Resident Modules <ref-route-res>

#idx("RESIDENT", "start")
#idx("HTTPD440W")
#idx("HTTPD441W")
#idx("HTTPD442W")
By default (#cmd("RESIDENT=YES"), @ref-prm) HTTPD loads each module of a
#cmd("MOD") route once, after it has read the member and before it serves any
request. It holds the copy until the end. A request then finds the module in
storage and does not fetch it from the library. HTTPD does this once for each
program name, however many routes use the name. The comparison of the names
ignores the case.

A module that HTTPD does not hold still works. Each request fetches it, as with
#cmd("RESIDENT=NO"). The start shows why in a message.

#tab(caption: [Messages of the resident modules])[
  #table(columns: (1.1in, 1fr),
    [Message], [Meaning],
    [#cmd("HTTPD440W")], [The module is not found at the start. HTTPD does not
      hold it. If it is there later, the route works.],
    [#cmd("HTTPD441W")], [The module is not reentrant and not serially
      reusable. A copy that is held could run twice, so HTTPD does not hold the
      module.],
    [#cmd("HTTPD442W")], [The load failed. The message gives the abend code and
      the reason code.],
  )
]

A module that HTTPD holds is loaded in the main task of the server. The end of
the server releases it. The command #cmd("D CONFIG") shows the number of modules
held (@ref-con).

== When a Module Does Not Run <ref-route-fail>

#idx("module load failure")
#idx("HTTPD908E")
#idx("HTTPD906W")
#idx("503")
If HTTPD cannot run the module of a #cmd("MOD") route, or the module ends badly,
HTTPD first releases the storage of the module (a failure of that gives
#cmd("HTTPD906W") with the program name and the return code). Then it tells the
client and the operator.

For the client, HTTPD sends status 503 with the header
#cmd("Content-Type: text/plain") if the module has not sent a status line yet.
Then it sends the line #cmd("External program name text") to the body, where
#var("text") is one of the reasons in the table. For the operator, HTTPD writes
#cmd("HTTPD908E") with the program name and the same text.

#tab(caption: [The reasons for a failure])[
  #table(columns: (2.2in, 1fr),
    [Text], [Cause],
    [#cmd("was not found")], [The module is in neither the STEPLIB nor the link
      list (completion code S806).],
    [#cmd("could not be loaded")], [The LINK failed with another completion
      code, for example a fetch error or a storage shortage. A module that
      returns -1 itself gives the same text.],
    [#cmd("was dispatched without a program name")], [The route has no program
      name.],
    [#cmd("could not be started, ESTAE create failed")], [HTTPD could not set up
      the error exit for the LINK, so the module did not run.],
    [#cmd("returned rc n")], [The module ran and ended with the negative return
      code n. A module that returns a code of zero or more is not a failure.],
    [#cmd("failed with Sxxx ABEND")], [The module ended with the system abend
      xxx.],
    [#cmd("failed with Unnnn ABEND")], [The module ended with the user abend
      nnnn, in decimal. #cmd("U0801") with the console line
      #cmd("@@CRT0 - No storage for C stack") means that the region had no
      block for the stack of the module (@ref-stc-region).],
  )
]

Only the abends leave a dump to look for. The other reasons mean that the module
did not run, or ended by itself.
