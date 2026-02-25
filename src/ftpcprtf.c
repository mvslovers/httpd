/* FTPCPRTF.C */
#include "httpd.h"

/* Send formatted string to data connection */
int
ftpcprtf(FTPC *ftpc, char *fmt, ...)
{
    int         sent = 0;
    int         rc;
    int         len;
    int         i;
    va_list     args;
    char        buf[4096];

    /* print text and args to client data connection */
    va_start(args, fmt);
    len = vsprintf(buf, fmt, args);
    va_end(args);

    /* wtodumpf(buf, len, "%s ebcdic", __func__); */
    for(i=0; i < len; i++) {
        if (buf[i]=='\n') {
            buf[i] = 0x0A;
            continue;
        }
        buf[i] = ebc2asc[buf[i]];
    }
    /* wtodumpf(buf, len, "%s ascii", __func__); */

    /* the data socket is non-blocking, so we do our send in a loop */
    for(i=0; i < len; i += rc) {
        rc = send(ftpc->data_socket, &buf[i], len-i, 0);
        if (rc < 0) {
            if (errno==EWOULDBLOCK) {
                /* send would have blocked, so we continue our attempts */
                rc = 0;
                continue;
            }
            wtof("%s send() rc=%d, errno=%d", __func__, rc, errno);
            return rc;
        }
        sent += rc;
    }

    return sent;
}
