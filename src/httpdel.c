/* HTTPDEL.C
** Process DELETE request
** Transitions to next state as needed.
*/
#include "httpd.h"

extern int
httpdel(HTTPC *httpc)
{
    int rc;

    http_enter("httpdel()\n");

    /* not implemented */
    rc = http_resp_not_implemented(httpc);

quit:
    httpc->state = CSTATE_DONE;
    http_exit("httpdel()\n");
    return rc;
}
