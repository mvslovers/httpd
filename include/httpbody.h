/* HTTPBODY.H
** Classify a request message body length (RFC 7230 §3.3.3).
**
** Free of httpd.h so the pure classification logic unit-tests on the host as
** well as on MVS (see test/tstbody.c).
*/
#ifndef HTTPBODY_H
#define HTTPBODY_H

/* classification of an incoming request body */
enum {
    HTTP_BODY_NONE = 0,     /* no body to read (no CL & no TE, CL 0, or a
                               Transfer-Encoding body we do not decode here)   */
    HTTP_BODY_READ,         /* read *len bytes                                 */
    HTTP_BODY_TOO_LARGE,    /* Content-Length exceeds max -> 413               */
    HTTP_BODY_BAD           /* malformed Content-Length -> 400                 */
};

/* Classify the request body from the Content-Length header value (cl, NULL or
** empty when absent), whether a Transfer-Encoding header is present, and the
** maximum number of bytes the server will buffer (max).  On HTTP_BODY_READ
** *len is set to the byte count (1..max); it is 0 otherwise.
**
** RFC 7230 §3.3.3: a Transfer-Encoding means the length is framed elsewhere
** (we do not decode it -> NONE); with neither header a request body length is
** 0 (-> NONE); a Content-Length must be a non-negative decimal integer.
*/
int httpbody(const unsigned char *cl, int te_present, long max, long *len);

#endif /* HTTPBODY_H */
