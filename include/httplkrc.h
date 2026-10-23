/* HTTPLKRC.H
** Turn the outcome of a LINK into the single code http_link() hands back.
**
** Free of httpd.h so the decision unit-tests on the host as well as on MVS
** (see test/tstlkrc.c).
*/
#ifndef HTTPLKRC_H
#define HTTPLKRC_H

/* http_link() failure codes.  Keep in sync with httpcgi.h.  A negative rc that
** is NOT one of these is a negated abend code, formatted 0x00sssuuu; these sit
** above that range so they can never be mistaken for one.  Anything here means
** the module never ran, so there is no dump to go looking for.
**
** Residual ambiguity, by design: a module that deliberately returns -1 is
** indistinguishable from one that could not be loaded, because __link()
** collapses both into *prc = -1.  Server modules return 0. */
#define HTTP_LINK_ENOLOAD (-0x01000001) /* LINK failed, not S806 (see ENOMOD) */
#define HTTP_LINK_ENOPGM  (-0x01000002) /* route carries no program name    */
#define HTTP_LINK_EESTAE  (-0x01000003) /* __linkds could not set the ESTAE */
#define HTTP_LINK_ENOMOD  (-0x01000004) /* module not found (S806)          */

/* A module that ran to completion and returned a NEGATIVE rc of its own.
** It cannot be passed through raw: -5 from the module and the negated abend
** code of a U0005 are the same integer, and reporting the one as the other is
** the whole of #131.  The rc is folded into a reserved range instead, so
** httppcgi() can report what happened without claiming an abend.
**
** Only a module with its own __start can get here -- cgistart clamps a
** negative main() rc to 0, because a CGI reports failures through the HTTP
** response and R15 is not that channel.  The fold is bounded to the abend
** range on the way in, so it cannot overflow. */
#define HTTP_LINK_EPGMRC_BASE (-0x02000000)
#define HTTP_LINK_EPGMRC(rc)  (HTTP_LINK_EPGMRC_BASE + (rc))
#define HTTP_LINK_IS_PGMRC(v) ((v) <= HTTP_LINK_EPGMRC_BASE)
#define HTTP_LINK_PGMRC(v)    ((v) - HTTP_LINK_EPGMRC_BASE)

/* LINK completion code of a module that is in neither STEPLIB nor the link
** list, as __linkdsx() reports it right-aligned (S806). */
#define HTTP_LINK_CC_NOTFOUND 0x806

/* Map what __linkdsx() returned to the http_link() code.
**   rc      __linkdsx() return value: 0, a positive 0x00sssuuu abend code, or
**           negative when it could not establish its ESTAE
**   prc     the linked program's rc, or __link()'s -1 when LINK failed
**   abcode  LINK's completion code, 0 unless LINK took its error exit
**
** Returns prc when the module ran and returned a non-negative rc; otherwise
** the negated abend code, one of the HTTP_LINK_E* codes, or a folded
** HTTP_LINK_EPGMRC() value. */
int httplkrc(int rc, int prc, unsigned abcode);

#endif /* HTTPLKRC_H */
