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
        /* Say so.  Before libc370#82 a storage shortage here abended S878 and
           try(serve_client) logged HTTPD062E; now it is a quiet -1 that every
           caller turns into CSTATE_RESET, i.e. a connection reset with no
           response and no log line -- indistinguishable from a malformed
           request (issue #162).  This is the one choke point every env-var
           caller passes through, and it still knows which variable failed,
           which the reset sites no longer do. */
        if (errno == E2BIG)
            wtof("HTTPD905E Environment variable %.32s too large "
                 "client(%08X)", name, httpc);
        else
            wtof("HTTPD904E No storage for environment variable %.32s "
                 "client(%08X)", name, httpc);
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
        wtof("HTTPD904E No storage for environment variable %.32s "
             "client(%08X)", name, httpc);
        rc = -1;
    }

quit:
    return rc;
}
