/* TSTACCT.H
** The RACF account the MVS tests that log on for real (TSTGUID, TSTGPWD,
** TSTSESS) use, picked by the system they run on.
**
** The tests cannot carry one password: IBMUSER has SYS1 on the MVS/CE image
** and IBMPASS on the TK5 image, and RACF refuses the other one.  The runner
** (mbt) logs on as the target's own user, so the user the job runs under
** says which image this is.  A system that is not in the table is not
** guessed at: a wrong password in a login attempt is counted against the
** account on a real stand.  The test says so and skips the part that needs
** the login; it does not fail.
**
** One entry per image the tests are run on.
*/
#ifndef TSTACCT_H
#define TSTACCT_H

#include <mvs/racf.h>
#include <string.h>

struct tstacct {
    const char *runas;      /* user the test job runs under (the target's) */
    const char *user;       /* account to log on with                      */
    const char *pass;       /* its password, upper case                    */
};

static const struct tstacct tstacct_known[] = {
    { "IBMUSER", "IBMUSER", "SYS1"    },    /* MVS/CE */
    { "HERC01",  "IBMUSER", "IBMPASS" },    /* TK5    */
};

/* the userid of the job's own ACEE into out (at least 9 bytes), or "" */
static void
tstacct_runas(char *out)
{
    ACEE     *acee = racf_get_acee();
    unsigned  len;

    out[0] = 0;
    if (!acee) return;
    len = (unsigned char) acee->aceeuser[0];
    if (len > 8) len = 8;
    memcpy(out, acee->aceeuser + 1, len);
    out[len] = 0;
}

/* the account for the system this job runs on; NULL when it is not known
   (runas gets the job's user for the message) */
static const struct tstacct *
tstacct_find(char *runas)
{
    unsigned n;

    tstacct_runas(runas);
    for (n = 0; n < sizeof(tstacct_known) / sizeof(tstacct_known[0]); n++) {
        if (strcmp(runas, tstacct_known[n].runas) == 0) return &tstacct_known[n];
    }
    return NULL;
}

#endif
