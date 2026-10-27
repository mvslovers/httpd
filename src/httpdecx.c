/* HTTPDECX.C
** Percent-decoding of a URI component, with the code page passed in.
**
** Kept free of httpd.h (char literals only) so the decision table unit-tests
** on the host.  '0'..'9', 'a'..'f' and 'A'..'F' are each contiguous in both
** EBCDIC and ASCII, so the hex test is correct under either encoding.
*/
#include "httpdecx.h"

static int
hexval(unsigned char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* The byte for the escape at s ("%xx"), or -1 if s is not one we decode.
   s[2] is read only when s[1] is a hex digit, so never past the NUL. */
static int
escbyte(const unsigned char *s, const unsigned char *atoe)
{
    int hi = hexval(s[1]);
    int lo = hi < 0 ? -1 : hexval(s[2]);
    int v;

    if (lo < 0) return -1;

    v = (hi << 4) | lo;
    if (v == 0 || atoe[v] == 0) return -1;

    return atoe[v];
}

unsigned char *
httpdecx(unsigned char *str, const unsigned char *atoe, int plus)
{
    unsigned char   *result = str;
    unsigned char   *out    = str;

    while (*str) {
        int c = (*str == '%') ? escbyte(str, atoe) : -1;

        if (c >= 0) {
            *out++ = (unsigned char)c;
            str += 3;
        }
        else if (plus && *str == '+') {
            *out++ = ' ';
            str++;
        }
        else {
            *out++ = *str++;        /* not an escape we decode: keep it */
        }
    }

    *out = 0;
    return result;
}
