/* FTPDTERM.C */
#include "httpd.h"

void
ftpdterm(FTPD **ppftpd)
{
    FTPD        *ftpd;
    FTPC        *ftpc;
    int         i;
    unsigned    n;
    unsigned    count;

    if (!ppftpd) goto quit;
    ftpd = *ppftpd;
    if (!ftpd) goto quit;

    ftpd->flags |= FTPD_FLAG_QUIESCE;
    ftpd->flags |= FTPD_FLAG_SHUTDOWN;

    if (ftpd->ftpd_thread) {
        CTHDTASK *task = ftpd->ftpd_thread;

        for(i=0; (i < 10) && (!(task->termecb & 0x40000000)); i++) {
            if (i) {
                wtof("FTPD040I Waiting for FTPD thread to terminate (%d)\n",
                    i);
            }
            /* we need to wait 1 second */
            __asm__("STIMER WAIT,BINTVL==F'100'   1 seconds");
        }

        if(!(task->termecb & 0x40000000)) {
            wtof("FTPD041I Force detaching FTPD thread\n");
        }
        cthread_delete(&ftpd->ftpd_thread);
        ftpd->ftpd_thread = 0;
    }

    if (ftpd->ftpc) {
        /* terminate all clients */
        count = array_count(&ftpd->ftpc);
        for(n=0; n < count; n++) {
            ftpc = ftpd->ftpc[n];
            if (!ftpc) continue;
            ftpcterm(&ftpc);
            ftpd->ftpc[n] = 0;
        }
        array_free(&ftpd->ftpc);
        ftpd->ftpc = 0;
    }

    if (ftpd->listen >= 0) {
        /* close listener socket */
        closesocket(ftpd->listen);
        ftpd->listen = -1;
    }

    if (ftpd->stats) {
        /* close statistics file */
        fclose(ftpd->stats);
        ftpd->stats = 0;
    }
    if (ftpd->dbg) {
        /* close debug file */
        fclose(ftpd->dbg);
        ftpd->dbg = 0;
    }

    /* free the FTPD handle */
    free(ftpd);
    *ppftpd = 0;

quit:
    return;
}

