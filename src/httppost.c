/* HTTPPOST.C
** Process POST request
** Transitions to next state as needed.
*/
#include "httpd.h"

extern int
httppost(HTTPC *httpc)
{
    int rc;

    http_enter("httppost()\n");

    /* not implemented */
    rc = http_resp_not_implemented(httpc);

quit:
    httpc->state = CSTATE_DONE;
    http_exit("httppost(), rc=%d\n", rc);
    return rc;
}
