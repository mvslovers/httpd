#include "httpd.h"

/* call http_cmp() directly (HTTPCMP), not through the httpx vector -- this is
   the server's own code path and used to reach it as the bare CSECT name
   httpcmp(), which had no prototype in scope. */
#undef http_cmp

HTTPROUTE *httpfcgi(HTTPD *httpd, const char *path)
{
    HTTPROUTE     *route    = NULL;
    unsigned    count;
    unsigned    n;

    if (!httpd) goto quit;
    if (!path) goto quit;

    count = array_count(&httpd->route);
    for (n=0; n < count; n++) {
        HTTPROUTE *p = httpd->route[n];

        if (!p) continue;
        if (p->wild) {
            /* use pattern matching */
            if (__patmat(path, p->path)) {
                route = p;
                break;
            }
        }
        else {
            /* use caseless string compare */
            if (http_cmp(path, p->path)==0) {
                route = p;
                break;
            }
        }
    }

quit:
    return route;
}

