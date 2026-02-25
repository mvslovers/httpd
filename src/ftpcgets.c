/* FTPCGETS.C */
#include "httpd.h"

char *
ftpcgets(FTPC *ftpc)
{
    CLIBGRT         *grt    = __grtget();
    HTTPD           *httpd  = grt->grtapp1;
    char            *ptr    = 0;
    time_t          start;
    time_t          now;
    fd_set          read;
    timeval         wait;
    int             len;
    int             maxsock;
    int             rc;
    unsigned char   c;

    if (!ftpc) goto quit;

    ftpc->cmdlen = 0;

    /* set the wait interval */
    wait.tv_sec     = 5;
    wait.tv_usec    = 0;

    time(&start);

    do {
        /* read one character at a time from socket */
        memset(&read, 0, sizeof(fd_set));
        FD_SET(ftpc->client_socket, &read);
        maxsock = ftpc->client_socket + 1;

        /* wait until we have something to read OR the client closes the socket */
        rc = selectex(maxsock, &read, NULL, NULL, &wait, NULL);
        if (rc<0) break;    /* client closed the socket */
        if (rc==0) {
            time(&now);
            if ((now - start) > (15*60)) break;   /* 15 minutes have passed with no client input */
            if (httpd->flag & HTTPD_FLAG_QUIESCE) break;    /* server is shutting down */
            continue;   /* otherwise we keep waiting */
        }

        /* otherwise, we *should* have something to read */
        len = recv(ftpc->client_socket, &c, 1, 0);
        /* wtof("%s recv(%d) len=%d, c=%02X", __func__, ftpc->client_socket, len, c); */
        if (len <= 0) break;
        if (c==0x0D) continue;  /* discard CR */
        if (c==0x0A) {          /* done on LF */
            ptr = ftpc->cmd;
            break;
        }
        /* convert character to EBCDIC and store in buffer */
        ftpc->cmd[ftpc->cmdlen++] = asc2ebc[c];
    } while (ftpc->cmdlen < (sizeof(ftpc->cmd)-1));

    ftpc->cmd[ftpc->cmdlen] = 0;

quit:
#if 0
    wtof("%s \"%s\"", __func__, ptr?ptr:"(null)");
#endif
    return ptr;
}
