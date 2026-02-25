/* FTPDUSER.C */
#include "httpd.h"

int
ftpduser(FTPC *ftpc)
{
    int     len = 0;
    char    *p;

    p = strtok(ftpc->cmd, " ");
    if (p) p = strtok(NULL, "");

    if (p) len = strlen(p);
    if (len < 1 || len > 8) {
        if (p && strncmpi("anonymous", p, 9)==0) {
            wtof("HTTPD200I user 'anonymous' logged into FTP");
            if (ftpc->acee) {
                /* release previous security invironment */
                racf_logout(&ftpc->acee);
                ftpc->acee = 0;
            }
            ftpcmsg(ftpc, "230 User logged in, proceed");
            ftpc->state = FTPSTATE_IDLE;
            ftpc->flags &= ~FTPC_FLAG_CWDPDS;
            ftpc->flags &= ~FTPC_FLAG_CWDDS;
            if (ftpc->ufs) {
                ufs_set_acee(ftpc->ufs, ftpc->acee);    /* clear ACEE for anonymous access */
                ufs_chgdir(ftpc->ufs, "/");             /* CD to root "/" */
            }
            else {
                strcpy(ftpc->cwd, "SYS2");              /* Any HLQ will do */
                ftpc->flags |= FTPC_FLAG_CWDDS;         /* dataset level */
            }
            goto quit;
        }
        ftpcmsg(ftpc, "501 Syntax error in parameters");
        goto quit;
    }

    memcpy(ftpc->cwd, p, len);
    ftpc->cwd[len] = 0;
    for(p=ftpc->cwd; *p; p++) {
        *p = toupper(*p);
    }
    ftpc->flags = FTPC_FLAG_CWDDS;
    ftpc->state = FTPSTATE_LOGIN;

    ftpcmsg(ftpc, "331 User name okay, need password");

quit:
    return 0;
}

