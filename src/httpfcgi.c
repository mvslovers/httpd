#include "httpd.h"

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
            if (httpcmp(path, p->path)==0) {
                cgi = p;
                break;
            }
        }
    }

quit:
    return cgi;
}

