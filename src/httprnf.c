/* HTTPRISE.C
** HTTP not found 404
*/
#include "httpd.h"

int
httprnf(HTTPC *httpc, const UCHAR *path)
{
    int rc;

    http_enter("httprnf()\n");

    /* Send not found response */
    rc = http_resp(httpc, 404);
    if (rc) goto quit;

    rc = http_printf(httpc, "Cache-Control: no-store\r\n");
    if (rc) goto quit;

    rc = http_printf(httpc, "Content-Type: text/plain\r\n");
    if (rc) goto quit;

    rc = http_printf(httpc, "\r\n");
    if (rc) goto quit;

    if (path) {
        rc = http_printf(httpc, "404 document \"%s\" not found\n", path);
    }

quit:
    http_exit("httprnf(), rc=%d\n", rc);
    return rc;
}
