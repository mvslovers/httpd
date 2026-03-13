/* FTPDPWD.C */
#include "httpd.h"

int
ftpdpwd(FTPC *ftpc)
{
    const char *cwd = ftpc->cwd;

    if (!ftpc->ufs) goto do_send;
    if (ftpc->flags & FTPC_FLAG_CWDDS) goto do_send;
    if (ftpc->flags & FTPC_FLAG_CWDPDS) goto do_send;

    cwd = ftpc->ufs->cwd.path;

do_send:
    ftpcmsg(ftpc, "257 \"%s\" current directory", cwd);

quit:
    return 0;
}
