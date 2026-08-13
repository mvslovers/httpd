/* TSTSTAT.C
** Tests for httpstat() -- the HTTP status code -> status line table
** (src/httpstat.c) that httpresp() puts on the wire.
**
** httpstat() is free of httpd.h, so this is a DUAL test: it runs natively via
** `make test-host` and on MVS via `make test-mvs`.
**
** The regression this exists for (issue #181): a code the table does not know
** is answered with 500, so a missing entry does not fail loudly -- it puts
** "500 Internal Server Error" in front of a body that says something else.
** 505 had been missing that way since httpin.c started emitting it.
*/
#include <stdio.h>
#include <string.h>
#include "httpstat.h"
#include <mbtcheck.h>

/* Does code map to exactly this status line? */
static int is(int code, const char *want)
{
    const char *got = httpstat(code);
    return got != NULL && strcmp(got, want) == 0;
}

/* Every status code httpd itself passes to http_resp().  Produced by
**
**     grep -rhoE "http_resp\(\s*httpc\s*,\s*[0-9]+" --include="*.c" src \
**       credentials | grep -oE "[0-9]+$" | sort -un
**
** and it is the assertion that catches the next 505: a code the server emits
** but cannot spell.  Codes coming from CGI modules stay open-ended -- those
** are what httpresp()'s WTO on the unknown path is for.
*/
static const int emitted[] = {
    200, 302, 303, 400, 401, 403, 404, 405, 413, 414, 500, 501, 503, 505
};

int main(void)
{
    int         i;
    int         code;
    const char *s;
    char        want[8];
    int         missing = 0;
    int         malformed = 0;
    int         known = 0;

    printf("=== HTTPD httpstat (status line) tests ===\n");

    /* -- the three codes issue #181 asked for -------------------------- */
    CHECK(is(206, "206 Partial Content"),     "206 Partial Content");
    CHECK(is(412, "412 Precondition Failed"), "412 Precondition Failed");
    CHECK(is(429, "429 Too Many Requests"),   "429 Too Many Requests");

    /* -- and the one found while adding them: httpin.c answers an
    **    unsupported HTTP version with 505, which had no entry, so
    **    "GET / HTTP/9.9" went out as 500 with a 505 body behind it. */
    CHECK(is(505, "505 HTTP Version Not Supported"),
          "505 HTTP Version Not Supported (emitted by httpin.c)");

    /* -- a sample of the pre-existing table, unchanged ----------------- */
    CHECK(is(200, "200 OK"),                    "200 OK");
    CHECK(is(204, "204 No Content"),            "204 No Content");
    CHECK(is(304, "304 Not Modified"),          "304 Not Modified");
    CHECK(is(401, "401 Unauthorized"),          "401 Unauthorized");
    CHECK(is(404, "404 Not Found"),             "404 Not Found");
    CHECK(is(500, "500 Internal Server Error"), "500 Internal Server Error");
    CHECK(is(507, "507 Insufficient Storage"),  "507 Insufficient Storage");

    /* -- every code httpd emits must have a phrase --------------------- */
    for (i = 0; i < (int)(sizeof(emitted) / sizeof(emitted[0])); i++) {
        if (httpstat(emitted[i]) == NULL) {
            printf("  ...no status line for %d\n", emitted[i]);
            missing++;
        }
    }
    CHECK(missing == 0, "every status code httpd emits has a status line");

    /* -- invariant: a status line starts with its own code, one blank,
    **    then a non-empty reason phrase.  Catches a copy-paste typo such
    **    as case 412 returning "421 ...".  sprintf/strncmp keep the
    **    comparison encoding-safe (no hardcoded digit codes). */
    for (code = 100; code <= 599; code++) {
        s = httpstat(code);
        if (s == NULL) continue;
        known++;
        sprintf(want, "%d ", code);
        if (strncmp(s, want, 4) != 0 || s[4] == '\0') {
            printf("  ...malformed status line for %d: \"%s\"\n", code, s);
            malformed++;
        }
    }
    CHECK(malformed == 0, "each status line is \"<code> <non-empty phrase>\"");
    CHECK(known >= 26, "the table holds all 26 codes (22 original + 4 added)");

    /* -- unknown codes answer NULL, not a guess ------------------------ */
    CHECK(httpstat(418) == NULL, "an unlisted code (418) -> NULL");
    CHECK(httpstat(599) == NULL, "an unlisted code (599) -> NULL");
    CHECK(httpstat(0) == NULL,   "0 -> NULL");
    CHECK(httpstat(-1) == NULL,  "-1 -> NULL");
    CHECK(httpstat(999) == NULL, "999 -> NULL");
    CHECK(httpstat(20) == NULL,  "a truncated code (20) -> NULL");

    return mbt_test_summary("TSTSTAT");
}
