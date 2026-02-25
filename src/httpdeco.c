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
            /* convert %xx ASCII value to EBCDIC character */
            temp[0] = str[1];
            temp[1] = str[2];
            temp[2] = 0;
            out[0] = asc2ebc[strtoul(temp, NULL, 16)];
            if (str[1] && str[2]) str+=2;
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
