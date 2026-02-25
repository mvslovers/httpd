/* FTPDPORT.C */
#include "httpd.h"

int
ftpdport(FTPC *ftpc)
{
    unsigned    addr    = 0;
    unsigned    port    = 0;
    char        *p;

    if (ftpc->state == FTPSTATE_INITIAL || ftpc->state == FTPSTATE_LOGIN) {
        ftpcmsg(ftpc, "530 Not logged in");
        goto quit;
    }

    /* parse "PORT h1,h2,h3,h4,p1,p2" */

    p = strtok(ftpc->cmd, " ");     /* "PORT" */
    if (p) p = strtok(NULL, " ");   /* "h1,h2,h3,h4,p1,p2" */
    if (p) p = strtok(p, ",");      /* "h1" */
    if (p) {
        /* "h1" */
        addr = atoi(p);
        p = strtok(NULL, ",");      /* "h2" */
    }
    if (p) {
        /* "h2" */
        addr = addr << 8;
        addr += atoi(p);
        p = strtok(NULL, ",");      /* "h3" */
    }
    if (p) {
        /* "h3" */
        addr = addr << 8;
        addr += atoi(p);
        p = strtok(NULL, ",");      /* "h4" */
    }
    if (p) {
        /* "h4" */
        addr = addr << 8;
        addr += atoi(p);
        p = strtok(NULL, ",");      /* "p1" */
    }
    if (p) {
        /* "p1" */
        port = atoi(p);
        p = strtok(NULL, ",");      /* "p2" */
    }
    if (p) {
        /* "p2" */
        port = port << 8;
        port += atoi(p);
    }

    if (addr == 0 || port == 0) {
        ftpcmsg(ftpc, "501 Syntax error in parameters");
        goto quit;
    }

    ftpc->data_addr = addr;
    ftpc->data_port = port;
    if (ftpc->data_socket >= 0) {
        closesocket(ftpc->data_socket);
        ftpc->data_socket = -1;
    }
    ftpc->flags &= 0xFF - FTPC_FLAG_PASSIVE;

    ftpcmsg(ftpc,"200 PORT command okay");

quit:
    return 0;
}

