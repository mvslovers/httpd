/*
** TSTPRM -- a read error on DD:HTTPPRM must refuse the start, not bring the
**           server up on half a Parmlib.
**
** Since libc370 1.0.4 an uncorrectable I/O error is ferror() + errno EIO
** instead of ABEND S001, and feof() is deliberately NOT set.  That turns
** http_prm_read()'s
**
**     while (fgets(line, sizeof(line), fp)) parse_line(httpd, line);
**
** into a silent truncation: whatever was parsed before the bad block stays,
** everything after it keeps its set_defaults() value, and the function used
** to answer 0 -- so HTTPD would start, look healthy, and be missing routes
** with the AUTH= policies attached to them.  That is the same hazard
** HTTPD420E refuses to start on, arriving through the reader instead of the
** parser.
**
** The error vector is libc370's own, from its #147 item 3 probe: write a
** normal FB/3120 data set, then read it back through a DD whose DCB override
** claims BLKSIZE=80.  The first READ meets a 3120-byte block with an 80-byte
** buffer -- a wrong-length record, an uncorrectable I/O error.  Both DDs are
** allocated here through __dsalcf(DDNAME=...), so the vector needs nothing
** from the JCL and the test carries its own fixture.
**
** Two rounds, and the first one matters as much as the second:
**
**   round 1  a HEALTHY HTTPPRM is read and parsed, with the member's values
**            actually in the HTTPD block.  Without this the second round
**            proves nothing -- a http_prm_read() that failed for any reason
**            at all would pass it.
**   round 2  the SAME member behind the lying BLKSIZE returns non-zero.
**
** http_prm_read() rather than http_config(): everything http_config() does
** after the read binds the listener and initializes UFS, which a test running
** beside a live server must not do.  The guard is in the half tested here.
**
** MVS-only: the vector is a real BSAM condition, and DD: names, __dsalcf()
** and WTO have no host equivalent.
**
** PARM='<dsn>' names the scratch data set (default IBMUSER.HTTPD.TPRM);
** it is created and deleted by this test.
**
** RC 0 = all checks passed.
*/
#include "httpd.h"

#include <clibio.h>
#include <clibwto.h>
#include <mbtcheck.h>

#define DFLT_DSN        "IBMUSER.HTTPD.TPRM"

/* Written into the scratch data set, and asserted back in round 1.
** Deliberately not what set_defaults() would leave behind (8080 / 9), so
** "parsed" and "did not parse" cannot be confused. */
#define TSTPRM_PORT     2280
#define TSTPRM_MAXTASK  17

static const char * const parmlines[] = {
    "# TSTPRM -- Parmlib read-error probe",
    "PORT=2280",
    "MAXTASK=17",
    NULL
};

/* --------------------------------------------------------------------
** Write the Parmlib member through a correctly described DD.
** Returns 0 on success.
** ----------------------------------------------------------------- */
static int
write_parmlib(const char *dsn)
{
    char  dd[9];
    char  ddspec[16];
    FILE *fp;
    int   i;
    int   rc;

    rc = __dsalcf(dd, "DDNAME=TSTPRMW;DSN=%s;DISP=(NEW,CATLG,DELETE);"
                      "DSORG=PS;RECFM=FB;LRECL=80;BLKSIZE=3120;"
                      "SPACE=TRK(1,1)", dsn);
    if (rc != 0) {
        printf("  __dsalcf(TSTPRMW) failed rc=%d\n", rc);
        return -1;
    }

    snprintf(ddspec, sizeof(ddspec), "dd:%s", dd);

    fp = fopen(ddspec, "w");
    if (!fp) {
        printf("  fopen(%s) failed errno=%d\n", ddspec, errno);
        __dsfree(dd);
        return -1;
    }

    for (i = 0; parmlines[i]; i++)
        fprintf(fp, "%s\n", parmlines[i]);

    fclose(fp);
    __dsfree(dd);

    return 0;
}

/* --------------------------------------------------------------------
** Allocate DD:HTTPPRM over the data set written above.  blksize 0 means
** "tell the truth" (take the DSCB); anything else is the lying override
** that makes the first READ a wrong-length record.
** ----------------------------------------------------------------- */
static int
alloc_httpprm(const char *dsn, int blksize)
{
    char dd[9];
    int  rc;

    if (blksize == 0)
        rc = __dsalcf(dd, "DDNAME=" HTTPD_PARMLIB_DD ";DSN=%s;DISP=SHR", dsn);
    else
        rc = __dsalcf(dd, "DDNAME=" HTTPD_PARMLIB_DD ";DSN=%s;DISP=SHR;"
                          "RECFM=FB;LRECL=80;BLKSIZE=%d", dsn, blksize);

    if (rc != 0)
        printf("  __dsalcf(%s,blksize=%d) failed rc=%d\n",
               HTTPD_PARMLIB_DD, blksize, rc);

    return rc;
}

int
main(int argc, char **argv)
{
    const char *dsn = DFLT_DSN;
    HTTPD       httpd;
    char        dd[9];
    int         rc;

    if (argc > 1 && argv[1] && argv[1][0])
        dsn = argv[1];

    printf("TSTPRM -- a DD:%s read error must refuse the start "
           "(dsn '%s')\n\n", HTTPD_PARMLIB_DD, dsn);

    if (write_parmlib(dsn) != 0) {
        /* No vector, no verdict -- say so loudly rather than pass on a test
        ** that never ran.  wtof() as well as printf(), the libc370 #145
        ** lesson: a stdio failure can take buffered SYSPRINT with it. */
        wtof("TSTPRM: CANNOT BUILD THE FIXTURE DATA SET -- NO VERDICT");
        printf("FAIL: fixture data set could not be written\n");
        return 8;
    }

    /* ---- round 1: a healthy Parmlib is read, and parsed ------------ */

    if (alloc_httpprm(dsn, 0) == 0) {
        memset(&httpd, 0, sizeof(httpd));
        rc = http_prm_read(&httpd);

        CHECK_EQ(rc, 0, "healthy HTTPPRM: http_prm_read() returns 0");
        CHECK_EQ((int)httpd.port, TSTPRM_PORT, "healthy HTTPPRM: PORT parsed");
        CHECK_EQ((int)httpd.cfg_maxtask, TSTPRM_MAXTASK,
                 "healthy HTTPPRM: MAXTASK parsed");

        __dsfree(HTTPD_PARMLIB_DD);
    } else {
        CHECK(0, "healthy HTTPPRM: allocated");
    }

    /* ---- round 2: the same member behind a lying BLKSIZE ----------- */

    if (alloc_httpprm(dsn, 80) == 0) {
        memset(&httpd, 0, sizeof(httpd));
        errno = 0;
        rc = http_prm_read(&httpd);

        /* The point of the whole test.  Before the guard this was 0 and
        ** HTTPD started on whatever had been parsed. */
        CHECK(rc != 0,
              "read error on HTTPPRM: http_prm_read() returns non-zero");

        /* The measurement behind the check above.  Round 1 read the same
        ** member successfully, so the two values here say how far round 2
        ** got before the error: PORT is the first keyword, MAXTASK the
        ** second, and whichever comes back at its set_defaults() value is
        ** the part of the member that went missing.  That is the truncated
        ** configuration the guard exists to refuse. */
        printf("  (round 2: rc=%d errno=%d port=%d maxtask=%d)\n",
               rc, errno, (int)httpd.port, (int)httpd.cfg_maxtask);

        __dsfree(HTTPD_PARMLIB_DD);
    } else {
        CHECK(0, "read error on HTTPPRM: allocated");
    }

    /* Scratch data set: gone either way, so a re-run finds a clean slate
    ** rather than a NEW allocation that fails against the leftover. */
    if (__dsalcf(dd, "DSN=%s;DISP=(OLD,DELETE)", dsn) == 0)
        __dsfree(dd);

    return mbt_test_summary("TSTPRM");
}
