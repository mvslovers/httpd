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
    httpc->start = 0.0;
    httpc->end = 0.0;
    memset(httpc->buf, 0, CBUFSIZE);

    /* clear ACEE on UFS session between requests */
    if (httpc->ufs) {
        ufs_set_acee(httpc->ufs, NULL);
    }

    /* if this is was HTTP1.1 or higher client then we
    ** *could* transition to CSTATE_IN.  We'll leave that
    ** for another time.
    */
    httpc->state = CSTATE_CLOSE;

    http_exit("httprese(), rc=%d\n", rc);
    return rc;
}
