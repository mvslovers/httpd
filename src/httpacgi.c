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

    /* add to array of CGI */
    array_add(&httpd->httpcgi, cgi);

quit:
    return cgi;
}
