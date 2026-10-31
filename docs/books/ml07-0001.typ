#import "bookmaster/bookmaster.typ": *

#show: book.with(
  title: "HTTPD for MVS 3.8j",
  subtitle: "User's Guide",
  short-title: "HTTPD User's Guide",
  product: "HTTPD",
  number: "ML07-0001-0",
  date: "October 2026",
  authors: ("Mike Großmann",),
  edition: [
    #text(font: head-font, weight: "bold", size: 11pt)[First Edition (October 2026)]

    This edition applies to Version 4 Release 5 of HTTPD, the HTTP server for
    MVS 3.8j (httpd 4.5.1), and to all subsequent releases and modifications
    until otherwise indicated in new editions.

    *Draft.* This is a draft edition, as the last digit of the number says.
    The console and client sessions in the chapters on configuring and
    running HTTPD were captured from development builds of HTTPD 4.5.0 and
    4.5.1, which show the version #raw("4.5.0-DEV") or #raw("4.5.1-DEV").
    The sessions in the chapters on
    installing and upgrading HTTPD were captured on a TK5 system with a package
    build of HTTPD 4.5.1 that was made from the source of the release branch,
    with the version in the project file changed from #raw("4.5.1-dev") to
    #raw("4.5.1"):
    the install on a system without HTTPD and the upgrade from 4.5.0 to 4.5.1.
    A first install of the same package build on an MVS/CE system showed the
    same start. A start of the published package of 4.5.1 was not measured.
    The sessions in the chapter on removing HTTPD were captured on
    release 4.4.0 on a TK5 system, except the section on a development build,
    which comes from development builds of 4.5.0. The removal job for 4.5.0
    was run on a TK5 system as well. The removal job for 4.5.1 was run on an
    MVS/CE system that held the released 4.5.0 and on a TK5 system that held
    4.5.1. The chapters name what they measured.

    Comments on this book may be addressed to the issue tracker of the
    mvslovers/httpd repository on GitHub.

    © Copyright Mike Großmann 2026. All rights reserved.
  ],
)

#part("index.html", title: [HTTPD User's Guide])[
#titlepage()
#contents()
#figures()

#heading(numbering: none)[About This Book] <about>

This book shows how to install, configure and run HTTPD (httpd), the HTTP
server for MVS 3.8j: how to put it on a system, how to describe routes and
server modules in the configuration member, how HTTPD authenticates a client
with the security system, how it serves files, how an operator starts,
queries and stops it, how to write a server module of one's own, and how to
upgrade and remove it. Every configuration keyword, console command,
interface function, record layout and message in full is the subject of the
companion volume, the _HTTPD Reference_.

The book describes HTTPD 4.5, as it comes with the release. HTTPREXX and
HTTPLUA, the server modules for REXX Server Pages and for Lua, have their own
books.

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
  [Chapter 1], [“Introducing HTTPD”.],
  [Chapter 2], [“Installing HTTPD”.],
  [Chapter 3], [“A First Request”.],
  [Chapter 4], [“Configuring HTTPD”.],
  [Chapter 5], [“Routes and Server Modules”.],
  [Chapter 6], [“Authentication and Security”.],
  [Chapter 7], [“Serving Files”.],
  [Chapter 8], [“Operating HTTPD”.],
  [Chapter 9], [“Writing a Server Module”.],
  [Chapter 10], [“Upgrading HTTPD”.],
  [Chapter 11], [“Removing HTTPD”.],
  [Chapter 12], [“Troubleshooting”.],
  [Appendix A], [“Glossary”.],
)

== Related Publications

#deflist(width: 1.35in,
  [ML07-0002], [_HTTPD Reference_: the Parmlib keywords, the route syntax,
    the console commands, the started task, the HTTP behaviour and limits,
    the server module interface, the display modules, the SMF records and the
    messages of HTTPD.],
  [ML05-0001], [_UFSD User's Guide_: the file system server that holds the
    document tree HTTPD serves.],
  [ML02-0001], [_MBT User's Guide_: the build tool that builds HTTPD and
    packages its releases.],
)

#mainmatter()
]
#set page(numbering: "1")

#part("ug-intro.html", include "guide/ug-intro.typ")
#part("ug-install.html", include "guide/ug-install.typ")
#part("ug-first.html", include "guide/ug-first.typ")
#part("ug-config.html", include "guide/ug-config.typ")
#part("ug-routes.html", include "guide/ug-routes.typ")
#part("ug-security.html", include "guide/ug-security.typ")
#part("ug-static.html", include "guide/ug-static.typ")
#part("ug-ops.html", include "guide/ug-ops.typ")
#part("ug-module.html", include "guide/ug-module.typ")
#part("ug-upgrade.html", include "guide/ug-upgrade.typ")
#part("ug-remove.html", include "guide/ug-remove.typ")
#part("ug-trouble.html", include "guide/ug-trouble.typ")

#show: appendices
#part("ug-glossary.html", include "guide/ug-glossary.typ")

#part("index-terms.html", title: [Index])[
#heading(numbering: none)[Index]
#make-index()
]
