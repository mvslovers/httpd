/* HTTPCLOS.C
** Close client handle
*/
#include "httpd.h"

extern int
httpclos(HTTPC *httpc)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    unsigned    n;
    unsigned    count;

    lock(httpd,0);
    count = array_count(&httpd->httpc);
    for(n=0; n < count; n++) {
        if (httpd->httpc[n]==httpc) {
            array_del(&httpd->httpc, n+1);
            break;
        }
    }
    unlock(httpd,0);

    if (httpc) {
        if (httpd->active_connections > 0)
            httpd->active_connections--;

        /* make sure we closed the file */
        if (httpc->fp) http_done(httpc);
        if (httpc->ufp) ufs_fclose(&httpc->ufp);
        if (httpc->ufs) ufsfree(&httpc->ufs);

        /* make sure we reset the handle */
        if (httpc->env) http_reset(httpc);

        /* close the socket */
        if (httpc->socket > 0) {
            closesocket(httpc->socket);
            httpc->socket = 0;
        }

        /* release the client handle */
        free(httpc);
    }

    return rc;
}
