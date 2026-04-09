/* HTTPSEND.C
** Send data buffer asis to client socket
** Returns number of bytes sent, or -1 on error
*/
#include "httpd.h"

/* send_raw() - send raw bytes to socket without chunk framing */
static int
send_raw(HTTPC *httpc, const UCHAR *buf, int len)
{
    int rc;
    int pos = 0;

    for(pos=0; pos < len; pos+=rc) {
        if (pos < 0) {
            wtof("httpsend() pos underflow %d", pos);
            break;
        }
        rc = send(httpc->socket, &buf[pos], len-pos, 0);
        if (rc>0) {
            httpc->sent += rc;
        }
        else {
            /* allow for EWOULDBLOCK */
            if (errno == EWOULDBLOCK) break;

            /* an error occured */
            if (httpc->state < CSTATE_DONE) {
                httpc->state = CSTATE_DONE;
                return -1;
            }
            break;
        }
    }

    return pos;
}

extern int
httpsend(HTTPC *httpc, const UCHAR *buf, int len)
{
    int rc;

    if (httpc->chunked) {
        /* RFC 7230 chunked transfer encoding */
        UCHAR hdr[16];
        int hdrlen;

        if (len <= 0) return 0;

        /* chunk header: hex size + CRLF (convert EBCDIC to ASCII) */
        hdrlen = sprintf((char *)hdr, "%x\r\n", len);
        http_etoa(hdr, hdrlen);
        rc = send_raw(httpc, hdr, hdrlen);
        if (rc < 0) return rc;

        /* chunk data (already in ASCII from caller) */
        rc = send_raw(httpc, buf, len);
        if (rc < 0) return rc;

        /* chunk trailer: CRLF in ASCII (0x0D 0x0A) */
        {
            UCHAR crlf[2];
            crlf[0] = 0x0D;
            crlf[1] = 0x0A;
            rc = send_raw(httpc, crlf, 2);
        }
        if (rc < 0) return rc;

        return len;
    }

    /* non-chunked: send raw */
    rc = send_raw(httpc, buf, len);

    return rc;
}
