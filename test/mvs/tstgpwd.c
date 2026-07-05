/* TSTGPWD.C
** Test for http_get_password() (HTTPGPWD, src/httpxauth.c).
**
** Issue #111: a CGI (mvsMF) submitting a batch job through the JES2 internal
** reader needs the caller's plaintext password for the JOB card, for both
** Basic and token auth.  The password is retained (blowfish-encrypted) in
** httpc->cred->id.password, but a CGI cannot decrypt it with credid_dec():
** that reads the blowfish key via credkey()/__wsaget() from the current GRT's
** WSA, which is only initialized in httpd's own GRT by cred_init() (the #109
** wall).  http_get_password() instead uses the key pointer httpd caches in the
** HTTPD block (httpd->credkey) and blowfish_decrypt()s directly -- a pure
** function of (data, key) valid from any execution context.
**
** This test drives the real http_get_password() (HTTP_PRIVATE, like tstguid):
**   Path 1 (no RACF): a CRED carrying an encrypted CREDID built the way
**     cred_login() does (credid_update() plaintext + credid_enc()); the HTTPD
**     block caches credkey() as httpd does at cred_init() time.
**       - a 4-char and an exactly-8-char password each round-trip (twice /
**         repeat read, mirroring a cache hit);
**       - a CRED with no password (passflg clear) -> NULL;
**       - no CRED / a NULL cached key -> NULL, output buffer left empty.
**   Path 2 (RACF): the real thing -- cred_login() with a lower-case password
**     that RACF folds to upper case, then http_get_password() must return the
**     folded (upper-case) form.  Skipped (not failed) without APF.
**
** MVS-only (blowfish + RACF SVCs / project headers).  http_get_password /
** credid_ / cred_ resolve from the [internal] autocall archive, so only this
** root is listed in project.toml.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <clibauth.h>   /* __autask() -- APF-authorise for racf_login() */
#include <mbtcheck.h>

/* build an encrypted CREDID exactly as cred_login() does: plaintext update
** (writes verbatim, clears the flag) followed by encrypt (sets the flag). */
static void
enc_cred(CREDID *id, unsigned addr, const char *pass)
{
    credid_init(id);
    credid_update(id, &addr, (unsigned char *)"IBMUSER", (unsigned char *)pass);
    credid_enc(id, id);
}

int main(void)
{
    HTTPD    *httpd;
    HTTPC    *httpc;
    CRED     *cred;
    CRED     *cred2;
    CREDID    id;
    unsigned char buf[64];
    unsigned char *ret;
    unsigned addr = 0xC0A8017B;   /* 192.168.1.123 */
    int      rc;
    int      apf;

    printf("=== HTTPD http_get_password (issue #111) tests ===\n");

    /* set up the blowfish key (NULL salt -> default, like an unconfigured
       cred_init()); credkey() is valid here in the test's own GRT. */
    rc = cred_init(NULL, 0);
    CHECK(rc == 0, "cred_init() succeeds");
    if (rc) return mbt_test_summary("TSTGPWD");

    httpd = (HTTPD *) calloc(1, sizeof(HTTPD));
    httpc = (HTTPC *) calloc(1, sizeof(HTTPC));
    CHECK(httpd != NULL && httpc != NULL, "HTTPD + HTTPC allocated");
    if (!httpd || !httpc) return mbt_test_summary("TSTGPWD");

    /* httpd caches the blowfish key pointer at cred_init() time -- the value
       http_get_password() reads instead of calling credkey() from CGI ctx. */
    httpd->credkey = credkey();
    CHECK(httpd->credkey != NULL, "httpd->credkey cached (non-NULL)");
    httpc->httpd = httpd;
    httpc->addr  = addr;

    /* --- Path 1a: 4-char password, no RACF ------------------------------- */
    enc_cred(&id, addr, "SYS1");
    cred = cred_new(&id, NULL, NULL, 0);
    CHECK(cred != NULL, "cred_new (encrypted 4-char pass) allocates a CRED");
    if (cred) {
        httpc->cred = cred;

        memset(buf, 0, sizeof(buf));
        ret = http_get_password(httpc, buf, sizeof(buf));
        CHECK(ret != NULL, "http_get_password returns non-NULL (pass present)");
        CHECK(strcmp((char *)buf, "SYS1") == 0,
              "http_get_password == SYS1 (read #1)");

        memset(buf, 0, sizeof(buf));
        http_get_password(httpc, buf, sizeof(buf));
        CHECK(strcmp((char *)buf, "SYS1") == 0,
              "http_get_password == SYS1 (read #2 / repeat, cache-hit shape)");

        cred_free(&cred);
    }

    /* --- Path 1b: exactly-8-char password (no NUL inside the block) ------- */
    enc_cred(&id, addr, "PASSWRD8");
    cred = cred_new(&id, NULL, NULL, 0);
    CHECK(cred != NULL, "cred_new (encrypted 8-char pass) allocates a CRED");
    if (cred) {
        httpc->cred = cred;
        memset(buf, 0, sizeof(buf));
        ret = http_get_password(httpc, buf, sizeof(buf));
        CHECK(ret != NULL && strcmp((char *)buf, "PASSWRD8") == 0,
              "http_get_password == PASSWRD8 (full 8-char block round-trips)");
        cred_free(&cred);
    }

    /* --- Path 1c: CRED with no password (passflg clear) -> NULL ---------- */
    credid_init(&id);                       /* all-zero: flag clear, no pass */
    cred = cred_new(&id, NULL, NULL, 0);
    CHECK(cred != NULL, "cred_new (no password) allocates a CRED");
    if (cred) {
        httpc->cred = cred;
        memset(buf, 0, sizeof(buf));
        ret = http_get_password(httpc, buf, sizeof(buf));
        CHECK(ret == NULL, "http_get_password NULL when CRED has no password");
        CHECK(buf[0] == 0, "output buffer stays empty (NUL) for no password");
        cred_free(&cred);
    }

    /* --- Path 1d: no CRED at all -> NULL --------------------------------- */
    httpc->cred = NULL;
    memset(buf, 0, sizeof(buf));
    ret = http_get_password(httpc, buf, sizeof(buf));
    CHECK(ret == NULL, "http_get_password NULL when unauthenticated (no CRED)");
    CHECK(buf[0] == 0, "output buffer stays empty (NUL) when unauthenticated");

    /* --- Path 1e: a NULL cached key -> NULL (never falls back to garbage) - */
    enc_cred(&id, addr, "SYS1");
    cred = cred_new(&id, NULL, NULL, 0);
    if (cred) {
        httpc->cred     = cred;
        httpd->credkey  = NULL;             /* simulate an un-cached key     */
        memset(buf, 0, sizeof(buf));
        ret = http_get_password(httpc, buf, sizeof(buf));
        CHECK(ret == NULL, "http_get_password NULL when no cached key");
        CHECK(buf[0] == 0, "output buffer stays empty (NUL) with no key");
        httpd->credkey  = credkey();        /* restore for Path 2            */
        cred_free(&cred);
    }

    /* --- Path 2: real cred_login() folds the password to upper case ------ */
    apf = __autask();       /* APF-authorise this task for racf_login()       */
    wtof("TSTGPWD: __autask() rc=%d", apf);
    if (apf != 0) {
        printf("  SKIP: no APF (rc=%d) -> RACF cred_login path skipped\n", apf);
        return mbt_test_summary("TSTGPWD");
    }

    /* pass a lower-case password: cred_login() upper-cases it before both
       racf_login() and encryption, so http_get_password() must return "SYS1". */
    cred = cred_login(addr, (unsigned char *)"IBMUSER", (unsigned char *)"sys1");
    CHECK(cred != NULL, "cred_login #1 (fresh) succeeds");
    if (cred) {
        httpc->cred = cred;
        memset(buf, 0, sizeof(buf));
        ret = http_get_password(httpc, buf, sizeof(buf));
        CHECK(ret != NULL && strcmp((char *)buf, "SYS1") == 0,
              "http_get_password == SYS1 (RACF-folded upper case, fresh read)");

        cred2 = cred_login(addr, (unsigned char *)"IBMUSER", (unsigned char *)"sys1");
        CHECK(cred2 == cred, "cred_login #2 returns the SAME cached CRED");
        if (cred2) {
            httpc->cred = cred2;
            memset(buf, 0, sizeof(buf));
            ret = http_get_password(httpc, buf, sizeof(buf));
            CHECK(ret != NULL && strcmp((char *)buf, "SYS1") == 0,
                  "http_get_password == SYS1 (cache-hit read)");
        }
    }

    return mbt_test_summary("TSTGPWD");
}
