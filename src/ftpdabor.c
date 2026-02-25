/* FTPDABOR.C */
#include "httpd.h"

int
ftpdabor(FTPC *ftpc)
{
    if (ftpc->state == FTPSTATE_TRANSFER) {
        /* cancel the transfer */
        if (ftpc->data_socket >= 0) {
            ftpcmsg(ftpc, "226 Closing data connection");
            closesocket(ftpc->data_socket);
            ftpc->data_socket = -1;
        }
        if (ftpc->fp) {
            fclose(ftpc->fp);
            ftpc->fp = NULL;
        }
        ftpc->flags &= (0xFF - (FTPC_FLAG_SEND + FTPC_FLAG_RECV));
        ftpc->state = FTPSTATE_IDLE;
        ftpc->len   = 0;
        ftpc->pos   = 0;
        goto quit;
    }

    if (ftpc->state == FTPSTATE_LOGIN) {
        ftpcmsg(ftpc, "503 Bad sequence of commands");
    }
    else {
        ftpc->state = FTPSTATE_IDLE;
        ftpcmsg(ftpc, "503 Bad sequence of commands");
    }

quit:
   return 0;
}
