/* HTTPGENV.C
** Get environment variable value
*/
#include "httpd.h"

extern UCHAR *
httpgenv(HTTPC *httpc, const UCHAR *name)
{
    unsigned    indx    = http_find_env(httpc, name);
    HTTPV       *v      = indx ? httpc->env[indx-1] : NULL;

quit:
    return (v ? v->value : NULL);
}
