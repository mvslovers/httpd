#include "httpd.h"
#include "httplkrc.h"

int httplink(HTTPC *httpc, const char *pgm)
{
    int         rc      = -1;   /* link return code     */
    unsigned    abcode  = 0;    /* LINK completion code */
    int         prc     = HTTP_LINK_ENOPGM; /* pgm return code  */
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
    if (HTTP_TRACING(httpc)) httptrc_mark(httpc, TM_LINK0);
    rc = __linkdsx(pgm, dcb, plist, &prc, &abcode);
    if (HTTP_TRACING(httpc)) httptrc_mark(httpc, TM_LINK1);

#if 0
    wtof("httplink rc=%d, prc=%d", rc, prc);
    ppa    = (void*)__ppaget();
    wtof("httplink ppa=%08X", ppa);
#endif

    /* Sort out what actually happened (#131, #320); httplkrc() holds the
    ** decision so it can be tested without a LINK. */
    prc = httplkrc(rc, prc, abcode);

quit:
    return prc;
}
