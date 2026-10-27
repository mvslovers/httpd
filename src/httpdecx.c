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

unsigned char *
httpdecx(unsigned char *str, const unsigned char *atoe, int plus)
{
    unsigned char   *result = str;
    unsigned char   *out    = str;

    while (*str) {
        if (*str == '%') {
            /* str[2] is read only when str[1] is a hex digit, so it is never
               past the terminating NUL */
            int hi = hexval(str[1]);
            int lo = hi < 0 ? -1 : hexval(str[2]);

            if (hi >= 0 && lo >= 0) {
                int v = (hi << 4) | lo;

                if (v != 0 && atoe[v] != 0) {
                    *out++ = atoe[v];
                    str += 3;
                    continue;
                }
            }
            *out++ = *str++;        /* not an escape we decode: keep the '%' */
            continue;
        }

        if (plus && *str == '+') {
            *out++ = ' ';
            str++;
            continue;
        }

        *out++ = *str++;
    }

    *out = 0;
    return result;
}
