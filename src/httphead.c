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

    /* HEAD answers what GET would answer -- status, headers, Content-Length --
    ** and stops at the blank line.  httpget() sees CSTATE_HEAD and sends no
    ** body; httpprtv() drops the text an error path prints after its
    ** headers and does not frame the response as chunked (#399).
    */
    rc = http_get(httpc);

    if (httpc->state == CSTATE_HEAD) httpc->state = CSTATE_DONE;
    http_exit("httphead(), rc=%d\n", rc);
    return rc;
}
