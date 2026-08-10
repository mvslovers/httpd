/* TSTSPM.C
** Probe TARGET for TSTSP -- measures the ambient heap subpool from INSIDE a
** load module that was reached by LINK.  Not a test in its own right; TSTSP
** is the driver and holds the expectations.
**
** Why a second load module is needed (issue #154):
**
** libc370#89 made the malloc subpool a runtime value held in PPAHEAPS of the
** PPA the current TCB owns, and #154's plan was to bracket __linkds() in
** httppcgi() with __setsp(n)/__setsp(old).  That cannot work: httpd's workers
** are cthreads, and CTHREAD (libc370 asm/@@crt0.asm:164-205) builds a CLIBCRT
** and its own stack but NEVER a CLIBPPA, and never stores one at 8(TCBFSAB).
** heapsp.c resolves the PPA tier-1 only and by design, so on a worker
** __setsp() is a no-op returning 0 and @@GETM stays on subpool 0.
**
** What should still hold is that the LINKed module's own @@CRT0 installs a PPA
** at 8(TCBFSAB) even on a cthread TCB, and @@EXITA takes it away again -- i.e.
** the subpool can only be set from INSIDE the module window.  That is the
** claim this pair measures, because the whole fix is built on it.
**
** Standalone (the runner executes every [[test]] module on its own, batch step
** and TSO CALL, both without a parm) there is nothing to measure: return 0.
** TSTSP LINKs us with PARM='PROBE' and reads the measurement out of the return
** code, which __linkds() hands back as prc:
**
**     RC = ambient*100 + header*10 + inherited
**
**     ambient    __getsp() after __setsp(7)      -- expect 7
**     header     subpool byte of a malloc'd block -- expect 7
**     inherited  PPAHEAPS we started with         -- expect 0 (the cthread
**                that LINKed us has no PPA to inherit from)
**
** So RC 770 means the mechanism is live.  RC 0 means the module has no usable
** PPA either and the whole approach is dead.  RC 9 in the header digit means
** the malloc failed and the block byte was not measured.
*/
#include <stdlib.h>
#include <string.h>
#include <clibos.h>

#define PROBE_SP    7           /* problem-state range is 1-127             */

int main(int argc, char **argv)
{
    unsigned char   inherited;
    unsigned char   ambient;
    unsigned char   header  = 9;    /* 9 == not measured (malloc failed)    */
    void            *p;

    if (argc < 2 || strcmp(argv[1], "PROBE") != 0) {
        return 0;                   /* standalone runner pass -- see above  */
    }

    inherited = __setsp(PROBE_SP);
    ambient   = __getsp();

    /* @@GETM's header is 8 bytes ahead of the returned pointer and carries
       the subpool in the high byte of its first word -- the same byte
       @@FREEM reads back, so this is what free() will act on. */
    p = malloc(64);
    if (p) {
        header = (unsigned char) (((unsigned *) p)[-2] >> 24);
        free(p);
    }

    __setsp(inherited);

    return (int) ambient * 100 + (int) header * 10 + (int) inherited;
}
