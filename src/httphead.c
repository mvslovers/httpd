/* HTTPHEAD.C
** Process HEAD request
** Transitions to next state as needed.
*/
#include "httpd.h"

extern int
httphead(HTTPC *httpc)
{
    int rc;

    http_enter("httphead()\n");

    /* not implemented */
    rc = http_resp_not_implemented(httpc);

quit:
    httpc->state = CSTATE_DONE;
    http_exit("httphead(), rc=%d\n", rc);
    return rc;
}
