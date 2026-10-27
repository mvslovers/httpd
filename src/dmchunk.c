/* DMCHUNK.C
** Pure helpers of the /.dm memory display (httpdm.c).
**
** Kept free of httpd.h (char literals only) so it unit-tests on the host.
*/
#include "dmchunk.h"

int
dmchunk(unsigned long value)
{
    if (value == 0) return DM_CHUNK_DEFAULT;

    /* clamp before rounding: rounding first masked the sum to eight bits, so
       253..255 (and every 256 and up) came out as 0 or a few bytes instead of
       the maximum */
    if (value > DM_CHUNK_MAX) return DM_CHUNK_MAX;

    return (int)((value + 3) & ~3UL);
}

int
dmesc(unsigned char c, char *out)
{
    const char  *rep;
    int         n = 0;

    switch (c) {
    case '<':   rep = "&lt;";   break;
    case '>':   rep = "&gt;";   break;
    case '&':   rep = "&amp;";  break;
    default:
        out[0] = (char)c;
        out[1] = 0;
        return 1;
    }

    while (rep[n]) {
        out[n] = rep[n];
        n++;
    }
    out[n] = 0;
    return n;
}
