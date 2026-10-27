/* HTTPSSI.C
** Pure helpers for Server Side Includes, see httpssi.h.
*/
#include <string.h>
#include "httpssi.h"

/* httpesc() lives in httpesc.c (HTTPESC) */
extern unsigned char *httpesc(unsigned char *, size_t, const unsigned char *);

size_t
httpssih(const char *buf, size_t len, size_t max)
{
    static const char   start[] = "<!--#";
    static const char   stop[]  = " -->";
    size_t  pos  = 0;
    size_t  i;
    size_t  k;
    size_t  hold = len;

    /* walk the directives the way ssi_file_read() finds them: a start, then
       the first " -->" after it */
    while (pos + 5 <= len) {
        for (i = pos; i + 5 <= len; i++) {
            if (memcmp(&buf[i], start, 5) == 0) break;
        }
        if (i + 5 > len) break;

        for (k = i + 5; k + 4 <= len; k++) {
            if (memcmp(&buf[k], stop, 4) == 0) break;
        }
        if (k + 4 > len) {
            /* no end in this piece */
            hold = i;
            goto limit;
        }
        pos = k + 4;
    }

    /* the piece may end in the first part of a start */
    for (k = 4; k > 0; k--) {
        if (len - pos >= k && memcmp(&buf[len - k], start, k) == 0) {
            hold = len - k;
            break;
        }
    }

limit:
    if (len - hold > max) hold = len;
    return hold;
}

size_t
httpssie(char *dst, size_t dstsize, const char **src)
{
    unsigned char   one[2];
    unsigned char   ent[8];
    size_t          o = 0;
    size_t          n;

    one[1] = 0;
    while (**src) {
        one[0] = (unsigned char)**src;
        httpesc(ent, sizeof(ent), one);
        n = strlen((char *)ent);
        if (o + n + 1 > dstsize) break;
        memcpy(&dst[o], ent, n);
        o += n;
        (*src)++;
    }
    dst[o] = 0;
    return o;
}
