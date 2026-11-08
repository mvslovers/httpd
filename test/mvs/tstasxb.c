/* TSTASXB.C
** Test that a login leaves the address space's own ACEE alone (issue #494).
**
** ASXBSENV, which racf_get_acee() reads, is the identity the whole server falls
** back on.  Up to libc370 2.29.1 racf_login() could hand that very ACEE back
** to its caller, and the caller's cred_free() or racf_logout() then released
** it.  The MVS tests log on with the account they run under (tstacct.h), which
** is the case that provokes it.  Since 2.29.2 racf_login() hands back no ACEE
** that ASXBSENV points at (mvslovers/libc370#778).
**
** This test records ASXBSENV and the name in its ACEE, then logs on and checks
** after every step that the pointer and the name are unchanged:
**   - racf_login() as the account: an ACEE of its own, not ASXBSENV's;
**   - cred_login() with the right password: a credential with an ACEE of its
**     own; with a wrong one: NULL (the 401 path), nothing released;
**   - cred_free() of the credential: ASXBSENV still points at the same block.
**
** MVS-only (RACF SVCs); needs APF like TSTGUID, and is skipped, not failed,
** without it or on a system whose test account is not known.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <mvs/apf.h>    /* __autask() -- APF-authorise for racf_login() */
#include <mbtcheck.h>
#include "tstacct.h"

/* the name in an ACEE as a C string (out holds at least 9 bytes) */
static void
acee_name(const ACEE *acee, char *out)
{
    unsigned len = 0;

    out[0] = 0;
    if (!acee) return;
    len = (unsigned char) acee->aceeuser[0];
    if (len > 8) len = 8;
    memcpy(out, acee->aceeuser + 1, len);
    out[len] = 0;
}

/* ASXBSENV is still the block it was, and still names the same user */
static int
asxb_intact(ACEE *asxb, const char *name)
{
    char now[9];

    if (racf_get_acee() != asxb) return 0;
    acee_name(racf_get_acee(), now);
    return strcmp(now, name) == 0;
}

int main(void)
{
    ACEE     *asxb;
    ACEE     *acee;
    CRED     *cred;
    CREDTOK   tok;
    char      before[9];
    char      runas[9];
    unsigned char wrong[9];
    const struct tstacct *acct;
    unsigned  addr = 0xC0A80164;   /* 192.168.1.100 */
    int       apf;
    int       racf_rc = 0;

    printf("=== HTTPD login keeps the address space ACEE (issue #494) tests ===\n");

    asxb = racf_get_acee();
    CHECK(asxb != NULL, "the address space has an ACEE (ASXBSENV)");
    if (!asxb) return mbt_test_summary("TSTASXB");
    acee_name(asxb, before);

    apf = __autask();       /* APF-authorise this task for racf_login() */
    wtof("TSTASXB: __autask() rc=%d", apf);
    if (apf != 0) {
        printf("  SKIP: no APF (rc=%d) -> RACF login paths skipped\n", apf);
        return mbt_test_summary("TSTASXB");
    }

    acct = tstacct_find(runas);
    if (!acct) {
        printf("  SKIP: no test account known for the user this job runs under "
               "(%s) -> RACF login paths skipped\n", runas);
        return mbt_test_summary("TSTASXB");
    }

    if (cred_init(NULL, 0) != 0) {
        CHECK(0, "cred_init() succeeds");
        return mbt_test_summary("TSTASXB");
    }

    /* --- racf_login() itself ---------------------------------------------- */
    acee = racf_login(acct->user, acct->pass, NULL, &racf_rc);
    CHECK(acee != NULL, "racf_login() with the right password gives an ACEE");
    CHECK(acee != asxb, "and it is not the address space's own");
    CHECK(asxb_intact(asxb, before), "ASXBSENV is unchanged after racf_login()");
    if (acee) {
        racf_logout(&acee);
        CHECK(asxb_intact(asxb, before),
              "ASXBSENV is unchanged after racf_logout() of that ACEE");
    }

    /* --- cred_login(): the Basic path ------------------------------------- */
    memset(wrong, 0, sizeof(wrong));
    memcpy(wrong, "NOTTHEPW", 8);
    CHECK(cred_login(addr, (unsigned char *)acct->user, wrong, 0) == NULL,
          "cred_login() with a wrong password is refused (the 401 path)");
    CHECK(asxb_intact(asxb, before), "ASXBSENV is unchanged after a refusal");

    cred = cred_login(addr, (unsigned char *)acct->user,
                      (unsigned char *)acct->pass, 0);
    CHECK(cred != NULL, "cred_login() with the right password succeeds");
    CHECK(cred && cred->acee && cred->acee != asxb,
          "the credential holds an ACEE of its own, not the address space's");
    CHECK(asxb_intact(asxb, before), "ASXBSENV is unchanged after cred_login()");

    /* the credential is freed with its ACEE: that must not be ASXBSENV's */
    if (cred) {
        tok = cred->token;
        CHECK(credtok_logout(&tok) == 0, "the credential is logged out");
        CHECK(asxb_intact(asxb, before),
              "ASXBSENV is unchanged after the credential and its ACEE are freed");
    }

    return mbt_test_summary("TSTASXB");
}
