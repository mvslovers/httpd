/* TSTLINK.C
** Tests for http_link() / httplink() failure reporting (issue #131).
**
** Issue #131: a route whose MOD= module was not in the STEPLIB answered
**   503 ... External program MVSMF failed with U0001 ABEND
** and there was no abend to go looking for.  __link() installs a LINK SVC
** error-return address, so a module that cannot be loaded does NOT abend
** S806: MVS takes the error exit, __link() returns -1, __linkds() reports 0
** (nothing abended) and leaves that -1 in prc.  httplink() passed the -1
** straight out and httppcgi() read every negative rc as a negated abend code
** -- 1, below 4095, formatted as the user abend U0001.
**
** The fix keeps "could not be loaded" and "abended" apart on the way out of
** httplink(), using sentinels outside the 0x00sssuuu abend range (clibtry.h).
** These tests pin both halves of that:
**
**   T1  the libc370 contract this all rests on -- __linkds() on a module that
**       does not exist returns 0 (no abend) with prc == -1
**   T2  http_link() maps that to HTTP_LINK_ENOLOAD, and in particular NOT to
**       -1, which is what used to surface as U0001
**   T3  a route dispatched without a program name reports itself as such
**   T4  no sentinel can collide with a real abend code
**   T5  a module that RUNS and returns a negative rc (TSTLNKM, rc -5) comes
**       back folded, not as the negated abend code of a U0005
**
** The cgistart clamp (a negative main() rc becomes 0) is deliberately NOT
** tested here.  cgistart refuses to run without a server context -- RC 12 from
** not_a_server_module() before main() is called (#141) -- so neither a
** standalone step nor a LINK driven from the fabricated HTTPC below ever
** reaches the clamp.  It needs a live server, which is where it was checked.
**
** MVS-only (LINK SVC, httpd.h -> crent370 internals).  HTTP_PRIVATE is defined
** so http_link() resolves to the real HTTPLINK entry (called directly) instead
** of the httpx-vector macro; the body resolves from the [internal] archive.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <mbtcheck.h>

/* a module name that is not in any STEPLIB, LINKLIST or LPA */
#define NOSUCH  "NOSUCHXX"

/* the LINK target that returns a negative rc; keep in sync with tstlnkm.c */
#define PGMRC_MOD   "TSTLNKM"
#define PGMRC_VALUE (-5)

int main(void)
{
    HTTPC   *httpc;
    int     rc;
    int     prc;

    printf("=== HTTPD http_link failure reporting (issue #131) tests ===\n");

    /* --- T1: the libc370 contract.  If this ever changes, T2's mapping is
       reading the wrong signal and the U0001 report comes back. */
    prc = 0;
    rc = __linkds(NOSUCH, NULL, NULL, &prc);
    CHECK_EQ(rc, 0, "__linkds on a missing module does not abend");
    CHECK_EQ(prc, -1, "__linkds leaves __link()'s -1 in prc");

    /* --- T2: httplink() turns that into its own code, not into an abend.
       A zeroed HTTPC is enough: httplink() only reads httpc->httpd (NULL is
       fine, the module never runs) and REQUEST_PATH from the empty env. */
    httpc = (HTTPC *) calloc(sizeof(HTTPC), 1);
    CHECK(httpc != NULL, "HTTPC allocated");
    if (!httpc) return mbt_test_summary("TSTLINK");

    rc = http_link(httpc, NOSUCH);
    CHECK_EQ(rc, HTTP_LINK_ENOLOAD, "missing module reports ENOLOAD");
    CHECK(rc != -1, "missing module is not reported as abend code 1 (U0001)");

    /* --- T3: no program name is its own answer, not a load failure */
    rc = http_link(httpc, NULL);
    CHECK_EQ(rc, HTTP_LINK_ENOPGM, "NULL program name reports ENOPGM");

    rc = http_link(httpc, "");
    CHECK_EQ(rc, HTTP_LINK_ENOPGM, "empty program name reports ENOPGM");

    /* --- T5: a module that runs and returns a negative rc.  TSTLNKM only
       answers with PGMRC_VALUE when it gets a parm, and httplink() builds the
       parm from REQUEST_PATH -- without it the module takes its standalone
       path and returns 0, which would make this test pass for a reason that
       has nothing to do with the fold. */
    rc = http_set_env(httpc, "REQUEST_PATH", "/tstlnkm");
    CHECK_EQ(rc, 0, "REQUEST_PATH set (TSTLNKM needs a parm)");

    rc = http_link(httpc, PGMRC_MOD);
    CHECK(HTTP_LINK_IS_PGMRC(rc), "negative module rc comes back folded");
    CHECK_EQ(HTTP_LINK_PGMRC(rc), PGMRC_VALUE, "the folded rc decodes back");
    CHECK(rc != PGMRC_VALUE,
          "negative module rc is not the negated abend code of a U0005");

    free(httpc);

    /* --- T4: the safety argument for the sentinels.  ___try() formats an
       abend code as 0x00sssuuu, so it can never exceed 0x00FFFFFF; every
       sentinel must sit above that or httppcgi() would decode it as one. */
    CHECK((unsigned)(-HTTP_LINK_ENOLOAD) > 0x00FFFFFF,
          "ENOLOAD is outside the abend code range");
    CHECK((unsigned)(-HTTP_LINK_ENOPGM) > 0x00FFFFFF,
          "ENOPGM is outside the abend code range");
    CHECK((unsigned)(-HTTP_LINK_EESTAE) > 0x00FFFFFF,
          "EESTAE is outside the abend code range");

    /* the folded module-rc range must clear the abend range too, and must not
       swallow the three sentinels */
    CHECK(HTTP_LINK_IS_PGMRC(HTTP_LINK_EPGMRC(-1)),
          "the smallest folded module rc is recognized as one");
    CHECK(!HTTP_LINK_IS_PGMRC(-0x00FFFFFF),
          "the largest abend code is not read as a folded module rc");
    CHECK(!HTTP_LINK_IS_PGMRC(HTTP_LINK_ENOLOAD) &&
          !HTTP_LINK_IS_PGMRC(HTTP_LINK_ENOPGM) &&
          !HTTP_LINK_IS_PGMRC(HTTP_LINK_EESTAE),
          "no sentinel is read as a folded module rc");
    CHECK_EQ(HTTP_LINK_PGMRC(HTTP_LINK_EPGMRC(-0x00FFFFFF)), -0x00FFFFFF,
             "the clamp boundary round-trips through the fold");

    return mbt_test_summary("TSTLINK");
}
