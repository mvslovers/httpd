/* HTTPRNIM.C
** HTTP response not implemented 501
** Transitions to next state as needed.
*/
#include "httpd.h"

extern int
httprnim(HTTPC *httpc)
{
    int rc;
    
    http_enter("httprnim()\n");

    /* Send not implemented response */
    rc = http_resp(httpc, 501);
    if (rc) goto quit;

    rc = http_printf(httpc, "Cache-Control: no-store\r\n");
    if (rc) goto quit;
        
    rc = http_printf(httpc, "Content-Type: text/plain\r\n");
    if (rc) goto quit;
        
    rc = http_printf(httpc, "\r\n");
    if (rc) goto quit;
    
    rc = http_printf(httpc, "501 Not Implemented\n");

quit:
    http_exit("httprnim(), rc=%d\n", rc);
    return rc;        
}
