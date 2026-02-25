/* HTTPPRTV.C
** HTTP printv
*/
#include "httpd.h"

extern int
httpprtv(HTTPC *httpc, const char *fmt, va_list args)
{
    int     rc      = 0;
    int     pos;
    int     len;
    UCHAR   buf[5120];

    len = vsprintf(buf, fmt, args);
    if (len < 0) {
        rc = -1;
    }
    else {
#if 0   /* debugging */
        wtof("httpprtv:");
        for(pos=0; pos < len; pos+=80) {
            int j = len - pos;
            if (j > 80) j = 80;
            wtof("%-*.*s", j, j, &buf[pos]);
        }
#endif
        http_etoa(buf, len);
        for(pos=0; pos < len; pos+=rc) {
            rc = http_send(httpc, &buf[pos], len-pos);
#if 0
            wtof("http_send() rc=%d", rc);
#endif
            if (rc<0) goto quit;
        }
        rc = 0;
    }

quit:
#if 0
    wtof("httpprtv() rc=%d", rc);
#endif
    return rc;
}
