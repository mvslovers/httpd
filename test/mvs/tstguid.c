/* TSTGUID.C
** Test for http_get_userid() (HTTPGUID, src/httpxauth.c).
**
** Issue #109: http_get_userid() returned garbage for a CGI-invoked request.
** It decrypted httpc->cred->id with credid_dec(), whose blowfish key
** (credkey()) lives in per-GRT WSA and is only initialized in httpd's own GRT
** by cred_init().  A CGI reached through the HTTPX vector (e.g. mvsMF) runs
** under its own C runtime / GRT, where that key is still zero, so credid_dec()
** decrypted with an all-zero key and returned garbage.  The fix reads the
** userid from the RACF ACEE (aceeuser) instead -- no key, correct from any
** execution context.
**
** This test drives the real http_get_userid() (HTTP_PRIVATE, like tstgctx):
**   Path 1 (no RACF): a CRED carrying a fabricated ACEE whose aceeuser holds
**     "IBMUSER" -> http_get_userid() must return it (twice / repeat read); a
**     CRED with no ACEE -> NULL (no key path, no garbage).
**   Path 2 (RACF): the real thing -- cred_login() (which builds a real ACEE),
**     a repeat cred_login() the credential cache satisfies with the SAME CRED,
**     and http_get_userid() reads of both.  Skipped (not failed) without APF.
**
** MVS-only (RACF SVCs / project headers).  http_get_userid resolves from the
** [internal] autocall archive, so only this root is listed in project.toml.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <clibauth.h>   /* __autask() -- APF-authorise for racf_login() */
#include <mbtcheck.h>

/* fabricate an ACEE carrying a length-prefixed userid in aceeuser (byte 0 =
** length, bytes 1.. = userid), matching what racf_login() produces */
static void
fake_acee(ACEE *acee, const char *user)
{
    unsigned len = strlen(user);

    memset(acee, 0, sizeof(*acee));
    if (len > 8) len = 8;
    acee->aceeuser[0] = (char) len;
    memcpy(acee->aceeuser + 1, user, len);
}

int main(void)
{
    HTTPC    *httpc;
    CRED     *cred;
    CRED     *cred2;
    ACEE      acee;
    unsigned char buf[64];
    unsigned char *ret;
    unsigned addr = 0xC0A8017B;   /* 192.168.1.123 */
    int      apf;

    printf("=== HTTPD http_get_userid (issue #109) tests ===\n");

    httpc = (HTTPC *) calloc(sizeof(HTTPC), 1);
    CHECK(httpc != NULL, "HTTPC allocated");
    if (!httpc) return mbt_test_summary("TSTGUID");
    httpc->addr = addr;

    /* --- Path 1: CRED with a fabricated ACEE (no RACF), real http_get_userid */
    fake_acee(&acee, "IBMUSER");
    cred = cred_new(NULL, NULL, &acee, CRED_FLAG_KEEP_ACEE);
    CHECK(cred != NULL, "cred_new (fabricated ACEE) allocates a CRED");
    if (cred) {
        httpc->cred = cred;

        memset(buf, 0, sizeof(buf));
        ret = http_get_userid(httpc, buf, sizeof(buf));
        CHECK(ret != NULL, "http_get_userid returns non-NULL (ACEE present)");
        CHECK(strcmp((char *)buf, "IBMUSER") == 0,
              "http_get_userid == IBMUSER (from ACEE, read #1)");

        memset(buf, 0, sizeof(buf));
        http_get_userid(httpc, buf, sizeof(buf));
        CHECK(strcmp((char *)buf, "IBMUSER") == 0,
              "http_get_userid == IBMUSER (from ACEE, read #2 / repeat)");

        /* no ACEE -> NULL, and the output buffer is left empty (not garbage) */
        cred->acee = NULL;
        memset(buf, 0, sizeof(buf));
        ret = http_get_userid(httpc, buf, sizeof(buf));
        CHECK(ret == NULL, "http_get_userid returns NULL when the CRED has no ACEE");
        CHECK(buf[0] == 0, "output buffer stays empty (NUL) when unauthenticated");
    }

    /* --- Path 2: real cred_login() + cache hit, real http_get_userid() ---- */
    apf = __autask();       /* APF-authorise this task for racf_login()       */
    wtof("TSTGUID: __autask() rc=%d", apf);
    if (apf != 0) {
        printf("  SKIP: no APF (rc=%d) -> RACF cred_login path skipped\n", apf);
        return mbt_test_summary("TSTGUID");
    }

    cred = cred_login(addr, (unsigned char *)"IBMUSER", (unsigned char *)"SYS1", 0);
    CHECK(cred != NULL, "cred_login #1 (fresh) succeeds");
    if (cred) {
        httpc->cred = cred;
        memset(buf, 0, sizeof(buf));
        ret = http_get_userid(httpc, buf, sizeof(buf));
        CHECK(ret != NULL && strcmp((char *)buf, "IBMUSER") == 0,
              "http_get_userid == IBMUSER (cred_login fresh read)");

        cred2 = cred_login(addr, (unsigned char *)"IBMUSER", (unsigned char *)"SYS1", 0);
        CHECK(cred2 != NULL, "cred_login #2 (cache hit) succeeds");
        CHECK(cred2 == cred, "cache hit returns the SAME cached CRED object");
        if (cred2) {
            httpc->cred = cred2;
            memset(buf, 0, sizeof(buf));
            ret = http_get_userid(httpc, buf, sizeof(buf));
            CHECK(ret != NULL && strcmp((char *)buf, "IBMUSER") == 0,
                  "http_get_userid == IBMUSER (CACHE-HIT read -- the #109 repro)");
        }
    }

    return mbt_test_summary("TSTGUID");
}
