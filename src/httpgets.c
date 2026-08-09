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

    int saw_cr = 0;
#if HTTPD_DEBUG_217
    /* Separates the two hang shapes (#159).  Each poll pass is ~0.5 s, so a
       client that has not timed out after seconds*4 passes means the wait is
       returning but the deadline test never fires.  A worker that hangs with
       no such message never came back out of cthread_timed_wait() at all. */
    unsigned polls = 0;
    unsigned limit = (seconds + 1) * 4;
#endif

    for(i=0; i < max; ) {
        /* get one character from client socket */
        c = http_getc(httpc);
        if (c < 0) {
            if (errno == EWOULDBLOCK) {
                /* check for timeout */
                time64(&now);
                if (__64_cmp(&now, &timeout) == __64_SMALLER) {
#if HTTPD_DEBUG_217
                    if (++polls == limit) {
                        wtof("HTTPD902D poll overrun client(%08X) socket(%d) "
                             "polls(%u) limit(%us) -- deadline not firing",
                            httpc, httpc->socket, polls, seconds);
                    }
#endif
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

        /* check for ASCII CR */
        if (c == 0x0D) {
            saw_cr = 1;
            continue;
        }

        /* check for ASCII LF */
        if (c == 0x0A) {
            /* LF terminates line (CRLF or bare LF both accepted) */
            buf[i++] = '\n';    /* EBCDIC newline   */
            buf[i]   = 0;       /* end of string    */
            rc       = i;       /* length of string */
            break;
        }

        /* data character after CR without LF = bare CR (RFC 7230 §3.5) */
        if (saw_cr) {
            errno = EINVAL;
            rc = -1;
            goto quit;
        }

        /* translate ASCII to EBCDIC and save character */
        buf[i++] = (UCHAR)asc2ebc[c];
    }
    
quit:
    return rc;
}
