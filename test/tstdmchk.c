/* TSTDMCHK.C
** Tests for dmchunk() and dmesc() -- the pure helpers of the /.dm memory
** display (src/dmchunk.c, issue #394).
**
** dmchunk(): the chunk size was rounded to a word with (n+3) & 0xFC before it
** was cut to 64, so 253..255 came out as 0 (and then as the default 16) and
** 256 and up as a few bytes, instead of the maximum.
** dmesc(): the character column is built into a buffer of fixed size; the
** longest replacement bounds how large that buffer must be.
**
** Both are free of httpd.h, so this is a DUAL test.
*/
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "dmchunk.h"
#include <mbtcheck.h>

int main(void)
{
    char          out[DM_ESC_MAX + 1];
    unsigned long v;
    int           ok;
    int           i;

    printf("=== HTTPD /.dm helper tests ===\n");

    /* the default */
    CHECK(dmchunk(0) == 16, "0 gives the default of 16");

    /* word rounding in range */
    CHECK(dmchunk(1) == 4, "1 rounds up to 4");
    CHECK(dmchunk(4) == 4, "4 stays 4");
    CHECK(dmchunk(5) == 8, "5 rounds up to 8");
    CHECK(dmchunk(16) == 16, "16 stays 16");
    CHECK(dmchunk(61) == 64, "61 rounds up to 64");
    CHECK(dmchunk(64) == 64, "64 stays 64");

    /* above the maximum: cut to it -- 253..255 and 256+ used to wrap */
    CHECK(dmchunk(65) == 64, "65 is cut to 64");
    CHECK(dmchunk(252) == 64, "252 is cut to 64");
    CHECK(dmchunk(253) == 64, "253 is cut to 64 (was 16)");
    CHECK(dmchunk(254) == 64, "254 is cut to 64 (was 16)");
    CHECK(dmchunk(255) == 64, "255 is cut to 64 (was 16)");
    CHECK(dmchunk(256) == 64, "256 is cut to 64 (was 16)");
    CHECK(dmchunk(257) == 64, "257 is cut to 64 (was 4)");
    CHECK(dmchunk(260) == 64, "260 is cut to 64 (was 4)");
    CHECK(dmchunk(4096) == 64, "4096 is cut to 64");
    CHECK(dmchunk(ULONG_MAX) == 64, "ULONG_MAX (a negative number given) is cut to 64");

    /* every input lands on a multiple of 4 in 4..64 */
    ok = 1;
    for (v = 0; v < 2000; v++) {
        int c = dmchunk(v);
        if (c < 4 || c > DM_CHUNK_MAX || (c & 3) != 0) ok = 0;
    }
    CHECK(ok, "0..1999 all give a multiple of 4 in 4..64");

    /* the character column */
    CHECK(dmesc('a', out) == 1 && strcmp(out, "a") == 0, "an ordinary character stands for itself");
    CHECK(dmesc('.', out) == 1 && strcmp(out, ".") == 0, "a dot stands for itself");
    CHECK(dmesc('<', out) == 4 && strcmp(out, "&lt;") == 0, "'<' becomes &lt;");
    CHECK(dmesc('>', out) == 4 && strcmp(out, "&gt;") == 0, "'>' becomes &gt;");
    CHECK(dmesc('&', out) == 5 && strcmp(out, "&amp;") == 0, "'&' becomes &amp;");

    /* no replacement is longer than the documented maximum */
    ok = 1;
    for (i = 0; i < 256; i++) {
        char big[16];
        memset(big, 'G', sizeof(big));
        if (dmesc((unsigned char)i, big) > DM_ESC_MAX) ok = 0;
        if (big[DM_ESC_MAX + 1] != 'G') ok = 0;
    }
    CHECK(ok, "no byte value needs more than DM_ESC_MAX characters");

    return mbt_test_summary("TSTDMCHK");
}
