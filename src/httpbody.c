/* HTTPBODY.C
** Classify a request message body length (RFC 7230 §3.3.3).
**
** Kept free of httpd.h (char literals only) so the RFC decision table
** unit-tests dual (host + MVS).  Digits '0'..'9' are contiguous in both EBCDIC
** and ASCII, so the parse is correct under either encoding.
*/
#include "httpbody.h"

int
httpbody(const unsigned char *cl, int te_present, long max, long *len)
{
    long        v = 0;

    if (len) *len = 0;

    /* Transfer-Encoding present: the body is framed elsewhere (e.g. chunked),
       not by Content-Length.  We do not decode it here -> read nothing. */
    if (te_present) return HTTP_BODY_NONE;

    /* No Content-Length and no Transfer-Encoding -> body length is 0. */
    if (!cl || !*cl) return HTTP_BODY_NONE;

    /* optional leading whitespace (in case the header parser did not trim) */
    while (*cl == ' ' || *cl == '\t') cl++;

    /* require at least one decimal digit */
    if (*cl < '0' || *cl > '9') return HTTP_BODY_BAD;

    for (; *cl >= '0' && *cl <= '9'; cl++) {
        if (v > max) return HTTP_BODY_TOO_LARGE;    /* before *10: no overflow */
        v = v * 10 + (*cl - '0');
    }
    if (v > max) return HTTP_BODY_TOO_LARGE;

    /* optional trailing whitespace, then nothing else (no sign, no junk) */
    while (*cl == ' ' || *cl == '\t') cl++;
    if (*cl) return HTTP_BODY_BAD;

    if (v == 0) return HTTP_BODY_NONE;

    if (len) *len = v;
    return HTTP_BODY_READ;
}
