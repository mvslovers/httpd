/* HTTPATOE.C
** Convert ASCII to EBCDIC
*/
#include "httpd.h"

extern UCHAR *
httpatoe(UCHAR *buf, int len)
{
    int i;
    
    if (!len) len = strlen(buf);
    if (len<=0) goto quit;
    
    for(i=0; i < len; i++) {
        buf[i] = asc2ebc[buf[i]];
    }

quit:
    return buf;
}