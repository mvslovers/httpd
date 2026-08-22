/* httpacgi() - add path and pgm to array of HTTPCGI in HTTPD handle
   note: pgm and path much be literal string constants that do not change.
   A NULL pgm registers a program-less (LOC=) route: a static path prefix
   that carries auth policy but falls through to httpget instead of a CGI.

   The login argument is ignored (#105).  It fed the global LOGIN bitmask,
   which is retired -- AUTH= per route is the only authentication policy now.
   The parameter stays because this function sits in the httpx vector at 0x104
   and CGI modules are compiled against that signature; the route it builds is
   HTTP_AUTH_NONE (calloc) whatever is passed, so a module registering a route
   gets a public one and nothing pretends otherwise.
*/
#include "httpd.h"

HTTPCGI *httpacgi(HTTPD *httpd, const char *pgm, const char *path, int login)
{
    HTTPCGI *cgi    = NULL;
    UCHAR   sp;

    /* The route table lives as long as the server, and this is reachable from
       module context through the httpx vector, so pin every allocation below
       -- the HTTPCGI, both strdup()s and the array growth -- to subpool 0
       (issue #154).  During a module window the ambient subpool is the
       module's, and a route freed when that module abends would leave the
       server matching requests against released storage. */
    sp = __setsp(0);

    if (!httpd) goto quit;
    if (!path) goto quit;

    cgi = calloc(1, sizeof(HTTPCGI));
    if (!cgi) goto quit;

    /* set the eye catcher */
    strcpy(cgi->eye, HTTPCGI_EYE);
    if (strchr(path,'*')) cgi->wild++;
    if (strchr(path,'?')) cgi->wild++;
    (void)login;                            /* retired -- see above */
    cgi->len  = strlen(path);
    cgi->path = strdup(path);
    cgi->pgm  = pgm ? strdup(pgm) : NULL;   /* NULL pgm == LOC route */

    /* on OOM don't register a half-built entry -- a NULL path would NULL-deref
       later at CGI match; a NULL pgm from a failed strdup (pgm was non-NULL)
       would NULL-deref at CGI link.  (For a successfully registered entry the
       strdup storage is AS-lifetime by design.) */
    if (!cgi->path || (pgm && !cgi->pgm)) {
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
    __setsp(sp);
    return cgi;
}
