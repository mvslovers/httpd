/* FTPCOPEN.C */
#include "httpd.h"

/* open data connection, returns socket number *or* -1 for error */
int
ftpcopen(FTPC *ftpc)
{
    int                 rc;
    struct sockaddr_in  sockaddr_in = {0};
    int                 len = sizeof(sockaddr_in);
    int                 maxsock = 0;
    timeval             wait = {0};
    fd_set              read = {0};

    if (ftpc->data_socket >= 0) {
        /* if we have a data socket, we assume it's a listener socket */
        ftpc->flags &= 0xFF - FTPC_FLAG_PASSIVE;    /* reset the passive flag */
        ftpcmsg(ftpc, "150 Listening for data connection");

        /* set the wait interval */
        wait.tv_sec     = 30;
        wait.tv_usec    = 0;

        /* build fd_set for listener socket */
        FD_SET(ftpc->data_socket, &read);
        maxsock = ftpc->data_socket + 1;

        /* wait for connection from FTP client */
        rc = select(maxsock, &read, NULL, NULL, &wait);
        /* wtof("%s select() rc=%d", __func__, rc); */
        if (rc < 0) goto quit;
        if (rc==0) {
            /* timeout */
            rc = -1;    /* so that quit will close the socket */
            goto quit;
        }

        /* accept connection from FTP client */
        rc = accept(ftpc->data_socket, &sockaddr_in, &len);
        /* wtof("%s accept() rc=%d", __func__, rc); */

        /* we close the listener socket and return the new socket or error */
        closesocket(ftpc->data_socket);
        ftpc->data_socket = -1;
        goto quit;
    }

    rc = socket(AF_INET, SOCK_STREAM, 0);
    if (rc < 0) goto quit;
    ftpc->data_socket = rc;

    sockaddr_in.sin_family = AF_INET;
    sockaddr_in.sin_port   = htons(ftpc->data_port);
    sockaddr_in.sin_addr.s_addr = htonl(ftpc->data_addr);
    rc = connect(ftpc->data_socket, &sockaddr_in, len);
    if (rc < 0) goto quit;

    rc = ftpc->data_socket;
    ftpcmsg(ftpc,"150 Data connection opened");

quit:
    if (rc < 0) {
        /* an error occured */
        if (ftpc->data_socket >= 0) {
            /* close the data socket */
            closesocket(ftpc->data_socket);
            ftpc->data_socket = -1;
        }
    }
    else {
        /* set non-blocking on the new socket (rc) */
        len = 1;
        if (ioctlsocket(rc, FIONBIO, &len)) {
            wtof("FTPD053E Unable to set non-blocking I/O for socket %d\n",
                rc);
        }
    }
    return rc;
}
