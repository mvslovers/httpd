/* FTPDSYST.C */
#include "httpd.h"

int
ftpdsyst(FTPC *ftpc)
{
    ftpcmsg(ftpc, "215 UNIX Type: L8");
    return 0;
}
