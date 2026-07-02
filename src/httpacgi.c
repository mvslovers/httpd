/* httpacgi() - add path and pgm to array of HTTPCGI in HTTPD handle
   note: pgm and path much be literal string constants that do not change.
*/
#include "httpd.h"

HTTPCGI *httpacgi(HTTPD *httpd, const char *pgm, const char *path, int login)
{
    HTTPCGI *cgi    = NULL;

    if (!httpd) goto quit;
    if (!pgm) goto quit;
    if (!path) goto quit;

    cgi = calloc(1, sizeof(HTTPCGI));
    if (!cgi) goto quit;

    /* set the eye catcher */
    strcpy(cgi->eye, HTTPCGI_EYE);
    if (strchr(path,'*')) cgi->wild++;
    if (strchr(path,'?')) cgi->wild++;
    cgi->login = (login ? 1 : 0);
    cgi->len  = strlen(path);
    cgi->path = strdup(path);
    cgi->pgm  = strdup(pgm);

    /* on OOM don't register a half-built entry -- a NULL path/pgm would
       NULL-deref later at CGI match/link.  (For a successfully registered
       entry the strdup storage is AS-lifetime by design.) */
    if (!cgi->path || !cgi->pgm) {
        free(cgi->path);
        free(cgi->pgm);
        free(cgi);
        cgi = NULL;
        goto quit;
    }

    /* add to array of CGI; free the entry on an array_add OOM so it doesn't
       leak (it isn't registered, so no later reference to it) */
    if (array_add(&httpd->httpcgi, cgi)) {
        free(cgi->path);
        free(cgi->pgm);
        free(cgi);
        cgi = NULL;
    }

quit:
    return cgi;
}
