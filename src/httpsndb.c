/* HTTPSNDB.C
** Send binary file, file handle is already open.
*/
#include "httpd.h"

int
httpsndb(HTTPC *httpc)
{
    int     rc  = 0;

    http_enter("httpsndb()\n");

    rc = http_send_file(httpc, 1);

#if 0
    if (rc) goto quit;
    if (httpc->state==CSTATE_DONE) goto quit;

    rc = http_send_file(httpc, 1);
#endif

quit:
    http_exit("httpsndb()\n");
    return rc;
}
