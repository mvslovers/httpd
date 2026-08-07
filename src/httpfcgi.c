#include "httpd.h"

/* call http_cmp() directly (HTTPCMP), not through the httpx vector -- this is
   the server's own code path and used to reach it as the bare CSECT name
   httpcmp(), which had no prototype in scope. */
#undef http_cmp

HTTPCGI *httpfcgi(HTTPD *httpd, const char *path)
{
    HTTPCGI     *cgi    = NULL;
    unsigned    count;
    unsigned    n;

    if (!httpd) goto quit;
    if (!path) goto quit;

    count = array_count(&httpd->httpcgi);
    for (n=0; n < count; n++) {
        HTTPCGI *p = httpd->httpcgi[n];

        if (!p) continue;
        if (p->wild) {
            /* use pattern matching */
            if (__patmat(path, p->path)) {
                cgi = p;
                break;
            }
        }
        else {
            /* use caseless string compare */
            if (http_cmp(path, p->path)==0) {
                cgi = p;
                break;
            }
        }
    }

quit:
    return cgi;
}

