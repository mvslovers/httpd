/* TSTEXPIRE.C
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

    return mbt_test_summary("TSTEXPIRE");
}
