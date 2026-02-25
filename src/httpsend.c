/* HTTPSEND.C
** Send data buffer asis to client socket
** Returns number of bytes sent, or -1 on error
*/
#include "httpd.h"

extern int
httpsend(HTTPC *httpc, const UCHAR *buf, int len)
{
    int rc;
    int pos = 0;

#if 0
    wtof("httpsend(%08X,%08X,%d)", httpc, buf, len);
#endif

    /* send data to socket */
    for(pos=0; pos < len; pos+=rc) {
        if (pos < 0) {
            wtof("httpsend() pos underflow %d", pos);
            break;
        }
#if 0
        wtof("calling send(%d,%08X,%d,0)",
            httpc->socket, &buf[pos], len-pos);
#endif
        rc = send(httpc->socket, &buf[pos], len-pos, 0);
#if 0
        wtof("send() rc=%d", rc);
#endif
        if (rc>0) {
            /* update bytes sent count */
            httpc->sent += rc;
#if 0
            wtof("httpc->sent=%d", httpc->sent);
#endif
        }
        else {
#if 0
            wtof("httpsend() send rc=%d, errno=%d, socket=%d", rc, errno, httpc->socket);
#endif
            /* allow for EWOULDBLOCK */
            if (errno == EWOULDBLOCK) break;

            /* an error occured */
            if (httpc->state < CSTATE_DONE) {
                /* transtion to done state */
#if 0
                wtof("httpsend() changing httpc->state=CSTATE_DONE");
#endif
                httpc->state = CSTATE_DONE;
                goto quit;  /* return with negative rc */
            }
            break;
        }
    }

    rc = pos;

quit:
#if 0
    wtof("httpsend() rc=%d", rc);
#endif
    return rc;
}
