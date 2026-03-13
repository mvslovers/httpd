/* FTPDRMD.C */
#include "httpd.h"

int
ftpdrmd(FTPC *ftpc)
{
    int         rc          = 0;
    int         i           = 0;
    char        *p          = 0;
    char        buf[256]    = {0};
    const char *cwd         = ftpc->cwd;

    /* get parameters */
    p = strtok(ftpc->cmd, " ");
    if (p) p = strtok(NULL, " ");

    /* are we doing file system or dataset processing? */
    if (!ftpc->ufs) goto do_dataset;
    if (ftpc->flags & FTPC_FLAG_CWDDS) goto do_dataset;
    if (ftpc->flags & FTPC_FLAG_CWDPDS) goto do_dataset;

    /* file system it is */
    cwd = ftpc->ufs->cwd.path;

    if (p) {
        /* skip quotes */
        while(*p=='\'') p++;
        strtok(p, "\'");

        rc = ufs_rmdir(ftpc->ufs, p);
        if (rc) {
            ftpcmsg(ftpc, "550-%s", strerror(rc));
            ftpcmsg(ftpc, "550 Requested action not taken");
            rc = 0;
        }
        else {
            ftpcmsg(ftpc, "250 directory \"%s\" removed", p);
        }
        goto quit;
    }

do_dataset:
    if (!p) {
        ftpcmsg(ftpc, "501 Syntax error in parameters");
        goto quit;
    }

    /* we'll add a call to remove() at some point, but for now we'll
    ** disallow any deletes just to be a little bit safer.
    */
#if 1
    ftpcmsg(ftpc, "550 Requested action not taken");
#else
    ftpcmsg(ftpc, "250 directory removed");
#endif

quit:
    return 0;
}
