/* HTTPPRTV.C
** HTTP printv
*/
#include "httpd.h"

/* libc370 ships usleep() (src/clib/usleep.c) but its headers carry
   no prototype for it */
extern int usleep(unsigned usec);

/* A zero-progress send (0 from http_send(), socket send buffer full)
** is retried after a pause instead of busy-spinning the worker at
** 100% CPU (issue #199); after SEND_STALL_MAX consecutive
** zero-progress calls the client is treated as gone.  The policy
** itself lives in httpd.h -- httpsend.c applies the same budget.
*/

/* Send the whole buffer, tolerating short writes.  Once the client
** is marked done no further progress can come at all - fail
** immediately instead of pausing.
*/
static int
send_all(HTTPC *httpc, const UCHAR *buf, int len)
{
    int rc;
    int pos;
    int stall = 0;

    for (pos = 0; pos < len; pos += rc) {
        rc = http_send(httpc, &buf[pos], len - pos);
        if (rc < 0) return -1;
        if (rc == 0) {
            if (httpc->state >= CSTATE_DONE) return -1;
            /* a stopping server must not sit out the stall budget:
               shutdown waits for the workers, so every worker wait
               has to honor quiesce (#122, #205) */
            if (httpc->httpd->flag
                & (HTTPD_FLAG_QUIESCE | HTTPD_FLAG_SHUTDOWN)) {
                httpc->state = CSTATE_DONE;
                return -1;
            }
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
httpprtv(HTTPC *httpc, const char *fmt, va_list args)
{
    int     rc      = 0;
    int     len;
    UCHAR   buf[5120];

    /* A client at or past CSTATE_DONE is finished: either the response
       is complete, or a failed send marked it dead (#199).  Late output
       would corrupt a kept-alive stream in the first case and is doomed
       in the second -- refuse it before paying for vsnprintf/etoa/send,
       because the display modules emit hundreds of calls per request
       without checking any of them (#203). */
    if (httpc->state >= CSTATE_DONE)
        return -1;

    len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len >= (int)sizeof(buf))
        len = sizeof(buf) - 1;
    if (len < 0) {
        rc = -1;
    }
    else {
        /* auto-detect Content-Length header from any caller */
        if (len >= 15 && http_cmpn(buf, "Content-Length:", 15) == 0)
            httpc->content_length_set = 1;

        /* auto-detect header terminator (blank line = "\r\n" only)
           and inject Transfer-Encoding: chunked for HTTP/1.1 if needed */
        if (len == 2 && buf[0] == '\r' && buf[1] == '\n'
            && httpc->resp > 0 && !httpc->rdw) {
            /* Responses that never carry a message body (RFC 7230 3.3.1):
               1xx, 204 No Content and 304 Not Modified. These must not be
               chunked: injecting Transfer-Encoding: chunked makes httpdone()
               append a "0\r\n\r\n" terminator, which is an illegal body and
               is rejected by strict HTTP parsers (e.g. llhttp / Node.js).
               NOTE: a response to HEAD is body-less too — add it here if HEAD
               ever returns a real status (it currently returns 501). */
            int bodyless = httpc->resp == 204 || httpc->resp == 304
                         || (httpc->resp >= 100 && httpc->resp < 200);

            httpc->rdw = 1; /* headers ended — don't re-trigger */

            if (!bodyless && !httpc->content_length_set && !httpc->chunked) {
                UCHAR *ver = http_get_env(httpc, "REQUEST_VERSION");
                if (ver && http_cmp(ver, "HTTP/1.1") == 0) {
                    /* inject Transfer-Encoding header before blank line */
                    /* An ARRAY, deliberately -- `UCHAR *te = "..."` would
                    ** make http_etoa() translate the string literal in place,
                    ** i.e. store into module storage.  That is unwritable when
                    ** the module is fetched from an APF-authorized or LNKLST
                    ** library (#197), and it would corrupt the literal for
                    ** every later request besides. */
                    UCHAR te[] = "Transfer-Encoding: chunked\r\n";
                    int te_len = sizeof(te) - 1;
                    http_etoa(te, te_len);
                    rc = send_all(httpc, te, te_len);
                    if (rc < 0) goto quit;
                    /* send the blank line (header terminator) */
                    http_etoa(buf, len);
                    rc = send_all(httpc, buf, len);
                    if (rc < 0) goto quit;
                    /* enable chunk framing for subsequent body data */
                    httpc->chunked = 1;
                    rc = 0;
                    goto quit;
                }
            }
        }

        /* normal path: convert EBCDIC → ASCII and send */
        http_etoa(buf, len);
        rc = send_all(httpc, buf, len);
    }

quit:
    return rc;
}
