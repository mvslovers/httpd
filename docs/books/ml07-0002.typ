#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "HTTPD for MVS 3.8j",
  subtitle: "Reference",
  short-title: "HTTPD Reference",
  product: "HTTPD",
  number: "ML07-0002-1",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[First Edition (October 2026)]

    This edition applies to Version 4 Release 5 of HTTPD, the HTTP server for
    MVS 3.8j (httpd 4.5.1), and to all subsequent releases and modifications
    until otherwise indicated in new editions.

    The console sessions in it that show the version of the server were
    captured on an MVS/CE system, a throwaway container, from a build of the
    4.5.1 source made before the release was published. The build stamp in
    them is not the one of the published package, which was not captured for
    this edition. The other console and client sessions may come from earlier
    builds of 4.5.0 and 4.5.1, which show the version #raw("4.5.0-DEV") or
    #raw("4.5.1-DEV").

    Comments on this book may be addressed to the issue tracker of the
    mvslovers/httpd repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#part("index.html", title: [HTTPD Reference])[
#titlepage()
#contents()
#figures()

#heading(numbering: none)[About This Book] <about>

This book describes HTTPD (httpd), the HTTP server for MVS 3.8j, in full: the
keywords of the configuration member, the syntax of a route, the operator
commands, the started task and the files of the package, the HTTP behaviour
and the limits of the server, the interface that server modules call, the
display modules, the SMF records and the messages. How to use them together
is the subject of the companion volume, the _HTTPD User's Guide_.

== The Examples in This Book

The console screens and the client sessions in this book are output that was
recorded from a running HTTPD. None of them was typed by hand. Before they were
printed, some values were replaced: passwords by asterisks, session tokens by
#cmd("<token>"), the digits of job and task numbers by #cmd("n"), the names and
addresses of the test systems by #cmd("localhost"), #cmd("mvs.example.org") and
the address #cmd("192.0.2.17") of the documentation range, and the home
directory of the user by #cmd("$HOME"). The addresses in the examples are
therefore not real addresses.

== How This Book Is Organized

#deflist(width: 1.35in,
  [Chapter 1], [“The HTTPPRM Keywords”.],
  [Chapter 2], [“Routes and Authentication Policy”.],
  [Chapter 3], [“Console Commands”.],
  [Chapter 4], [“The Started Task and the Package”.],
  [Chapter 5], [“HTTP Behaviour and Limits”.],
  [Chapter 6], [“The Server Module Interface”.],
  [Chapter 7], [“The Display Modules”.],
  [Chapter 8], [“SMF Records”.],
  [Appendix A], [“Messages”.],
)

== Related Publications

#deflist(width: 1.35in,
  [ML07-0001], [_HTTPD User's Guide_: installing, configuring, running and
    removing HTTPD, routes, authentication, serving files, and writing a
    server module.],
)

#mainmatter()
]
#set page(numbering: "1")

#part("ref-prm.html", include "ref/ref-prm.typ")
#part("ref-route.html", include "ref/ref-route.typ")
#part("ref-con.html", include "ref/ref-con.typ")
#part("ref-stc.html", include "ref/ref-stc.typ")
#part("ref-http.html", include "ref/ref-http.typ")
#part("ref-api.html", include "ref/ref-api.typ")
#part("ref-dm.html", include "ref/ref-dm.typ")
#part("ref-smf.html", include "ref/ref-smf.typ")

#show: appendices
#part("apx-messages.html", include "ref/apx-messages.typ")

#part("index-terms.html", title: [Index])[
#heading(numbering: none)[Index]
#make-index()
]
