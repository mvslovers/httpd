/* FTPDNOOP.C */
#include "httpd.h"

int
ftpdnoop(FTPC *ftpc)
{
    ftpcmsg(ftpc, "200 %-4.4s command okay", ftpc->cmd);

    return 0;
}
