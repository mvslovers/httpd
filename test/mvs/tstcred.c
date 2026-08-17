/* TSTCRED.C
** Isolation / regression test for issue #109: http_get_userid() returns
** still-encrypted CREDID.userid bytes on a repeat (cache-hit) read of a
** stored CRED, while the first (fresh) read decodes correctly.
**
** http_get_userid() (src/httpxauth.c) decrypts httpc->cred->id through
** credid_dec().  cred_find_by_token() hands the SAME cached CRED object back
** on every repeat request, and nothing in httpd or the credentials package
** rewrites a stored cred->id after cred_new() stores it -- so a second decrypt
** of that object must behave exactly like the first.  This test isolates the
** credential layer from HTTP / RACF to check that invariant directly:
**
**   1. build an encrypted CREDID the way cred_login() does
**      (credid_update() writes plaintext + clears the flag, then
**       credid_enc() encrypts + sets CREDID_USER_ENCRYPT),
**   2. store it in a CRED via cred_new() (the cached object),
**   3. decrypt the SAME stored CRED twice and assert the userid round-trips
**      both times, and
**   4. assert the CREDID_USER_ENCRYPT flag survives both reads (credid_dec()
**      must never mutate its input -- it only writes *out).
**
** blowfish_encrypt/decrypt live in crent370, so this is an MVS-only test
** (make test-mvs), like tstgctx.  It needs no RACF login and no APF auth: it
** never calls cred_login()/racf_login(), so it runs anywhere cred_init() can
** build the blowfish key.  The credid_ / cred_ bodies resolve from the
** [internal] autocall archive, so only this root is listed in project.toml.
*/
#include "cred.h"
#include <mbtcheck.h>

int main(void)
{
    CREDID   id;
    CREDID   out;
    CRED     *cred;
    unsigned addr = 0xC0A8017B;   /* 192.168.1.123 */
    int      rc;

    printf("=== HTTPD credential decrypt (issue #109) tests ===\n");

    /* set up the blowfish key (NULL salt -> the default blowfish_key_setup
       object code, same as an unconfigured cred_init()) */
    rc = cred_init(NULL, 0);
    CHECK(rc == 0, "cred_init() succeeds");
    if (rc) return mbt_test_summary("TSTCRED");

    /* 1. build the CREDID exactly as cred_login() does: plaintext update
          (clears the flag) followed by encrypt (sets the flag). */
    credid_init(&id);
    credid_update(&id, &addr, (unsigned char *)"IBMUSER", (unsigned char *)"SYS1");
    CHECK(!(id.userflg & CREDID_USER_ENCRYPT),
          "flag clear after credid_update (plaintext written)");
    CHECK(strcmp((char *)id.userid, "IBMUSER") == 0,
          "userid is plaintext after credid_update");

    credid_enc(&id, &id);
    CHECK((id.userflg & CREDID_USER_ENCRYPT) != 0,
          "flag set after credid_enc");
    CHECK(strcmp((char *)id.userid, "IBMUSER") != 0,
          "userid is ciphertext after credid_enc");

    /* 2. store it in a CRED -- the object cred_find_by_token() returns on
          every cache hit (cred_new() copies *id verbatim). */
    cred = cred_new(&id, NULL, NULL, 0);
    CHECK(cred != NULL, "cred_new() allocates a CRED");
    if (!cred) return mbt_test_summary("TSTCRED");
    CHECK((cred->id.userflg & CREDID_USER_ENCRYPT) != 0,
          "stored CRED keeps the encrypt flag");

    /* 3. first (fresh) read -- the login-time decode that is known to work */
    memset(&out, 0, sizeof(out));
    credid_dec(&cred->id, &out);
    CHECK(strcmp((char *)out.userid, "IBMUSER") == 0,
          "read #1 (fresh) decrypts to IBMUSER");

    /* 4. second read of the SAME stored CRED -- the reported cache-hit path.
          Nothing rewrote cred->id in between, so it must still decrypt. */
    memset(&out, 0, sizeof(out));
    credid_dec(&cred->id, &out);
    CHECK(strcmp((char *)out.userid, "IBMUSER") == 0,
          "read #2 (cache hit) decrypts to IBMUSER");

    /* 5. the flag must survive both reads (credid_dec() only writes *out) */
    CHECK((cred->id.userflg & CREDID_USER_ENCRYPT) != 0,
          "stored CRED still flagged after two decrypts");

    /* 6. token minting (#188): credtok_rand() must NOT be a function of the
          CREDID, and credtok_gen() must still be, so the two cannot be
          confused at a call site.  The determinism of credtok_gen() is what
          made a leaked token an offline oracle for the password; the whole
          point of the replacement is that the same credentials do not
          reproduce the same token. */
    {
        CREDTOK r1 = credtok_rand(credkey(), &id);
        CREDTOK r2 = credtok_rand(credkey(), &id);
        CREDTOK d1 = credtok_gen(&id);
        CREDTOK d2 = credtok_gen(&id);

        CHECK(memcmp(&r1, &r2, sizeof(CREDTOK)) != 0,
              "credtok_rand twice on the same CREDID -> DIFFERENT tokens");
        CHECK(memcmp(&d1, &d2, sizeof(CREDTOK)) == 0,
              "credtok_gen twice on the same CREDID -> same token (still deterministic)");
        CHECK(memcmp(&r1, &d1, sizeof(CREDTOK)) != 0,
              "a random token is not the derived one");

        /* a NULL key must not make it deterministic either -- that is the
           cred_new() fallback path, which has no key to pass */
        {
            CREDTOK n1 = credtok_rand(NULL, &id);
            CREDTOK n2 = credtok_rand(NULL, &id);

            CHECK(memcmp(&n1, &n2, sizeof(CREDTOK)) != 0,
                  "credtok_rand(NULL key) still yields DIFFERENT tokens");
        }
    }

    /* diagnostic dumps: on a failing MVS run the stored ciphertext can be
       eyeballed against mvsMF's fn=userid "hex" field, and the flag byte
       against the CREDID_USER_ENCRYPT (0x80) hypothesis in the issue. */
    wtof("TSTCRED: stored userflg=%02X (expect 80)", cred->id.userflg);
    wtodumpf(cred->id.userid, 9, "TSTCRED stored userid (ciphertext)");
    wtodumpf(out.userid, 9, "TSTCRED decrypted userid (expect IBMUSER)");

    cred_free(&cred);

    return mbt_test_summary("TSTCRED");
}
