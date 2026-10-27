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
    MVS 3.8j (httpd 4.5.0), and to all subsequent releases and modifications
    until otherwise indicated in new editions.

    *Draft.* HTTPD 4.5 is still in development, and so is this book. The
    console and client sessions in it were captured from a development build
    of HTTPD 4.5. The chapters on installing, upgrading and removing HTTPD
    were measured on release 4.4.0 and are to be measured again on the
    release 4.5.0.

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
