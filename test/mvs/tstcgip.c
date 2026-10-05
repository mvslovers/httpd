/* TSTCGIP.C
** Probe for issue #275 in cgistart's __start: a parameter whose length
** halfword is 1..3 with a zero third byte is not a TSO command buffer.
**
** __start() took any parameter with a nonzero length and a zero third byte
** for TSO-shaped, then copied length - 4 bytes from behind the 4-byte
** prefix.  A length of 1..3 made that copy length negative, and memcpy()
** took it as a huge size.  Now the TSO shape needs a length of at least 4,
** as in libc370's own startup (mvslovers/libc370#446).
**
** Not in the test runner, and cannot be: a cgistart module run on its own
** ends RC 12 in not_a_server_module() before main(), so as a [[test]] its
** standalone batch and TSO steps would fail the suite.  mbt has no test that
** is only a LINK target.
**
** Built as three programs from this source.  The driver (TSTCGIP, default)
** LINKs the target named in its PARM with the list httplink() builds --
** { A(parm), A(HTTPD), A(HTTPC)|VL } -- except that the parm is
** A(X'0002',X'00','X') and the two control blocks are zeroed fakes carrying
** only their eye-catchers and httpc->httpd.  That is what cgistart looks for:
** TSTLINK's fabricated HTTPC has no eye-catcher, which is why it never got
** past the server-context check.  With a zeroed HTTPD, http_get_httpx() is
** NULL, so the subpool and printf() paths through the vector stay out.
**
** The target (-DTARGET) links cgistart and returns 7 when argv[0] is its own
** program name, 3 otherwise.  It does no I/O.  TSTCGPT is linked with the
** fixed cgistart, TSTCGPR with the one before the fix -- the red control.
** The old copy need not abend: memcpy() is an MVCL, and with a length near
** 16 MB source and target overlap, so MVCL sets CC 3 and moves nothing.
** What shows is argv[0], taken from the TSO path.
**
** Build (cgistart objects from the fixed tree and from the commit before it):
**   cc370 -O1 -Wall -Wextra -Werror -Iinclude -c src/cgistart.c -o cgifix.o
**   cc370 -O1 -Wall -Wextra -Werror -Iinclude -c <old cgistart.c> -o cgiold.o
**   cc370 -O1 -Wall -Wextra -Werror -Iinclude -DTARGET -c \
**         test/mvs/tstcgip.c -o target.o
**   cc370 -O1 -Wall -Wextra -Werror -Iinclude -c test/mvs/tstcgip.c \
**         -o driver.o
**   ld370 -e @@CRT0 driver.o -lcc370rt -lc --rent --reus -iebcopy -o TSTCGIP
**   ld370 -e @@CRT0 cgifix.o target.o -lcc370rt -lc --rent --reus \
**         -iebcopy -o TSTCGPT
**   ld370 -e @@CRT0 cgiold.o target.o -lcc370rt -lc --rent --reus \
**         -iebcopy -o TSTCGPR
**   (ld370 also needs -L for the cc370 sysroot lib directory)
** then pack the three into one XMIT, RECEIVE it into a scratch library, and
** run tests/jcl/tstcgip.jcl.
**
** mvsdev JOB01434, 2026-10-05: GREEN CC 0, program rc 7 (argv[0] TSTCGPT);
** RED (cgistart before the fix) CC 1, program rc 3 (argv[0] empty).
**
** RC: 0 = the target ran and returned 7, 1 = it did not.
*/
#include <stdio.h>
#include <string.h>

#ifdef TARGET
int main(int argc, char **argv)
{
    /* parsed as a PARM, argv[0] is the program name; taken for a TSO
       buffer, it is the empty start of the copied text */
    return (argc == 1 && argv[0] && strncmp(argv[0], "TSTCGP", 6) == 0)
           ? 7 : 3;
}
#else
#include <stdlib.h>
#include <mvs/link.h>
#include "httpd.h"

int main(int argc, char **argv)
{
    static const unsigned char parm[4] = { 0x00, 0x02, 0x00, 'X' };
    const char  *target = argc > 1 ? argv[1] : "TSTCGPT";
    HTTPD       *httpd;
    HTTPC       *httpc;
    unsigned    plist[3];
    int         rc  = -99;
    int         lrc;

    httpd = calloc(1, sizeof(HTTPD));
    httpc = calloc(1, sizeof(HTTPC));
    if (!httpd || !httpc) {
        printf("driver: out of storage\n");
        return 1;
    }
    strcpy(httpd->eye, HTTPD_EYE);
    strcpy(httpc->eye, HTTPC_EYE);
    httpc->httpd = httpd;

    plist[0] = (unsigned)parm;
    plist[1] = (unsigned)httpd;
    plist[2] = (unsigned)httpc | 0x80000000;    /* VL style plist */

    lrc = __linkds(target, NULL, plist, &rc);
    printf("driver: __linkds(%s) = %d, program rc = %d\n", target, lrc, rc);
    printf("%s\n", (lrc == 0 && rc == 7) ? "PASS" : "FAIL");

    free(httpc);
    free(httpd);
    return (lrc == 0 && rc == 7) ? 0 : 1;
}
#endif
