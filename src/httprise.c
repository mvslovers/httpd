/* HTTPRISE.C
** HTTP response internal error 500
*/
#include "httpd.h"

int
httprise(HTTPC *httpc)
{
    int rc;

    http_enter("httprise()\n");

    /* Send internal error response */
    rc = http_resp(httpc, 500);
    if (rc) goto quit;

    rc = http_printf(httpc, "Content-Type: text/plain\r\n");
    if (rc) goto quit;

    rc = http_printf(httpc, "\r\n");
    if (rc) goto quit;

    rc = http_printf(httpc, "500 Internal Server Error\n");

quit:
    http_exit("httprise(), rc=%d\n", rc);
    return rc;
}
