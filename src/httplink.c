#include "httpd.h"

int httplink(HTTPC *httpc, const char *pgm)
{
    void        *ppa    = NULL;
    int         rc      = -1;   /* link return code     */
    int         prc     = -1;   /* pgm return code      */
    void        *dcb    = NULL; /* no DCB for link      */
    UCHAR       *path   = http_get_env(httpc, "REQUEST_PATH");;
    struct {
        unsigned short  len;
        char            buf[256];
    } parms = {0, ""};
    unsigned    plist[4];

    if (!pgm) goto quit;    /* NULL program, quit       */
    if (!*pgm) goto quit;   /* "" program name, quit    */

    if (path) {
        /* put request in quotes as parameter string */
        snprintf(parms.buf, sizeof(parms.buf)-1, "\"%s\"", path);
        parms.buf[sizeof(parms.buf)-1] = 0;
        parms.len = strlen(parms.buf);
    }

    /* build parameter list for the program we're linking to */
    plist[0]    = (unsigned)&parms;
    plist[1]    = (unsigned)httpc->httpd;
    plist[2]    = (unsigned)httpc | 0x80000000; /* VL style plist */
    plist[3]    = 0;

    /* link to pgm, with ESTAE */
#if 0
    wtof("httplink pgm=\"%s\"", pgm);
    ppa    = (void*)__ppaget();
    wtof("httplink ppa=%08X", ppa);
#endif
    rc = __linkds(pgm, dcb, plist, &prc);

#if 0
    wtof("httplink rc=%d, prc=%d", rc, prc);
    ppa    = (void*)__ppaget();
    wtof("httplink ppa=%08X", ppa);
#endif

    /* if the link failed, return that as the return code */
    if (rc > 0) prc = rc * -1;
    if (rc < 0) prc = rc;

quit:
    return prc;
}
