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

    return mbt_test_summary("TSTLINK");
}
