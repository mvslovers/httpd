#import "../bookmaster/bookmaster.typ": *

= Serving Files <ug-static>

#idx("static files")
HTTPD answers a request that no route claims by sending a file from its
document root. This chapter shows where those files are, how they get there,
and what HTTPD makes of a request for one: the content type, the directory
rules, the path rules and the methods it accepts. The sessions use #cmd("curl")
as in @ug-first. The Reference describes the same rules as a list. It is the
chapter "HTTP Behaviour and Limits" in the _HTTPD Reference_, and the
keywords are in "The HTTPPRM Keywords".

== Where the Files Are <ug-static-docroot>

#idx("document root")
#idx("DOCROOT")
#idx("UFS")
#idx("UFSD")
HTTPD does not read files from MVS data sets. It reads them from a UFS file
system, which UFSD provides. The Parmlib keyword #cmd("DOCROOT") names the
directory in that file system where the files are. The default is
#cmd("/www"). HTTPD puts the value in front of the path of the request:
a request for #cmd("/hello.txt") reads the file #cmd("/www/hello.txt").

Message #cmd("HTTPD001I") names the document root when HTTPD is ready
(@ug-first-console). A request for a file that HTTPD cannot open gets the
same answer as a request for a file that is not there (@ug-first-404).

Two parts make the document root:

- *A disk image.* The UFS file system lives in an MVS data set. The release of
  HTTPD includes an image with the welcome page. You build an image of your
  own with the host tool #cmd("ufsd-utils") (@ug-static-image).
- *A mount.* UFSD attaches the image to the path #cmd("/www"). The mount is
  a statement in the Parmlib member of UFSD, not of HTTPD
  (@ug-static-mount).

The keyword #cmd("DOCROOT") has three rules. The value must start with a
slash, or HTTPD ignores it without a message and keeps the old value. HTTPD
removes one slash at the end of a longer value. HTTPD keeps at most 127
characters. The Reference has the details.

== Building a Web Root Image <ug-static-image>

#idx("web root image")
#idx("ufsd-utils")
#idx("HTTPD.WEBROOT.UFS")
To publish your own files, build an image on a workstation. The host tool
#cmd("ufsd-utils") of UFSD does it in three commands. The first creates an
empty image. The second copies a directory tree into it. The third lists what
is in it:

#screen(raw(read("../ex/ug-static/image-build.txt")))

Read the output of the first command as follows. The image is 1 MB. It has
256 blocks of 4096 bytes, and 2 of them hold the inode table with room for 64
inodes. #cmd("--owner") and #cmd("--group") set the owner and the group of the
root directory. The size and the number of inodes limit how many files and how
much data the image holds. Use #cmd("--size") to make it larger, and see the
_UFSD User's Guide_ for the other options.

The output of #cmd("cp -r") has one line for each file and each directory. At
the end of each file line the tool says #cmd("txt") or #cmd("bin"). It stores
a text file in EBCDIC, in the code page IBM-1047, and a binary file as it is.
HTTPD translates a UFS text file back with the same table
(@ug-static-xlate).

The create command suggests the next step, uploading the image with
#cmd("ufsd-utils upload"). The data set that takes the image is a
sequential data set with the record format U and a block size of 4096, the
block size of the image. The job #cmd("HTTPWEBR") in the sample library
allocates such a data set under the name #cmd("HTTPD.WEBROOT.UFS"). The upload
must be binary. @ug-install has the whole procedure, with the
installation of the image that ships with HTTPD.

== Mounting the Image <ug-static-mount>

#idx("MOUNT")
#idx("UFSD", "mount")
UFSD mounts the image at the start from its own Parmlib member. The statement
for the data set above and the default document root is:

#screen(```
MOUNT    DSN(HTTPD.WEBROOT.UFS)  PATH(/www)  MODE(RO)
```)

#cmd("MODE(RO)") mounts the image for reading. HTTPD only reads. The path in
the statement must be the value of #cmd("DOCROOT"), letter for letter.
Start UFSD before HTTPD, so that the first request finds the file system.
@ug-install shows the installation of the image, and the _UFSD User's Guide_
describes the Parmlib member and the operator commands. UFSD changes an
operator command to upper case before it reads it. A mount point such as
#cmd("/www") therefore comes from the Parmlib statement (@ug-install).

To replace the content, stop HTTPD and UFSD, upload the new image to the same
data set, and start UFSD, then HTTPD. @ug-ops describes stopping and starting.

== Changing the Document Root <ug-static-change>

#idx("DOCROOT", "changing")
The document root need not be the root of the mounted file system. If the
image has a directory #cmd("docs"), the member below serves that directory as
the whole site:

#screen(raw(read("../ex/ug-config/docroot-subdir-prm.txt")))

After the restart, the path #cmd("/") reads #cmd("/www/docs/index.html"):

#screen(raw(read("../ex/ug-config/docroot-subdir-root.txt")))

The file #cmd("/www/docs/manual.txt") is now the path #cmd("/manual.txt"):

#screen(raw(read("../ex/ug-config/docroot-subdir-file.txt")))

A file outside the new root is not found, although it is in the image:

#screen(raw(read("../ex/ug-config/docroot-subdir-outside.txt")))

The file #cmd("/www/hello.txt") exists, but the new document root is
#cmd("/www/docs"). A change of #cmd("DOCROOT") takes effect at the next start
of HTTPD (@ug-config).

== Directory Requests <ug-static-dirs>

#idx("directory request")
#idx("index.html")
#idx("default.html")
A path that ends in a slash names a directory. HTTPD does not list a
directory. It looks for the file #cmd("index.html") in it, and if that is not
there, for #cmd("default.html"). A directory with an index file answers with
the file:

#screen(raw(read("../ex/ug-static/dir-index.txt")))

A directory without #cmd("index.html") and #cmd("default.html") gets a 404.
The text repeats the path of the request:

#screen(raw(read("../ex/ug-static/dir-noindex.txt")))

A directory in a path without the closing slash is not a file, and HTTPD does
not add the slash for the client. It does not answer with a redirect:

#screen(raw(read("../ex/ug-static/dir-noslash.txt")))

So a link to a directory has to end in a slash. The path #cmd("/docs") gets
the answer above, and the path #cmd("/docs/") gets the index file.

== Content Types <ug-static-mime>

#idx("MIME types")
#idx("Content-Type")
#idx("file extension")
HTTPD chooses the #cmd("Content-Type") from the extension of the file name:
the text after the last dot in the last part of the path. If that part has no
dot, HTTPD looks at the whole path. The comparison
ignores case, so #cmd("LOGO.PNG") and #cmd("logo.png") are the same.

#tab(caption: [Some content types])[
  #table(columns: (1.4in, 2.4in, 1fr),
    [Extension], [Content-Type], [Sent as],
    [#cmd("html"), #cmd("htm")], [#cmd("text/html")], [text],
    [#cmd("css")], [#cmd("text/css")], [text],
    [#cmd("js")], [#cmd("application/x-javascript")], [text],
    [#cmd("txt")], [#cmd("text/plain")], [text],
    [#cmd("xml")], [#cmd("text/xml")], [text],
    [#cmd("svg")], [#cmd("image/svg+xml")], [text],
    [#cmd("png")], [#cmd("image/png")], [binary],
    [#cmd("jpg"), #cmd("jpeg")], [#cmd("image/jpeg")], [binary],
    [#cmd("gif")], [#cmd("image/gif")], [binary],
    [#cmd("ico")], [#cmd("image/x-icon")], [binary],
    [#cmd("pdf")], [#cmd("application/pdf")], [binary],
    [#cmd("zip")], [#cmd("application/zip")], [binary],
    [any other, or none], [#cmd("text/plain")], [text],
  )
]

The whole table is in "HTTP Behaviour and Limits" in the _HTTPD Reference_.
Three captures show the effect. A style sheet is text and has its type:

#screen(raw(read("../ex/ug-static/css.txt")))

A PNG image has its type, and the body is sent as it is stored. The capture
shows only the headers:

#screen(raw(read("../ex/ug-static/png.txt")))

A file whose extension is not in the table is sent as #cmd("text/plain"), also
when the file is JSON:

#screen(raw(read("../ex/ug-static/json.txt")))

#note[*Unknown extensions are text.* HTTPD translates every file with an
unknown extension as text, and a binary file of that kind arrives damaged.
Keep to the extensions in the table for binary files, or rename the file.]

=== Text, Binary and the Code Page <ug-static-xlate>

#idx("code page", "static files")
#idx("CODEPAGE")
#idx("text and binary")
The last column of the table says how HTTPD sends the body.

- *Text.* HTTPD translates every byte from EBCDIC to ASCII. It uses the table
  for IBM-1047, whatever the Parmlib keyword #cmd("CODEPAGE") says.
- *Binary.* HTTPD sends the bytes as they are stored in the image.

The keyword #cmd("CODEPAGE") selects the table for the status line, the
headers and the text of the responses that HTTPD writes itself, and for the
decoding of the request (see "Paths" below). It has no effect on the content
of a file. So a text file in the image has to be in IBM-1047. The tool
#cmd("ufsd-utils") writes the files of a text type in that code page
(@ug-static-image). The chapter @ug-config tells how to set #cmd("CODEPAGE").

=== Server Side Includes <ug-static-ssi>

#idx("SSI")
#idx("server side includes")
HTTPD scans a file of the type #cmd("text/html") or
#cmd("text/x-server-parsed-html") for server side includes while it sends it.
These are the extensions #cmd("htm"), #cmd("html"), #cmd("shtm") and
#cmd("shtml"). A directive such as #cmd("<!--#include virtual=\"/part.html\" -->")
is replaced by the text it names. Because the final size is not known, the
response has no #cmd("Content-Length") and uses chunked transfer, as the
response of the welcome page in @ug-first-welcome does. The directives and
their limits are in "HTTP Behaviour and Limits" in the _HTTPD Reference_.

Any other file, also a file with the extension #cmd("txt"), is sent without a
scan.

== Paths <ug-static-path>

#idx("path", "decoding")
#idx("percent encoding")
#idx("path traversal")
HTTPD does five things to the path of a request before it joins it to the
document root:

- It cuts off the query. The text after the first question mark is not part of
  the file name.
- It decodes the path. Each #cmd("%xx") becomes the character with that
  hexadecimal value, translated through the table of #cmd("CODEPAGE"). A plus
  sign becomes a blank, also in the path. To ask for a plus sign in a file
  name, write #cmd("%2B").
- It refuses a path with two dots in a row. The check is on the decoded
  path, so #cmd("%2e%2e") does not help either. The answer is a 404. A file
  name that has two dots in a row, such as #cmd("a..b.txt"), cannot be served.
- It cuts the path to 255 characters, and the joined name of the document root
  and the path to 255 characters. The client is not told.
- It looks at the extension for the content type (@ug-static-mime).

The answer for a path that does not name a file is the 404 that the first
chapter shows. Its text contains the decoded path:

#screen(raw(read("../ex/ug-static/not-found.txt")))

== What Is Not Supported <ug-static-methods>

#idx("methods", "static files")
#idx("HEAD")
#idx("501 Not Implemented")
#idx("405 Method Not Allowed")
A static file is read-only for the client and always whole. HTTPD answers a
GET. It does not answer anything else for a file.

*Methods.* HEAD, POST, PUT and DELETE for a static file get
#cmd("501 Not Implemented"). The curl option #cmd("-I") sends a HEAD
request, as @ug-first-client shows. The three other methods:

#screen(raw(read("../ex/ug-static/post-to-file.txt")))

#screen(raw(read("../ex/ug-static/put-file.txt")))

#screen(raw(read("../ex/ug-static/delete-file.txt")))

The responses to POST and PUT have #cmd("Connection: close") because the
requests have a body. The response to DELETE, which has none, keeps the
connection. A client cannot change a file in the image over HTTP. Replace the
image to change the content (@ug-static-mount).

A method that HTTPD does not know at all gets #cmd("405 Method Not Allowed")
and the connection is closed. The answer has no #cmd("Allow") header. HTTPD
sends the 405 before it looks at the route, so it is the same for every path:

#screen(raw(read("../ex/ug-static/method-not-allowed.txt")))

*Ranges.* HTTPD does not look at a #cmd("Range") header. It sends the whole
file with status 200 and no #cmd("Content-Range") header:

#screen(raw(read("../ex/ug-static/range-ignored.txt")))

*Conditional requests.* HTTPD does not look at #cmd("If-Modified-Since")
either. It never answers with 304, and it sends no #cmd("Last-Modified") or
#cmd("ETag") header:

#screen(raw(read("../ex/ug-static/conditional-ignored.txt")))

A client that sends these headers gets the whole file each time. A server
module on a route with a program decides about the methods #cmd("GET"),
#cmd("HEAD"), #cmd("POST"), #cmd("PUT") and #cmd("DELETE") and about the
headers for its own paths. @ug-routes tells how routes are matched.

== Static Locations <ug-static-loc>

#idx("LOC")
#idx("static locations")
A request that no route claims is served from the document root, and nobody
needs to log in for it. To ask for a login for a part of the tree, name it in
a #cmd("LOC=") route. A #cmd("LOC=") route has no program. HTTPD serves the
path as a static file, as in this chapter, after the check of the route has
passed. The Parmlib member of the test system has these lines:

#screen(```
LOC=/pub/*                               AUTH=NONE
LOC=/private/*                           AUTH=BASIC
LOC=/admin/*                             AUTH=BASIC RES=FACILITY:HTTPD.ADMIN
LOC=/denied/*                            AUTH=BASIC RES=FACILITY:HTTPD.DENIED
```)

A path under #cmd("/pub/") is public:

#screen(raw(read("../ex/ug-security/pub-open.txt")))

A path under #cmd("/private/") asks for a login, and a request without
credentials gets a 401 with a #cmd("WWW-Authenticate") header:

#screen(raw(read("../ex/ug-security/basic-no-credentials.txt")))

A user who logs in but has no access to the resource of the route gets a 403:

#screen(raw(read("../ex/ug-security/denied-403.txt")))

The syntax of the routes and the order in which HTTPD tries them are in
@ug-routes. The modes of #cmd("AUTH=") and the resource check are in
@ug-security.

== When UFSD Is Not There <ug-static-noufsd>

#idx("UFSD", "not running")
#idx("HTTPD044W")
#idx("UFS=0")
HTTPD reads static files through UFSD. It creates the control block of its
file system when it starts. This step does not contact UFSD, and HTTPD does
not check that UFSD runs. The message
#cmd("HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM") says that HTTPD could not
create that block. It does not say that UFSD is down.

HTTPD asks UFSD for a session when it opens a file, for each request. In each
of these cases HTTPD gets no file, and the answer is the 404 of
@ug-first-404:

- HTTPD cannot get a session from UFSD.
- Nothing is mounted at the document root, or the mount is at another path.
- The file or the directory is not in the image.

The Parmlib keyword #cmd("UFS=0") switches the file system off. HTTPD then
serves no file, and every path that no route claims gets a 404. Leave the
keyword at its default if you want to serve files. A route with a program is
dispatched without the file system.

When every static request gets a 404, check in this order: that UFSD runs, that
the image is mounted (the operator command #cmd("F UFSD,MOUNT LIST") shows the
mounts), that the mount point is the value of #cmd("DOCROOT"), and that the file
is in the image. See also @ug-trouble.

== What Can Go Wrong <ug-static-wrong>

#idx("troubleshooting", "static files")
- *A binary file arrives damaged.* Its extension is not in the table, so HTTPD
  sent it as text (@ug-static-mime).
- *Text shows wrong characters.* The file in the image is not in IBM-1047, or
  the upload of the image was not binary.
- *A link to a directory gives 404.* The link has no closing slash
  (@ug-static-dirs).
- *A file with a double dot in its name is not found.* HTTPD refuses any path
  with two dots in a row (@ug-static-path).
- *A browser shows an old copy of a file.* Among the files,
  only HTML documents get #cmd("Cache-Control: no-store"), and HTTPD sends no
  #cmd("Last-Modified") or #cmd("ETag") for any file. The browser decides by
  its own rules for the others.
- *The change of #cmd("DOCROOT") has no effect.* HTTPD reads the Parmlib at
  start only. Restart it (@ug-ops).
