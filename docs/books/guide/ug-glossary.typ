#import "../bookmaster/bookmaster.typ": *

= Glossary <ug-glossary>

This glossary defines the terms of HTTPD, and of HTTP, MVS and the security
product as far as this book uses them. A term printed in _italics_ in a
definition has an entry of its own.

#let g(..items) = deflist(width: 1.45in, ..items.pos().enumerate().map(
  ((i, x)) => if calc.even(i) { par(justify: false, strong(x)) } else { x }))

#g(
  [ACEE], [The accessor environment element. It holds the identity of a user
    for the security product. HTTPD creates one for each login, and one for the
    _started task_ (@ug-security).],
  [APF], [The authorized program facility. A program in an APF-authorized
    library can run with authority. HTTPD needs it, and it obtains it from the
    library or with _SVC 244_ (@ug-install, @ug-trouble).],
  [AUTH=], [The route option that sets how a route asks for a login. The values
    are #cmd("NONE"), #cmd("FORM"), #cmd("BASIC") and #cmd("TOKEN"). A route
    without it is public (@ug-routes, @ug-security).],
  [Basic], [The login mode #cmd("AUTH=BASIC"). The server answers a request
    without credentials with 401 and a #cmd("WWW-Authenticate") header that
    names the _realm_. A request with the header #cmd("X-CSRF-ZOSMF-HEADER")
    gets the 401 without that header (@ug-security).],
  [chunked transfer], [A way to send a body of unknown length. The response has
    the header #cmd("Transfer-Encoding: chunked") and no #cmd("Content-Length").
    The server adds the header itself when a module ends the headers with a
    blank line of its own (@ug-module, @ug-trouble).],
  [display module], [A _server module_ of HTTPD that shows the state of the
    server over HTTP, for example HTTPDMTT for the _master trace table_
    (@ug-ops).],
  [DOCROOT], [The Parmlib keyword that names the _UFS_ directory with the static
    files. The default is #cmd("/www") (@ug-static).],
  [FMID], [The id of a function SYSMOD in _SMP_. HTTPD has one for each minor
    release. The id starts with #cmd("THTP") and ends with three digits, for
    example #cmd("THTP440"). A new release deletes the id of the earlier one,
    and an id is used once (@ug-upgrade, @ug-remove).],
  [FORM], [The login mode #cmd("AUTH=FORM"). A request without a login gets
    a 302 to the login form (@ug-security).],
  [HTTPC], [The control block of one client, 4096 bytes. The server creates it
    when it accepts a connection and frees it when the connection closes. A
    _server module_ works with a pointer to it (@ug-module).],
  [HTTPD], [The HTTP server for MVS 3.8j, and the name of its control block.
    The block holds the state of the whole server (@ug-intro, @ug-module).],
  [HTTPX], [The function vector. A _server module_ calls the functions of the
    server through it. New entries are added at the end, so a module built
    against an earlier release keeps working (@ug-module).],
  [keep-alive], [A connection that stays open after a response, so that the
    client can send the next request on it. It holds one _worker_ until it
    closes. #cmd("KEEPALIVE_TIMEOUT") limits the wait between two requests
    (@ug-config, @ug-trouble).],
  [link list], [The list of libraries that the system searches for a load
    module after the STEPLIB. HTTPD finds its _server modules_ in the STEPLIB
    and then in the link list (@ug-routes).],
  [listener], [The socket on which the server waits for connections. The
    _socket thread_ accepts the connections on it. #cmd("PORT") sets its port
    (@ug-config).],
  [load module], [A program in a load library. A _server module_ is a load
    module that HTTPD runs with the LINK service (@ug-module).],
  [LOC], [The Parmlib keyword for a route that has no program. It makes a
    path prefix a place for _static files_, with its own #cmd("AUTH=")
    (@ug-routes, @ug-static).],
  [master trace table], [The table in which the system keeps the most recent
    console messages. The _display module_ HTTPDMTT shows it over HTTP
    (@ug-ops, @ug-trouble).],
  [MOD], [The Parmlib keyword for a route that runs a _server module_. The
    first route that matches the path wins (@ug-routes).],
  [Parmlib], [The data set with the configuration of HTTPD. The DD name is
    #cmd("HTTPPRM"). The member is chosen with #cmd("M=") on the start command,
    and the default is #cmd("HTTPPRM0") (@ug-config).],
  [RACF], [The security product of IBM. HTTPD asks the security product,
    through _SAF_, to check a user id and a password and to check access to a
    resource.],
  [RAKF], [A security product for MVS 3.8j that answers the _SAF_ calls of
    HTTPD. A new profile is active after the started task #cmd("RAKFPROF") has
    run (@ug-security, @ug-trouble).],
  [realm], [The name that the server gives in the Basic challenge and in the
    login form. The Parmlib keyword #cmd("REALM") sets it (@ug-security).],
  [RES=], [The route option that names a resource in a class of the security
    product. A logged-in user needs READ access to it. Without access the
    answer is 403 (@ug-security).],
  [resident module], [A _server module_ that the server loads once at the start
    and holds until it ends. After a new build, stop the task and start it
    again (@ug-routes, @ug-trouble).],
  [SAF], [The system authorization facility. The interface through which a
    program calls the security product (@ug-security).],
  [server module], [A load module that serves the requests of a route and
    runs in the task of HTTPD. mvsMF is one (@ug-module).],
  [session], [The state of a login in the server: the user, the _ACEE_, the
    address of the client and a _token_. It ends by idle time
    (#cmd("SESSION_TIMEOUT")), by age (#cmd("SESSION_MAXAGE")), by logout, or
    when the server ends (@ug-security, @ug-config).],
  [SMF], [The system management facilities. HTTPD can write SMF records
    (@ug-ops, @ug-config).],
  [SMP], [The program that installs and removes products on MVS in units
    called SYSMODs. HTTPD is installed as a function SYSMOD with an _FMID_
    (@ug-install, @ug-remove).],
  [socket thread], [The thread that waits on the _listener_ and gives each new
    connection to a _worker_. It has a stack of 32 KB (@ug-ops).],
  [started task], [A program that the operator starts with the #cmd("S")
    command from a cataloged procedure. HTTPD runs as a started task, and the
    procedure is #cmd("HTTPD") (@ug-ops).],
  [static file], [A file that the server sends as it is, from the _UFS_ under
    _DOCROOT_.
    (@ug-static).],
  [STCUSER], [The PARM keyword of the procedure that names the user id under
    which the started task runs after its start. The matching keyword for the
    group is #cmd("STCGROUP"). The default is the user #cmd("HTTPD") in the group
    #cmd("USER"). Only if the logon fails does the task keep the identity that
    it inherited (@ug-security, @ug-trouble).],
  [STEPLIB], [The DD of the procedure with the load libraries. The system
    searches it first and then the _link list_ (@ug-routes).],
  [SVC 244], [The service call through which a program obtains _APF_
    authorization at run time. The profile #cmd("SVC244") in the class
    #cmd("FACILITY") gates it (@ug-trouble).],
  [token], [A value that stands for a login. The server sets it in the cookie
    #cmd("Sec-Token"). A route with #cmd("AUTH=TOKEN") answers a request
    without credentials with a bare 401 (@ug-security).],
  [UFS], [The Unix-like file system with directories and long names that UFSD
    provides. HTTPD reads its _static files_ from it (@ug-static).],
  [UFSD], [The product that serves a _UFS_ to other tasks. It mounts the
    _webroot image_ at the path that is the value of _DOCROOT_ (@ug-static).],
  [webroot image], [The file system image with the static files of HTTPD. The
    package delivers it as #cmd("httpd-webroot.img"). It is uploaded to a data
    set and mounted by UFSD (@ug-install, @ug-static).],
  [worker], [A thread that serves the requests of one connection from start
    to end. The pool has #cmd("MINTASK") to #cmd("MAXTASK") workers, and each
    has a stack of 64 KB (@ug-config, @ug-trouble).],
)
