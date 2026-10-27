/* HTTPSHFT.C
** Drop the part of a send buffer that has gone out (issue #440).
**
** Kept free of httpd.h so the arithmetic unit-tests dual (host + MVS).
*/
#include <string.h>
#include "httpshft.h"

unsigned
httpshft(unsigned char *buf, unsigned size, unsigned pos, unsigned len)
{
    unsigned    left;

    if (len > size) len = size;

    if (pos >= len) {
        memset(buf, 0, size);
        return 0;
    }

    left = len - pos;
    memmove(buf, &buf[pos], left);
    memset(&buf[left], 0, len - left);
    return left;
}
