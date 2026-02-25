/* HTTPDENV.C
** Delete environment variable
*/
#include "httpd.h"

extern int
httpdenv(HTTPC *httpc, const UCHAR *name)
{
    int         rc      = 0;
    unsigned    indx    = http_find_env(httpc, name);
    
    if (indx) {
        free(httpc->env[indx-1]);
        array_del(&httpc->env, indx);
    }

quit:
    return rc;
}
