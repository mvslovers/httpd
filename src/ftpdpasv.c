/* FTPDPASV.C */
#include "httpd.h"

int
ftpdpasv(FTPC *ftpc)
{
    int                 rc  = 0;
    int                 len = 0;
    struct sockaddr_in  in  = {0};
    unsigned char       *a;
    unsigned char       *p;

#if 0 /* old method was to bind localhost for the data listener socket */
    in.sin_family       = AF_INET;
    in.sin_addr.s_addr  = 0x7F000001;   /* 127.0.0.1    */
    /* note we're not setting a port as we want the system to assign a port */
#else
    /* The client has requested PASV which means he wants us to open a listening port
     * on this server for the data connection. So we will use the client socket to
     * obtain the servers host address as we want to bind the data socket to the
     * same host address.
     */
    len = sizeof(in);
    if (getsockname(ftpc->client_socket, (void *)&in, &len) < 0) {
        ftpcmsg(ftpc, "500 Unable to get client socket name");
        goto failed;
    }
    /* we clear the port number so that the host will assign an unused port
     * for the data connection when we bind the new socket.
     */
    in.sin_port = 0;
#endif

    /* wtof("%s socket(%d,%d,%d)", __func__, AF_INET, SOCK_STREAM, IPPROTO_TCP); */
    ftpc->data_socket   = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ftpc->data_socket < 0) {
    	ftpcmsg(ftpc, "500 Unable to allocate passive data socket");
    	goto failed;
    }

    /* wtof("%s socket=%d", __func__, ftpc->data_socket); */
    rc = bind(ftpc->data_socket, &in, sizeof(in));
    if (rc < 0) {
        wtof("%s bind failed for socket=%d, rc=%d, errno=%d", __func__, ftpc->data_socket, rc, errno);
        ftpcmsg(ftpc, "500 Unable to bind data socket");
        goto failed;
    }

    /* get socket address and port */
    len = sizeof(in);
    if (getsockname(ftpc->data_socket, (void *)&in, &len) < 0) {
        ftpcmsg(ftpc, "500 Unable to get data socket name");
        goto failed;
    }

    /* listen on the data socket */
    if (listen(ftpc->data_socket, 1) < 0) {
        ftpcmsg(ftpc, "500 Unable to listen on data socket");
        goto failed;
    }

    /* save socket address and port */
    ftpc->data_addr = in.sin_addr.s_addr;
    ftpc->data_port = in.sin_port;
    ftpc->flags     |= FTPC_FLAG_PASSIVE;

    a = (unsigned char*)&ftpc->data_addr;
    p = (unsigned char*)&ftpc->data_port;
    ftpcmsg(ftpc, "227 Entering Passive Mode (%u,%u,%u,%u,%u,%u)",
        a[0], a[1], a[2], a[3], p[2], p[3]);
    goto quit;

failed:
	if (ftpc->data_socket >= 0) {
		closesocket(ftpc->data_socket);
	}
	ftpc->data_socket   = -1;
	ftpc->flags         &= 0xFF - FTPC_FLAG_PASSIVE;

quit:
    return 0;
}
