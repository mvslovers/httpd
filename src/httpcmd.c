/* HTTPCMD.C - issue command via diagnose 8
*/
#include "httpd.h"

static int sendcmd(const char *cmd, int cmdlen, char *resp, int respmax);

int
httpcmd(const char *cmd, char *resp, int respmax)
{
    int     rc      = 0;
    int     cmdlen  = strlen(cmd);
    char    *p;

    memset(resp, 0, respmax);

    rc = try(sendcmd, cmd, cmdlen, resp, respmax);

    resp[respmax-1]=0;
    for(p=resp; *p; p++) {
        if (*p == 0x25) {
            *p = '\n';
        }
    }

    return rc;
}

static int sendcmd(const char *cmd, int cmdlen, char *resp, int respmax)
{
    int     rc;

    cmdlen = (int)(0x40000000 | cmdlen);

    __asm__("MODESET MODE=SUP,KEY=ZERO");

    __asm__("\n"
"         L     2,%1           Command buffer\n"
"         L     3,%3           Response buffer\n"
"         L     4,%2           Command length\n"
"         L     5,%4           Response length\n"
"         LRA   2,0(2)\n"
"         LRA   3,0(3)\n"
"         DC    X'83240008'    Issue command with DIAG8\n"
"         ST    4,%0           Save result\n"
"*" : "=m"(rc) : "m"(cmd), "m"(cmdlen), "m"(resp), "m"(respmax)
    : "2", "3", "4", "5");

    __asm__("MODESET MODE=PROB,KEY=NZERO");

    return rc;
}
