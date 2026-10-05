/* httpstrt.c - HTTPD's own startup step, run by libc370's __start() through
   the __premain() hook before the standard streams are opened and before
   main().  Everything else (environment, time zone, parameter list, argv)
   is libc370's startup.

   HTTPD refuses SYSPRINT, SYSTERM and SYSIN: those are the ddnames the
   default streams open, so their presence means the PROC is not HTTPD's.
   The streams are HTTPDOUT, HTTPDERR and HTTPDIN (else 'NULLFILE') instead.

   The startup refers to __premain weakly, and a weak reference does not
   pull an object out of an archive -- so this source is listed in the HTTPD
   module's own sources, not left to autocall. */
#include "stdio.h"
#include "stdlib.h"
#include <mvs/crt.h>                /* __premain()                      */
#include <mvs/wto.h>                /* wtof()                           */
#include "httpdmsg.h"               /* MSG_DD_* operator messages       */

int
__premain(char *parm, char *pgmname, void **pgmr1)
{
    int         errors  = 0;
    FILE        *fp;
    (void)parm;
    (void)pgmname;
    (void)pgmr1;

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

    if (errors) return EXIT_FAILURE;

    /* open our HTTPD datasets */
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
        /* leave no stream pointing at a closed FILE */
        if (stdin)  { fclose(stdin);  stdin  = NULL; }
        if (stderr) { fclose(stderr); stderr = NULL; }
        if (stdout) { fclose(stdout); stdout = NULL; }
        return EXIT_FAILURE;
    }

    return 0;
}
