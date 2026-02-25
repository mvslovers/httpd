/* FTPDTYPE.C */
#include "httpd.h"

int
ftpdtype(FTPC *ftpc)
{
    char    *p;

    p = strtok(ftpc->cmd, " ");
    if (p) p = strtok(NULL, "");

    switch(p ? *p : 0) {
    case 'a':
    case 'A':
        ftpc->type = FTPC_TYPE_ASCII;
        goto okay;
    case 'e':
    case 'E':
        ftpc->type = FTPC_TYPE_EBCDIC;
        goto okay;
    case 'i':
    case 'I':
        ftpc->type = FTPC_TYPE_IMAGE;
        goto okay;
    case 'l':
    case 'L':
        ftpc->type = FTPC_TYPE_LOCAL;
        goto okay;
    default:
        ftpcmsg(ftpc, "501 Syntax error in parameters");
        goto quit;
    }

okay:
    ftpcmsg(ftpc, "200 TYPE command okay");

quit:
    return 0;
}
