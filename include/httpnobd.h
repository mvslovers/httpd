/* HTTPNOBD.H
** Does a response carry a message body?
**
** Free of httpd.h so the decision unit-tests on the host as well as on MVS
** (see test/tstnobd.c).  The name is 8 characters so it needs no asm() CSECT
** alias -- same reason as httpstat() and httpbody().
*/
#ifndef HTTPNOBD_H
#define HTTPNOBD_H

/* Return 1 when the response never carries a message body, 0 when it may.
**
** RFC 7230 3.3.3: a response to HEAD, any 1xx, 204 No Content and 304 Not
** Modified end at the blank line after the headers, whatever the headers
** say.  Such a response must not be framed as chunked: httpdone() would
** append the "0\r\n\r\n" terminator, which the client reads as the start of
** the next response on a kept-alive connection.
**
** head is non-zero when the request method is HEAD, status the response code.
*/
int httpnobd(int head, int status);

#endif /* HTTPNOBD_H */
