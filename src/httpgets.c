#include "httpd.h"

int http_gets(HTTPC *httpc, UCHAR *buf, unsigned max)
{
    HTTPD       *httpd  = httpc->httpd;
    int         rc      = 0;
    int         c;
    int         i;
    time64_t    now;
    time64_t    timeout;
    unsigned    seconds;
    unsigned    ecb;

    if (httpc->request_count > 0 && httpd->cfg_keepalive_timeout) {
        seconds = httpd->cfg_keepalive_timeout;
    } else {
        seconds = httpd->cfg_client_timeout;
        if (seconds == 0) seconds = 10;
    }
    time64(&now);
    __64_add_u32(&now, seconds, &timeout);
        
    if (!buf) {
        /* use the client buffer */
        buf = httpc->buf;
        max = CBUFSIZE;
        memset(buf, 0, max);
    }

    for(i=0; i < max; ) {
        /* get one character from client socket */
        c = http_getc(httpc);
        if (c < 0) {
            if (errno == EWOULDBLOCK) {
                /* check for timeout */
                time64(&now);
                if (__64_cmp(&now, &timeout) == __64_SMALLER) {
                    /* no timeout, sleep for 0.50 seconds */
                    errno = ecb = 0;
                    cthread_timed_wait(&ecb, 50, 0);
                    continue;
                }
                else {
                    /* timeout has occured */
                    char *fmt = "client %08X on socket %d timed out after %u seconds\n";

                    if (i > 0) {
                        if (httpd->client & HTTPD_CLIENT_INMSG) {
                            wtof(fmt, httpc, httpc->socket, seconds);
                        }
                        if (httpd->client & HTTPD_CLIENT_INDUMP) {
                            wtodumpf(httpc->buf, i, "Receive Buffer");
                        }
        
                        http_dbgf(fmt, httpc, httpc->socket, seconds);
                    }
                    httpc->state = CSTATE_CLOSE;
                    errno = ETIMEDOUT;
                }   /* check for timeout */
            }   /* errno == EWOULDBLOCK */
            rc = c;
            goto quit;
        }

        /* check for ASCII CR and discard */
        if (c==0x0D) continue;

        /* check for ASCII LF */
        if (c != 0x0A) {
            /* not ASCII LF, translate ASCII to EBCDIC and save character */
            buf[i++] = (UCHAR)asc2ebc[c];
            continue;
        }

        /* ASCII LF */
        buf[i++] = '\n';    /* EBCDIC newline   */
        buf[i]   = 0;       /* end of string    */
        rc       = i;       /* length of string */
        break;
    }
    
quit:
    return rc;
}
