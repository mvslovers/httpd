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
            httpc->rdw = 1; /* headers ended — don't re-trigger */

            if (!httpc->content_length_set && !httpc->chunked) {
                UCHAR *ver = http_get_env(httpc, "REQUEST_VERSION");
                if (ver && http_cmp(ver, "HTTP/1.1") == 0) {
                    /* inject Transfer-Encoding header before blank line */
                    UCHAR te[] = "Transfer-Encoding: chunked\r\n";
                    int te_len = sizeof(te) - 1;
                    http_etoa(te, te_len);
                    for (pos = 0; pos < te_len; pos += rc) {
                        rc = http_send(httpc, &te[pos], te_len - pos);
                        if (rc < 0) goto quit;
                    }
                    /* send the blank line (header terminator) */
                    http_etoa(buf, len);
                    for (pos = 0; pos < len; pos += rc) {
                        rc = http_send(httpc, &buf[pos], len - pos);
                        if (rc < 0) goto quit;
                    }
                    /* enable chunk framing for subsequent body data */
                    httpc->chunked = 1;
                    rc = 0;
                    goto quit;
                }
            }
        }

        /* normal path: convert EBCDIC → ASCII and send */
        http_etoa(buf, len);
        for(pos=0; pos < len; pos+=rc) {
            rc = http_send(httpc, &buf[pos], len-pos);
            if (rc<0) goto quit;
        }
        rc = 0;
    }

quit:
    return rc;
}
