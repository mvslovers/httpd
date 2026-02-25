/* HTTPSQEN.C
** Set QUERY environment variable
*/
#include "httpd.h"

extern int
httpsqen(HTTPC *httpc, const UCHAR *name, const UCHAR *value)
{
    char        newname[256];
    
    sprintf(newname, "QUERY_%s", name);

    return http_set_env(httpc, newname, value);
}
