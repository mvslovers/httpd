/* TSTBODY.C
** Tests for httpbody() -- the request-body length classifier (src/httpbody.c,
** RFC 7230 §3.3.3) used by S7's Content-Length enforcement.
**
** httpbody() is free of httpd.h, so this is a DUAL test: it runs natively via
** `make test-host` (real execution of the decision table) and on MVS via
** `make test-mvs`.
*/
#include <stdio.h>
#include "httpbody.h"
#include <mbtcheck.h>

/* classify a Content-Length string; returns the code and sets *n */
static int cls(const char *cl, int te, long max, long *n)
{
    *n = -1;
    return httpbody((const unsigned char *)cl, te, max, n);
}

int main(void)
{
    long n;

    printf("=== HTTPD httpbody (Content-Length) tests ===\n");

    /* no body: absent CL, empty CL, or CL == 0 */
    CHECK(cls(NULL, 0, 3999, &n) == HTTP_BODY_NONE && n == 0,
          "absent Content-Length -> no body (RFC 3.3.3)");
    CHECK(cls("", 0, 3999, &n) == HTTP_BODY_NONE, "empty Content-Length -> no body");
    CHECK(cls("0", 0, 3999, &n) == HTTP_BODY_NONE, "Content-Length 0 -> no body");

    /* read: a valid length within the cap */
    CHECK(cls("100", 0, 3999, &n) == HTTP_BODY_READ && n == 100,
          "\"100\" -> read 100 bytes");
    CHECK(cls("3999", 0, 3999, &n) == HTTP_BODY_READ && n == 3999,
          "length exactly at the cap -> read");
    CHECK(cls("50", 0, 50, &n) == HTTP_BODY_READ && n == 50,
          "cap boundary honoured for a smaller max");

    /* too large: over the cap, and a huge value must not overflow */
    CHECK(cls("4000", 0, 3999, &n) == HTTP_BODY_TOO_LARGE, "one over the cap -> 413");
    CHECK(cls("100", 0, 50, &n) == HTTP_BODY_TOO_LARGE, "over a smaller cap -> 413");
    CHECK(cls("99999999999999999999", 0, 3999, &n) == HTTP_BODY_TOO_LARGE,
          "huge Content-Length -> 413, no overflow");

    /* bad: sign, non-numeric, trailing junk */
    CHECK(cls("-5", 0, 3999, &n) == HTTP_BODY_BAD, "negative -> 400");
    CHECK(cls("abc", 0, 3999, &n) == HTTP_BODY_BAD, "non-numeric -> 400");
    CHECK(cls("12abc", 0, 3999, &n) == HTTP_BODY_BAD, "trailing junk -> 400");
    CHECK(cls("+7", 0, 3999, &n) == HTTP_BODY_BAD, "leading '+' -> 400");

    /* surrounding whitespace is tolerated (in case the header wasn't trimmed) */
    CHECK(cls(" 100 ", 0, 3999, &n) == HTTP_BODY_READ && n == 100,
          "leading/trailing OWS tolerated");

    /* Transfer-Encoding present -> framed elsewhere, don't read as CL body */
    CHECK(cls("100", 1, 3999, &n) == HTTP_BODY_NONE,
          "Transfer-Encoding present -> not a Content-Length body");
    CHECK(cls(NULL, 1, 3999, &n) == HTTP_BODY_NONE,
          "Transfer-Encoding present, no CL -> none");

    return mbt_test_summary("TSTBODY");
}
