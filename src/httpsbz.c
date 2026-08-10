/* HTTPSBZ.C
** Add client to busy array
*/
#include "httpd.h"

/* http_set_busy() */
int
httpsbz(HTTPC *httpc)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    int         lockrc;

    if (!httpd) goto quit;
    if (!httpc) goto quit;
    if (httpd != httpc->httpd) goto quit;
    
    if (strcmp(httpd->eye, HTTPD_EYE)!=0) goto quit;

    /* make sure we don't already have this client in our array */
    rc = http_is_busy(httpc);
    if (rc) {
        http_dbgf("Client is already in server busy array\n");
        goto quit;
    }

    lockrc = lock(&httpd->busy, LOCK_EXC);

    /* add this client to our busy array.  Pinned to subpool 0 (issue #154):
       http_set_busy() is on the httpx vector, so a module can reach it, and
       the array's backing store belongs to the server -- it outlives every
       request and is trimmed by http_reset_busy() on whichever worker gets
       there.  Only the growth allocation is at stake, but growth is exactly
       what happens under load. */
    {
        UCHAR sp = __setsp(0);

        rc = array_add(&httpd->busy, httpc);

        __setsp(sp);
    }
    if (rc) {
        http_dbgf("Error adding client to busy array\n");
    }

    if (lockrc==0) unlock(&httpd->busy, LOCK_EXC);

quit:
    return rc;
}
