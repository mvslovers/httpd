/* HTTPISB.C
** See if client handle is in busy array
*/
#include "httpd.h"

/* http_is_busy() */
int
httpisb(HTTPC *httpc)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         isbusy  = 0;
    int         lockrc  = 8;
    unsigned    count;
    unsigned    n;

    /* validate handles */
    if (!httpd) goto quit;  /* no server handle */
    if (!httpc) goto quit;  /* no client handle */
    if (httpd != httpc->httpd) goto quit; /* something is wrong */

    /* check for bad HTTPD handle */
    if (strcmp(httpd->eye, HTTPD_EYE)!=0) goto quit;

    /* lock the busy array */
    lockrc  = lock(&httpd->busy, LOCK_EXC);
    count   = array_count(&httpd->busy);

    if (!count) goto quit;  /* nothing is in busy array */

    for(n=0; n < count; n++) {
        if (httpd->busy[n] == httpc) {
            isbusy++;
            break;
        }
    }

quit:
    if (lockrc==0) unlock(&httpd->busy, LOCK_EXC);
    return isbusy;
}
