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

    if (httpc->ufp) {
        /* close UFS file handle */
        ufs_fclose(&httpc->ufp);
    }

    /* send terminating chunk for chunked transfer encoding */
    if (httpc->chunked) {
        /* "0\r\n\r\n" in ASCII — disable framing first */
        UCHAR term[5];
        httpc->chunked = 0;
        term[0] = 0x30;  /* '0' */
        term[1] = 0x0D;  /* CR  */
        term[2] = 0x0A;  /* LF  */
        term[3] = 0x0D;  /* CR  */
        term[4] = 0x0A;  /* LF  */
        http_send(httpc, term, 5);
    }

    httpsecs(&httpc->end);

quit:
    /* transition to next state */
    httpc->state = CSTATE_REPORT;

    return rc;
}
