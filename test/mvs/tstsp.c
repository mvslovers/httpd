/* TSTSP.C
** Where can httpd set the CGI heap subpool?  (issue #154, stage 2)
**
** libc370#89 shipped the runtime heap subpool: PPAHEAPS in the PPA the current
** TCB owns, recorded per allocation, released with one FREEMAIN SP=n.  #154's
** plan was to bracket __linkds() in httppcgi() with __setsp(n)/__setsp(old) so
** everything a CGI allocates can be reclaimed when it abends.
**
** That plan rests on the LINKing TCB having a PPA, and httpd's workers do not:
** CTHREAD (libc370 asm/@@crt0.asm:164-205) builds a CLIBCRT and a CTHDSTK but
** never a CLIBPPA, and stores nothing at 8(TCBFSAB).  heapsp.c resolves tier-1
** only -- deliberately, because the owner-TCB fallback would let __setsp() on a
** cthread mutate the MAIN task's ambient subpool while @@GETM on that cthread
** keeps using subpool 0.
**
** So this test measures, on the real shape of the problem (a subtask that LINKs
** a module), where the ambient subpool can and cannot be set:
**
**   main task   __setsp() bites          -- the API works in this job at all
**   cthread     __setsp() is a no-op     -- so httppcgi() is the WRONG place
**   LINKed module   __setsp() bites      -- so cgistart is the RIGHT place
**
** The last one is what the fix is built on: the module's own @@CRT0 installs a
** PPA at 8(TCBFSAB) even on a cthread TCB and @@EXITA pops it on return, which
** is exactly the bracket #154 needs -- entered from inside the module rather
** than around the LINK.
**
** MVS-only: cthreads, LINK and GETMAIN are SVCs (make test-mvs).
** The probe target is the TSTSPM module, LINKed from the worker below.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <clibos.h>
#include <cliblink.h>
#include <clibthrd.h>
#include <mbtcheck.h>

#define PROBE_SP    7           /* problem-state range is 1-127             */

#define LEAK_KB     512         /* what one TSTSPM LEAK call abandons       */
#define LEAK_ROUNDS 64          /* ... x64 = 32 MB, unreachable in 24 bits  */
                                /* ... unless the reclaim really gives it   */
                                /* ... back, so this cannot pass by accident */

/* what the worker measured -- filled on the cthread, checked on the main task
   so no stdio runs from the subtask while the measurement is in flight */
typedef struct probe PROBE;
struct probe {
    unsigned char   amb_before;     /* __getsp() as the cthread found it    */
    unsigned char   amb_after;      /* __getsp() after __setsp(PROBE_SP)    */
    unsigned char   setsp_rc;       /* what __setsp() returned there        */
    unsigned char   header;         /* subpool byte of a malloc there       */
    int             linkrc;         /* __linkds() rc (< 0 == abend)         */
    int             prc;            /* TSTSPM's return code                 */

    int             rounds;         /* leak+reclaim cycles completed        */
    int             short_kb;       /* first round that came up short (KB)  */
    int             freerc;         /* first nonzero FREEMAIN rc            */
};

static int worker(PROBE *pr, void *unused);
static int link_probe(const char *parm, int *prc);
static int reclaim(unsigned char sp);

int main(void)
{
    PROBE       pr;
    CTHDTASK    *task;
    unsigned char   prev;
    unsigned char   ambient;
    void        *p;
    unsigned char   header = 9;

    printf("=== HTTPD #154 CGI subpool placement ===\n");

    memset(&pr, 0, sizeof(pr));

    /* -- control: the main task HAS a PPA (crt1 built one), so the API must
          bite here.  If this fails the library is not the one we think it is
          and every other expectation below is meaningless. */
    prev    = __setsp(PROBE_SP);
    ambient = __getsp();
    p       = malloc(64);
    if (p) {
        header = (unsigned char) (((unsigned *) p)[-2] >> 24);
        free(p);
    }
    __setsp(prev);

    CHECK_EQ(ambient, PROBE_SP, "main task: __setsp() sets the ambient subpool");
    CHECK_EQ(header, PROBE_SP,  "main task: a malloc'd block carries it");
    CHECK_EQ(__getsp(), prev,   "main task: the ambient subpool is restored");

    /* -- the shape httpd actually has: a subtask that LINKs a module -- */
    task = cthread_create((void *) worker, &pr, NULL);
    CHECK(task != NULL, "worker thread created");
    if (!task) return mbt_test_summary("TSTSP");

    cthread_wait(&task->termecb);

    CHECK_EQ(pr.amb_before, 0, "worker: starts on subpool 0 (no PPA)");
    CHECK_EQ(pr.setsp_rc,   0, "worker: __setsp() reports no previous value");
    CHECK_EQ(pr.amb_after,  0, "worker: __setsp() is a NO-OP -- not the bracket point");
    CHECK_EQ(pr.header,     0, "worker: its allocations stay in subpool 0");

    /* -- inside the LINKed module: the module's own @@CRT0 owns a PPA -- */
    CHECK_EQ(pr.linkrc, 0, "worker: LINK to TSTSPM succeeded");
    CHECK_EQ(pr.prc, PROBE_SP * 100 + PROBE_SP * 10 + 0,
             "module: __setsp() bites, the block carries it, nothing inherited");

    if (pr.prc != PROBE_SP * 100 + PROBE_SP * 10) {
        /* spell the digits out -- a bare 'expected 770, got N' costs a
           re-derivation of the encoding at the moment it matters most */
        printf("  TSTSPM rc=%d (ambient=%d header=%d inherited=%d)\n",
               pr.prc, pr.prc / 100, (pr.prc / 10) % 10, pr.prc % 10);
    }

    /* -- the defect and the fix, in the shape httppcgi() uses -- */
    CHECK_EQ(pr.freerc, 0, "reclaim: FREEMAIN RC,SP=n returns 0");
    CHECK_EQ(pr.rounds, LEAK_ROUNDS, "reclaim: all leak+reclaim rounds ran");
    CHECK_EQ(pr.short_kb, LEAK_KB,
             "reclaim: every round got its full allocation back");

    if (pr.short_kb != LEAK_KB) {
        printf("  round %d obtained only %d KB of %d -- storage was not "
               "returned\n", pr.rounds + 1, pr.short_kb, LEAK_KB);
    }

    cthread_delete(&task);

    return mbt_test_summary("TSTSP");
}

/* runs on the cthread -- the same kind of TCB an httpd worker is */
static int
worker(PROBE *pr, void *unused)
{
    void        *p;
    int         prc = -1;
    int         i;

    (void) unused;

    pr->amb_before = __getsp();
    pr->setsp_rc   = __setsp(PROBE_SP);
    pr->amb_after  = __getsp();

    p = malloc(64);
    if (p) {
        pr->header = (unsigned char) (((unsigned *) p)[-2] >> 24);
        free(p);
    }
    else {
        pr->header = 9;             /* 9 == not measured, see TSTSPM        */
    }

    /* whatever __setsp() did above (nothing, if the claim holds), undo it
       before the LINK so the module measures an inheritance of 0 */
    __setsp(pr->setsp_rc);

    pr->linkrc = link_probe("PROBE", &prc);
    pr->prc    = prc;

    /* -- leak + reclaim, LEAK_ROUNDS times ------------------------------
       Each round LINKs a module that allocates LEAK_KB in subpool PROBE_SP
       and returns without freeing any of it -- exactly what an abending CGI
       leaves behind -- and then releases the subpool the way httppcgi() does.
       LEAK_ROUNDS x LEAK_KB is 32 MB, which a 24-bit address space cannot
       satisfy: if the reclaim does not really give the storage back, a later
       round comes up short and short_kb records it.  This is the defect and
       the fix, without a server. */
    pr->short_kb = LEAK_KB;

    for (i = 0; i < LEAK_ROUNDS; i++) {
        int frc;

        prc = -1;
        if (link_probe("LEAK", &prc)) break;     /* LINK itself failed      */

        if (prc < LEAK_KB) {
            pr->short_kb = prc;                  /* first round to fall short */
            reclaim(PROBE_SP);
            break;
        }

        frc = reclaim(PROBE_SP);
        if (frc && !pr->freerc) {
            pr->freerc = frc;
            break;
        }

        pr->rounds++;
    }

    return 0;
}

/* link_probe() - LINK TSTSPM with a one-word PARM, the way httplink.c does. */
static int
link_probe(const char *parm, int *prc)
{
    struct {
        unsigned short  len;
        char            buf[8];
    } parms;
    unsigned    plist[1];

    memset(&parms, 0, sizeof(parms));
    strncpy(parms.buf, parm, sizeof(parms.buf) - 1);
    parms.len = (unsigned short) strlen(parms.buf);
    plist[0]  = (unsigned) &parms | 0x80000000;  /* VL style, last entry    */

    return __linkds("TSTSPM", NULL, plist, prc);
}

/* reclaim() - release the whole subpool, the same expansion httppcgi() uses:
   R form, SP= and no length, so MVS releases everything the task holds there
   (SR 1,1 is what says "subpool release"), conditional so a bad request is a
   return code rather than an abend. */
static int
reclaim(unsigned char sp)
{
    int         rc  = 0;
    unsigned    usp = sp;

    __asm__("FREEMAIN RC,SP=(%1)\n\t"
            "LR\t%0,15              save the return code"
            : "=r"(rc) : "r"(usp) : "0", "1", "14", "15");

    return rc;
}
