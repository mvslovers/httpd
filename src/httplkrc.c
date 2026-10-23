/* HTTPLKRC.C
** Turn the outcome of a LINK into the single code http_link() hands back.
**
** Kept free of httpd.h so the decision unit-tests dual (host + MVS).
*/
#include "httplkrc.h"

int
httplkrc(int rc, int prc, unsigned abcode)
{
    if (rc > 0) {
        /* the module abended; rc is the 0x00sssuuu completion code */
        return rc * -1;
    }
    if (rc < 0) {
        /* __linkdsx() could not establish its ESTAE, so it never issued the
        ** LINK: the module did not run and did not abend */
        return HTTP_LINK_EESTAE;
    }
    if (prc == -1) {
        /* No abend, and __link()'s failure return: MVS took the LINK SVC
        ** error-return exit.  S806 says the module is in neither STEPLIB nor
        ** the link list; any other code (S106 fetch error, S80A storage, ...)
        ** keeps its own answer. */
        return abcode == HTTP_LINK_CC_NOTFOUND ? HTTP_LINK_ENOMOD
                                               : HTTP_LINK_ENOLOAD;
    }
    if (prc < 0) {
        /* The module ran and returned a negative rc of its own.  Clamp first,
        ** so the fold stays inside an int. */
        if (prc < -0x00FFFFFF) prc = -0x00FFFFFF;
        return HTTP_LINK_EPGMRC(prc);
    }
    return prc;
}
