/* HTTPUPCS.C
**
** Upper case a string into a caller-supplied buffer.
**
** The console house style is upper case; only values keep their original
** case (see include/httpdmsg.h, rule 3).  Every literal that reaches a WTO
** is already written upper case in the catalog -- this exists for the few
** strings that arrive lower case at RUNTIME and therefore cannot be spelled
** correctly in the catalog:
**
**   MBT_VERSION       "4.0.0-dev"        -> "4.0.0-DEV"
**   MBT_COMMIT        "a69a370-dirty"    -> "A69A370-DIRTY"
**   libc370_version() "libc370 1.0.2..." -> "LIBC370 1.0.2..."
**
** Deliberately NOT exported through the HTTPX vector: no CGI needs it, and
** adding it there would mean touching httpd.h, httpcgi.h and httpx.c under
** the append-only rule for nothing.
**
** toupper() is the EBCDIC-correct fold here -- the compiler's locale tables
** are CP037, so no explicit range check is needed or wanted.
*/
#include "httpd.h"

const char *
http_upcase(char *dst, unsigned n, const char *src)
{
    unsigned i;

    if (!dst || n == 0) return dst;
    if (!src) { dst[0] = '\0'; return dst; }

    for (i = 0; i + 1U < n && src[i]; i++)
        dst[i] = (char)toupper((unsigned char)src[i]);
    dst[i] = '\0';

    return dst;
}
