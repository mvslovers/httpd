/* HTTPFENV.C
** Find environment variable
*/
#include "httpd.h"

extern unsigned
httpfenv(HTTPC *httpc, const UCHAR *name)
{
    unsigned    indx    = 0;
    HTTPV       *v      = NULL;
    unsigned    count   = array_count(&httpc->env);
    unsigned    n;
    
    for(n=0; n<count; n++) {
        v = httpc->env[n];
        if (http_cmp(v->name, name)==0) {
            indx = n+1;
            break;
        }
    }

quit:
    return indx;
}
