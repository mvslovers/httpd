/* FTPDPC.C
** Process client
*/
#include "httpd.h"

typedef struct {
    char    *word;                  /* keyword name         */
    int     len;                    /* keyword length       */
    int     (*func)(FTPC *ftpc);    /* function pointer     */
} FTP_CMDS;

static FTP_CMDS login_cmds[] = {
    {"noop", 4, ftpdnoop},  /* No operation--send positive reply            */
    {"pass", 4, ftpdpass},  /* PASS xxxxxxxx                                */
    {"quit", 4, ftpdquit},  /* Quit                                         */
    {"user", 4, ftpduser},  /* USER xxxxxxxx                                */
};
#define MAX_LOGIN_CMDS (sizeof(login_cmds) / sizeof(FTP_CMDS))

static FTP_CMDS cmds[] = {
    {"abor", 4, ftpdabor},  /* Abort current operation                      */
#if 0
    {"acct", 4, ftpdacct},  /* ACCT xxxxxxxx                                */
    {"allo", 4, ftpdallo},  /* Allocate space for file                      */
    {"appe", 4, ftpdappe},  /* Append xxxxxxx with data -- UPLOAD APPEND    */
    {"cdup", 4, ftpdcdup},  /* Change Working Directory to parent           */
#endif
    {"cwd",  3, ftpdcwd},   /* Change Working Directory xxxxxxxx            */
    {"dele", 4, ftpddele},  /* Delete xxxxxxx from file system              */
#if 0
    {"help", 4, ftpdhelp},  /* Print help text                              */
#endif
    {"list", 4, ftpdlist},  /* Directory List via data connection           */
    {"mkd",  3, ftpdmkd},   /* Make Directory                               */
#if 0
    {"mode", 4, ftpdmode},  /* Mode {s,b,c} -- STREAM, BLOCK, COMPRESSED    */
    {"nlst", 4, ftpdnlst},  /* Name List via data connection                */
#endif
    {"noop", 4, ftpdnoop},  /* No operation--send positive reply            */
    {"opts", 4, 0       },  /* valid command, but we don't implement it     */
    {"pass", 4, ftpdpass},  /* PASS xxxxxxxx                                */
    {"pasv", 4, ftpdpasv},  /* Returns port of server for data transfer     */
    {"port", 4, ftpdport},  /* Port h1,h2,h3,h4,p1,p2 of client for data    */
    {"pwd",  3, ftpdpwd},   /* Print name of current working directory      */
    {"quit", 4, ftpdquit},  /* Quit                                         */
#if 0
    {"rein", 4, ftpdrein},  /* Reinitialize                                 */
#endif
    {"retr", 4, ftpdretr},  /* Retrieve xxxxxxxx -- DOWNLOAD filename       */
    {"rmd",  3, ftpdrmd},   /* Remove Directory                             */
#if 0
    {"rnfr", 4, ftpdrnfr},  /* Rename from xxxxxxxx                         */
    {"rnft", 4, ftpdrnft},  /* Rename to xxxxxxxx                           */
    {"site", 4, ftpdsite},  /* Site specific options                        */
    {"smnt", 4, ftpdsmnt},  /* Structure Mount--mount different file system */
    {"stat", 4, ftpdstat},  /* Print FTP server information                 */
#endif
    {"stor", 4, ftpdstor},  /* Store xxxxxxxx in file system -- UPLOAD      */
#if 0
    {"stou", 4, ftpdstou},  /* Store unique -- Server chooses filename      */
    {"stru", 4, ftpdstru},  /* Structure {f,r,p} -- FILE, RECORD, PAGE      */
#endif
    {"syst", 4, ftpdsyst},  /* Print operating system type                  */
    {"type", 4, ftpdtype},  /* Type {a,e,i,l} -- ASCII, EBCDIC, IMAGE, LOCAL*/
    {"user", 4, ftpduser},  /* USER xxxxxxxx                                */
    /* synonyms for the 3 letter versions of these commands */
    {"xmkd", 4, ftpdmkd},   /* Make Directory                               */
    {"xpwd", 4, ftpdpwd},   /* Print name of current working directory      */
    {"xrmd", 4, ftpdrmd},   /* Remove Directory                             */
};
#define MAX_CMDS (sizeof(cmds) / sizeof(FTP_CMDS))

static int  find(const void *key, const void *item);

int ftpd_process_client(FTPC *ftpc)
{
    int         rc      = 0;
    int         len     = strlen(ftpc->cmd);
    FTPD        *ftpd   = ftpc->ftpd;
    char        *ptr;
    FTP_CMDS    *cmd;
#if 0
    wtof("Enter %s", __func__);
	wto_traceback(NULL);
#endif
    if (!ftpc) goto quit;

    switch(ftpc->state) {
    case FTPSTATE_INITIAL:
        /* the FTP client just connected to us, so send a welcome and wait for commands */
        ftpcmsg(ftpc, "220-Welcome to the HTTPD FTP server on port %d", ftpc->ftpd->port);
        ftpcmsg(ftpc, "220 Anonymous login allowed. Login is required.");
        if (ftpd->sys) {
            /* initialize for Unix File System access */
            if (ftpc->ufs) ufsfree(&ftpc->ufs);
            ftpc->ufs = ufsnew();
        }
        ftpc->state = FTPSTATE_LOGIN;
        break;

    case FTPSTATE_LOGIN:
        ptr = ftpcgets(ftpc);
        if (!ptr) {
            /* wtof("%s client connection terminated", __func__); */
            ftpc->state = FTPSTATE_TERMINATE;
            break;
        }

        /* look for valid command */
        cmd = bsearch(ftpc->cmd, login_cmds, MAX_LOGIN_CMDS, sizeof(FTP_CMDS), find);
        if (!cmd) {
            /* command not found */
            if (len > 20) strcpy(&ftpc->cmd[20], "...");
            ftpcmsg(ftpc, "500 Syntax error, command \"%s\" not recognized", ftpc->cmd);
            break;  /* not fatal, we'll get called again to process the next command */
        }

        /* command found */
        rc = cmd->func(ftpc);
        break;

    case FTPSTATE_IDLE:
        ptr = ftpcgets(ftpc);
        if (!ptr) {
            /* wtof("%s client connection terminated", __func__); */
            ftpc->state = FTPSTATE_TERMINATE;
            break;
        }

        /* look for valid command */
        cmd = bsearch(ftpc->cmd, cmds, MAX_CMDS, sizeof(FTP_CMDS), find);
        if (!cmd) {
            /* command not found */
            if (len > 20) strcpy(&ftpc->cmd[20], "...");
            ftpcmsg(ftpc, "500 Syntax error, command \"%s\" not recognized", ftpc->cmd);
            break;  /* not fatal, we'll get called again to process the next command */
        }

        /* command found */
        if (ftpc->cmd[cmd->len]!=' ' && ftpc->cmd[cmd->len]!=0) {
            /* only a partial match */
            if (len > 20) strcpy(&ftpc->cmd[20], "...");
            ftpcmsg(ftpc, "500 Syntax error, command \"%s\" not recognized", ftpc->cmd);
            break;  /* not fatal, we'll get called again to process the next command */
        }

        if (!cmd->func) {
            ftpcmsg(ftpc, "502 Command not implemented");
            break;
        }

        /* call the command function */
        rc = cmd->func(ftpc);

        /* sync file system buffers to disk */
        if (ftpc->ufs) ufs_sync(ftpc->ufs);
        break;

    case FTPSTATE_TRANSFER:
        if (ftpc->flags & FTPC_FLAG_SEND) {
            ftpcsend(ftpc);
        }
        else if (ftpc->flags & FTPC_FLAG_RECV) {
            ftpcrecv(ftpc);
        }
        else {
            /* abort transfer */
            ftpdabor(ftpc);
            ftpc->state = FTPSTATE_IDLE;
        }
        break;

    case FTPSTATE_TERMINATE:
        if (ftpd->sys) {
            if (ftpc->ufs) {
                /* sync file system buffers to disk */
                ufs_sync(ftpc->ufs);

                /* terminate Unix File System access */
                ufsfree(&ftpc->ufs);
            }
        }
        break;
    }


quit:
#if 0
    wtof("Exit %s", __func__);
#endif
    return rc;
}

static int
find(const void *vitem, const void *vkey)
{
    FTP_CMDS    *item   = (FTP_CMDS *)vitem;
    char        *key    = (char *)vkey;
#if 0
    wtodumpf(key, 16, "key");
    wtodumpf(item->word, item->len, "item->word");
#endif
    /* compare key with keyword ignoring case */
    return(strncmpi(item->word, key, item->len));
}
