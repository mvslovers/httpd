/* HTTPSEND.C
** Send data buffer asis to client socket
** Returns number of bytes sent, or -1 on error
*/
#include "httpd.h"
#include "httpdmsg.h"

/* libc370 ships usleep() (src/clib/usleep.c) but its headers carry
   no prototype for it */
extern int usleep(unsigned usec);

/* send_raw() - send raw bytes to socket without chunk framing */
static int
send_raw(HTTPC *httpc, const UCHAR *buf, int len)
{
    int rc;
    int pos = 0;

    for(pos=0; pos < len; pos+=rc) {
        if (pos < 0) {
            wtof(MSG_SEND_UNDERFLOW, pos);
            break;
        }
        rc = send(httpc->socket, &buf[pos], len-pos, 0);
        if (rc>0) {
            httpc->sent += rc;
        }
        else {
            /* allow for EWOULDBLOCK */
            if (errno == EWOULDBLOCK) break;

            /* an error occured - the socket is dead.  Report failure,
               never pos: a 0 return means "no progress, retry later"
               to callers, and a dead socket must never look retryable
               or http_printv() spins on it forever (issue #199).  0
               stays reserved for EWOULDBLOCK. */
            if (httpc->state < CSTATE_DONE)
                httpc->state = CSTATE_DONE;
            return -1;
        }
    }

    return pos;
}

/* send_raw_all() - send the whole buffer or fail.
**
** A chunk is only parseable if the byte count its header announced is
** followed by exactly that many bytes, so the chunked path cannot
** tolerate the partial send that send_raw() reports on EWOULDBLOCK
** (issue #201).  A closed receive window is waited out; a dead socket
** and an exhausted budget both fail.
**
** Returns 0 when everything was sent, -1 otherwise.  Every -1 leaves
** the client at CSTATE_DONE -- http_send_file() tests only for a
** positive return, so that state is what stops it.
*/
static int
send_raw_all(HTTPC *httpc, const UCHAR *buf, int len)
{
    int rc;
    int pos;
    int stall = 0;

    for (pos = 0; pos < len; pos += rc) {
        rc = send_raw(httpc, &buf[pos], len - pos);
        if (rc < 0) {
            /* set here rather than relying on the caller: the
               MSG_SEND_UNDERFLOW guard also returns negative, and the
               invariant below must hold for every path */
            if (httpc->state < CSTATE_DONE) httpc->state = CSTATE_DONE;
            return -1;
        }
        if (rc == 0) {
            /* no progress: EWOULDBLOCK, the only case send_raw()
               still reports as 0 */
            if (httpc->state >= CSTATE_DONE) return -1;
            if (++stall > SEND_STALL_MAX) {
                httpc->state = CSTATE_DONE;
                return -1;
            }
            usleep(SEND_STALL_PAUSE);
        }
        else {
            stall = 0;
        }
    }

    return 0;
}

extern int
httpsend(HTTPC *httpc, const UCHAR *buf, int len)
{
    int rc;

    if (httpc->chunked) {
        /* RFC 7230 chunked transfer encoding */
        UCHAR hdr[16];
        UCHAR crlf[2];
        int hdrlen;

        if (len <= 0) return 0;

        /* chunk header: hex size + CRLF (convert EBCDIC to ASCII) */
        hdrlen = sprintf((char *)hdr, "%x\r\n", len);
        http_etoa(hdr, hdrlen);

        /* chunk trailer: CRLF in ASCII (0x0D 0x0A) */
        crlf[0] = 0x0D;
        crlf[1] = 0x0A;

        /* Header, data (already in ASCII from the caller) and trailer
           each go out completely or not at all.  Once any of them is
           short the stream is unparseable for good, so drop framing --
           otherwise httpdone() would append a "0\r\n\r\n" telling the
           client a truncated response had ended normally -- and close
           the connection rather than carry a torn chunk into the next
           request on it. */
        if (send_raw_all(httpc, hdr, hdrlen) < 0
         || send_raw_all(httpc, buf, len) < 0
         || send_raw_all(httpc, crlf, 2) < 0) {
            httpc->chunked = 0;
            httpc->keepalive = 0;
            return -1;
        }

        return len;
    }

    /* non-chunked: send raw */
    rc = send_raw(httpc, buf, len);

    return rc;
}
