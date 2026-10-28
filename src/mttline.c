/* MTTLINE.C
** Pure helper of the /.dmtt Master Trace Table display (httpdmtt.c).
**
** Kept free of httpd.h (char literals only) so it unit-tests on the host.
** The replacement table is httpesc()'s, called one byte at a time so that a
** replacement either fits the destination whole or is left for the next call.
*/
#include <stddef.h>
#include <string.h>
#include "mttline.h"

extern unsigned char *httpesc(unsigned char *, size_t, const unsigned char *);

#define MTT_REP_MAX 7   /* longest replacement ("&quot;") plus the NUL */

size_t
mttline(unsigned char *dst, size_t dstsize,
        const unsigned char *src, size_t srclen, int html)
{
    unsigned char   one[2];
    unsigned char   rep[MTT_REP_MAX];
    size_t          n = 0;
    size_t          o = 0;
    size_t          l;

    if (!dst || dstsize == 0) return 0;

    one[1] = 0;
    while (n < srclen && src[n]) {
        if (html) {
            one[0] = src[n];
            httpesc(rep, sizeof(rep), one);
            l = strlen((const char *)rep);
        }
        else {
            rep[0] = src[n];
            l = 1;
        }

        if (o + l >= dstsize) break;

        memcpy(dst + o, rep, l);
        o += l;
        n++;
    }

    dst[o] = 0;
    return n;
}
