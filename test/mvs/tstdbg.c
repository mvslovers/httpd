/*
** TSTDBG -- a write error on DD:HTTPDBG must be reported once and must switch
**           tracing off, not leave it silently dead (issue #264).
**
** Since libc370 1.0.4 a failed write is ferror() instead of ABEND S001, and
** since 1.0.6 the FILE then refuses every later write.  The dbg*.c writers
** never looked, so a full or broken HTTPDBG left DEBUG=1 running with no
** trace and nothing on the console.  They now ask dbgfail() after each write.
**
** The error vector is a data set that fills up: DD:HTTPDBG is allocated over
** a new TRK(1,0) data set and written until libc370 reports ENOSPC (the
** same vector libc370's own fail-fast test uses).  The test carries its own
** fixture through __dsalcf(DDNAME=HTTPDBG), so it needs nothing from the JCL.
**
**   round 1  a HEALTHY HTTPDBG: dbgopen() opens it, the writers write, and
**            tracing stays on.  Without this round 2 proves nothing -- a
**            writer that switched tracing off for any reason would pass it.
**   round 2  the full data set: the writers switch tracing off by themselves
**            (httpd.dbg becomes NULL) and every writer is harmless after it.
**   round 3  the gate itself, on the FILE that failed in round 2: however
**            many writes and checks follow, dbgfail() reports exactly once.
**
** The console is not captured; "exactly one HTTPD039W" is dbgfail()'s return
** value, which is 1 only for the caller that issued the WTO.  Round 3 issues
** one further HTTPD039W, so a run of this test leaves two on the console.
**
** MVS-only: DD: names, __dsalcf() and WTO have no host equivalent.
**
** PARM='<dsn>' names the scratch data set (default IBMUSER.HTTPD.TDBG);
** it is created and deleted by this test.
**
** RC 0 = all checks passed.
*/
#include "httpd.h"

#include <stdio.h>
#include <mvs/dynalloc.h>
#include <mvs/wto.h>
#include <mbtcheck.h>

#define DFLT_DSN    "IBMUSER.HTTPD.TDBG"
#define DBG_DD      "HTTPDBG"

/* a one-track data set holds a few hundred of these; the loop is a bound so
** a library that never fails the write ends the test instead of hanging it */
#define MAXLINES    20000

static const char line[] =
    "TSTDBG filler line to fill the trace data set 0123456789\n";

static int
alloc_httpdbg(const char *dsn, const char *space)
{
    char dd[9];
    int  rc;

    rc = __dsalcf(dd, "DDNAME=" DBG_DD ";DSN=%s;DISP=(NEW,CATLG,DELETE);"
                      "DSORG=PS;RECFM=FB;LRECL=133;BLKSIZE=1330;"
                      "SPACE=%s", dsn, space);
    if (rc != 0)
        printf("  __dsalcf(%s,%s) failed rc=%d\n", DBG_DD, space, rc);

    return rc;
}

static void
scrap_httpdbg(const char *dsn)
{
    char dd[9];

    __dsfree(DBG_DD);
    if (__dsalcf(dd, "DSN=%s;DISP=(OLD,DELETE)", dsn) == 0)
        __dsfree(dd);
}

int
main(int argc, char **argv)
{
    const char  *dsn = DFLT_DSN;
    CLIBGRT     *grt = __grtget();
    void        *grtapp1 = grt->grtapp1;
    HTTPD       httpd;
    FILE        *fp;
    int         i;
    int         reports;
    int         n;

    if (argc > 1 && argv[1] && argv[1][0])
        dsn = argv[1];

    printf("TSTDBG -- a DD:%s write error must switch tracing off, once "
           "(dsn '%s')\n\n", DBG_DD, dsn);

    /* dbg*.c find the server block through the GRT, like every module */
    memset(&httpd, 0, sizeof(httpd));
    grt->grtapp1 = &httpd;

    /* ---- round 1: a healthy trace data set ------------------------- */

    if (alloc_httpdbg(dsn, "TRK(5,5)") == 0) {
        httpd.dbg_enabled = 1;
        dbgopen(&httpd);
        fp = httpd.dbg;

        CHECK(fp != NULL && fp != stderr, "healthy HTTPDBG: dbgopen() opens it");
        if (fp && fp != stderr) {
            dbgs("TSTDBG healthy line\n");
            dbgf("TSTDBG healthy %s %d\n", "dbgf", 1);
            dbgw("TSTDBG healthy dbgw\n", 20);
            dbgdump((void *)line, 16, "TSTDBG healthy dump");
            dbgenter("TSTDBG %s\n", "enter");
            dbgexit("TSTDBG %s\n", "exit");

            CHECK(httpd.dbg == fp, "healthy HTTPDBG: tracing stays on");
            CHECK(!ferror(fp), "healthy HTTPDBG: no error on the FILE");
            CHECK_EQ(dbgfail(&httpd, fp), 0,
                     "healthy HTTPDBG: dbgfail() reports nothing");
            CHECK(httpd.dbg == fp, "healthy HTTPDBG: still on after the gate");
            fclose(fp);
        }
        httpd.dbg = NULL;
        scrap_httpdbg(dsn);
    } else {
        CHECK(0, "healthy HTTPDBG: allocated");
    }

    /* ---- round 2: a trace data set that fills up ------------------- */

    memset(&httpd, 0, sizeof(httpd));
    grt->grtapp1 = &httpd;

    if (alloc_httpdbg(dsn, "TRK(1,0)") == 0) {
        httpd.dbg_enabled = 1;
        dbgopen(&httpd);
        fp = httpd.dbg;

        CHECK(fp != NULL && fp != stderr, "full HTTPDBG: dbgopen() opens it");
        if (fp && fp != stderr) {
            errno = 0;
            for (n = 0; n < MAXLINES && httpd.dbg; n++)
                dbgs(line);

            printf("  (round 2: tracing off after %d lines, errno=%d "
                   "ferror=%d)\n", n, errno, ferror(fp) != 0);

            CHECK(n < MAXLINES, "full HTTPDBG: the write fails before the bound");
            CHECK(ferror(fp) != 0, "full HTTPDBG: the FILE is in error");
            CHECK(httpd.dbg == NULL,
                  "full HTTPDBG: the writers switched tracing off");

            /* every writer, with tracing off: no crash, still off */
            dbgs(line);
            dbgf("TSTDBG after %d\n", n);
            dbgw(line, (int)sizeof(line) - 1);
            dbgdump((void *)line, 16, "TSTDBG after");
            dbgenter("TSTDBG after\n");
            dbgexit("TSTDBG after\n");
            dbgtime(NULL);
            CHECK(httpd.dbg == NULL,
                  "full HTTPDBG: every writer is harmless with tracing off");
            CHECK_EQ(dbgfail(&httpd, fp), 0,
                     "full HTTPDBG: nothing more to report once it is off");

            /* ---- round 3: exactly one report from the gate ---------- */

            httpd.dbg = fp;
            reports = 0;
            for (i = 0; i < 25; i++) {
                fputs(line, fp);
                reports += dbgfail(&httpd, fp);
            }
            CHECK_EQ(reports, 1,
                     "failed FILE: 25 writes and checks report exactly once");
            CHECK(httpd.dbg == NULL, "failed FILE: tracing is off after it");

            fclose(fp);
        }
        httpd.dbg = NULL;
        scrap_httpdbg(dsn);
    } else {
        CHECK(0, "full HTTPDBG: allocated");
    }

    grt->grtapp1 = grtapp1;

    return mbt_test_summary("TSTDBG");
}
