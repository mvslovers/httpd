#include "httpd.h"

int httplink(HTTPC *httpc, const char *pgm)
{
    int         rc      = -1;   /* link return code     */
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
    rc = __linkds(pgm, dcb, plist, &prc);

#if 0
    wtof("httplink rc=%d, prc=%d", rc, prc);
    ppa    = (void*)__ppaget();
    wtof("httplink ppa=%08X", ppa);
#endif

    /* Sort out what actually happened (#131).  __linkds() returns 0 when
    ** nothing abended and leaves the linked program's rc -- or __link()'s own
    ** -1 -- in prc, so "abended" and "never ran" are two different answers and
    ** must not both come out as a negated abend code. */
    if (rc > 0) {
        /* the module abended; rc is the 0x00sssuuu completion code */
        prc = rc * -1;
    }
    else if (rc < 0) {
        /* __linkds() could not establish its ESTAE, so it never issued the
        ** LINK: the module did not run and did not abend */
        prc = HTTP_LINK_EESTAE;
    }
    else if (prc == -1) {
        /* No abend, and __link()'s failure return.  MVS took the LINK SVC
        ** error-return exit -- the module is not in STEPLIB/LINKLIST, or the
        ** LINK failed for another reason.  There is no abend code to report
        ** here, and no dump to go looking for. */
        prc = HTTP_LINK_ENOLOAD;
    }
    else if (prc < 0) {
        /* The module ran and returned a negative rc of its own.  Fold it into
        ** its own range so httppcgi() does not read it as a negated abend
        ** code; clamp first, so the fold stays inside an int. */
        if (prc < -0x00FFFFFF) prc = -0x00FFFFFF;
        prc = HTTP_LINK_EPGMRC(prc);
    }

quit:
    return prc;
}
