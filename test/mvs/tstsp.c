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
};

static int worker(PROBE *pr, void *unused);

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

    cthread_delete(&task);

    return mbt_test_summary("TSTSP");
}

/* runs on the cthread -- the same kind of TCB an httpd worker is */
static int
worker(PROBE *pr, void *unused)
{
    struct {
        unsigned short  len;
        char            buf[8];
    } parms;
    unsigned    plist[1];
    void        *p;
    int         prc = -1;

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

    /* LINK the probe target with PARM='PROBE', VL-style plist like
       httplink.c builds */
    memset(&parms, 0, sizeof(parms));
    strcpy(parms.buf, "PROBE");
    parms.len = (unsigned short) strlen(parms.buf);
    plist[0]  = (unsigned) &parms | 0x80000000;

    pr->linkrc = __linkds("TSTSPM", NULL, plist, &prc);
    pr->prc    = prc;

    return 0;
}
