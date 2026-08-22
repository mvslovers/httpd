/* HTTPFENV.C
** Find environment variable
**
** v is dereferenced without a NULL check on purpose: httpc->env has no empty
** slot below array_count() -- http_set_env() is its only producer and adds a
** checked non-NULL HTTPV.  See docs/development.md, "Dynamic arrays: no holes
** below the count" (#229).  This is the hottest lookup in the server; do not
** add a check that can never fire.
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

    return indx;
}
