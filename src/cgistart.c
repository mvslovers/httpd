/*********************************************************************/
/*                                                                   */
/*  cgistart.c - common gateway interface startup/termination code   */
/*                                                                   */
/*********************************************************************/

/* Callers of C program may be from TSO or Batch.
   TSO supplies the args in a struct like:
   00 length of args first byte
   01 length of args second byte
   02 always zero
   03 length of program name in args starting in byte 04
   04 args....

   For TSO, the length of args includes the 4 byte prefix area size.

   Batch supplies the args in a struct like:
   00 length of args first byte
   01 length of args second byte
   02 args....

   For Batch, the args are those specified in the PARM='...' for
   the EXEC statement in the JCL.
*/

#include <stdlib.h>
#include <string.h>
#include <clibgrt.h>
#include <clibppa.h>
#include <clibary.h>
#include <clibenv.h>
#include "httpcgi.h"

#define MAXPARMS 50 /* maximum number of arguments we can handle */

extern int main(int argc, char **argv);
extern void __exita(int status);

/* initialize to 0 to prevent linkage editor from trying to resolve httpx */
HTTPX *httpx = 0;

/* we want to use the httpx pointer in the httpd struct for the various
   http_xxx functions, so we define httpx to do just that
*/
#define httpx   http_get_httpx(httpd)

/* not_a_server_module() - say why this module cannot run, then end the step.
**
** Everything below that a server module needs comes from HTTPD: the server
** context through the GRT, and the DDs the started task allocates.  Reaching
** __start() without them means somebody ran the module by itself -- a
** `CALL 'HTTPD.LINKLIB(HTTPDSL)'` from TSO, or an EXEC PGM= in a batch job.
** That used to end in a bare __exita(): a silent RC 12 with no message
** anywhere, so the person who made the mistake was told nothing (issue #141).
**
** Reporting it needs an output channel, which is the very thing that may be
** missing -- so open one.  fopen() on a name starting with '*' goes to
** libc370's __fpstar(), which allocates the destination dynamically via SVC 99
** and picks it by environment: the terminal under TSO foreground, a SYSOUT
** dataset in batch and in TSO background.  Measured in all three (#141).
**
** Do NOT branch on GRTFLAG1_TSO to choose a destination.  That flag is derived
** from the shape of the parameter list a few lines below, not from the
** environment, and it comes back clear for a parameterless CALL in every one
** of the three -- including TSO foreground.  Letting __fpstar() decide is both
** correct and less code.
*/
static void
not_a_server_module(const char *pgmname, const char *missing)
{
    FILE    *own    = NULL;         /* a channel we had to open ourselves */
    FILE    *msg    = stdout;       /* prefer one that already works      */

    if (!msg) {
        own = fopen("*SYSPRINT", "w");
        msg = own;
    }

    if (msg) {
        fprintf(msg, "%-8.8s is an HTTPD server module and cannot run on its"
                     " own.\n", pgmname ? pgmname : "This module");
        fprintf(msg, "Start it through the HTTPD server, which passes the"
                     " server context and\n");
        fprintf(msg, "allocates the files it needs.\n");
        fprintf(msg, "Missing here: %s\n", missing);

        if (own) fclose(own);
        else     fflush(msg);
    }

    __exita(EXIT_FAILURE);
}

/* we want the internal label for __start as "cgistart" for use with dumps */
__asm__("\n&FUNC    SETC 'cgistart'");
int
__start(char *p, char *pgmname, int tsojbid, void **pgmr1)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = NULL;
    HTTPC       *httpc  = NULL;
    int         x;
    int         argc;
    unsigned    u;
    char        *argv[MAXPARMS + 1];
    int         rc;
    int         parmLen;
    int         progLen = 0;
    char        parmbuf[310];

    /* we're going to process the callers parameter list first so we
       can decide is we'll bypass the opens for the permanent datasets.
    */
    if (pgmr1) {
        /* save the program parameter list values (max 10 pointers)
           note: the first pointer is always the raw EXEC PGM=...,PARM
           or CPPL (TSO) address.
        */
        for(x=0; x < 10; x++) {
            u = (unsigned)pgmr1[x];
            /* add to array of pointers from caller */
            arrayadd(&grt->grtptrs, (void*)(u&0x7FFFFFFF));

            if (u) {
                if (strcmp((void*)u, HTTPD_EYE)==0) {
                    /* this is a HTTPD pointer */
                    httpd        = (HTTPD*)(u&0x7FFFFFFF);
                    grt->grtapp1 = httpd;
                }
                if (strcmp((void*)u, HTTPC_EYE)==0) {
                    /* this is a HTTPC pointer */
                    httpc        = (HTTPC*)(u&0x7FFFFFFF);
                    grt->grtapp2 = httpc;
                }
            }

            if (u&0x80000000) break; /* end of VL style address list */
        }
    }

    /* if we got a HTTPC and we didn't get a HTTPD,
       then use the HTTPD from the HTTPC handle */
    if (httpc && !httpd) {
        httpd = httpc->httpd;
        grt->grtapp1 = httpd;
    }

    /* GRTFLAG1_TSO records the SHAPE OF THE PARAMETER LIST, not the
       environment, despite what its name and libc370's clibgrt.h comment
       suggest.  It says "byte 2 is zero, so bytes 0-3 are a TSO command-style
       prefix and the parm starts at byte 4" -- which is what the argv[0]
       parsing further down needs, and all it is used for here.

       It is NOT a usable "am I under TSO" test.  Measured (issue #141) on a
       parameterless CALL it comes back clear in batch, in TSO background under
       IKJEFT01, and in TSO foreground alike, because parmLen is then 0 and the
       condition never fires.  The PPA flags are the environment ones --
       PPAFLAG_TSOFG for foreground, TSOFG|TSOBG for "TSO at all" (and see
       libc370 #72 before trusting those either).

       The old comment here claimed this determines how the permanent files are
       opened.  It does not, and did not: the fopen() calls below never looked
       at it. */
    parmLen = ((unsigned int)p[0] << 8) | (unsigned int)p[1];
    if ((parmLen > 0) && (p[2] == 0)) {
        grt->grtflag1 |= GRTFLAG1_TSO;
        progLen = (unsigned int)p[3];
    }

    /* The server context is what a module cannot substitute for, so test it
       first: without it no set of DDs would make the module work, and the
       message should name the real problem rather than the first DD to fail. */
    if (!httpd && !httpc) {
        not_a_server_module(pgmname, "the HTTPD server context (GRT)");
    }

    stdout = fopen("DD:HTTPDOUT", "w");
    if (!stdout && !httpc) not_a_server_module(pgmname, "DD:HTTPDOUT");

    stderr = fopen("DD:HTTPDERR", "w");
    if (!stderr && !httpc) not_a_server_module(pgmname, "DD:HTTPDERR");

    stdin = fopen("DD:HTTPDIN", "r");
    if (!stdin) stdin = fopen("'NULLFILE'", "r");
    if (!stdin && !httpc) not_a_server_module(pgmname, "DD:HTTPDIN");

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
#if 0
	rc = 0;
	printf("HTTP/1.0 200 OK\n\n");
	printf("{ \"%s\" }\n", "cgistart");
#else
    rc = main(argc, argv);
#endif
    __exit(rc);
    return (rc);
}

int
printf(const char *format, ...)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    va_list arg;
    int ret;

#if 0
    wtof("printf in cgistart httpd=%08X, httpc=%08X, httpx=%08X, grt=%08X, ppa=%08X",
         httpd, httpc, httpx, grt, ppa);
#endif

    va_start(arg, format);
    if (httpc && httpx && httpx->http_printv) {
#if 0
		vwtof(format, arg);
#endif
        ret = (httpx->http_printv)(httpc, format, arg);
    }
    else {
        ret = vfprintf(stdout, format, arg);
    }
    va_end(arg);

    return (ret);
}
