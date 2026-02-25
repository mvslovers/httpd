/*#define LIB_STDIO*/
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "stddef.h"
#include "clibcrt.h"

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
    int         progLen;
    char        parmbuf[310];

    /* need to know if this is a TSO environment straight away
       because it determines how the permanent files will be
       opened */
    parmLen = ((unsigned int)p[0] << 8) | (unsigned int)p[1];
    if ((parmLen > 0) && (p[2] == 0)) {
        grt->grtflag1 |= GRTFLAG1_TSO;
        progLen = (unsigned int)p[3];
    }

    /* Check for SYSPRINT DD allocation */
    fp = fopen("DD:SYSPRINT", "w");
    if (fp) {
        errors++;
        wtof("*******************************************************");
        wtof("*   SYSPRINT DD not allowed for HTTPD program.        *");
        wtof("*                                                     *");
        wtof("*   HTTPD uses HTTPDOUT DD for its STDOUT dataset.    *");
        wtof("*******************************************************");
        fclose(fp);
    }

    /* Check for SYSTERM DD allocation */
    fp = fopen("DD:SYSTERM", "w");
    if (fp) {
        errors++;
        wtof("*******************************************************");
        wtof("*   SYSTERM DD not allowed for HTTPD program.         *");
        wtof("*                                                     *");
        wtof("*   HTTPD uses HTTPDERR DD for its STDERR dataset.    *");
        wtof("*******************************************************");
        fclose(fp);
    }

    /* Check for SYSIN DD allocation */
    fp = fopen("DD:SYSIN", "r");
    if (fp) {
        errors++;
        wtof("*******************************************************");
        wtof("*   SYSIN DD not allowed for HTTPD program.           *");
        wtof("*                                                     *");
        wtof("*   HTTPD uses HTTPDIN DD for its STDIN dataset.      *");
        wtof("*******************************************************");
        fclose(fp);
    }

    if (errors) __exita(EXIT_FAILURE);

    /* open our HTTPD datasest */
    stdout = fopen("DD:HTTPDOUT", "w");
    if (!stdout) {
        errors++;
        wtof("*******************************************************");
        wtof("*   HTTPDOUT DD not defined, unable to open STDOUT.   *");
        wtof("*******************************************************");
    }

    stderr = fopen("DD:HTTPDERR", "w");
    if (!stderr) {
        errors++;
        wtof("*******************************************************");
        wtof("*   HTTPDERR DD not defined. unable to open STDERR.   *");
        wtof("*******************************************************");
    }

    stdin = fopen("DD:HTTPDIN", "r");
    if (!stdin) stdin = fopen("'NULLFILE'", "r");
    if (!stdin) {
        errors++;
        wtof("*******************************************************");
        wtof("*   HTTPDIN DD not defined, unable to open STDIN.     *");
        wtof("*******************************************************");
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
