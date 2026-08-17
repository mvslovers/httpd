/* TSTLOUT.C
** Test for credtok_logout_arr() (CREDTKLA, credentials/src/credtokl.c) and its
** credtok_logout() wrapper -- the store side of http_logout() (HTTPLOUT).
**
** Issue #113: http_logout() was a silent no-op when called from a CGI.  It
** reached the store via credtok_logout() -> cred_array(), which reads the
** credential array out of the *current* GRT's WSA.  A CGI runs under its own
** GRT, where that slot is empty, so the token scan found nothing, the CRED
** stayed in httpd's real store, and the token kept resolving -- logout returned
** "success" but never invalidated the session.  (At the time the token was
** derived from the CREDID, so re-presenting the credentials reproduced it too;
** since #188 it is random, and this test mints its own with credtok_gen() only
** because it needs a value it can look up again.)  The fix splits
** out credtok_logout_arr(array, token), which operates on a caller-supplied
** array pointer (httpd caches its own in HTTPD.credarr at cred_init() time), so
** logout reaches httpd's real store from any GRT.  Same class as #109/#111.
**
** This test runs single-GRT (a unit test cannot cross the CGI GRT boundary --
** that is verified live via mvsMF #168), so it guards the *refactor*: given an
** explicit array, credtok_logout_arr() must find, unlink and free the matching
** CRED so the token stops resolving; the wrapper must still work via
** cred_array(); and the NULL/absent cases must be no-ops.  It needs no RACF and
** no APF: the CREDs carry no ACEE, so cred_free() does no racf_logout().
**
** MVS-only (SHA256 token + array/lock SVCs live in crent370), like tstcred.
*/
#include "cred.h"
#include <mbtcheck.h>

/* build + store a CRED for (userid,addr) and return it; *tok gets its token */
static CRED *
add_cred(unsigned addr, const char *user, CREDTOK *tok)
{
    CREDID id;
    CRED   *cred;

    credid_init(&id);
    credid_update(&id, &addr, (unsigned char *)user, (unsigned char *)"SYS1");
    *tok = credtok_gen(&id);

    cred = cred_new(&id, tok, NULL, 0);   /* no ACEE -> cred_free needs no RACF */
    if (cred) cred_add(cred);
    return cred;
}

int main(void)
{
    CRED     ***array;
    CRED     *credA;
    CRED     *credB;
    CREDTOK   tokA;
    CREDTOK   tokB;
    CREDTOK   tokX;
    CREDID    idX;
    unsigned  before;
    int       rc;

    printf("=== HTTPD credtok_logout (issue #113) tests ===\n");

    rc = cred_init(NULL, 0);
    CHECK(rc == 0, "cred_init() succeeds");
    if (rc) return mbt_test_summary("TSTLOUT");

    array = cred_array();
    CHECK(array != NULL, "cred_array() returns the store");
    if (!array) return mbt_test_summary("TSTLOUT");

    /* --- credtok_logout_arr(): the array-explicit path http_logout() uses --- */
    credA = add_cred(0xC0A80101, "USERA", &tokA);
    CHECK(credA != NULL, "cred_new/cred_add (USERA) succeeds");
    if (credA) {
        CHECK(cred_find_by_token(&tokA) == credA,
              "token resolves to the CRED before logout");

        before = array_count(array);
        rc = credtok_logout_arr(array, &tokA);
        CHECK(rc == 0, "credtok_logout_arr returns 0 (found + freed)");
        CHECK(cred_find_by_token(&tokA) == NULL,
              "token no longer resolves after logout (the #113 fix)");
        CHECK(array_count(array) == before - 1,
              "the CRED was unlinked from the store");
    }

    /* logging the same (now absent) token out again finds nothing -> rc 1 */
    rc = credtok_logout_arr(array, &tokA);
    CHECK(rc == 1, "credtok_logout_arr on an absent token is a no-op (rc 1)");

    /* a token that was never added -> rc 1 */
    credid_init(&idX);
    credid_update(&idX, NULL, (unsigned char *)"NOBODY", (unsigned char *)"X");
    tokX = credtok_gen(&idX);
    rc = credtok_logout_arr(array, &tokX);
    CHECK(rc == 1, "credtok_logout_arr on an unknown token is a no-op (rc 1)");

    /* NULL guards must not crash and must report "nothing done" (rc 1) */
    CHECK(credtok_logout_arr(NULL, &tokA) == 1, "NULL array -> rc 1");
    CHECK(credtok_logout_arr(array, NULL) == 1, "NULL token -> rc 1");

    /* --- the credtok_logout() wrapper still works via cred_array() --------- */
    credB = add_cred(0xC0A80102, "USERB", &tokB);
    CHECK(credB != NULL, "cred_new/cred_add (USERB) succeeds");
    if (credB) {
        CHECK(cred_find_by_token(&tokB) == credB,
              "wrapper: token resolves before logout");
        rc = credtok_logout(&tokB);
        CHECK(rc == 0, "credtok_logout() wrapper returns 0");
        CHECK(cred_find_by_token(&tokB) == NULL,
              "wrapper: token no longer resolves after logout");
    }

    return mbt_test_summary("TSTLOUT");
}
