/* HTTPNENV.C
** Allocate a new environment variable
*/
#include "httpd.h"

extern HTTPV *
httpnenv(const UCHAR *name, const UCHAR *value)
{
    int         namelen = strlen(name);
    int         vallen  = value ? strlen(value) : 0;
    HTTPV       *v      = calloc(1, sizeof(HTTPV)+namelen+vallen);

    if (v) {
        strcpy(v->eye, HTTPV_EYE);
        strcpy(v->name, name);
        v->value = &v->name[namelen+1];
        if (vallen) strcpy(v->value, value);
    }

quit:
    return v;
}
