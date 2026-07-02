/* TSTDECO.C
** Tests for http_decode() / httpdeco() -- the in-place URI percent/plus
** decoder (src/httpdeco.c).
**
** Contract exercised:
**   - "+"    decodes to a space
**   - "%xx"  decodes to asc2ebc[<hex>] (the ASCII code is mapped to EBCDIC
**            via the active codepage table), consuming both hex digits
**   - ordinary bytes pass through unchanged
**   - the result is decoded in place and NUL-terminated
**
** EBCDIC note: httpdeco emits asc2ebc[...] for a "%xx" escape, so the expected
** value of a decoded escape MUST be written as asc2ebc[<code>] (never a bare
** hex literal) to stay correct on both cc370 (EBCDIC) and a host build. The
** "+", pass-through and length checks use character literals, which the
** compiler encodes for the target -- so they hold on either platform.
**
** httpdeco() pulls no SVCs, but it is reached here through httpd.h (which drags
** in the crent370 runtime headers), so like TSTGCTX this is an MVS-target test
** (make test-mvs); asc2ebc and HTTPDECO resolve from the [internal] autocall
** archive, so only the test root is listed in project.toml.
**
** HTTP_PRIVATE selects the real HTTPDECO entry (called directly) instead of the
** httpx-vector macro.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <mbtcheck.h>

int main(void)
{
    UCHAR buf[64];

    printf("=== HTTPD http_decode tests ===\n");

    /* pass-through: no escapes, unchanged */
    strcpy((char *)buf, "hello");
    http_decode(buf);
    CHECK(strcmp((char *)buf, "hello") == 0, "plain text passes through");

    /* '+' decodes to a space */
    strcpy((char *)buf, "a+b");
    http_decode(buf);
    CHECK(strcmp((char *)buf, "a b") == 0, "'+' decodes to space");

    /* a single "%xx" escape -> asc2ebc[code], both hex digits consumed */
    strcpy((char *)buf, "%41");
    http_decode(buf);
    CHECK(strlen((char *)buf) == 1, "one escape yields one byte");
    CHECK(buf[0] == asc2ebc[0x41], "%41 decodes to asc2ebc[0x41]");

    /* two adjacent escapes decode independently, in order */
    strcpy((char *)buf, "%41%42");
    http_decode(buf);
    CHECK(strlen((char *)buf) == 2, "two escapes yield two bytes");
    CHECK(buf[0] == asc2ebc[0x41] && buf[1] == asc2ebc[0x42],
          "adjacent escapes decode in order");

    /* mixed literal + escape + literal */
    strcpy((char *)buf, "a%20b");
    http_decode(buf);
    CHECK(strlen((char *)buf) == 3, "mixed input keeps surrounding literals");
    CHECK(buf[0] == 'a' && buf[1] == asc2ebc[0x20] && buf[2] == 'b',
          "%20 decodes between two literals");

    /* lowercase hex digits are accepted */
    strcpy((char *)buf, "%2f");
    http_decode(buf);
    CHECK(buf[0] == asc2ebc[0x2f], "lowercase hex escape decodes");

    /* case-insensitivity: %2F and %2f agree */
    strcpy((char *)buf, "%2F");
    http_decode(buf);
    CHECK(buf[0] == asc2ebc[0x2f], "uppercase hex escape decodes the same");

    /* boundary: a trailing '%' with no hex digits (relates to S5 in the
       refactoring backlog). Current contract: the escape is not advanced,
       the leading literal survives, and no crash occurs. buf is padded so
       the decoder's str[2] look-ahead stays inside this array. When S5 is
       fixed this assertion is updated to the new trailing-'%' contract. */
    memset(buf, 0, sizeof(buf));
    strcpy((char *)buf, "a%");
    http_decode(buf);
    CHECK(buf[0] == 'a', "trailing '%' keeps the preceding literal (no crash)");

    return mbt_test_summary("TSTDECO");
}
