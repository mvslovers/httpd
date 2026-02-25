/* HTTPAUTH.C - Authorize our environment dynamically
*/
#include "httpd.h"

static void authorize(void);

int
httpauth(const char *id)
{
    int     rc      = 0;
    int     steplib = 0;

    if (!id) id = "HTTPD";

    /* check for APF authorization */
    __asm__("TESTAUTH\tFCTN=1\n\tST\t15,0(,%0)" : : "r"(&rc));
    if (rc==0) {
        wtof("HTTPD010I %s is APF authorized", id);
    }
    else {
        /* get APF authorization */
        try(authorize,0);
        __asm__("TESTAUTH\tFCTN=1\n\tST\t15,0(,%0)" : : "r"(&rc));
        if (rc==0) {
            wtof("HTTPD011I %s was APF authorized via SVC 244", id);
        }
        else {
            wtof("HTTPD012E %s unable to dynamically obtain APF authorization",
                id);
            rc = -1;
            goto quit;
        }
    }

    __asm__("\n"
"         MODESET KEY=ZERO,MODE=SUP\n"
"         ICM   1,15,PSATOLD-PSA(0) Get our TCB address\n"
"         ICM   1,15,TCBJLB-TCB(1)  Get STEPLIB DCB\n"
"         BZ    APFQUIT             No STEPLIB\n"
"         USING IHADCB,1            DECLARE IT\n"
"         L     1,DCBDEBAD          LOAD DEB FOR STEPLIB\n"
"         N     1,=X'00FFFFFF'      FIX HIGH BYTE\n"
"         USING DEBBASIC,1\n"
"         OI    DEBFLGS1,DEBAPFIN   TURN ON APF LIBRARY BIT\n"
"         DROP  1\n"
"APFQUIT  DS    0H\n"
"         ST    1,%0\n"
"         MODESET KEY=NZERO,MODE=PROB" : "=m"(steplib));

    if (steplib) {
        wtof("HTTPD013I STEPLIB is now APF authorized");
    }

quit:
    return rc;
}

static void authorize(void)
{
    __asm__(
        "SR\t0,0\n\t"
        "LA\t1,1\n\t"
        "SVC\t244"
    );
}

__asm__("PRINT NOGEN");
__asm__("IHAPSA ,            MAP LOW STORAGE");
__asm__("CVT DSECT=YES");
__asm__("IKJTCB DSECT=YES");
__asm__("DCBD DSORG=PO,DEVD=DA");
__asm__("IEZDEB");
__asm__("PRINT GEN");
__asm__("CSECT");
