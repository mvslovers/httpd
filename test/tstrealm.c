/* TSTREALM.C
** Tests for httprlm() -- the Basic-auth realm built from the SMF ID (#191).
**
** httprlm() is free of project headers, so this is a DUAL test: it runs
** natively via `make test-host` and on MVS via `make test-mvs`.
**
** The cases that matter are the degenerate ones.  __smfid() hands over a fixed
** 4-byte field that is blank padded and NOT NUL-terminated, so dropping either
** bound reads past it; and an empty realm is not a cosmetic problem -- realm is
** a required parameter of the Basic challenge (RFC 7617), and realm="" would
** collapse every protection space on the origin into one.
*/
#include <stdio.h>
#include <string.h>
#include <mbtcheck.h>
#include "httprlm.h"

int main(void)
{
    char buf[HTTP_REALM_MAX];

    printf("=== HTTPD httprlm (Basic realm from SMF ID) tests ===\n");

    /* the ordinary case: a full 4-character id, not NUL-terminated */
    CHECK(strcmp(httprlm("MVSC", buf, sizeof(buf)), "MVSC") == 0,
          "4-character id -> used as is");

    /* blank padded -- the trailing blanks must not reach the browser */
    CHECK(strcmp(httprlm("MVS ", buf, sizeof(buf)), "MVS") == 0,
          "trailing blank trimmed");
    CHECK(strcmp(httprlm("M   ", buf, sizeof(buf)), "M") == 0,
          "single character + blanks trimmed");

    /* the field is 4 bytes and not terminated: nothing beyond it may be read */
    CHECK(strcmp(httprlm("ABCDEFGH", buf, sizeof(buf)), "ABCD") == 0,
          "reads at most 4 characters");

    /* degenerate ids fall back rather than yielding an empty realm */
    CHECK(strcmp(httprlm(NULL, buf, sizeof(buf)), HTTP_REALM_FALLBACK) == 0,
          "NULL id -> fallback");
    CHECK(strcmp(httprlm("    ", buf, sizeof(buf)), HTTP_REALM_FALLBACK) == 0,
          "all-blank id -> fallback");
    CHECK(strcmp(httprlm("", buf, sizeof(buf)), HTTP_REALM_FALLBACK) == 0,
          "empty id -> fallback");

    /* never an empty string, whatever the input */
    CHECK(httprlm(NULL, buf, sizeof(buf))[0] != '\0',
          "result is never empty (realm is required, RFC 7617)");

    /* a short buffer truncates, it does not overflow */
    {
        char small[3];

        CHECK(strcmp(httprlm("MVSC", small, sizeof(small)), "MV") == 0,
              "short buffer truncates to fit");
        CHECK(small[2] == '\0', "short buffer stays terminated");
    }

    /* a zero-length buffer must be left alone, not written to */
    {
        char guard[2];

        guard[0] = 'X';
        guard[1] = 'Y';
        httprlm("MVSC", guard, 0);
        CHECK(guard[0] == 'X', "outlen 0 writes nothing");
    }

    return mbt_test_summary("TSTREALM");
}
