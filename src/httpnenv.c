/* HTTPNENV.C
** Allocate a new environment variable
*/
#include "httpd.h"

extern HTTPV *
httpnenv(const UCHAR *name, const UCHAR *value)
{
    size_t      namelen = strlen(name);
    size_t      vallen  = value ? strlen(value) : 0;
    size_t      total   = sizeof(HTTPV) + namelen + vallen + 2;
    HTTPV       *v;

    if (namelen + vallen > 8192) return NULL; /* sanity limit */

    v = calloc(1, total);

    if (v) {
        strcpy(v->eye, HTTPV_EYE);
        strcpy(v->name, name);
        v->value = &v->name[namelen+1];
        if (vallen) strcpy(v->value, value);
    }

quit:
    return v;
}
