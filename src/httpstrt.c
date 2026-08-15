/*#define LIB_STDIO*/
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stddef.h"
#include "time.h"
#include "clibcrt.h"
#include "clibenv.h"                /* loadenv()                        */
#include "clibwto.h"                /* wtof()                           */
#include "httpdmsg.h"               /* MSG_DD_* operator messages       */

#define MAXPARMS 50 /* maximum number of arguments we can handle */

extern int main(int argc, char **argv);
extern void __exita(int status);

int
__start(char *p, char *pgmname, int tsojbid, void **pgmr1)
{
    CLIBGRT     *grt    = __grtget();
    int         errors  = 0;
    FILE        *fp;
    int         x;
    int         argc;
    unsigned    u;
    char        *argv[MAXPARMS + 1];
    int         rc;
    int         parmLen;
    int         progLen = 0;
    char        parmbuf[310];

    /* GRTFLAG1_TSO records the SHAPE OF THE PARAMETER LIST, not the
       environment -- see the longer note at the same point in cgistart.c.  It
       means "bytes 0-3 are a TSO command-style prefix, the parm starts at byte
       4", which is what the argv[0] parsing further down needs and all it is
       used for.  Measured clear in batch, TSO background and TSO foreground on
       a parameterless call (issue #141), so it cannot answer "am I under TSO".

       The old comment claimed this determines how the permanent files are
       opened.  It does not; the fopen() calls below never looked at it. */
    parmLen = ((unsigned int)p[0] << 8) | (unsigned int)p[1];
    if ((parmLen > 0) && (p[2] == 0)) {
        grt->grtflag1 |= GRTFLAG1_TSO;
        progLen = (unsigned int)p[3];
    }

    /* Check for SYSPRINT DD allocation */
    fp = fopen("DD:SYSPRINT", "w");
    if (fp) {
        errors++;
        wtof(MSG_DD_SYSPRINT);
        fclose(fp);
    }

    /* Check for SYSTERM DD allocation */
    fp = fopen("DD:SYSTERM", "w");
    if (fp) {
        errors++;
        wtof(MSG_DD_SYSTERM);
        fclose(fp);
    }

    /* Check for SYSIN DD allocation */
    fp = fopen("DD:SYSIN", "r");
    if (fp) {
        errors++;
        wtof(MSG_DD_SYSIN);
        fclose(fp);
    }

    if (errors) __exita(EXIT_FAILURE);

    /* open our HTTPD datasest */
    stdout = fopen("DD:HTTPDOUT", "w");
    if (!stdout) {
        errors++;
        wtof(MSG_DD_NO_STDOUT);
    }

    stderr = fopen("DD:HTTPDERR", "w");
    if (!stderr) {
        errors++;
        wtof(MSG_DD_NO_STDERR);
    }

    stdin = fopen("DD:HTTPDIN", "r");
    if (!stdin) stdin = fopen("'NULLFILE'", "r");
    if (!stdin) {
        errors++;
        wtof(MSG_DD_NO_STDIN);
    }
    
    if (errors) {
        if (stdin)  fclose(stdin);
        if (stderr) fclose(stderr);
        if (stdout) fclose(stdout);
        __exita(EXIT_FAILURE);
    }
    
    /* load any environment variables */
    if (loadenv("dd:SYSENV")) {
        /* no SYSENV DD, try ENVIRON DD */
        loadenv("dd:ENVIRON");
    }

    /* initialize time zone offset for this thread */
    tzset();

    if (parmLen >= sizeof(parmbuf) - 2) {
        parmLen = sizeof(parmbuf) - 1 - 2;
    }
    if (parmLen < 0) parmLen = 0;

    /* We copy the parameter into our own area because
       the caller hasn't necessarily allocated room for
       a terminating NUL, nor is it necessarily correct
       to clobber the caller's area with NULs. */
    memset(parmbuf, 0, sizeof(parmbuf));
    if (grt->grtflag1 & GRTFLAG1_TSO) {
        parmLen -= 4;
        memcpy(parmbuf, p+4, parmLen);
    }
    else {
        memcpy(parmbuf, p+2, parmLen);
    }
    p = parmbuf;

    if (pgmr1) {
        /* save the program parameter list values (max 10 pointers)
           note: the first pointer is always the raw EXEC PGM=...,PARM
           or CPPL (TSO) address.
        */
        for(x=0; x < 10; x++) {
            u = (unsigned)pgmr1[x];
            /* add to array of pointers from caller */
            arrayadd(&grt->grtptrs, (void*)(u&0x7FFFFFFF));
            if (u&0x80000000) break; /* end of VL style address list */
        }
    }

    if (grt->grtflag1 & GRTFLAG1_TSO) {
        argv[0] = p;
        for(x=0;x<=progLen;x++) {
            if (argv[0][x]==' ') {
                argv[0][x]=0;
                break;
            }
        }
        p += progLen;
    }
    else {       /* batch or tso "call" */
        argv[0] = pgmname;
        pgmname[8] = '\0';
        pgmname = strchr(pgmname, ' ');
        if (pgmname) *pgmname = '\0';
    }

    while (*p == ' ') p++;

    x = 1;
    if (*p) {
        while(x < MAXPARMS) {
            char srch = ' ';

            if (*p == '"') {
                p++;
                srch = '"';
            }
            argv[x++] = p;
            p = strchr(p, srch);
            if (!p) break;

            *p = '\0';
            p++;
            /* skip trailing blanks */
            while (*p == ' ') p++;
            if (*p == '\0') break;
        }
    }
    argv[x] = NULL;
    argc = x;

    rc = main(argc, argv);

    __exit(rc);
    return (rc);
}
