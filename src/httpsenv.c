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
    /* add the new variable to the array */
    array_add(&httpc->env, v);

quit:
    return rc;
}
