/* TSTLNKM.C
** LINK target for TSTLINK -- a module that runs to completion and returns a
** NEGATIVE rc of its own.  Not a test in its own right; TSTLINK is the driver
** and holds the expectations.
**
** Issue #131: a negative rc from the linked module and a negated abend code
** are the same integer in prc, so httppcgi() used to report "U0005 ABEND" for
** a module that never abended.  httplink() now folds a module rc into its own
** range (HTTP_LINK_EPGMRC), and that fold is what this module lets TSTLINK
** exercise on the real LINK path instead of on macro arithmetic alone.
**
** It is built with crt1 (@@CRT0 without the CTHREAD IDENTIFY), NOT with
** cgistart: cgistart clamps a negative main() rc to 0, which is exactly the
** guarantee that keeps a CGI out of this case.  A module with its own __start
** has no such clamp, and this is what one looks like.
**
** Run standalone by the test runner (batch step and TSO CALL, no parm) it must
** not fail the suite, so it returns 0 unless it was given a parm.
*/
#include <stdio.h>

#define TSTLNKM_RC  (-5)        /* keep in sync with tstlink.c */

int main(int argc, char **argv)
{
    /* argv[0] is the program name; the runner passes nothing further, TSTLINK
       LINKs us with the quoted request path httplink() builds */
    if (argc < 2 || !argv[1] || !argv[1][0]) {
        printf("TSTLNKM: no parm, standalone run -- rc 0\n");
        return 0;
    }

    printf("TSTLNKM: parm \"%s\" -- returning rc %d\n", argv[1], TSTLNKM_RC);

    return TSTLNKM_RC;
}
