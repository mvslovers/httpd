/* HTTPDONE.C
** Close any open files
** Executed by the server when the client state is CSTATE_DONE.
** Transitions to next client state.
*/
#include "httpd.h"

extern int
httpdone(HTTPC *httpc)
{
    int rc = 0;

    if (httpc->fp) {
        /* close file handle */
        fclose(httpc->fp);
        httpc->fp = NULL;
    }

    httpsecs(&httpc->end);

quit:
    /* transition to next state */
    httpc->state = CSTATE_REPORT;

    return rc;
}
