/* HTTPPCGI.C
** Process CGI request
** Transitions to next state as needed.
*/
#include "httpd.h"

extern int
httppcgi(HTTPC *httpc, HTTPCGI *cgi)
{
    int         rc      = 0;

    http_enter("httppcgi()\n");

    /* link to external program */
    rc = httplink(httpc, cgi->pgm);
    if (rc < 0) {
        /* some kind of ABEND occurred */
        unsigned abcode = (unsigned) (rc * -1);   /* make positive again */

        if (httpx) {
            /* we're running in the HTTPD server */
            if (!httpc->resp) {
                /* no response header was issued by CGI program */
                http_resp(httpc,503);   
                http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
                http_printf(httpc, "\r\n");
            }

            if (abcode > 4095) {
                /* system abend occurred */
                http_printf(httpc, "External program %s failed with S%03X ABEND", cgi->pgm, abcode >> 12);
            }
            else {
                /* user abend code */
                http_printf(httpc, "External program %s failed with U%04d ABEND", cgi->pgm, abcode);
            }
            http_printf(httpc, "\n");
        }

        if (abcode > 4095) {
            /* system abend occurred */
            wtof("External program %s failed with S%03X ABEND", cgi->pgm, abcode >> 12);
        }
        else {
            /* user abend code */
            wtof("External program %s failed with U%04d ABEND", cgi->pgm, abcode);
        }
    }

quit:
    httpc->state = CSTATE_DONE;
    http_exit("httppcgi()\n");
    return rc;
}

