#import "../bookmaster/bookmaster.typ": *

= Troubleshooting <ug-trouble>

#idx("troubleshooting")
#idx("symptoms")
This chapter lists what goes wrong in the order in which you meet it. First the
server does not start. Then it starts, but not as the Parmlib member says. Then
a client gets no answer, or an answer with the wrong status. Last, a module
fails, or a response is slow. Each entry has the symptom, the cause and the
cure.

The causes come from the messages and from the behaviour of the server. The
_HTTPD Reference_ has them in full: "Messages" lists every message, "Routes and
Authentication Policy" lists what each login mode answers, and "HTTP Behaviour
and Limits" lists the status codes. This chapter points to them by title.

The console screens and client sessions in this chapter were captured on a
test system. The user id is IBMUSER, the host is #cmd("localhost"), and the
addresses in the console lines are test addresses.

== Where to Look First <ug-trouble-first>

#idx("messages", "reading")
Two places tell you what is wrong.

*The console.* HTTPD writes a message for what an operator can act on. Every
message has an id of the form #cmd("HTTPDnnnX"). The last letter is I, W or E
for information, warning or error. The letter does not tell you whether the
server stops. #cmd("HTTPD023E") and #cmd("HTTPD026E") are errors that let the
server start. The Reference lists, for every message, what the server does
afterwards. Every message that goes to the console also goes into the Master
Trace Table of the system. @ug-trouble-mtt shows how to read it without a TSO
session.

*The response.* A request that a client got wrong is answered with a status
and a text of one line. It does not go to the console. A wrong password, for
example, is not written to the console by HTTPD. So read the status line that
the client got, and use #cmd("curl -i") to see it.

The next table sends you to the right section.

#tab(caption: [From the symptom to the section])[
  #table(columns: (2.6in, 1fr),
    [Symptom], [Section],
    [The job ends and #cmd("HTTPD001I") never appears], [@ug-trouble-start],
    [The server starts, but the port, the routes or the limits are not the
      ones in the member], [@ug-trouble-warn],
    [The client cannot connect, or the connection closes without an answer],
      [@ug-trouble-conn],
    [The client gets 302, 400, 401, 403, 404, 405, 413, 414, 501 or 505],
      [@ug-trouble-status],
    [A user cannot log in, or a changed password has no effect],
      [@ug-trouble-login],
    [The client gets 503, or a module works on one system and abends on
      another], [@ug-trouble-module],
    [A response of a module takes about five seconds], [@ug-trouble-wait],
    [Requests wait, or a worker abends], [@ug-trouble-workers],
    [One request takes long, and you want to know where the time goes],
      [@ug-trouble-slow],
  )
]

== A Start That Ends <ug-trouble-start>

#idx("start", "refused")
#idx("return code", "start refused")
#idx("HTTPD098I")
#idx("HTTPD099I")
A healthy start ends with #cmd("HTTPD001I ... READY") (see @ug-ops). Some
messages end the start before that. For most of them the server then writes
#cmd("HTTPD098I HTTPD SHUTTING DOWN"), #cmd("HTTPD416I") with the statistics
and #cmd("HTTPD099I HTTPD SHUTDOWN COMPLETE"), and the step ends with return
code 8. The message that names the cause comes before these three. In "A Start
That Ends" in "Messages", the Reference lists all of them.

#tab(caption: [The messages that end the start])[
  #table(columns: (0.95in, 0.45in, 1fr),
    [Message], [RC], [Cause and cure],
    [#cmd("HTTPD037E")], [8], [Another HTTPD holds the port (@ug-trouble-port).
      Stop it, or use another port.],
    [#cmd("HTTPD030E")], [8], [The bind to the port failed
      (@ug-trouble-port).],
    [#cmd("HTTPD028E"), #cmd("HTTPD031E")], [8], [The call to create the socket
      or to listen failed. The message shows the return code and the error
      number of the call.],
    [#cmd("HTTPD038E")], [8], [The server opened the member of the Parmlib and
      could not read it to the end. Check the data set of the DD #cmd("HTTPPRM")
      and start again.],
    [#cmd("HTTPD411E"), #cmd("HTTPD419E"), #cmd("HTTPD420E"),
      #cmd("HTTPD048E"), #cmd("HTTPD427E")], [8], [A route has no valid policy,
      or the value of #cmd("RES_NOPROFILE") is not valid
      (@ug-trouble-policy).],
    [#cmd("HTTPD033E"), #cmd("HTTPD064E")], [8], [The socket thread could not
      be created, or it is not running.],
    [#cmd("HTTPD090E")], [8], [The server could not set up the console
      interface. It stops at once, and no stop message follows.],
    [#cmd("HTTPD014E") to #cmd("HTTPD019W")], [12], [A DD name that the
      procedure must not have is there, or a DD of the server is missing
      (@ug-trouble-dd). The program ends before the banner.],
    [#cmd("HTTPD012E")], [], [The server could not obtain its authorization
      (@ug-trouble-apf). The step ends with the return code of the failed
      call.],
  )
]

=== The Port Is Busy <ug-trouble-port>

#idx("HTTPD037E")
#idx("HTTPD030E")
#idx("HTTPD030I")
#idx("port in use")
#idx("BIND_TRIES")
*Symptom.* The operator starts a second server, or restarts the first one, and
the start ends.

The server takes a lock on the port before it binds. The lock is named
#cmd("HTTPD.PORT.nnnnn"), where #cmd("nnnnn") is the port with five digits.
When another HTTPD address space holds it, the server writes #cmd("HTTPD037E")
and ends at once. It does not try again. A second server on a different port
starts. This is the Parmlib member of the second server:

#screen(raw(read("../ex/ug-ops/refuse-port-in-use-prm.txt")))

The first server serves the port 8081 already. This is the start of the
second:

#screen(raw(read("../ex/ug-ops/refuse-port-in-use.txt")))

Read the capture from the top. The server logged on and registered its route
(#cmd("HTTPD036I")) before it came to the port. Then #cmd("HTTPD037E") named the
port, and the three stop messages followed. The cure is in the message: stop
the other server, or give this one another #cmd("PORT").

When the lock is free and the bind still fails, the server writes
#cmd("HTTPD030E") with the return code and the error number of the call. If
the error is EADDRINUSE, for example after a quick restart, the server waits
and tries again. Each try gives #cmd("HTTPD030I EADDRINUSE, WAITING FOR TCPIP
TO RELEASE HTTP PORT n"). The keywords #cmd("BIND_TRIES") and
#cmd("BIND_SLEEP") set the number of tries and the seconds between them. When
no try works, the server writes #cmd("HTTPD030E") a second time and the start
ends. Any other error of the bind ends the start with the second
#cmd("HTTPD030E") and no try. @ug-config shows how to change the port.
"The HTTPPRM Keywords" in the _HTTPD Reference_ has the two keywords and their
ranges.

=== A Route Without a Valid Policy <ug-trouble-policy>

#idx("HTTPD411E")
#idx("HTTPD419E")
#idx("HTTPD420E")
#idx("HTTPD048E")
#idx("HTTPD427E")
#idx("AUTH=", "wrong value")
#idx("RES_NOPROFILE", "wrong value")
*Symptom.* The server writes #cmd("HTTPD420E ... HTTPD WILL NOT START") and
ends.

A route that has no #cmd("AUTH=") is public. A route that loses its policy by
a mistake would become public, so the server refuses to start instead. Three
mistakes cause it. The first is a mode that is not one of the
four. This member has a typing error in the mode:

#screen(raw(read("../ex/ug-config/refuse-bad-auth-prm.txt")))

The start ends with three messages:

#screen(raw(read("../ex/ug-config/refuse-bad-auth.txt")))

#cmd("HTTPD411E") names the wrong value and the four right ones.
#cmd("HTTPD419E") names the route that could not be registered.
#cmd("HTTPD420E") is written at the end of the member and ends the start. Write
#cmd("NONE"), #cmd("FORM"), #cmd("BASIC") or #cmd("TOKEN") and start again.

The second mistake is the old keyword #cmd("LOGIN") with an operand that asked
for a login. The keyword is retired. A member with #cmd("LOGIN=BASIC") would
make every route without #cmd("AUTH=") public, so the server stops:

#screen(raw(read("../ex/ug-config/refuse-login-basic-prm.txt")))

#screen(raw(read("../ex/ug-config/refuse-login-basic.txt")))

Remove the #cmd("LOGIN") line and write #cmd("AUTH=") on each route. @ug-routes
shows the four modes. A #cmd("LOGIN") line with no operand, or with
#cmd("NONE"), gives only the warning #cmd("HTTPD048W") (@ug-trouble-warn).

The third mistake is a value of #cmd("RES_NOPROFILE") that is neither
#cmd("ALLOW") nor #cmd("DENY"). The keyword exists to close a gate. If the
server read a misspelled value as #cmd("ALLOW"), the gate would stay open
without a word, so it stops. The lines of the member that matter:

#screen(raw(read("../ex/ug-config/refuse-bad-noprofile-prm.txt")))

The start ends with this console, the JES lines left out:

#screen(raw(read("../ex/ug-config/refuse-bad-noprofile.txt")))

#cmd("HTTPD427E") shows the value, cut after 16 characters. The text of
#cmd("HTTPD420E") is the same for every cause, so the cause is in the message
before it. Write #cmd("ALLOW") or #cmd("DENY") and start again.

=== The Procedure Has the Wrong DD Names <ug-trouble-dd>

#idx("SYSPRINT", "not allowed")
#idx("HTTPD014E")
*Symptom.* A message from #cmd("HTTPD014E") to #cmd("HTTPD019W") appears, there
is no banner, and the step ends with return code 12.

The server uses the DD names #cmd("HTTPDOUT"), #cmd("HTTPDERR") and
#cmd("HTTPDIN") for its standard streams. The procedure must not have the DD
names #cmd("SYSPRINT"), #cmd("SYSTERM") or #cmd("SYSIN"). The routine in front
of the server tests for them first, and it ends the program when it finds one
(#cmd("HTTPD014E"), #cmd("HTTPD015E"), #cmd("HTTPD016E")). Then it opens the
three DDs of the server. A DD that cannot be opened gives #cmd("HTTPD017W"),
#cmd("HTTPD018W") or #cmd("HTTPD019W"), and the letter W does not save the
start: all six conditions end the program. Compare your procedure with the
sample member. The Reference has it in "The Started Task and the Package".

=== Authorization Fails <ug-trouble-apf>

#idx("HTTPD012E")
#idx("HTTPD002I")
#idx("APF", "troubleshooting")
#idx("SVC 244")
*Symptom.* The console shows
#cmd("HTTPD012E name UNABLE TO DYNAMICALLY OBTAIN APF AUTHORIZATION") first.
The banner and the C library line follow, each with a leading plus sign
because the step is not authorized, and there is no #cmd("HTTPD002I"). The
step ends.

The server needs to be authorized. At the entry it tests whether the step is
authorized. If it is not, the server asks for the authorization with SVC 244,
and tests again. The message says that the step is still not authorized. The
profile #cmd("SVC244") in the class #cmd("FACILITY") gates the SVC. The
account that counts is the one the task has when it starts, before the server
logs on as #cmd("STCUSER"). Check the permissions of that account.
"Authorization" in "The Started Task and the Package" in the _HTTPD Reference_
describes the call.

When the start works, #cmd("HTTPD002I") tells how the step was authorized.

#tab(caption: [The two forms of HTTPD002I])[
  #table(columns: (2.3in, 1fr),
    [Message], [Meaning],
    [#cmd("HTTPD002I AUTHORIZED BY LIBRARY (MODULE KEY 0)")], [The step was
      authorized at the entry, by the APF authorization of its library. The system
      fetches the modules with the protection key 0.],
    [#cmd("HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)")], [The step was not
      authorized at the entry. The server authorized the task itself. The
      system fetched the modules with the key 8.],
  )
]

Both forms are a good start. They differ for the modules that the server runs
(@ug-trouble-s0c4). If you expected the first form and got the second, check
the APF list for the libraries of the STEPLIB.

== The Start Goes On, but Not as Intended <ug-trouble-warn>

#idx("warnings", "start")
A warning does not stop the start. The server skips the bad line or keeps the
default, and goes on. Read the console from the banner down to
#cmd("HTTPD001I") after every change of the member. The next table lists the
common ones.

#tab(caption: [Messages that let the start go on])[
  #table(columns: (0.95in, 1fr),
    [Message], [Meaning and cure],
    [#cmd("HTTPD020W")], [The DD #cmd("HTTPPRM") cannot be opened. The server
      starts on its defaults: port 8080, 3 to 9 workers, the document root
      #cmd("/www") and no routes. Add the DD, or correct the member name
      (@ug-trouble-nomember).],
    [#cmd("HTTPD023E")], [The value of #cmd("PORT") is not a number from 1 to
      65535. The server keeps the earlier value or the default.],
    [#cmd("HTTPD026W")], [A line is not a comment and has no equals sign. The
      server skips it.],
    [#cmd("HTTPD029W")], [The keyword is not known. The server skips the line.
      A typing error in a keyword gives this message.],
    [#cmd("HTTPD026E"), #cmd("HTTPD032E")], [The idle time or the maximum age
      of a login is not longer than #cmd("CLIENT_TIMEOUT"). The letter is E,
      but it is a warning. Raise #cmd("SESSION_TIMEOUT") or
      #cmd("SESSION_MAXAGE").],
    [#cmd("HTTPD048W")], [The retired keyword #cmd("LOGIN") has no effect.
      Remove it and write #cmd("AUTH=") on each route.],
    [#cmd("HTTPD412W"), #cmd("HTTPD413W"), #cmd("HTTPD414W")], [A route option
      is malformed (#cmd("HTTPD412W")), unknown (#cmd("HTTPD413W")), or given on a
      public route (#cmd("HTTPD414W")). The server ignores the option.],
    [#cmd("HTTPD425W")], [The resource of #cmd("RES=") has no profile. The route
      is served with no gate (@ug-trouble-login).],
    [#cmd("HTTPD426W")], [The same, with #cmd("RES_NOPROFILE=DENY") in the
      member. The route answers every request with 403 until the profile
      exists (@ug-trouble-403).],
    [#cmd("HTTPD440W"), #cmd("HTTPD441W"), #cmd("HTTPD442W")], [The module of a
      route could not be held at the start. The route works. The module is
      fetched for each request (@ug-trouble-module).],
    [#cmd("HTTPD003W"), #cmd("HTTPD004W")], [The server could not log on as
      #cmd("STCUSER"). It runs with the identity the task inherited
      (@ug-trouble-login).],
    [#cmd("HTTPD444W")], [The value of #cmd("TRACE") is not 0 and not a number
      from 16 to 1024. The request trace stays off (@ug-config-trace).],
    [#cmd("HTTPD067W")], [The server could not get the storage for the request
      trace. It runs without it (@ug-config-trace).],
    [#cmd("HTTPD044W")], [The server could not create its handle to the file
      system. It serves no static files (@ug-trouble-static).],
    [#cmd("HTTPD047E")], [The server could not set up its credential key.
      Logins do not work.],
    [#cmd("HTTPD034W")], [The server could not create the worker threads. The
      routes that run programs are disabled.],
  )
]

=== The Member Is Not There <ug-trouble-nomember>

#idx("HTTPD020W")
#idx("HTTPPRM", "member missing")
*Symptom.* The server answers on port 8080 although the member says another
port, and it has none of your routes.

The start below used the member name #cmd("NOSUCH") on the #cmd("S") command.
The server could not open the DD, wrote #cmd("HTTPD020W"), and started on its
defaults:

#screen(raw(read("../ex/ug-ops/refuse-member-missing.txt")))

The server listened on 8080 and had no routes. Static files from the document
root are all that it served. The cure is to start with the right member, or to
add the missing DD to the procedure. The file name of the capture says
"refuse", but the server did not refuse: a missing member is not an error for
HTTPD.

=== Lines the Server Skips <ug-trouble-skip>

#idx("HTTPD029W")
#idx("HTTPD026W")
A keyword that HTTPD does not know is skipped with #cmd("HTTPD029W"), and the
keyword is shown in upper case:

#screen(raw(read("../ex/ug-config/warn-unknown-keyword.txt")))

A line without an equals sign gives #cmd("HTTPD026W") with the first 40
characters of the line:

#screen(raw(read("../ex/ug-config/warn-unrecognized-line.txt")))

In both cases the server went on. The setting that you meant to make is not
made, so a keyword with a typing error leaves the default in force. The line
#cmd("PORT=0") is the same kind of case:

#screen(raw(read("../ex/ug-config/refuse-bad-port.txt")))

The server wrote #cmd("HTTPD023E") and went on, and the capture shows that it
listened on the default 8080. The message has the letter E, and the server
still started. @ug-config describes what a bad line or value does.

The same happens with the options of a route. This member has one of each
mistake, and the next capture shows what the server said:

#screen(raw(read("../ex/ug-routes/warn-route-options-prm.txt")))

#screen(raw(read("../ex/ug-routes/warn-route-options.txt")))

The first route (#cmd("MOD=NOSUCH")) is the one of @ug-trouble-module. Then
follow #cmd("HTTPD414W") (a resource on a public route is ignored),
#cmd("HTTPD412W") (the resource has no colon), #cmd("HTTPD413W") (an option that
does not exist) and #cmd("HTTPD425W") (the profile does not exist, so the route
is not gated; with #cmd("RES_NOPROFILE=DENY") it is #cmd("HTTPD426W") and the
route refuses every request). The server registered all four routes in the
end, and it did not stop.

=== Sessions That Are Too Short <ug-trouble-sess>

#idx("HTTPD026E")
#idx("HTTPD032E")
The server compares the idle time and the maximum age of a login with
#cmd("CLIENT_TIMEOUT") at the end of the start. A login that a request is
still using must not be dropped. This member sets both values below the client
time-out:

#screen(raw(read("../ex/ug-config/warn-session-unsafe-prm.txt")))

#screen(raw(read("../ex/ug-config/warn-session-unsafe.txt")))

Both messages are warnings, and the server started. The messages show the
idle time and the maximum age in minutes and the client time-out in seconds.
Raise #cmd("SESSION_TIMEOUT") and
#cmd("SESSION_MAXAGE"), or lower #cmd("CLIENT_TIMEOUT").

== The Client Cannot Connect <ug-trouble-conn>

#idx("connection refused")
#idx("time-out", "client")
#idx("HTTPD065W")
*The connection is refused or does not open.* The server is not running, it did
not reach #cmd("HTTPD001I"), or the client uses another port. Look on the
console for #cmd("HTTPD054I LISTENING ON ANY PORT n"). The command
#cmd("F HTTPD,D P") shows the port that the server serves:

#screen(raw(read("../ex/ug-ops/d-ports.txt")))

The server binds to all interfaces of the host. If the port is right and the
connection still fails, the cause is outside HTTPD.

*The connection closes and the client gets no answer.* The server closes a
connection without a response when a time-out ends. The limit is
#cmd("CLIENT_TIMEOUT") seconds (default 10) for the first request of a
connection and #cmd("KEEPALIVE_TIMEOUT") seconds (default 5) for each later
one. A client that sends part of a request line and then waits is closed. If
#cmd("CLIENT_TIMEOUT_MSG") is on, which is the default, and the server has
received at least one character, it writes a line to the console. The line has
no message id. The next screens use a time-out of 3 seconds. The client sends
13 characters of a request line and waits:

#screen(raw(read("../ex/ug-config/client-timeout-client.txt")))

#screen(raw(read("../ex/ug-config/client-timeout-console.txt")))

The line names the client block and the socket, and the dump shows what the
server had received. A client that reuses a connection after more than the
keep-alive time finds it closed. That is not a fault of the server. Send the
next request on a new connection.

*The client waits for the data and the connection ends.* When the client does
not take data, the server tries again every 0.1 second. After 100 tries
without progress, that is 10 seconds, it gives up on the client and writes
#cmd("HTTPD065W"). It writes this message at most once in 60 seconds. If
the server cannot read a file from the file system in the middle of a response,
it writes #cmd("HTTPD045E"), cuts the response and closes the connection. It
writes this message at most once in 60 seconds, and a count at its end says how
many errors it held back. A client that sees a response that stops in the middle
should look for these two messages.

== The Status Is Not 200 <ug-trouble-status>

#idx("status codes", "troubleshooting")
The error responses of the server have a text body of one line. The next table
lists the status codes and where to read on. "Status Codes" in "HTTP Behaviour
and Limits" in the _HTTPD Reference_ has the full list.

#tab(caption: [The status codes that point to a problem])[
  #table(columns: (0.45in, 1fr),
    [Code], [Cause],
    [302], [A route with form login and no session. This is the login form
      working (@ug-trouble-form).],
    [400], [The request is malformed: a bad request line, header or parameter.],
    [401], [The route needs a login, and the request has none or a wrong one
      (@ug-trouble-401).],
    [403], [The user is known, but the resource check of the route denies the
      access (@ug-trouble-403).],
    [404], [There is no file for the path (@ug-trouble-404).],
    [405], [The method is not one that the server knows
      (@ug-trouble-method).],
    [413], [The body of a POST is larger than 3999 bytes, and the route has no
      program. A module route gets such a body unread.],
    [414], [The request line is longer than 3998 characters.],
    [500], [The server cannot build the response, or a module asked for a
      status code that the server does not know. The server then writes
      #cmd("HTTPD909E") to the console.],
    [501], [POST, PUT or DELETE for a static file (@ug-trouble-method).],
    [503], [A module does not run (@ug-trouble-module).],
    [505], [The version of the request is not HTTP/1.0 and not HTTP/1.1.],
  )
]

=== 401, the Route Needs a Login <ug-trouble-401>

#idx("401")
#idx("WWW-Authenticate")
#idx("realm")
A route with #cmd("AUTH=BASIC") answers a request without credentials with
401 and a challenge. The header #cmd("WWW-Authenticate") carries the realm,
which is the value of #cmd("REALM"):

#screen(raw(read("../ex/ug-security/basic-no-credentials.txt")))

A route with #cmd("AUTH=TOKEN") answers with 401 and no challenge:

#screen(raw(read("../ex/ug-security/token-no-credentials.txt")))

The server never sends the header for a token route, whatever the request
holds. A browser then shows no password dialog. The same is true for a request
that has the header #cmd("X-CSRF-ZOSMF-HEADER"), also on a Basic route: the
browser would open its own dialog, so the server leaves the header out. This
is the answer of the mvsMF route, which is a token route, to a request with no
login:

#screen(raw(read("../ex/ug-security/mvsmf-token-without.txt")))

A 401 does not say why. A user that does not exist and a user with a wrong
password get the same answer as a request with no credentials:

#screen(raw(read("../ex/ug-security/basic-unknown-user.txt")))

#screen(raw(read("../ex/ug-security/basic-bad-password.txt")))

So the status line cannot tell the three apart. These are the causes to check.

- *No credential.* The server looks for the login in this order: the cookie
  #cmd("Sec-Token"), the cookie #cmd("LtpaToken2"), a Bearer token, and Basic
  credentials. A client that sends none of them gets the 401.
- *The user is not known, or the password is wrong.* An unknown user leaves a
  line from the security product on the console. HTTPD writes none. The next
  screen shows the lines that one session left on the test system: the
  security product refused the user #cmd("NOSUCH"), then a login and a logout
  of IBMUSER were recorded.

  #screen(raw(read("../ex/ug-security/console-session.txt")))

  A wrong password for a known user does not appear on the console. The
  password of the client is not logged.
- *The client has another address.* The login is bound to the address of the
  client. A user that logged in from another address does not count. A client
  behind a proxy or a gateway that changes the address between two requests
  loses its login.
- *The login has ended.* HTTPD drops a login that is idle for
  #cmd("SESSION_TIMEOUT") minutes (default 30), and one that is older than
  #cmd("SESSION_MAXAGE") minutes (default 480). A client that sends user and
  password with every request does not notice this. A client with a cookie or
  a token must log in again. #cmd("F HTTPD,D L") shows who is logged in:

  #screen(raw(read("../ex/ug-ops/d-login.txt")))

@ug-security describes the four modes and the sources of a login.

=== 302, the Login Form <ug-trouble-form>

#idx("302")
#idx("Sec-Uri")
A route with #cmd("AUTH=FORM") answers a GET without a login with 302 and
#cmd("Location: /login"). It also sets the cookie #cmd("Sec-Uri"), which holds
the address of the request, so that the form can send the user back:

#screen(raw(read("../ex/ug-security/form-redirect.txt")))

This is not a fault. The client is meant to follow the redirect and show the
form. A request
to a form route with a method other than GET and POST gets status 200 and the
text #cmd("Invalid REQUEST_METHOD"), because the server cannot send such a
request to a form.

=== 403, the Resource Is Denied <ug-trouble-403>

#idx("403")
#idx("RES=", "403")
The route has #cmd("RES=class:resource"). The user is logged in, but the
security product does not give the user READ access to the resource. The answer
is 403 with the body #cmd("403 Forbidden"):

#screen(raw(read("../ex/ug-security/denied-403.txt")))

The resource of this route is a profile that nobody may read. To cure a 403,
permit the user to read the profile. Then activate the change. A profile that
you define or permit is not in force until RAKF has read it. The tool that
defines the profiles says so, in its own words, "Run RAKFPROF to activate the
changes". The started task #cmd("RAKFPROF") does it:

#screen(raw(read("../ex/ug-install/rakf-reload.txt")))

The opposite fault is silent. Under RAKF a resource that has no profile is not
protected, and the product answers that the access is allowed. The route then
lets everybody pass. At the start the server tests every route that
has #cmd("RES=") and a mode other than #cmd("NONE"), and it writes
#cmd("HTTPD425W NO PROFILE FOR class:resource -- path NOT GATED") for each one
that has no profile (see the capture in @ug-trouble-skip). A typing error in
the resource name looks the same. A class that RAKF does not define gives the
same with RAKF 1.2.x (measured on TK5). RAKF 2.0.0 (MVS/CE) gives the return
code 8 instead, with no #cmd("HTTPD425W"), and no user passes. Treat #cmd("HTTPD425W") as an
error of your configuration. "What the Server Asks of the Security Product"
in the _HTTPD Reference_ describes the check.

With #cmd("RES_NOPROFILE=DENY") in the member, the route is not silent. The
start writes #cmd("HTTPD426W NO PROFILE FOR class:resource -- path REFUSES ALL
REQUESTS"), and every logged-in user gets 403 with the body
#cmd("403 Forbidden: no RACF profile protects this resource"). That body is the
difference from the 403 above: it says that no profile exists, not that the
profile does not permit the user. Define the profile and run #cmd("RAKFPROF").
The server needs no restart (@ug-security-noprofile).

For the console route of mvsMF there is a second answer. mvsMF 1.4.0 and later
check READ on the same profile themselves. HTTPD answers 403 first to a user
without READ. A user who passes the route when the profile is not defined gets
status 500 from mvsMF, with the error category 4, because mvsMF refuses every
user until the profile exists. That is the answer with the default
#cmd("RES_NOPROFILE=ALLOW"). With #cmd("RES_NOPROFILE=DENY") HTTPD answers 403
with the body above before mvsMF is called. Define the profile.

=== 404, There Is No File <ug-trouble-404>

#idx("404")
A request for a path that no route claims is served from the document root. A
file that is not there, and a file that HTTPD cannot open, get the same answer:

#screen(raw(read("../ex/ug-static/not-found.txt")))

The body names the decoded path. If only one file is missing, check the name
and the directory in the image. If every request for a file gets a 404, the
cause is more likely the file system. @ug-trouble-static has the checks.

=== 405 and 501, the Method <ug-trouble-method>

#idx("501")
#idx("405")
#idx("HEAD")
A static file answers GET and HEAD. A POST, PUT or DELETE for a static file
gets 501:

#screen(raw(read("../ex/ug-static/post-to-file.txt")))

The answer to the POST says #cmd("Connection: close"). Use GET to read a file.

If #cmd("curl -I") gets 501, the server is HTTPD 4.4.0 or earlier. These
releases do not answer a HEAD for a static file with the headers of a GET. The
console message #cmd("HTTPD000I") at the start names the version
(@ug-first-console). HTTPD 4.5.0 answers the HEAD (@ug-static-methods).

A method that the server does not know gets 405:

#screen(raw(read("../ex/ug-static/method-not-allowed.txt")))

@ug-static has more on files.

== Logins <ug-trouble-login>

#idx("login", "troubleshooting")
@ug-trouble-401 and @ug-trouble-403 cover what a client sees. Three more
faults concern the server and its security product.

=== The Server Has Not Its Own Identity <ug-trouble-stc>

#idx("HTTPD004W")
#idx("HTTPD003W")
#idx("STCUSER")
*Symptom.* The console shows #cmd("HTTPD004W RACINIT ENVIR=CREATE FAILED FOR
user/group RC=n") at the start.

At the start the server logs on as the user id of #cmd("STCUSER") and the group
of #cmd("STCGROUP"), which the procedure passes in the PARM string. When the
logon fails, the server writes #cmd("HTTPD004W") and keeps the identity that the
task inherited. A cause is that the user id is not defined. The server goes on,
and that is the danger: on a standard RAKF profile set the inherited account
holds ALTER authority on every data set of the system. Define the user id and
the group, then restart. #cmd("HTTPD003W") says that the server
could not enter supervisor state and so skipped the logon. "The Identity of the
Task" in "The Started Task and the Package" in the _HTTPD Reference_ has the
steps. On the healthy start the line is #cmd("HTTPD004I STC IDENTITY SET TO
user/group VIA RACINIT"), as the captures in this chapter show.

=== A Changed Password Does Not Take Effect <ug-trouble-stale>

#idx("session", "stale")
#idx("password change")
#idx("SESSION_MAXAGE")
#idx("credential cache")
*Symptom.* An administrator changes a password in the security product, or
revokes a user. A client that logged in earlier is still accepted with the old
password, and the new password works as well.

HTTPD keeps a login in a table in its storage. A request with the same user,
password and address finds the entry and is answered from it. The server does
not ask the security product again, so the product's answer at the time of the
login is what counts. An entry ends in one of these ways:

- It is idle for #cmd("SESSION_TIMEOUT") minutes (default 30).
- It is older than #cmd("SESSION_MAXAGE") minutes (default 480), counted from
  the login. Activity does not renew it. The next request then asks the
  security product again, and a revoked user or a changed password take effect
  at that moment.
- The client logs out.
- The server stops.

The new password is a different key for the table, so a request with it asks the
security product and works. Only the old password is served from the table.

To end a login at once, restart the server, or have the user log out. If a
shorter life is what the site needs, lower #cmd("SESSION_TIMEOUT") and
#cmd("SESSION_MAXAGE"). Keep both above #cmd("CLIENT_TIMEOUT") in seconds, or
the server warns (@ug-trouble-sess). @ug-config shows both keywords.

From HTTPD 4.5 a module that asks for a new session can find that the security
product refuses the logon. Then the server marks the entry as stale. It does
not answer by password or token from that entry any more, and it frees the
entry 600 seconds later. The module answers 401. "Sessions" in "The Server
Module Interface" in the _HTTPD Reference_ has the result codes.

== A Module Does Not Run <ug-trouble-module>

#idx("module", "troubleshooting")
#idx("503")
#idx("HTTPD908E")
#idx("S806")
*Symptom.* The client gets 503 and a line that starts with
#cmd("External program"), and the console shows #cmd("HTTPD908E").

The route names a module, and the server could not run it, or the module ended
badly. This is a route to a module that is not in any library:

#screen(raw(read("../ex/ug-routes/route-nosuch-module-prm.txt")))

#screen(raw(read("../ex/ug-routes/nosuch-module-request.txt")))

The body says #cmd("External program NOSUCH was not found"). The server also
wrote #cmd("HTTPD908E EXTERNAL PROGRAM NOSUCH was not found") to the console.
The start had already warned about this module with #cmd("HTTPD440W MODULE
NOSUCH NOT FOUND AT STARTUP, NOT HELD") (see the capture in
@ug-trouble-skip). The text after the module name is the reason, and it is the
same for the client and for the operator.

#tab(caption: [The reasons for a failed module])[
  #table(columns: (1.9in, 1fr),
    [Text], [Cause and cure],
    [was not found], [The module is in neither the STEPLIB nor the link list.
      The completion code is S806. Check the name in the route, and check that
      the member is in a library of the STEPLIB of the procedure.],
    [could not be loaded], [The LINK failed, and the cause is not S806. The
      module did not run.],
    [was dispatched without a program name], [The route has no program name.],
    [could not be started, ESTAE create failed], [The server could not set up
      the error exit for the LINK, so the module did not run.],
    [returned rc n], [The module ran and returned a negative code n of its
      own.],
    [failed with Sxxx ABEND], [The module ended with the system abend xxx. Look
      for the dump (@ug-trouble-s0c4).],
    [failed with Unnnn ABEND], [The module ended with the user abend nnnn,
      in decimal. #cmd("U0801"), with the console line
      #cmd("@@CRT0 - No storage for C stack"), is a region that is too small,
      see below.],
  )
]

Only the abends leave a dump to look for. The other reasons mean that the
module did not run or ended by itself. After the module ended, the server frees
its storage. If that fails, it writes #cmd("HTTPD906W") with the module name and
the return code, and the storage stays until the server restarts. The worker
lives on in every case and serves the next request.

*The abend is U0801, and it comes under load.* Every request that runs a module
takes one block of storage for its stack: about 256 KB for a module that sets
nothing, which is the case for REXX and Lua, and about 64 KB for mvsMF. Up to
#cmd("MAXTASK") requests run at once. When the region has no free block of that
size, the console shows #cmd("@@CRT0 - No storage for C stack") and
#cmd("HTTPD908E EXTERNAL PROGRAM name failed with U0801 ABEND"), and the client
gets 503. The other requests go on. Raise the #cmd("REGION") of the procedure
(16M held in the one run that was measured) before you raise #cmd("MAXTASK"). If
16M is not enough, lower #cmd("MAXTASK"). "The Started Task and the Package" in
the _HTTPD Reference_ has the sizes and the measurement.

*The module is in a library, and the route still fails.* The LINK searches the
STEPLIB of the started task first, and then the link list. Check that the
library is in the STEPLIB of the procedure that started the server, and that
the member has the name in the route. If the member comes from a development
library, that library has to be in the concatenation, and it has to come before
the production library so that its module wins.

*A new build of the module has no effect.* By default the server loads each
module once at the start and holds it until it stops. The requests use that
copy. Stop the task before you replace the load library, and start it again.
For development you can switch the holding off with #cmd("RESIDENT=NO"). A
module that was not found at the start (#cmd("HTTPD440W")) is not held. It
works as soon as it is in the library.

=== A Module Abends with S0C4 <ug-trouble-s0c4>

#idx("S0C4")
#idx("module", "static data")
#idx("storage key")
*Symptom.* The client gets 503 and
#cmd("External program name failed with S0C4 ABEND"). The same module worked
on the test system.

A module that the system fetches from an APF-authorized library or from the
link list lands in storage with the protection key 0. Its code runs with the
key 8. The first write to the storage of the module abends with S0C4. The
library decides, not the attribute AC(1) and not RENT. The module works when its
library is not in the APF list: the system fetches it with the key 8, and the
write is allowed. The same load module abends on a system where the library
is authorized.

This is why #cmd("HTTPD002I") matters (see @ug-trouble-apf). The form BY LIBRARY
(MODULE KEY 0) says that the modules are fetched with the key 0, and every
write to the static data of a module abends. The form BY SVC (MODULE KEY 8)
hides the fault. The key in the message is inferred from the way the step was
authorized and not measured.

The cure is in the module, not in the authorization. A module must not write to
a variable at file scope or to a static variable of a function, and it must not
change a string constant in place. Put the state into a local variable, onto
the heap, or into the storage that #cmd("__wsaget()") returns. Read-only data
such as tables is fine. The declaration decides for strings that a function
translates in place: #cmd("UCHAR text[] = \"...\"") is a copy on the stack, and
#cmd("UCHAR *text = \"...\"") points to the constant itself. A call of
#cmd("__super()") does not help, because a module can be in key 0 without the
right to switch. "Storage Rules" in "The Server Module Interface" in the
_HTTPD Reference_ has the rules. @ug-module shows a module that follows them.
Test a module with its library in the APF list before you call it done.

== A Response Takes Five Seconds <ug-trouble-wait>

#idx("keep-alive", "wait")
#idx("header terminator")
#idx("Transfer-Encoding")
*Symptom.* A module answers 200 with the right body, but the client needs
about five seconds before it is done.

Look at the headers of such a response. This is a module that has the pitfall:

#screen(raw(read("../ex/ug-module/pitfall-blank-line-headers.txt")))

There is no #cmd("Content-Length") and no #cmd("Transfer-Encoding"), and the
header #cmd("Connection") says #cmd("keep-alive"). The client cannot tell where
the body ends. It waits until the server closes the connection, and the server
does this only when its keep-alive wait ends. The time of the whole request
shows it:

#screen(raw(read("../ex/ug-module/pitfall-blank-line.txt")))

The same module, written correctly, takes this:

#screen(raw(read("../ex/ug-module/timing.txt")))

The cause is how the server finds the end of the headers. #cmd("http_printf")
watches for a call whose text is exactly #cmd("\"\\r\\n\""). When it sees that
call, it adds #cmd("Transfer-Encoding: chunked") for an HTTP/1.1 request that
has no #cmd("Content-Length"), and it turns chunking on. A blank line that is
joined to the last header in one call is not found. The module with the pitfall
ends its headers like this:

#screen(```c
http_resp(httpc, 200);
http_printf(httpc, "Content-Type: text/plain\r\n\r\n");
http_printf(httpc, "Hello, %s!\n", name);
```)

The right module sends the blank line as a call of its own:

#screen(```c
http_resp(httpc, 200);
http_printf(httpc, "Content-Type: text/plain\r\n");
http_printf(httpc, "\r\n");
http_printf(httpc, "Hello, %s!\n", name);
```)

A module can also send a #cmd("Content-Length") header and then it is not
chunked. The wait in the capture is close to the default of
#cmd("KEEPALIVE_TIMEOUT"), which is 5 seconds. "Chunked Transfer" in "HTTP
Behaviour and Limits" in the _HTTPD Reference_ has the rules, and @ug-module
shows the complete module.

== A Request Is Slow <ug-trouble-slow>

#idx("request trace", "slow request")
#idx("TRACE")
*Symptom.* A client says that one request takes long. You want to know whether
the time went to the client, to the server, to a module or to the send. The
request trace records the time of each phase of every request, so it answers
this. It has to be on before the request is made, because the server reads
#cmd("TRACE=") only at its start. If it is off, switch it on with the steps in
@ug-config-trace and wait for the problem to come again.

The target #cmd("TRACE") of HTTPDSRV shows the table. The parameter #cmd("min")
is a number of milliseconds. It leaves out the finished requests that took less.
A request that is still running is always in the answer. The first line of the
answer holds the totals, and the filter #cmd("tail -n +2") cuts it. The filter
#cmd("cut") keeps the words of interest: the number of the request, its worker,
its number on the connection, whether it still runs, the last phase it reached,
its age in milliseconds, the times of the request line and of the end of the
headers, the number of waits for the client, and the path.

The first capture was made while a client held a request back. The client had
sent the request line and no header line, and the readout is from the third
second:

#screen(raw(read("../ex/ug-trouble/trace-slow-live.txt")))

The first line is the request of the slow client. It still runs (#cmd("live=1")).
Its last phase is #cmd("line1"): the server read the request line after 4 ms,
and since then it has waited for the header lines. The word #cmd("req") is
#cmd("-"), because the end of the headers was not reached. The server has
gone back to waiting for the client seven times (#cmd("polls=7")), and each
wait lasts up to half a second. The time is not the time of the server. The second line is the request
that read the trace.

The second capture was made after the client had sent the rest:

#screen(raw(read("../ex/ug-trouble/trace-slow-done.txt")))

The request ended with #cmd("last=end") after 5,537 ms. The end of the headers
came at 5,517 ms, so the server needed 20 ms for everything that follows. The
cause of the long request is the client.

When the time is not in the wait for the client, find the largest gap between
two phases. The next table says what each gap is. The Reference lists the
phases in "The Display Modules" (target #cmd("TRACE")).

#tab(caption: [Where the time of a request goes])[
  #table(columns: (1.5in, 1fr),
    [Gap], [Meaning],
    [accept to #cmd("run")], [The request waited for a worker. All workers
      were busy (@ug-trouble-workers).],
    [#cmd("run") to #cmd("req")], [The server waited for the client to send the
      request line and the headers. #cmd("polls") counts the waits of half a
      second.],
    [#cmd("req") to #cmd("parse")], [The server parsed the request. For a POST
      it also read the body, so a client that sends its body slowly shows up
      here.],
    [#cmd("parse") to #cmd("cred")], [The server resolved the credential of the
      request.],
    [#cmd("cred") to #cmd("route")], [The server checked the policy of the
      route, with the RACF check of #cmd("RES=") if the route has one.],
    [#cmd("lnk0") to #cmd("lnk1")], [The module ran. The time includes the load
      of the module if the server does not hold it in storage
      (@ug-trouble-module).],
    [#cmd("snd1") to #cmd("sndl")], [The server sent the data. For a
      chunked response, #cmd("txst") counts the pauses of a tenth of a second
      in which TCP/IP took no data: the client or the network is slow to
      receive.],
  )
]

The trace shows what the server did and nothing of the network. A request that
is fast in the trace and slow for the client was slow in the client or in the
network. On a connection that stays open, the record of the second
request starts at the end of the first one, so its #cmd("line1") includes the
time that the client needed to send it. A finished request that took a second or
more stays in the table longer than a fast one, so a slow request is still there
when you look.

== Static Files Are Not Served <ug-trouble-static>

#idx("UFSD", "troubleshooting")
#idx("HTTPD044W")
*Symptom.* Every request for a file gets 404, also for files that are in the
image.

HTTPD reads files through UFSD. The message
#cmd("HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM") says that the server could
not create its handle for the file system. It does not say that UFSD is down.
A request for a file that HTTPD cannot open gets the 404, whatever the reason.
Check in this order:

+ UFSD runs.
+ The image is mounted. The operator command #cmd("F UFSD,MOUNT LIST") shows
  the mounts.
+ The mount point is the value of #cmd("DOCROOT"). The default is #cmd("/www").
+ The file is in the image, with the path of the request.
+ The Parmlib member does not set #cmd("UFS=0"). The value 0 switches the file
  system off, and then no file is served.

@ug-static shows the document root and the mount.

== Requests Wait or Workers Abend <ug-trouble-workers>

#idx("workers", "busy")
#idx("HTTPD062E")
#idx("HTTPD063E")
*Requests wait.* A worker serves one connection until it is closed. A
connection with keep-alive holds its worker, also while the connection is idle,
for at most #cmd("KEEPALIVE_TIMEOUT") seconds between two requests. When all
workers up to #cmd("MAXTASK") are busy, a further client has to wait for a free
worker. Raise #cmd("MAXTASK") with #cmd("F HTTPD,SET MAXTASK n"), but read about
the region in @ug-trouble-module first, or lower
#cmd("KEEPALIVE_TIMEOUT") (@ug-config). The command #cmd("F HTTPD,D T") shows
the threads of the server. If one request is stuck in a module, the module is
the cause, and more workers do not cure it.
At the stop the server waits up to 10 seconds for the socket thread to end. It
writes #cmd("HTTPD040I") while it waits and #cmd("HTTPD041I") when it detaches
the task.

*A worker abends.* An abend in a module does not stop the worker
(@ug-trouble-module). An abend in the code of the server gives
#cmd("HTTPD062E ABEND code IN WORKER(..) CLIENT(..) SOCKET(n)"). The code is the
abend word in eight hex digits. The server closes the client, and the worker
goes on with the next request. #cmd("HTTPD063E") says that the worker could not
set up its recovery, and that it dropped the client. A repeated
#cmd("HTTPD062E") with the same code points to the server and not to a
module. @ug-ops describes the recovery.

== Reading the Master Trace Table <ug-trouble-mtt>

#idx("master trace table")
#idx("/.dmtt")
#idx("HTTPDMTT")
Every console message goes into the Master Trace Table, and HTTPD writes to the
console only what an operator can act on. So the table is where you look after
a failure, also when nobody watched the console. The display module HTTPDMTT
shows the table over HTTP when a route for it exists (@ug-ops shows the route).
This request returns the whole table as plain text, and #cmd("grep") picks out
the messages of HTTPD:

#screen(```
curl -s -u IBMUSER:password "http://localhost:8080/.dmtt?d=1" | grep HTTPD
```)

With the parameter #cmd("d=1") the answer is plain text. Without it, the answer
is an HTML page. The module does not search or select. The table holds only
the recent messages, so a server that has run for a long time may have lost the
messages of its start. Read the table soon after a failure. This is the end of
the table after the start of the test server. The capture shows the last
lines:

#screen(raw(read("../ex/ug-ops/dmtt-tail.txt")))

The table is the whole console log of the system. Protect the route with a
login mode, and with a resource check if you can (@ug-security).

== What to Collect <ug-trouble-report>

#idx("problem report")
When a problem stays, collect these items before you ask for help:

- the messages of the start, from the banner to #cmd("HTTPD001I"), or the lines
  of the failure, from the console or from the Master Trace Table
- the Parmlib member that the server read
- the output of #cmd("F HTTPD,D V") and #cmd("F HTTPD,D C"), which show the
  version and the configuration that the server runs
- the status line and the body of the response, from #cmd("curl -i")
- for a module that abended, the name and the abend code from #cmd("HTTPD908E")
  and the dump, if you have it

Remove passwords and the names of your own systems from what you send.
