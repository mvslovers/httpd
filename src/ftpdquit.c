/* FTPDQUIT.C */
#include "httpd.h"

int
ftpdquit(FTPC *ftpc)
{
    if (ftpc->state == FTPSTATE_TRANSFER) {
        ftpdabor(ftpc);
    }

    ftpc->state = FTPSTATE_TERMINATE;

    return 0;
}
