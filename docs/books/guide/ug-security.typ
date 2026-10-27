#import "../bookmaster/bookmaster.typ": *

= Authentication and Security <ug-security>

#idx("authentication")#idx("security")#idx("RACF")#idx("RAKF")
HTTPD checks every request against the routes of the Parmlib member
(@ug-routes). A route can ask the client for a user ID and a password, and it
can ask the security product whether that user may use a resource. This
chapter shows the four ways a route can ask, what the client sees, how long a
login lasts, which identity the server itself uses, and what HTTPD does not
do. The syntax of the route lines is in @ug-routes. The display modules that
show the logins are in @ug-ops. The rules are described in full in 'Routes and
Authentication Policy' in the _HTTPD Reference_.

The examples use the member from @ug-routes-mvsmf, the user ID #cmd("IBMUSER")
and the host name #cmd("localhost"). The passwords in the captures are masked.

== How a Request Is Checked <ug-security-flow>

#idx("authentication", "two stages")#idx("401")#idx("403")
HTTPD checks a request in two stages, before it serves a file or links to a
module. This holds for the methods GET, HEAD, PUT, POST and DELETE, for #cmd("MOD")
routes and for #cmd("LOC") routes alike.

+ *Who are you?* If the route needs a login and the client has none, HTTPD
  answers with a request to log in. The answer depends on the #cmd("AUTH=")
  mode of the route.
+ *May you?* This stage applies only to a route with #cmd("RES="). HTTPD asks
  the security product whether the user may read the resource. If not, the
  answer is 403.

HTTPD finds the user in the request itself. It tries these sources in this
order and takes the first one that gives a user:

+ the cookie #cmd("Sec-Token"),
+ the cookie #cmd("LtpaToken2"),
+ the header #cmd("Authorization: Bearer") with a session token,
+ the header #cmd("Authorization: Basic") with user ID and password.

The login is bound to the address of the client. A token that was issued to
one address does not work from another. Every route accepts every source. The
#cmd("AUTH=") mode of a route does not choose the source. It decides only what
a client without a login gets.

HTTPD changes the user ID and the password to upper case and uses the first 8
characters of each.

== AUTH=: What a Route Asks For <ug-security-auth>

#idx("AUTH=")#idx("AUTH=NONE")#idx("AUTH=BASIC")#idx("AUTH=FORM")#idx("AUTH=TOKEN")
Each route has one of four modes.

#tab(caption: [The values of AUTH=])[
  #table(columns: (0.8in, 1.2in, 1fr),
    [Mode], [Client without a login], [Use it for],
    [#cmd("NONE")], [Served.], [Public content.],
    [#cmd("BASIC")], [401 with a Basic challenge.], [Tools such as curl, and
      browsers that show a password dialog.],
    [#cmd("FORM")], [302 to the login page.], [Pages for people at a browser.],
    [#cmd("TOKEN")], [401 without a challenge.], [REST clients and scripts.],
  )
]

A route without #cmd("AUTH=") and without #cmd("RES=") is #cmd("NONE"). A route
with #cmd("RES=") and without #cmd("AUTH=") is #cmd("BASIC"). Write the mode on
every route, so that the member says what you mean. A mode that does not exist
refuses the start (@ug-config).

=== NONE

#idx("public route")
The route is public. HTTPD checks no login. The same holds for a path that no
route claims.

#screen(raw(read("../ex/ug-security/pub-open.txt")))

=== BASIC

#idx("Basic authentication")#idx("WWW-Authenticate")
A client without a login gets the status 401 and a Basic challenge. The
challenge names the realm (@ug-security-realm). A browser shows a password
dialog. The body is the text #cmd("401 Unauthorized"), and the response has
#cmd("Cache-Control: no-store").

#screen(raw(read("../ex/ug-security/basic-no-credentials.txt")))

A wrong password and an unknown user get the same answer, so a client cannot
tell which of the two it was.

#screen(raw(read("../ex/ug-security/basic-bad-password.txt")))
#screen(raw(read("../ex/ug-security/basic-unknown-user.txt")))

A correct password gets the file.

#screen(raw(read("../ex/ug-security/basic-good-password.txt")))

If the request has the header #cmd("X-CSRF-ZOSMF-HEADER"), whatever its value,
HTTPD leaves out the #cmd("WWW-Authenticate") header. The answer is still 401.
A browser then shows no password dialog.

=== FORM

#idx("form login")#idx("Sec-Token")#idx("Sec-Uri")#idx("/login")#idx("/logout")
A client without a login that sends a GET is sent to the login page. HTTPD
answers 302 with #cmd("Location: /login"). It stores the address that the
client asked for in the cookie #cmd("Sec-Uri"), encoded in base64, so that it
can send the user back after the login.

#screen(raw(read("../ex/ug-security/form-1-redirect.txt")))

The login page asks for a user ID and a password. The fields allow 8
characters.

#screen(raw(read("../ex/ug-security/form-login-fields.txt")))

The form sends a POST to #cmd("/login") with the fields #cmd("userid") and
#cmd("password"). A wrong password gets the page again, with the text
#cmd("Invalid userid or password"). The status is 200. HTTPD closes the
connection after every POST that has a body, so it does so after a failed login
and after a successful one:

#screen(raw(read("../ex/ug-security/form-2-post-bad.txt")))

A correct password gets the status 303 back to the address in #cmd("Sec-Uri").
HTTPD deletes #cmd("Sec-Uri") and sets the cookie #cmd("Sec-Token"). The
console shows #cmd("HTTPD007I") (@ug-security-console).

#screen(raw(read("../ex/ug-security/form-3-post-good.txt")))

The client sends the cookie with every later request, and the page is served:

#screen(raw(read("../ex/ug-security/form-4-use-cookie.txt")))

A GET of #cmd("/logout") ends the login. HTTPD clears both cookies:

#screen(raw(read("../ex/ug-security/form-5-logout.txt")))

After that the page asks for a login again.

#screen(raw(read("../ex/ug-security/form-6-after-logout.txt")))

A request with another method than GET or POST to a #cmd("FORM") route gets the
status 200 and a text that says #cmd("Invalid REQUEST_METHOD"). The form is for
browsers. Use #cmd("BASIC") or #cmd("TOKEN") for a program.

=== TOKEN

#idx("token")#idx("LtpaToken2")#idx("bearer token")
A client without a login gets the status 401, and HTTPD never sends a
challenge, whatever the request holds.

#screen(raw(read("../ex/ug-security/token-no-credentials.txt")))

The mode does not mean that Basic is refused. A request with a Basic header is
checked and served, like any other route.

#screen(raw(read("../ex/ug-security/token-with-basic.txt")))

A client that wants a token asks mvsMF for it. A POST to
#cmd("/zosmf/services/authenticate") with a Basic header returns a session
cookie #cmd("LtpaToken2"):

#screen(raw(read("../ex/ug-security/token-1-authenticate.txt")))

HTTPD accepts that cookie on its own #cmd("TOKEN") routes, too:

#screen(raw(read("../ex/ug-security/token-2-use-cookie.txt")))

The same cookie opens #cmd("/zosmf/info"). The next capture sends the cookie
without a password:

#screen(raw(read("../ex/ug-security/token-3-info-with-cookie.txt")))

The route #cmd("/zosmf/services/authenticate") is #cmd("AUTH=NONE") in the
member. HTTPD does not ask for a login there, because mvsMF does it itself.
A wrong password gets a 401 from mvsMF, with a JSON body:

#screen(raw(read("../ex/ug-security/token-4-bad-password.txt")))

When a request that mvsMF does not own has no login, you see the bare 401:

#screen(raw(read("../ex/ug-security/mvsmf-token-without.txt")))

=== When to Use TOKEN

#idx("TOKEN", "when to use")
Use #cmd("TOKEN") for routes that programs call, such as the REST API of
mvsMF. A program should get a plain 401 and handle it, not a login page or a
Basic challenge that it does not understand. A user at a browser does better with
#cmd("FORM"). A user of curl is fine with #cmd("BASIC"). The three modes
accept the same credentials, so a mix on one server causes no conflict.

== The Realm <ug-security-realm>

#idx("REALM")#idx("realm")#idx("HTTPD424W")
The keyword #cmd("REALM") names the server in the Basic challenge. The captures
above show #cmd("realm=\"MVS.EXAMPLE\""). The default is the SMF ID of the
system, with at most four characters. If the system has none, the default is
#cmd("MVS"). A value has 1 to 64 characters. It must not contain a control
character or one of the characters double quote, backslash, #cmd("<"),
#cmd(">") and #cmd("&"). Another value gives #cmd("HTTPD424W"), and the default
stays. The line #cmd("HTTPD136I") of #cmd("D C") shows the realm in effect
(@ug-config).

== How Long a Login Lasts <ug-security-session>

#idx("SESSION_TIMEOUT")#idx("SESSION_MAXAGE")#idx("session")#idx("credential cache")
HTTPD keeps what a client proved. For a Basic request it keeps an entry that
it finds by the address, the user ID and the password, with the RACF identity
that RACF returned. For a form login and for a token it keeps a session of its
own. Two keywords limit the time:

#tab(caption: [The limits of a login])[
  #table(columns: (1.7in, 0.6in, 1fr),
    [Keyword], [Default], [Meaning],
    [#cmd("SESSION_TIMEOUT")], [30], [Minutes without a request. Every request
      starts the time again.],
    [#cmd("SESSION_MAXAGE")], [480], [Minutes since the login, whatever the
      client does. Requests do not extend it.],
  )
]

Both values are in minutes. The value 0 switches a limit off. Both values go
from 0 to 65535. HTTPD removes the expired logins about once a minute, so
a login can last up to a minute longer than the limit says.

The limits must be longer than #cmd("CLIENT_TIMEOUT"), which counts in seconds.
A login that expires while a request is still read would be dropped under the
request. HTTPD compares the limits at the end of the start and warns:

#screen(raw(read("../ex/ug-config/warn-session-unsafe.txt")))

#cmd("HTTPD026E") is for #cmd("SESSION_TIMEOUT") and #cmd("HTTPD032E") for
#cmd("SESSION_MAXAGE"). The server starts. Raise the value. @ug-config shows
the member.

HTTPD keeps at most 256 logins. When the store is full, a new login is
rejected. A single client can hold at most 16 sessions. If it asks for a 17th,
HTTPD ends the client's session that has been idle the longest, but only if it
has been idle for at least 60 seconds. Otherwise the new login is refused, and the
form answers #cmd("Login failed, no session available"). Log out when a
script is done, so that it does not use up its places.

== A Changed RACF Password and the Cached Login <ug-security-pwchange>

#idx("password change")#idx("credential cache")#idx("stale credential")
A Basic request does not call RACF every time. HTTPD looks up the address, the
user ID and the password in its cache first. If it finds them, the request is
answered from the cache. This is why a Basic route is cheap. It has a
consequence when a password changes in RACF.

- *The new password works at once.* It is not in the cache, so HTTPD asks RACF.
- *The old password still works until the cached login ends.* A cache hit does
  not ask RACF. The login ends when it has been idle for #cmd("SESSION_TIMEOUT"),
  when it is older than #cmd("SESSION_MAXAGE"), or when a logout ends it.
  A client that uses the old password all the time keeps the login alive until
  #cmd("SESSION_MAXAGE"), counted from the first request. Requests do not
  extend that.

To make a changed password take effect for everyone at once, stop and start
HTTPD. That drops all logins.

*From HTTPD 4.5*, a login that starts a session of its own checks the cached
password with RACF again. This happens with a form login and with the
authenticate request of mvsMF. If RACF refuses it, HTTPD marks the cached login
as stale:

- RACF refuses with the return codes 4 (user not defined), 8 (wrong password),
  12 (password expired) or 28 (access revoked). Any other failure says nothing
  about the password. HTTPD then leaves the cache alone and writes
  #cmd("HTTPD009W").
- HTTPD never serves a stale login, not for the old password and not for a
  token.
- A stale login is removed after 10 minutes. The next login or the clean-up
  does this.
- If RACF accepts the password again within these 10 minutes, the mark is
  cleared.
- The form shows #cmd("Invalid userid or password") when RACF refuses the cached
  password.

A plain Basic request with the old password is not checked against RACF. It
stays in the cache until it ends or until a login of the kinds above marks it.

#note[HTTPD 4.4 and earlier never mark a login as stale. The old password
stays valid for Basic requests until the cache entry expires.]

== The Identity of the Server <ug-security-stc>

#idx("STCUSER")#idx("STCGROUP")#idx("ACEE")#idx("HTTPD004I")#idx("HTTPD004W")
#idx("started task identity")
RAKF has no table of started procedures. It gives every started task the same
account, STC/STCGROUP. On a standard RAKF profile set this account has ALTER
access to every data set. HTTPD does not want that. At the start it logs on as
a user ID of its own, with RACINIT and no password.

The user ID and the group come from the PARM string of the procedure, not from
the Parmlib member:

```
//HTTPD    EXEC PGM=HTTPD,REGION=8M,TIME=1440,
//            PARM='STCUSER=&STCUSER STCGROUP=&STCGRP'
```

The procedure has the symbols #cmd("STCUSER") (default #cmd("HTTPD")) and
#cmd("STCGRP") (default #cmd("USER")). You can override them in the START
command:

#screen(```
S HTTPD,STCUSER=WEBSRV,STCGRP=WEBGRP
```)

The messages tell what happened. They appear in every start capture of this
guide:

#screen(raw(read("../ex/ug-ops/start-healthy.txt")))

- #cmd("HTTPD004I STC IDENTITY SET TO HTTPD/USER VIA RACINIT") says that the
  server runs as the user ID and group shown.
- #cmd("HTTPD004W") says that the logon failed, for example because the user
  ID is not defined. The server keeps the identity that the task had, which
  is the more powerful one. Define the user ID and start again.
- #cmd("HTTPD003W") says that the server could not enter supervisor state for
  the logon. It also keeps the identity that the task had.

The user ID of #cmd("STCUSER") needs READ access to the Parmlib member and to
the document root. A server module that sets no identity of its own uses the
identity of the task.

At the stop the server puts the old identity back before it releases its APF
authorization. The authorization was taken under the old account and is given
back under the same account. For this reason the user ID of #cmd("STCUSER")
needs no access to the profile for SVC 244. 'The Started Task and the Package'
in the _HTTPD Reference_ explains the order.

== RES=: Checking a Profile <ug-security-res>

#idx("RES=")#idx("FACILITY class")#idx("HTTPD425W")#idx("RAKF")#idx("403")
A route with #cmd("RES=class:resource") adds the second stage. After the user
is known, HTTPD asks the security product whether the user may READ the
resource. HTTPD asks under the identity of the user, not under that of the
server. The profile belongs to you. HTTPD names none.

```
LOC=/admin/*    AUTH=BASIC RES=FACILITY:HTTPD.ADMIN
LOC=/denied/*   AUTH=BASIC RES=FACILITY:HTTPD.DENIED
```

On RAKF you define the profiles in the class #cmd("FACILITY") and permit the
users. RAKF prints this when you do. It reminds you to activate the
change:

#screen(raw(read("../ex/ug-security/rakf-define.txt")))

The lines say that the changes are not active until RAKFPROF has run.

A user who may read the resource gets the file:

#screen(raw(read("../ex/ug-security/admin-allowed.txt")))

A user who may not gets 403:

#screen(raw(read("../ex/ug-security/denied-403.txt")))

In this lab, #cmd("IBMUSER") may read #cmd("HTTPD.ADMIN"). The capture above
shows no permit for #cmd("HTTPD.DENIED"), and the user gets the 403. The 403 has the body
#cmd("403 Forbidden"). A user without a login never reaches this stage. The
user gets the 401 of the first stage.

#note[*A profile that does not exist can permit everybody.* When the security
product answers that no profile covers the resource (return code 4), HTTPD
counts the request as permitted. Whether the product gives that answer for an
unprotected resource depends on its settings. If it refuses instead (return
code 8 or above), the user gets the 403. A typing error in the resource name
looks the same as a missing profile. A class that RAKF does not know gives the
return code 8, so every user gets the 403.
HTTPD therefore asks once for each route with #cmd("RES=") at the start. For a
route without a profile it writes #cmd("HTTPD425W NO PROFILE FOR
class:resource -- path NOT GATED"). The route still works, and every user who
is logged in passes. Look for #cmd("HTTPD425W") after each change. The probe
is not made for #cmd("AUTH=NONE") routes. To make such a route refuse instead,
set #cmd("RES_NOPROFILE=DENY") (@ug-security-noprofile).]

HTTPD asks for the access READ for all methods. It has no separate permission
for PUT, POST or DELETE.

=== Refusing a Route Whose Profile Is Missing <ug-security-noprofile>

#idx("RES_NOPROFILE")#idx("HTTPD426W")#idx("HTTPD427E")#idx("HTTPD138I")
#idx("RES=", "profile missing")#idx("403", "no profile")
The default is the permissive reading of the note above. A route that was
written to close a resource is open to every user who can log in as long as its
profile is forgotten, misspelled or not loaded yet. The mvsMF console route
(#cmd("FACILITY:MVSMF.CONSOLE")) is the case that matters: without its profile
every user who can log in may issue operator commands through mvsMF up to
1.3.0.

The keyword #cmd("RES_NOPROFILE=DENY") turns the missing profile into a closed
route. It is one setting for the whole server, not an option of a route. A
switch on each route would have to be remembered on exactly the routes where a
profile is forgotten. The setting applies to every route with #cmd("RES=") that
is not #cmd("AUTH=NONE"). Put it in the member:

#screen(raw(read("../ex/ug-security/noprofile-deny-prm.txt")))

The route is the same as before. At the start, #cmd("HTTPD426W") replaces
#cmd("HTTPD425W") for each route without a profile. The next capture shows the
lines that matter. The other lines of the start are left out.

#screen(raw(read("../ex/ug-security/noprofile-deny-start.txt")))

The server starts. A route that refuses is the safe state, and you can define
the profile while the server runs. Until the profile exists, a request is
answered like this:

#tab(caption: [The answers of a route whose profile is missing])[
  #table(columns: (1.9in, 1fr),
    [Request], [Answer],
    [Without a login], [Status 401. The first stage answers, as before.],
    [A user who is logged in, whether or not the user would be permitted],
      [Status 403 with the body
      #cmd("403 Forbidden: no RACF profile protects this resource"). The class
      and the resource name are not in the answer. They are in
      #cmd("HTTPD426W").],
  )
]

Define the profile and permit the users, as in @ug-install-rakfsetup, and run
#cmd("RAKFPROF"). Do not restart HTTPD. The check is made for each request
against the profile as RAKF holds it then. After #cmd("RAKFPROF") has run, a
user whom the profile permits gets the real answer. A user whom it does not
permit gets the plain #cmd("403 Forbidden"), the same answer as for a profile
that refuses. Before #cmd("RAKFPROF") has run, #cmd("RDEFINE") and
#cmd("PERMIT") change nothing the server can see, and the route still answers
with the body about the missing profile. #cmd("HTTPD426W") belongs to the start
and stays in the log. A start after the profile exists does not write it.

The two bodies tell you which case you have: _no RACF profile protects this
resource_ means that RAKF has no profile for the resource. Check the class and
the name in the route against the profile, and check that #cmd("RAKFPROF") ran.
The plain #cmd("403 Forbidden") means that a profile exists and does not permit
the user.

*When to turn it on.* Turn it on when every route with #cmd("RES=") has its
profile and the start writes no #cmd("HTTPD425W"). On a new system, set it
before the first start if the profile definitions are part of the installation.
Do not turn it on while a route still depends on the permissive reading, for
example a #cmd("RES=") that is kept for documentation and has no profile behind
it. Drop that #cmd("RES=") first. Nothing changes without the keyword. A member
that does not have it behaves as before, and #cmd("D C") shows
#cmd("HTTPD138I RES_NOPROFILE ALLOW").

*What the setting does not do.* HTTPD sees only the answer of the security
product that says "no profile" (return code 4). It cannot tell why the product
had none to apply, and with #cmd("DENY") every such answer refuses. This guide
does not state in which conditions, other than a missing profile, a product
gives that answer. The setting does not touch a route with #cmd("AUTH=NONE"),
and it does not change the answer of a profile that exists.

*A wrong value stops the start.* A value that is neither #cmd("ALLOW") nor
#cmd("DENY") is not read as #cmd("ALLOW"). It gives #cmd("HTTPD427E") and
#cmd("HTTPD420E"), and the server does not start (@ug-trouble-policy).

== What Needs No Login <ug-security-exempt>

#idx("exempt paths")#idx("public paths")
Some requests pass without a login, whatever the route says.

- A path that no route claims. HTTPD serves it as a file.
- A route with #cmd("AUTH=NONE"). That includes the two #cmd("MVSMF") routes
  for #cmd("/zosmf/info") and #cmd("/zosmf/services/authenticate") in the member of
  @ug-routes-mvsmf. mvsMF does its own authentication there.
- The paths that match #cmd("/login.*") and #cmd("/favicon.*"), for the methods
  GET and HEAD. The case counts. This lets the login page load. A
  #cmd("MOD") route that claims such a path runs its module without a login.
- The paths #cmd("/login") and #cmd("/logout"). HTTPD handles them itself,
  whatever the routes or the method.

Everything else needs the login that its route asks for. The display modules
are no exception. They have routes with #cmd("AUTH=") like any module.

A route that stands above another one decides for the paths of both
(@ug-routes-order). Check a protected path after each change of the routes
without a login. The answer must be 401, 302 or 403.

== Console Messages for Logins <ug-security-console>

#idx("HTTPD007I")#idx("HTTPD008I")#idx("HTTPD008W")#idx("HTTPD009W")
#idx("console", "login messages")
HTTPD writes the logins that matter to the operator and nothing about a failed
password. That is the client's concern, and it appears in the response.

#tab(caption: [The messages for logins])[
  #table(columns: (0.95in, 1fr),
    [Message], [Meaning],
    [#cmd("HTTPD007I")], [A form login worked. The message gives the user ID,
      the IP address of the client and the address of the ACEE.],
    [#cmd("HTTPD008I")], [A GET of #cmd("/logout") ended a login. The text is
      #cmd("LOGOUT SUCCESSFUL").],
    [#cmd("HTTPD008W")], [The logout failed for a login that HTTPD knows. The
      browser sees the text #cmd("Logout failed.").],
    [#cmd("HTTPD009W")], [From HTTPD 4.5. A login could not create a session
      because RACF failed for another reason than the password.],
  )
]

A logout without a login answers #cmd("You are not logged in. No logout
performed."), and the console stays quiet.

The next capture shows a session on the console. The line from RAKF in the
middle is not an HTTPD message. RAKF reports a login of an unknown user ID,
here from a request with the user #cmd("NOSUCH"), in its own message
#cmd("RAKF0004"). HTTPD writes the other two:

#screen(raw(read("../ex/ug-security/console-session.txt")))

The command #cmd("D L") lists the logins that exist (@ug-ops).

== What HTTPD Does Not Do <ug-security-limits>

#idx("limits", "security")#idx("TLS")
HTTPD is a small server. Know the limits before you open it to a network.

- *HTTPD has no TLS code.* It speaks plain HTTP. User IDs, passwords, Basic
  headers and session cookies cross the network in clear text. Treat the
  network between the client and the server as one that others can read, or
  keep the clients on a network that you control.
- *The check asks for READ only.* One route has one #cmd("RES="), and the
  access is always READ. HTTPD has no separate permission for a method, and
  no check below the level of a route.
- *Routes match once.* The first route that matches decides. HTTPD does not
  warn when a general route hides a specific one.
- *A path without a route is public.* There is no default that denies.
- *A wrong resource name opens the route.* HTTPD warns once at the
  start (#cmd("HTTPD425W")) and not again. With #cmd("RES_NOPROFILE=DENY") the
  route is closed instead (@ug-security-noprofile). A class that RAKF does not
  know closes the route for every user, with or without the keyword.
- *A password is at most 8 characters.* HTTPD changes the user ID and the
  password to upper case and cuts both at 8 characters.
- *The cache can outlive a password change.* See @ug-security-pwchange.
- *HTTPD does not write failed logins to the console.* The client sees the
  401 or the form text. RAKF may write a message of its own, as the capture
  above shows.
