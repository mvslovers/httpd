/* FTPDPASS.C */
#include "httpd.h"

int
ftpdpass(FTPC *ftpc)
{
    int     rc      = 0;
    char    *p;
    ACEE    *acee;

    if (ftpc->state != FTPSTATE_LOGIN || !ftpc->cwd[0]) {
        ftpcmsg(ftpc, "503 Bad sequence of commands");
        goto quit;
    }

    if (ftpc->acee) {
        /* release previous security invironment */
        racf_logout(&ftpc->acee);
        ftpc->acee = 0;
    }

    p = strtok(ftpc->cmd, " ");
    if (p) p = strtok(NULL, " ");
    if (!p) goto reset;

    /* login user */
    acee = racf_login(ftpc->cwd, p, 0, &rc);
    if (!acee) {
        wtof("HTTPD201E Invalid login attempt for userid '%s'", ftpc->cwd);
        goto reset;
    }

    /* login success */
    wtof("HTTPD200I FTP login by user '%s' success", ftpc->cwd);
    ftpc->acee      = acee;

    /* set state to IDLE */
    ftpc->state     = FTPSTATE_IDLE;

    ftpc->flags     = 0;

    if (ftpc->ufs) {
        /* wtof("%s setting acee %08X", __func__, ftpc->acee); */
        ufs_set_acee(ftpc->ufs, ftpc->acee);
        /* try to change dir to user name */
        /* wtof("%s ufs_chgdir(%s)", __func__, ftpc->cwd); */
        if (ufs_chgdir(ftpc->ufs, ftpc->cwd)!=0) {
            /* that didn't work, so CD to root */
            /* wtof("%s ufs_chgdir(%s)", __func__, "/"); */
            ufs_chgdir(ftpc->ufs, "/");         /* CD to root "/" */
        }
    }
    else {
        /* wtof("%s No ftpc->ufs handle", __func__); */
        ftpc->flags |= FTPC_FLAG_CWDDS;         /* dataset level */
    }

    ftpcmsg(ftpc, "230 User logged in, proceed");
    goto quit;

reset:
    ftpc->state     = FTPSTATE_LOGIN;
    ftpc->cwd[0]    = 0;
    ftpcmsg(ftpc, "530 Not logged in");

quit:
    return 0;
}
