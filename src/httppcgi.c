/* HTTPPCGI.C
** Process CGI request
** Transitions to next state as needed.
*/
#include "httpd.h"

/* call http_link() directly (HTTPLINK), not through the httpx vector -- this is
   the server's own code path and used to reach it as the bare CSECT name
   httplink(), which had no prototype in scope. */
#undef http_link

/*
** reclaim() - release everything the CGI left in its heap subpool.
**
** The R form of FREEMAIN with SP= and no length is the "release the whole
** subpool" form (SR 1,1 tells MVS so); RC is used rather than RU so a bad
** request comes back as a return code instead of an abend inside the abend
** handler.  Subpools are per task (measured, libc370#89 T0), and this runs on
** the same worker TCB the module ran on, so it can only ever release this
** request's storage -- never another worker's live module storage.
**
** Nothing here is conditional on the CGI having allocated anything: releasing
** an empty subpool is a no-op with rc 0.
*/
static int
reclaim(unsigned char sp)
{
    int         rc  = 0;
    unsigned    usp = sp;

    __asm__("FREEMAIN RC,SP=(%1)\n\t"
            "LR\t%0,15              save the return code"
            : "=r"(rc) : "r"(usp) : "0", "1", "14", "15");

    return rc;
}

extern int
httppcgi(HTTPC *httpc, HTTPCGI *cgi)
{
    int         rc      = 0;

    http_enter("httppcgi()\n");

    /* link to external program */
    rc = http_link(httpc, cgi->pgm);
    if (rc < 0) {
        /* The module did not run to completion.  Which of the possible
        ** reasons it was decides the wording only -- everything below the
        ** reason text is common to all of them (#131). */
        const char  *pgm    = cgi->pgm ? cgi->pgm : "(none)";
        const char  *reason = NULL;
        char        detail[32];

        /* Release the CGI's storage FIRST, before anything else on this path
        ** (issue #154).  Until this existed, an abending CGI kept everything it
        ** held for the life of the address space: nothing runs its @@EXITA, the
        ** worker TCB survives by design, and libc370's malloc has no memmgr to
        ** reclaim from.  Enough of those and http_link() can no longer load ANY
        ** module and every request answers S80A until the server is restarted.
        **
        ** First, because the region may be tight -- which is how this defect
        ** used to be reached -- and the response below is more likely to get
        ** out with the storage already back.  Nothing on the response path
        ** points into the module's subpool: httpc->buf is inline, and the env,
        ** the UFS session and any file handle httpd holds for this request are
        ** pinned to subpool 0.
        **
        ** Unconditional since #174 (the RECLAIM= route option is retired):
        ** cgistart set the subpool for this module, so everything it malloc'd
        ** belongs to this request -- module storage that must survive is
        ** pinned with __getmsp(size, 0) by contract.  Releasing an empty
        ** subpool (a module built before #154, whose cgistart never asked)
        ** is a no-op with rc 0. */
        {
            int frc = reclaim(HTTP_CGI_SUBPOOL);

            if (frc) {
                wtof("HTTPD906W Storage reclaim for %s failed, rc=%d",
                     pgm, frc);
            }
        }

        /* Say what actually went wrong.  Only a real abend carries a code, and
        ** only a real abend leaves a dump worth looking for -- everything else
        ** here means the module never ran (#131). */
        switch (rc) {
        case HTTP_LINK_ENOLOAD:
            reason = "could not be loaded (not found in STEPLIB?)";
            break;
        case HTTP_LINK_ENOPGM:
            reason = "was dispatched without a program name";
            break;
        case HTTP_LINK_EESTAE:
            reason = "could not be started, ESTAE create failed";
            break;
        default:
            if (HTTP_LINK_IS_PGMRC(rc)) {
                /* the module ran and returned a negative rc of its own -- a
                ** failure worth reporting, but not an abend and not a dump */
                snprintf(detail, sizeof(detail),
                         "returned rc %d", HTTP_LINK_PGMRC(rc));
            }
            else {
                /* an abend after all; rc is the negated 0x00sssuuu code */
                unsigned abcode = (unsigned) (rc * -1);  /* positive again */

                if (abcode > 4095) {
                    /* system abend occurred */
                    snprintf(detail, sizeof(detail),
                             "failed with S%03X ABEND", abcode >> 12);
                }
                else {
                    /* user abend code */
                    snprintf(detail, sizeof(detail),
                             "failed with U%04d ABEND", abcode);
                }
            }
            reason = detail;
            break;
        }

        if (httpx) {
            /* we're running in the HTTPD server */
            if (!httpc->resp) {
                /* no response header was issued by CGI program */
                http_resp(httpc,503);
                http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
                http_printf(httpc, "\r\n");
            }

            http_printf(httpc, "External program %s %s", pgm, reason);
            http_printf(httpc, "\n");
        }

        wtof("HTTPD908E External program %s %s", pgm, reason);
    }

    httpc->state = CSTATE_DONE;
    http_exit("httppcgi()\n");
    return rc;
}

