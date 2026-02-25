/* HTTPETOA.C
** Convert EBCDIC to ASCII
*/
#include "httpd.h"

extern UCHAR *
httpetoa(UCHAR *buf, int len)
{
    int i;

    if (!len) len = strlen(buf);
    if (len<=0) goto quit;

    for(i=0; i < len; i++) {
        /* we translate special control characters ourselves */
        switch(buf[i]) {
        case '\r':          /* EBCDIC CR */
            buf[i]=0x0D;    /* ASCII CR */
            break;
        case '\n':          /* EBCDIC NL */
            buf[i]=0x0A;    /* ASCII LF */
            break;
        case 0xAD:          /* EBCDIC Left Square Bracket */
            buf[i]=0x5B;    /* ASCII Left Square Bracket */
            break;
        case 0xBD:          /* EBCDIC Right Square Bracket */
            buf[i]=0x5D;    /* ASCII Right Square Bracket */
            break;
        default:
            buf[i] = ebc2asc[buf[i]];
        }
    }

quit:
    return buf;
}
