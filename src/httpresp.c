/* HTTPRESP.C
** HTTP response
** Transitions to next state as needed.
*/
#include "httpd.h"
#include "httpdmsg.h"

extern int
httpresp(HTTPC *httpc, int resp)
{
    int     	rc  = 0;
    UCHAR   	*p;
    const char	*status;
    UCHAR    	date[40];
    time64_t	now;

    http_enter("httpresp(%d)\n", resp);

    /* The table is in httpstat.c -- free of httpd.h, so it unit-tests dual. */
    status = httpstat(resp);
    if (!status) {
        /* A code we have no phrase for.  Answering it with 500 is what this
        ** has always done; the WTO is what keeps the next gap from hiding.
        ** Silently, "server does not know this code" is indistinguishable on
        ** the wire from "server failed" -- that is how 505, which httpin.c
        ** emits, went out as a 500 unnoticed (issue #181). */
        wtof(MSG_BAD_STATUS, resp);
        status = httpstat(500);
    }

    httpc->resp = resp;

    /* match response version to client request version */
    {
        UCHAR *ver = http_get_env(httpc, "REQUEST_VERSION");
        if (ver && http_cmp(ver, "HTTP/1.1") == 0) {
            rc = http_printf(httpc, "HTTP/1.1 %s\r\n", status);
        } else {
            rc = http_printf(httpc, "HTTP/1.0 %s\r\n", status);
        }
    }
    if (rc) goto quit;

	now = time64(NULL);
    http_date_rfc1123( now, date, sizeof(date) );
    rc = http_printf(httpc, "Date: %s\r\n", date );
    if (rc) goto quit;

    p = http_server_name(httpc->httpd);
    if (p) {
        rc = http_printf(httpc, "Server: %s\r\n", p );
        if (rc) goto quit;
    }
    /* No address-space identity here: Jobname/Jobid/Node headers named the
    ** STC to unauthenticated callers and nothing consumed them (#209). */

    if (httpc->keepalive) {
        rc = http_printf(httpc, "Connection: keep-alive\r\n");
    } else {
        rc = http_printf(httpc, "Connection: close\r\n");
    }
    if (rc) goto quit;

quit:
    http_exit("httpresp(), rc=%d\n", rc);
    return rc;
}
