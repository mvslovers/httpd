/* HTTPSSI.C
** Pure helpers for Server Side Includes, see httpssi.h.
*/
#include <string.h>
#include "httpssi.h"

/* httpesc() lives in httpesc.c (HTTPESC) */
extern unsigned char *httpesc(unsigned char *, size_t, const unsigned char *);

#define SSI_NONE ((size_t)-1)

/* offset of the first occurrence of pat[0..n) in buf[from..len), or SSI_NONE */
static size_t
ssifind(const char *buf, size_t len, size_t from, const char *pat, size_t n)
{
    size_t  i = from;

    while (i + n <= len && memcmp(&buf[i], pat, n) != 0) i++;
    return (i + n <= len) ? i : SSI_NONE;
}

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
    i = ssifind(buf, len, pos, start, 5);
    while (i != SSI_NONE) {
        k = ssifind(buf, len, i + 5, stop, 4);
        if (k == SSI_NONE) {
            /* no end in this piece */
            hold = i;
            i = SSI_NONE;
        } else {
            pos = k + 4;
            i = ssifind(buf, len, pos, start, 5);
        }
    }

    /* the piece may end in the first part of a start */
    if (hold == len) {
        for (k = 4; k > 0 && hold == len; k--) {
            if (len - pos >= k && memcmp(&buf[len - k], start, k) == 0)
                hold = len - k;
        }
    }

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
