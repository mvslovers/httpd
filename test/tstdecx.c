/* TSTDECX.C
** Tests for httpdecx() -- the percent decoder behind http_decode() and the
** path decoder in httppars (src/httpdecx.c, issue #394).
**
** Found by reading the source: "%zz" went through strtoul() as 0 and "%00"
** is 0, and a 0 byte ends the string, so both cut the rest of the path off;
** and the path was decoded like a form value, which turned a "+" into a space.
**
** The code page is a parameter, so the test passes a table of its own and is
** free of httpd.h: a DUAL test.
*/
#include <stdio.h>
#include <string.h>
#include "httpdecx.h"
#include <mbtcheck.h>

static unsigned char tab[256];

struct row {
    const char *in;
    int         plus;
    const char *want;
    const char *name;
};

/* decode a copy of s */
static void dec(const char *s, int plus, char *out)
{
    strcpy(out, s);
    httpdecx((unsigned char *)out, tab, plus);
}

static const struct row rows[] = {
    /* ordinary decoding */
    { "hello",          1, "hello",       "plain text passes through" },
    { "%41",            1, "Z",           "%41 goes through the table" },
    { "a%20b",          1, "a b",         "%20 decodes between literals" },
    { "%2f%2F",         1, "//",          "upper and lower case hex agree" },
    { "%41%42",         1, "ZB",          "adjacent escapes decode in order" },

    /* '+' : a space in a query value, itself in a path */
    { "a+b",            1, "a b",         "query: '+' becomes a space" },
    { "a+b",            0, "a+b",         "path: '+' stays a '+'" },
    { "a%2Bb+c",        0, "a+b+c",       "path: %2B and '+' both give '+'" },
    { "a%20b",          0, "a b",         "path: %20 still decodes" },

    /* not an escape: kept as written, the rest of the string survives */
    { "/a%zzb",         0, "/a%zzb",      "%zz is kept and does not cut the path" },
    { "/a%4zb",         0, "/a%4zb",      "%4z is kept" },
    { "/a%z4b",         0, "/a%z4b",      "%z4 is kept" },
    { "/a%-1b",         0, "/a%-1b",      "a sign is not a hex digit" },
    { "/a% 4b",         0, "/a% 4b",      "a blank is not a hex digit" },
    { "x%4",            0, "x%4",         "incomplete escape at the end is kept" },
    { "a%",             0, "a%",          "a trailing '%' is kept" },
    { "%",              0, "%",           "a lone '%' is kept" },

    /* %00 would end the string: kept as written */
    { "/etc/x%00.html", 0, "/etc/x%00.html", "%00 is kept and does not cut the path" },
    { "%00",            1, "%00",         "a lone %00 is kept" },
};

int main(void)
{
    char out[64];

    printf("=== HTTPD httpdecx tests ===\n");

    /* a table that is not the identity, so a missing lookup shows; the ASCII
       code of each byte the rows use maps to the compiler's character */
    for (int i = 0; i < 256; i++) tab[i] = (unsigned char)i;
    tab[0x41] = 'Z';
    tab[0x20] = ' ';
    tab[0x2B] = '+';
    tab[0x2F] = '/';
    tab[0x42] = 'B';

    for (size_t n = 0; n < sizeof(rows) / sizeof(rows[0]); n++) {
        dec(rows[n].in, rows[n].plus, out);
        CHECK(strcmp(out, rows[n].want) == 0, rows[n].name);
    }

    /* an entry the table maps to NUL is not decoded either */
    tab[0x7F] = 0;
    dec("a%7Fb", 0, out);
    CHECK(strcmp(out, "a%7Fb") == 0, "an escape the table maps to NUL is kept");

    /* the result never grows */
    dec("%41%42%43", 0, out);
    CHECK(strlen(out) == 3, "three escapes give three bytes");

    return mbt_test_summary("TSTDECX");
}
