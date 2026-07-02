/* HTTPESC.C
** HTML-escape a string into a bounded destination buffer.
**
** Escapes the five characters significant in HTML text/attribute context so
** that untrusted input (e.g. a client-supplied cookie reflected into a form)
** cannot inject markup.  Always NUL-terminates and never writes past the
** destination; a value too long to fit is truncated safely.
**
** Kept free of httpd.h (only <stddef.h> + char literals) so the pure logic
** unit-tests on the host as well as on MVS.  Character literals are encoded
** for the target, so it is correct under both EBCDIC (cc370) and ASCII.
*/
#include <stddef.h>

typedef unsigned char UCHAR;

/* http_html_escape() */
UCHAR *
httpesc(UCHAR *dst, size_t dstsize, const UCHAR *src)
{
    size_t      o = 0;
    const char  *rep;
    size_t      rlen;
    size_t      k;

    if (!dst || dstsize == 0) return dst;
    if (!src) src = (const UCHAR *)"";

    while (*src) {
        switch (*src) {
        case '&':  rep = "&amp;";  rlen = 5; break;
        case '<':  rep = "&lt;";   rlen = 4; break;
        case '>':  rep = "&gt;";   rlen = 4; break;
        case '"':  rep = "&quot;"; rlen = 6; break;
        case '\'': rep = "&#39;";  rlen = 5; break;
        default:   rep = NULL;     rlen = 1; break;
        }

        /* need room for the replacement/byte plus the trailing NUL */
        if (o + rlen >= dstsize) break;

        if (rep) {
            for (k = 0; k < rlen; k++) dst[o+k] = (UCHAR)rep[k];
            o += rlen;
        }
        else {
            dst[o++] = *src;
        }
        src++;
    }

    dst[o] = 0;
    return dst;
}
