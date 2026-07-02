/* HTTPDECO.C
** Decode an escaped buffer
*/
#include "httpd.h"

extern UCHAR *
httpdeco(UCHAR *str)
{
    UCHAR   *result = str;
    UCHAR   *out    = str;
    UCHAR   temp[4];

    while(*str) {
        switch (*str) {
        case '%':
            /* convert %xx ASCII value to EBCDIC character.  Read str[1]/str[2]
               only when both are present -- otherwise a trailing '%' (str[1]
               is the NUL) would read one byte past the string.  An incomplete
               escape at end-of-string is passed through literally. */
            if (str[1] && str[2]) {
                temp[0] = str[1];
                temp[1] = str[2];
                temp[2] = 0;
                out[0] = asc2ebc[strtoul(temp, NULL, 16)];
                str += 2;
            }
            else {
                out[0] = *str;      /* '%' with no following pair */
            }
            break;
        case '+':
            /* convert '+' to ' ' */
            out[0] = ' ';
            break;
        default:
            out[0] = str[0];
            break;
        }
        out++;
        str++;
    }

    out[0] = 0;
    return result;
}
