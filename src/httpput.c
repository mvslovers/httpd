/* HTTPPUT.C
** Process PUT request
** Transitions to next state as needed.
*/
#include "httpd.h"

extern int
httpput(HTTPC *httpc)
{
    int rc;

    http_enter("httpput()\n");

    /* not implemented */
    rc = http_resp_not_implemented(httpc);

quit:
    httpc->state = CSTATE_DONE;
    http_exit("httpput()\n");
    return rc;
}
