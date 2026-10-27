#import "../bookmaster/bookmaster.typ": *

= SMF Records <ref-smf>

#idx("SMF", "records")
#idx("SMF", "record type")
#idx("accounting")
HTTPD writes two kinds of SMF record. The _request record_ tells what one HTTP
request did. The _session record_ tells what one client connection did. Both
start with the same 18-byte header and have a fixed body. This chapter lists
the record types, the header, both bodies field by field, the levels that
choose which records HTTPD writes, and how to read the fields.

HTTPD writes the records through the SMF routines of the C library. It writes
nothing while SMF is not active in the system (@ref-smf-levels).

== Record Types and Subtypes <ref-smf-types>

#idx("SMF", "type 243")
#idx("SMF", "subtype")
All records of HTTPD have one SMF record type. The default is 243. The Parmlib
keyword #cmd("SMF") sets another type in the range 128 to 255
(@ref-smf-levels). The subtype is a halfword in the body of the record, at
offset 26 (hex 1A).

#tab(caption: [The subtypes of the HTTPD record])[
  #table(columns: (0.8in, 1.0in, 0.9in, 1fr),
    [Subtype], [Name], [Length], [Meaning],
    [1], [Request], [124 bytes], [One record for each HTTP request that the
      level selects. HTTPD writes it when it has finished the request.],
    [2], [Session], [56 bytes], [One record for each client connection. HTTPD
      writes it when it closes the connection, and only at the level
      #cmd("ALL").],
  )
]

The eight-byte field #cmd("subsys") at offset 18 is always the characters
#cmd("HTTPD") followed by three blanks, in both subtypes. The characters are
in EBCDIC.

== The Header <ref-smf-hdr>

#idx("SMF", "header")
Every record starts with the standard SMF header of 18 bytes. HTTPD sets all
its fields. The body begins at offset 18.

#tab(caption: [The 18-byte header, the same in both subtypes])[
  #table(columns: (0.6in, 0.5in, 0.65in, 0.95in, 1fr),
    [Offset], [Hex], [Length], [Type], [Meaning],
    [0], [00], [2], [binary], [The length of the whole record: 124 for the
      request record, 56 for the session record.],
    [2], [02], [2], [binary], [The segmentation descriptor. HTTPD sets it to
      0.],
    [4], [04], [1], [flags], [The system indicator. HTTPD sets it to 2.],
    [5], [05], [1], [binary], [The record type: the value of #cmd("TYPE="),
      243 by default.],
    [6], [06], [4], [binary], [The time of day in hundredths of a second since
      midnight, local time.],
    [10], [0A], [1], [binary], [The century indicator of the date: 0 for the
      years 19xx, 1 for 20xx.],
    [11], [0B], [3], [packed], [The date as the digits #cmd("YYDDD") and the
      sign C, which is positive: the two digits of the year within the century and the three
      digits of the day of the year.],
    [14], [0E], [4], [characters], [The system ID (SID) of the system that
      writes the record.],
  )
]

HTTPD takes the time and the date from the local clock when it writes the
record. For a request record this is the time at which the request ended. For
a session record it is the time at which the connection closed.

== The Request Record <ref-smf-req>

#idx("SMF", "request record")
#idx("SMF", "subtype 1")
The request record has 124 bytes (hex 7C). HTTPD sets the whole record to
binary zeros first and then fills the fields. A field that the text calls
"padded with zeros" therefore ends in binary zeros, not in blanks.

#tab(caption: [The request record, subtype 1])[
  #table(columns: (0.6in, 0.5in, 0.65in, 0.95in, 1fr),
    [Offset], [Hex], [Length], [Type], [Meaning],
    [0], [00], [18], [header], [The header (@ref-smf-hdr).],
    [18], [12], [8], [characters], [#cmd("subsys"): #cmd("HTTPD") and three
      blanks.],
    [26], [1A], [2], [binary], [#cmd("subtype"): 1.],
    [28], [1C], [8], [characters], [#cmd("userid"): the user of the request.
      Eight blanks when the request has no credential.],
    [36], [24], [4], [binary], [#cmd("client_addr"): the IPv4 address of the
      client. The four bytes are in the order a.b.c.d (the network order), so
      the first byte is the first number of the address.],
    [40], [28], [4], [unsigned binary], [#cmd("resp_code"): the HTTP status
      code of the response, for example 200 or 404.],
    [44], [2C], [4], [unsigned binary], [#cmd("bytes_sent"): the number of
      bytes that HTTPD handed to the network for this response.],
    [48], [30], [4], [unsigned binary], [#cmd("duration_us"): the duration of
      the request in microseconds.],
    [52], [34], [8], [characters], [#cmd("method"): the request method, for
      example #cmd("GET"), cut to 8 characters and padded with zeros.],
    [60], [3C], [64], [characters], [#cmd("uri"): the path of the request,
      decoded and without the query string, cut to 64 characters and padded
      with zeros. A path of 64 or more characters has no terminating zero.],
  )
]

#deflist(width: 1.35in,
  [userid], [The user ID of the credential that the request carried. A user
    ID of fewer than 8 characters is not padded with blanks. The credential
    store pads it with binary zeros, so read the field up to the first zero
    or blank. A request without a credential, for example a request to a
    public route, has 8 blanks.],
  [bytes_sent], [The counter belongs to one request. It counts every byte that
    HTTPD sends for the response, which are the status line, the headers and
    the body, and the framing of a chunked response. The counter starts again
    at zero for the next request of a kept-alive connection. When the client
    breaks the connection, the field holds the bytes that went out before the
    break.],
  [duration_us], [The time from the start mark to the end mark of the request.
    HTTPD sets the start mark when it accepts the connection, and again each
    time it resets a kept-alive connection for the next request. For a request
    on a kept-alive connection the duration therefore includes the time that
    HTTPD waited for the request to arrive. HTTPD sets the end mark when the
    response is complete. A negative difference is stored as 0. The field is
    unsigned and holds about 4 294 seconds at most.],
  [method, uri], [The URI is the path only. The query string is not recorded.],
)

== The Session Record <ref-smf-sess>

#idx("SMF", "session record")
#idx("SMF", "subtype 2")
The session record has 56 bytes (hex 38). HTTPD writes it once for a client
connection, when it closes the connection. It exists only at the level
#cmd("ALL").

#tab(caption: [The session record, subtype 2])[
  #table(columns: (0.6in, 0.5in, 0.65in, 0.95in, 1fr),
    [Offset], [Hex], [Length], [Type], [Meaning],
    [0], [00], [18], [header], [The header (@ref-smf-hdr).],
    [18], [12], [8], [characters], [#cmd("subsys"): #cmd("HTTPD") and three
      blanks.],
    [26], [1A], [2], [binary], [#cmd("subtype"): 2.],
    [28], [1C], [8], [characters], [#cmd("userid"): the user of the
      connection when it closed. See below.],
    [36], [24], [4], [binary], [#cmd("client_addr"): the IPv4 address of the
      client, in network order as in the request record.],
    [40], [28], [4], [unsigned binary], [#cmd("connect_time"): the local time at
      which HTTPD accepted the connection, in hundredths of a second since
      midnight.],
    [44], [2C], [4], [unsigned binary], [#cmd("duration_us"): the duration of
      the connection in microseconds. See below.],
    [48], [30], [4], [unsigned binary], [#cmd("request_count"): the number of
      requests after which the connection stayed open. See below.],
    [52], [34], [4], [unsigned binary], [#cmd("total_bytes"): the sum of the
      #cmd("bytes_sent") values of the requests of the connection.],
  )
]

#deflist(width: 1.35in,
  [userid], [HTTPD takes the credential away from the connection when it
    resets the connection at the end of each request, and it writes the
    session record after that. The field is therefore normally 8 blanks. It
    holds a user ID only when the worker closes the connection without a
    reset, which is the case after a failure in the worker.],
  [duration_us], [HTTPD takes the local time of the close, in hundredths of a
    second since midnight, subtracts #cmd("connect_time") and multiplies the
    result by 10 000. If the close time is smaller than the connect time, which
    happens when the connection spans midnight, the field is 0. The resolution
    is a hundredth of a second.],
  [request_count], [HTTPD adds 1 each time it resets the connection for the next
    request. A connection that carried one request and was then closed by the
    server has the value 0. A connection that carried three requests and was
    left idle until the timeout has the value 3.],
)

== Levels and Parmlib Keywords <ref-smf-levels>

#idx("SMF", "level")
#idx("SMF", "keyword")
#idx("SMF", "TYPE")
The level chooses which records HTTPD writes. The Parmlib keyword is
#cmd("SMF"), with an optional #cmd("TYPE="). The list of all keywords is in
@ref-prm.

#screen(```
SMF=ALL TYPE=244
```)

#tab(caption: [The recording levels])[
  #table(columns: (0.9in, 1fr),
    [Level], [HTTPD writes],
    [#cmd("NONE")], [No records. This is the default.],
    [#cmd("ERROR")], [A request record for each request whose status code is
      400 or more.],
    [#cmd("AUTH")], [A request record for each request with the status 401 or
      403, and for each other status of 400 or more. The set of requests is the
      same as at #cmd("ERROR").],
    [#cmd("ALL")], [A request record for every request, and a session record
      for every connection.],
  )
]

The rules for the keyword:

- The level is the first word after the equal sign. HTTPD compares the word
  with the names #cmd("NONE"), #cmd("ERROR"), #cmd("AUTH") and #cmd("ALL"), in
  this order, for the length of the word that you write. The comparison is not
  case sensitive, and a word that is the start of a name is taken for that name.
  #cmd("ERR") is #cmd("ERROR"). #cmd("A") is #cmd("AUTH"), because #cmd("AUTH")
  comes before #cmd("ALL"). Write the whole name.
- A word that is not a level gives the message #cmd("HTTPD431W") with the
  value of the keyword. The level stays as it was, which is #cmd("NONE") at
  start.
- HTTPD searches the rest of the value for #cmd("TYPE="). Capital letters are
  required. It converts the number after it as a decimal number. The number
  must be in the range 128 to 255. A number outside the range gives the message
  #cmd("HTTPD432W") with the number, and the type stays as it was, which is 243
  at start.
- The keyword may be used without #cmd("TYPE="). The type then stays.

The counters of the server (the total of requests, of errors and of bytes
sent) are updated for every request at every level, also at #cmd("NONE").

When the level at start is above #cmd("NONE") and SMF is not active in the
system, HTTPD writes the message #cmd("HTTPD430W") to the console. The server
runs and writes no records. HTTPD checks for every record whether SMF is
active, so records appear when SMF becomes active later. The SMF parameters of
the installation decide which record types the system keeps. This book does
not describe them.

The operator can change the level while the server runs, with the command
#cmd("F HTTPD,SET STATS") and one of the four names. The word #cmd("RESET")
after the level, or alone after #cmd("STATS"), sets the counters to zero. The
messages #cmd("HTTPD126I") (level set) and #cmd("HTTPD125I") (counters reset)
confirm the two actions. The commands are in @ref-con, the messages in
@apx-messages. The display commands show the type and the level
(#cmd("HTTPD132I") for the configuration, #cmd("HTTPD123I") for the
statistics). A change by command lasts until the server stops. It does not
change the Parmlib member.

== Reading the Records <ref-smf-read>

#idx("SMF", "byte order")
#idx("SMF", "time")
#idx("SMF", "date")
A program that reads the records follows these rules.

- All binary numbers are big endian, as the System/370 stores them. A program
  on a little-endian machine swaps the bytes of the halfwords and of the 4-byte
  numbers.
- The fields marked "characters" are EBCDIC, also the user ID, the method and
  the URI. The IPv4 address is not text. It is four bytes, each one a number
  from 0 to 255.
- The time in the header and #cmd("connect_time") are hundredths of a second
  since midnight in local time. Divide by 100 for seconds. One hour is
  360 000, one minute is 6 000.
- The date in the header is split in two fields. The century indicator at
  offset 10 is 0 for 19xx and 1 for 20xx. The three bytes at offset 11 are
  packed decimal #cmd("YYDDDC"). The 15th of March 2027 is the 74th day of the
  year. The century indicator is 1 and the three bytes are hex #cmd("27 07 4C").
- The two #cmd("duration_us") fields are unsigned 4-byte numbers. They wrap
  after about 4 294 seconds (71 minutes).
- Read #cmd("uri") and #cmd("method") up to the first binary zero, or up to the
  length of the field.

#note[HTTPD fills the time, the date, the system indicator and the system ID
through the C library of the platform. The layout of these header fields in
this chapter is the layout that the library documents for them.]
