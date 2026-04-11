/* HTTPRESE.C
** Reset client handle
** Transitions to next client state.
*/
#include "httpd.h"

extern int
httprese(HTTPC *httpc)
{
    int         rc = 0;
    unsigned    n;
    unsigned    count;

    http_enter("httprese()\n");

    if (httpc->env) {
        /* free variables */
        count = array_count(&httpc->env);
        for(n=0;n<count;n++) {
            if (httpc->env[n]) {
                free(httpc->env[n]);
                httpc->env[n] = NULL;
            }
        }
        array_free(&httpc->env);
    }

	httpc->cred = NULL;
    httpc->len = 0;
    httpc->pos = 0;
    httpc->sent = 0;
    httpc->subtype = 0;
    httpc->substate = 0;
    httpc->chunked = 0;
    httpc->content_length_set = 0;
    httpc->rdw = 0;
    memset(httpc->buf, 0, CBUFSIZE);

    /* clear ACEE on UFS session between requests */
    if (httpc->ufs) {
        ufs_set_acee(httpc->ufs, NULL);
    }

    if (httpc->keepalive) {
        /* keep connection open for next request */
        httpc->request_count++;
        httpc->keepalive = 0;
        httpc->start = 0.0;
        httpc->end = 0.0;
        httpsecs(&httpc->start);
        httpc->state = CSTATE_IN;
    } else {
        httpc->start = 0.0;
        httpc->end = 0.0;
        httpc->state = CSTATE_CLOSE;
    }

    http_exit("httprese(), rc=%d\n", rc);
    return rc;
}
