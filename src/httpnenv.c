/* HTTPNENV.C
** Allocate a new environment variable
*/
#include "httpd.h"

extern HTTPV *
httpnenv(const UCHAR *name, const UCHAR *value)
{
    size_t      namelen = strlen(name);
    size_t      vallen  = value ? strlen(value) : 0;
    /* the block holds "name\0value\0" starting at v->name; size it to exactly
       that (offsetof, not sizeof(HTTPV), which counts name[2] + padding and
       over-allocates ~4 bytes per var) */
    size_t      total   = offsetof(HTTPV, name) + namelen + 1 + vallen + 1;
    HTTPV       *v;

    /* Both rejects return NULL, so set errno to tell the caller which one it
       was: httpsenv() reports an oversized variable differently from a storage
       shortage, and since libc370#82 the shortage is a real outcome rather
       than an S878 (issue #162). */
    if (namelen + vallen > 8192) {              /* sanity limit */
        errno = E2BIG;
        return NULL;
    }

    /* Pinned to subpool 0, not calloc'd (issue #154): http_set_env() is
       reached through the httpx vector, so this runs under the MODULE's
       module's ambient subpool (#174) -- but the HTTPV hangs off
       httpc->env and is freed by http_reset() AFTER the module returned.  In
       the module's subpool it would be freed twice: once by the abend-path
       reclaim, once by http_reset(). */
    v = __getmsp(total, 0);
    if (!v) errno = ENOMEM;
    else    memset(v, 0, total);

    if (v) {
        strcpy(v->eye, HTTPV_EYE);
        strcpy(v->name, name);
        v->value = &v->name[namelen+1];
        if (vallen) strcpy(v->value, value);
    }

    return v;
}
