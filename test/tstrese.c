/* TSTRESE.C
** Tests for http_reset() / httprese() -- the per-request reset between two
** requests on one keep-alive connection (src/httprese.c), issue #307.
**
** httpresp() records the status it sent in httpc->resp, and httppcgi() reads
** that field after a module abend: a zero means no status line went out yet,
** so it sends "503" and its headers first.  The reset did not clear the
** field, so on the second request of a connection it still held the first
** request's status, and an abend there sent the 503 text with no status line
** and no headers -- a bare body the client cannot parse (curl: 000, the
** connection dropped).
**
** The reset must therefore leave no response state of the previous request
** behind, for the keep-alive path and for the close path alike.
**
** httprese() traces through http_enter()/http_exit(), and dbgenter() reads
** the HTTPD block from grtapp1 -- a zeroed block (dbg NULL: no trace) stands
** in for the server's, as in TSTMIME.  HTTPRESE resolves from the [internal]
** autocall archive.
**
** HTTP_PRIVATE selects the real HTTPRESE entry (called directly) instead of
** the httpx-vector macro.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <mbtcheck.h>

/* the state a finished request leaves in the client handle */
static void
served(HTTPC *httpc, int keepalive)
{
    httpc->resp                 = 200;
    httpc->rdw                  = 1;
    httpc->chunked              = 1;
    httpc->content_length_set   = 1;
    httpc->sent                 = 1234;
    httpc->keepalive            = (UCHAR)keepalive;
    httpc->state                = CSTATE_RESET;
}

int main(void)
{
    HTTPD   httpd;
    HTTPC   *httpc;

    memset(&httpd, 0, sizeof(httpd));
    __grtget()->grtapp1 = &httpd;

    httpc = calloc(1, sizeof(HTTPC));
    if (!httpc) {
        printf("TSTRESE: no storage for HTTPC\n");
        return 8;
    }

    printf("=== HTTPD http_reset tests ===\n");

    /* keep-alive: the connection stays, the next request starts clean */
    served(httpc, 1);
    http_reset(httpc);
    printf("  keep-alive: resp=%d state=%d request_count=%u\n",
        httpc->resp, httpc->state, httpc->request_count);
    CHECK_EQ(httpc->resp, 0, "keep-alive reset clears the response status");
    CHECK_EQ(httpc->state, CSTATE_IN, "keep-alive reset reads the next request");
    CHECK_EQ(httpc->rdw, 0, "keep-alive reset clears end-of-headers");
    CHECK_EQ(httpc->chunked, 0, "keep-alive reset clears chunked");
    CHECK_EQ(httpc->content_length_set, 0,
        "keep-alive reset clears Content-Length-sent");
    CHECK_EQ(httpc->sent, 0, "keep-alive reset clears bytes sent");
    CHECK_EQ(httpc->request_count, 1, "keep-alive reset counts the request");

    /* close: no next request, but nothing stale either */
    served(httpc, 0);
    http_reset(httpc);
    CHECK_EQ(httpc->resp, 0, "close reset clears the response status");
    CHECK_EQ(httpc->state, CSTATE_CLOSE, "close reset closes the connection");

    free(httpc);
    return mbt_test_summary("TSTRESE");
}
