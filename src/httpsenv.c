/* HTTPSENV.C
** Set environment variable
*/
#include "httpd.h"

extern int
httpsenv(HTTPC *httpc, const UCHAR *name, const UCHAR *value)
{
    int         rc      = 0;
    HTTPV       *v      = NULL;

    /* allocate a new environment variable */
    v = http_new_env(name, value);
    if (!v) {
        rc = -1;
        goto quit;
    }

    /* if we already have this variable then delete it */
    http_del_env(httpc, name);
#if 0
	wtof("%s: name=\"%s\", value=\"%s\"", __func__, v->name, v->value);
#endif
    /* add the new variable to the array.  http_del_env() already removed any
       old value, so on an array_add OOM the variable would silently vanish and
       the new HTTPV would be an unreachable orphan -- free it and report. */
    if (array_add(&httpc->env, v)) {
        free(v);
        rc = -1;
    }

quit:
    return rc;
}
