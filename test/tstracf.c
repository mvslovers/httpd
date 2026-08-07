/* TSTRACF.C
** Tests for httpracf() -- the SAF authorization decision table (src/httpracf.c)
** behind http_check_auth()'s "0 == permitted" contract and the RES= route gate
** in auth_gate() (src/httppc.c).
**
** The case that matters is rc 4.  racf_auth() used to answer 0 for a resource
** with no profile because libc370 set RACHECK flag 0x10 believing it meant
** LOG=NONE (it is DSTYPE=V; libc370 #63).  With the flag corrected the same
** check answers 4, and any test of "rc == 0" / "rc != 0" flips an allowed
** access into a denial -- for a route carrying RES=FACILITY:MVSMF.ACCESS on a
** system without that profile, a 403 on every request.
**
** The -1 cases guard the other half: http_check_auth() answers -1 for an
** unauthenticated request, so the shorthand "rc <= 4" for "0 or 4" would let an
** unauthenticated caller through.  That is why this is a function and not an
** inline comparison.
**
** httpracf() is free of httpd.h, so this is a DUAL test: it runs natively via
** `make test-host` and on MVS via `make test-mvs`.
*/
#include <stdio.h>
#include "httpracf.h"
#include <mbtcheck.h>

int main(void)
{
    printf("=== HTTPD httpracf (SAF authorization rc) tests ===\n");

    /* the two SAF "allowed" answers */
    CHECK(httpracf(HTTP_RACF_PERMITTED) == 1,
          "rc 0 (a profile permits the access) -> allowed");
    CHECK(httpracf(HTTP_RACF_NOTPROT) == 1,
          "rc 4 (resource not protected) -> allowed (libc370 #63)");

    /* the constants must keep naming the SAF codes they claim to name */
    CHECK(HTTP_RACF_PERMITTED == 0, "HTTP_RACF_PERMITTED is SAF rc 0");
    CHECK(HTTP_RACF_NOTPROT == 4, "HTTP_RACF_NOTPROT is SAF rc 4");

    /* refusals: 8 is "access denied", and anything above stays a refusal */
    CHECK(httpracf(8) == 0, "rc 8 (access denied) -> denied");
    CHECK(httpracf(12) == 0, "rc 12 -> denied");
    CHECK(httpracf(16) == 0, "rc 16 -> denied");

    /* not an SAF rc: http_check_auth()'s unauthenticated sentinel.  A `rc <= 4`
       test would call these allowed -- they must not be. */
    CHECK(httpracf(-1) == 0,
          "rc -1 (unauthenticated) -> denied, NOT allowed by a <= 4 test");
    CHECK(httpracf(-8) == 0, "any negative rc -> denied");

    /* the odd values between the SAF multiples of 4 are not allowed answers */
    CHECK(httpracf(1) == 0, "rc 1 -> denied (only 0 and 4 allow)");
    CHECK(httpracf(3) == 0, "rc 3 -> denied");
    CHECK(httpracf(5) == 0, "rc 5 -> denied");

    return mbt_test_summary("TSTRACF");
}
