/* TSTSESS.C
** Test for http_new_session() (HTTPNSES, src/httpxauth.c) and cred_session()
** (CREDSESS, credentials/src/credsess.c) -- issue #288.
**
** Two clients logging on with the same userid and password from the same
** address shared ONE credential: cred_login() finds it by CREDID (address,
** userid, password) and hands the cached CRED back, token included.  A logout
** deleted that CRED, so the other client's next request answered 401.
**
** The fix gives each explicit login a private session -- its own CRED, token
** and ACEE -- that cred_find_by_id() never returns.  This test drives the real
** export on a live HTTPC the way mvsMF's login handler does:
**   - the Basic cache itself is unchanged: two cred_login()s, one CRED;
**   - two sessions from it get two different tokens, neither the cache's;
**   - logging one out leaves the other resolving (the #288 assertion), and the
**     Basic cache CRED with it;
**   - a session still answers http_get_password() (the job card, #164);
**   - CRED_SESSION_MAX: at the limit a login is refused while every session
**     was used within CRED_SESSION_IDLE, and otherwise ends the client's idle
**     one -- never another client's;
**   - no credential, or one without a password, gets no session;
**   - the result says why (#367): -1 nothing to log on with, -2 RACF refused,
**     -3 the client's sessions are all in use; a refusal marks the cache CRED
**     stale without freeing it; cred_login() never serves it and keeps it for
**     CRED_STALE_GRACE (asking RACF afresh, which may take it back), then
**     drops it, as does the reaper;
**   - http_logout() gives the slot back: an ended session no longer counts
**     against the limit.
**
** MVS-only (blowfish + RACF SVCs), and the RACF part needs APF like TSTGUID;
** without it that part is skipped, not failed.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <mvs/apf.h>    /* __autask() -- APF-authorise for racf_login() */
#include <mbtcheck.h>

#undef array_count

/* sessions in the store that carry this CREDID */
static unsigned
sessions_of(CRED ***array, CREDID *id)
{
    unsigned n, count, found = 0;

    count = array_count(array);
    for (n = 1; n <= count; n++) {
        CRED *c = array_get(array, n);

        if (c && (c->flags & CRED_FLAG_SESSION) &&
            memcmp(&c->id, id, sizeof(CREDID)) == 0) found++;
    }
    return found;
}

#define LOGIN_NOCRED  (-100)    /* cred_login() found nothing */
#define LOGIN_NOTOKEN (-101)    /* the session has no token to hand out */

/* log on through the Basic cache and turn it into a session, as a login does.
   Returns 0, or what http_new_session() returned. */
static int
login(HTTPC *httpc, unsigned addr, CREDTOK *tok)
{
    unsigned char ibmuser[] = "IBMUSER";
    unsigned char sys1[]    = "SYS1";
    int rc;

    httpc->addr = addr;
    httpc->cred = cred_login(addr, ibmuser, sys1, 0);
    if (!httpc->cred) return LOGIN_NOCRED;
    rc = http_new_session(httpc);
    if (rc != 0) return rc;
    return http_get_token(httpc, tok->c, sizeof(tok->c)) ==
           (int)sizeof(CREDTOK) ? 0 : LOGIN_NOTOKEN;
}

int main(void)
{
    HTTPD    *httpd;
    HTTPC    *a;
    HTTPC    *b;
    HTTPC    *x;
    CRED     ***array;
    CRED     *shared;
    CRED     *bare;
    CRED     *victim;
    CRED     *stale;
    CRED     *again;
    CREDID    id;
    CREDTOK   tokn;
    time64_t  zero;
    CREDTOK   tokstale;
    CREDID    noid;
    CREDTOK   toka;
    CREDTOK   tokb;
    CREDTOK   tokx;
    CREDTOK   tok[CRED_SESSION_MAX];
    unsigned char buf[16];
    unsigned char ibmuser[] = "IBMUSER";
    unsigned char sys1[]    = "SYS1";
    unsigned char oldpw[]   = "OLDPW";
    unsigned  addr  = 0xC0A8017B;   /* 192.168.1.123 */
    unsigned  addr2 = 0xC0A8017C;   /* another client, same user */
    unsigned  n, alive;
    int       rc;

    memset(&zero, 0, sizeof(zero));

    printf("=== HTTPD private login sessions (issue #288) tests ===\n");

    rc = cred_init(NULL, 0);
    CHECK(rc == 0, "cred_init() succeeds");
    if (rc) return mbt_test_summary("TSTSESS");

    httpd = (HTTPD *) calloc(1, sizeof(HTTPD));
    a     = (HTTPC *) calloc(1, sizeof(HTTPC));
    b     = (HTTPC *) calloc(1, sizeof(HTTPC));
    x     = (HTTPC *) calloc(1, sizeof(HTTPC));
    CHECK(httpd && a && b && x, "HTTPD + HTTPCs allocated");
    if (!httpd || !a || !b || !x) return mbt_test_summary("TSTSESS");

    /* what httpd caches at cred_init() time, and what the export must use */
    httpd->credkey = credkey();
    httpd->credarr = array = cred_array();
    a->httpd = b->httpd = x->httpd = httpd;

    /* --- no session without a password credential (no RACF needed) ------- */
    CHECK(http_new_session(a) == HTTP_SESSION_NONE,
          "no credential -> no session (-1)");

    credid_init(&noid);                      /* passflg clear: no password */
    bare = cred_new(&noid, NULL, NULL, 0);
    a->cred = bare;
    CHECK(http_new_session(a) == HTTP_SESSION_NONE,
          "credential without password -> -1");
    CHECK(a->cred == bare, "a refused session leaves httpc->cred alone");
    cred_free(&bare);
    a->cred = NULL;

    rc = __autask();        /* APF-authorise this task for racf_login() */
    wtof("TSTSESS: __autask() rc=%d", rc);
    if (rc != 0) {
        printf("  SKIP: no APF (rc=%d) -> RACF session paths skipped\n", rc);
        return mbt_test_summary("TSTSESS");
    }

    /* --- the Basic cache is unchanged by design --------------------------- */
    shared = cred_login(addr, ibmuser,
                        sys1, 0);
    CHECK(shared != NULL, "cred_login succeeds");
    if (!shared) return mbt_test_summary("TSTSESS");
    CHECK(cred_login(addr, ibmuser,
                     sys1, 0) == shared,
          "a second cred_login() is the same cached CRED");

    /* --- two logins, two sessions ----------------------------------------- */
    CHECK(login(a, addr, &toka) == 0, "login A gets a session");
    CHECK(login(b, addr, &tokb) == 0, "login B gets a session");
    CHECK(a->cred != shared && b->cred != shared && a->cred != b->cred,
          "each session is a CRED of its own");
    CHECK(memcmp(&toka, &tokb, sizeof(CREDTOK)) != 0,
          "A and B hold different tokens");
    CHECK(memcmp(&toka, &shared->token, sizeof(CREDTOK)) != 0 &&
          memcmp(&tokb, &shared->token, sizeof(CREDTOK)) != 0,
          "neither is the Basic cache's token");
    CHECK(a->cred->acee && a->cred->acee != shared->acee,
          "a session holds an ACEE of its own");
    CHECK(cred_find_by_id(&shared->id) == shared,
          "the CREDID lookup still finds the cache CRED, never a session");

    memset(buf, 0, sizeof(buf));
    CHECK(http_get_password(b, buf, sizeof(buf)) &&
          strcmp((char *)buf, "SYS1") == 0,
          "http_get_password() works on a session (#164)");
    memset(buf, 0, sizeof(buf));

    /* --- the #288 assertion ----------------------------------------------- */
    CHECK(http_logout(a) == 0, "logout A succeeds");
    CHECK(cred_find_by_token(&toka) == NULL, "A's token is dead");
    CHECK(cred_find_by_token(&tokb) == b->cred,
          "B's token still resolves after A logged out (#288)");
    CHECK(cred_find_by_id(&shared->id) == shared,
          "the Basic cache CRED survives the logout too");

    /* --- CRED_SESSION_MAX ------------------------------------------------- */
    CHECK(login(x, addr2, &tokx) == 0, "another client (other address) logs on");

    /* B is session 1; fill up to the limit */
    for (n = 0; n < CRED_SESSION_MAX - 1; n++) {
        rc = login(a, addr, &tok[n]);
        CHECK(rc == 0, "a further login of the same client gets a session");
    }
    CHECK(sessions_of(array, &shared->id) == CRED_SESSION_MAX,
          "the client now holds CRED_SESSION_MAX sessions");

    /* all of them were used this second: none may be ended under a request
       that could still hold it (CRED_SESSION_IDLE), so the login is refused */
    CHECK(login(a, addr, &tok[CRED_SESSION_MAX - 1]) == HTTP_SESSION_BUSY,
          "at the limit with every session recently used, the login is refused (-3)");
    CHECK(sessions_of(array, &shared->id) == CRED_SESSION_MAX,
          "and no session was ended for it");

    /* age one of them past CRED_SESSION_IDLE: that is the one to go */
    victim = cred_find_by_token(&tok[3]);
    CHECK(victim != NULL, "session 4 is alive before the next login");
    if (victim) memset(&victim->last, 0, sizeof(victim->last));

    CHECK(login(a, addr, &tok[CRED_SESSION_MAX - 1]) == 0,
          "with an idle session, the next login gets a session");
    CHECK(sessions_of(array, &shared->id) == CRED_SESSION_MAX,
          "one client never holds more than CRED_SESSION_MAX sessions");
    CHECK(cred_find_by_token(&tok[3]) == NULL,
          "the idle session is the one that was ended");
    alive = cred_find_by_token(&tokb) ? 1 : 0;
    for (n = 0; n < CRED_SESSION_MAX; n++) {
        if (cred_find_by_token(&tok[n])) alive++;
    }
    CHECK(alive == CRED_SESSION_MAX,
          "every other session of the client is still alive");
    CHECK(cred_find_by_token(&tokx) == x->cred,
          "the other client's session was not touched");

    /* --- an ended session gives its slot back (#367) ---------------------- */
    /* a->cred is the session just minted.  Every other session is still
       recent, so a session can only be had if the logout freed a slot -- no
       idle one is there to be evicted. */
    CHECK(http_logout(a) == 0, "logout of the newest session succeeds");
    CHECK(cred_find_by_token(&tok[CRED_SESSION_MAX - 1]) == NULL,
          "the logged-out session is gone from the store");
    CHECK(sessions_of(array, &shared->id) == CRED_SESSION_MAX - 1,
          "and no longer counts against the limit");
    CHECK(login(a, addr, &tokn) == 0,
          "so the next login gets a session without ending another one");
    CHECK(sessions_of(array, &shared->id) == CRED_SESSION_MAX,
          "the client is back at CRED_SESSION_MAX, not above it");
    credtok_logout_arr(array, &tokn);

    /* --- clean up: every session and the cache CRED ----------------------- */
    credtok_logout_arr(array, &tokb);
    credtok_logout_arr(array, &tokx);
    for (n = 0; n < CRED_SESSION_MAX; n++) credtok_logout_arr(array, &tok[n]);
    CHECK(sessions_of(array, &shared->id) == 0, "all sessions logged out");
    credtok_logout_arr(array, &shared->token);

    /* --- RACF refuses the cached password (#367) -------------------------- */
    /* a Basic cache entry whose password RACF no longer accepts is what a
       changed password leaves behind: cred_login() answers from the cache and
       never asks RACF.  The CRED is built by hand, with no ACEE. */
    id = credid_init(NULL);
    credid_update(&id, &addr, ibmuser, oldpw);
    credid_enc(&id, &id);
    tokstale = credtok_rand(httpd->credkey, &id);
    stale = cred_new(&id, &tokstale, NULL, 0);
    CHECK(stale != NULL && cred_add(stale) == 0, "a cache entry with a dead password");
    if (!stale) return mbt_test_summary("TSTSESS");
    CHECK(cred_login(addr, ibmuser, oldpw, 0) == stale,
          "the cache answers with it, RACF is not asked");

    a->addr = addr;
    a->cred = stale;
    CHECK(http_new_session(a) == HTTP_SESSION_REFUSED,
          "RACF refuses the logon -> -2");
    CHECK(a->cred == stale, "httpc->cred is left alone");
    CHECK(cred_find_by_token(&tokstale) == stale,
          "the entry the request holds is not freed under it");
    CHECK((stale->flags & CRED_FLAG_STALE) != 0, "it is marked stale");
    CHECK(difftime64(stale->stale, zero) > 0, "with the time it was marked");
    CHECK(sessions_of(array, &stale->id) == 0, "and no session was made");

    /* a marked entry is never served, and not freed within the grace period
       either: the lookup that finds it refreshes `last`, the floor is the
       mark.  cred_login() asks RACF, which refuses -> NULL */
    n = array_count(array);
    again = cred_login(addr, ibmuser, oldpw, 0);
    CHECK(again == NULL, "the old password is no longer accepted from the cache");
    CHECK(cred_find_by_token(&tokstale) == stale,
          "the marked entry is kept within the grace period");
    CHECK((stale->flags & CRED_FLAG_STALE) != 0 && array_count(array) == n,
          "still marked, nothing added");

    /* the reaper keeps it within the grace period whatever its `last` says,
       and frees it afterwards -- the idle TTL given is far beyond its age */
    memset(&stale->last, 0, sizeof(stale->last));
    CHECK(cred_reap(2000000000U, 0) == 0,
          "a fresh stale entry is not reaped, even with an ancient `last`");
    CHECK(cred_find_by_token(&tokstale) == stale, "it is still there");
    stale->stale = zero;                    /* marked long ago */
    stale->last  = time64(NULL);
    CHECK(cred_reap(2000000000U, 0) == 1,
          "a stale entry past the grace period is reaped, even with a fresh `last`");
    CHECK(cred_find_by_token(&tokstale) == NULL, "and gone");

    /* the other way out: past the grace period the next cred_login() drops it
       and asks RACF, even with SESSION_MAXAGE 0 (never) */
    stale = cred_new(&id, &tokstale, NULL, 0);
    CHECK(stale != NULL && cred_add(stale) == 0, "a second dead-password entry");
    if (!stale) return mbt_test_summary("TSTSESS");
    a->cred = stale;
    CHECK(http_new_session(a) == HTTP_SESSION_REFUSED, "refused again -> -2");
    stale->stale = zero;                    /* marked long ago */
    again = cred_login(addr, ibmuser, oldpw, 0);
    CHECK(again == NULL, "an aged stale entry is not served");
    CHECK(cred_find_by_token(&tokstale) == NULL, "and was dropped");

    /* RACF accepts the password again (set back): the marked entry is cleared
       and returned, not duplicated */
    shared = cred_login(addr, ibmuser, sys1, 0);
    CHECK(shared != NULL, "a fresh cache entry with a good password");
    if (!shared) return mbt_test_summary("TSTSESS");
    shared->flags |= CRED_FLAG_STALE;
    shared->stale  = time64(NULL);
    n = array_count(array);
    again = cred_login(addr, ibmuser, sys1, 0);
    CHECK(again == shared, "RACF accepts: the marked entry is taken back");
    CHECK((shared->flags & CRED_FLAG_STALE) == 0 && difftime64(shared->stale, zero) == 0,
          "mark and time are cleared");
    CHECK(array_count(array) == n, "no second CRED for the same credentials");
    credtok_logout_arr(array, &shared->token);

    return mbt_test_summary("TSTSESS");
}
