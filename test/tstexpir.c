/* TSTEXPIR.C
** Tests for credexp() -- the credential-reaper expiry decision (M2,
** credentials/src/credexp.c).
**
** credexp() is free of project headers, so this is a DUAL test: it runs
** natively via `make test-host` and on MVS via `make test-mvs`.  It only
** covers the (trivial) boundary logic; the reaper's real risk is the
** concurrency in cred_reap(), which is behavioural / MVS-only.
*/
#include <stdio.h>
#include <mbtcheck.h>

extern int credexp(long elapsed_secs, unsigned ttl_secs);

int main(void)
{
    printf("=== HTTPD credexp (session-timeout) tests ===\n");

    /* ttl == 0 disables reaping: never expired, whatever the age */
    CHECK(credexp(0, 0) == 0,        "ttl 0 -> never (idle 0)");
    CHECK(credexp(1000000, 0) == 0,  "ttl 0 -> never (very idle)");

    /* strict '>' boundary: exactly at the TTL is still live */
    CHECK(credexp(0, 30) == 0,       "idle 0 < ttl 30 -> live");
    CHECK(credexp(29, 30) == 0,      "idle 29 < ttl 30 -> live");
    CHECK(credexp(30, 30) == 0,      "idle 30 == ttl 30 -> live (strict >)");
    CHECK(credexp(31, 30) == 1,      "idle 31 > ttl 30 -> expired");
    CHECK(credexp(100000, 60) == 1,  "far past ttl -> expired");

    /* clock skew: last > now (negative elapsed) is never expired */
    CHECK(credexp(-1, 30) == 0,      "negative elapsed (skew) -> live");
    CHECK(credexp(-100000, 30) == 0, "large negative elapsed -> live");

    /* The hard max-age (#118) reuses this same decision with a different
       clock: the age since cred->created instead of the idle time, and
       SESSION_MAXAGE instead of SESSION_TIMEOUT.  These cases exist so the
       reuse is documented by a test rather than only by a comment -- a change
       to the boundary would break both callers, not just the reaper. */
    CHECK(credexp(3600, 0) == 0,      "maxage 0 -> no max-age (age 1h)");
    CHECK(credexp(28799, 28800) == 0, "age 8h-1s < maxage 8h -> live");
    CHECK(credexp(28800, 28800) == 0, "age 8h == maxage 8h -> live (strict >)");
    CHECK(credexp(28801, 28800) == 1, "age 8h+1s > maxage 8h -> expired");

    return mbt_test_summary("TSTEXPIR");
}
