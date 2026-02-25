/* HTTPRBZ.C
** Remove client from busy array
*/
#include "httpd.h"

/* http_reset_busy() */
int
httprbz(HTTPC *httpc)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         lockrc  = 8;
    unsigned    count   = array_count(&httpd->busy);
    unsigned    n;

    /* validate handles */
    if (!httpd) goto quit;  /* no server handle */
    if (!httpc) goto quit;  /* no client handle */
    if (httpd != httpc->httpd) goto quit; /* something is wrong */

    /* check for bad HTTPD handle */
    if (strcmp(httpd->eye, HTTPD_EYE)!=0) goto quit;

    lockrc = lock(&httpd->busy, LOCK_EXC);
    
    count  = array_count(&httpd->busy);
    if (!count) goto quit;  /* nothing is in busy array */

    for(n=0; n < count; n++) {
        if (httpd->busy[n] == httpc) {
            array_del(&httpd->busy,n+1);
            break;
        }
    }

quit:
    if (lockrc==0) unlock(&httpd->busy, LOCK_EXC);
    return 0;
}
