/* HTTPSSI.C
** Pure helpers for Server Side Includes, see httpssi.h.
*/
#include "httpssi.h"

size_t
httpssih(const char *buf, size_t len, size_t max)
{
    (void)buf;
    (void)max;
    return len;
}

size_t
httpssie(char *dst, size_t dstsize, const char **src)
{
    size_t  o = 0;

    while (**src && o + 1 < dstsize) dst[o++] = *(*src)++;
    dst[o] = 0;
    return o;
}
