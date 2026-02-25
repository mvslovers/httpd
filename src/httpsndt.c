/* HTTPSNDT.C
** Send text file, file handle is already open.
*/
#include "httpd.h"

int
httpsndt(HTTPC *httpc)
{
    int     rc  = 0;

    http_enter("httpsndt()\n");

    rc = http_send_file(httpc, 0);

    http_exit("httpsndt()\n");
    return rc;
}
