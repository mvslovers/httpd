/* HTTPPCS.C
** Process clients
*/
#include "httpd.h"

/* http_process_clients() */
int
httppcs(void)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         lockrc;
    unsigned    count;
    unsigned    n;
    HTTPC       *httpc;

    lockrc = lock(httpd,0);
    if (lockrc==0) goto locked; /* we have the lock */
    if (lockrc==8) goto locked; /* we already had the lock */
    wtof("%s lock failure", __func__);
    goto quit;

locked:
    count = array_count(&httpd->httpc);
    for(n=0; n < count; n++) {
        httpc = httpd->httpc[n];
        if (!httpc) continue;           /* no client handle? */

        http_process_client(httpc);
    }
    if (lockrc==0) unlock(httpd,0);

quit:
    return 0;
}
