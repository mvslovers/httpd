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

/* decode a copy of s */
static int dec(const char *s, int plus, char *out)
{
    strcpy(out, s);
    httpdecx((unsigned char *)out, tab, plus);
    return (int)strlen(out);
}

int main(void)
{
    char out[64];
    int  i;

    printf("=== HTTPD httpdecx tests ===\n");

    /* a table that is not the identity, so a missing lookup shows */
    for (i = 0; i < 256; i++) tab[i] = (unsigned char)i;
    tab[0x41] = 'Z';
    /* the ASCII code of each byte the checks use, to the compiler's char */
    tab[0x20] = ' ';
    tab[0x2B] = '+';
    tab[0x2F] = '/';
    tab[0x42] = 'B';

    /* ordinary decoding */
    dec("hello", 1, out);
    CHECK(strcmp(out, "hello") == 0, "plain text passes through");

    dec("%41", 1, out);
    CHECK(strcmp(out, "Z") == 0, "%41 goes through the table");

    dec("a%20b", 1, out);
    CHECK(strcmp(out, "a b") == 0, "%20 decodes between literals");

    dec("%2f%2F", 1, out);
    CHECK(strcmp(out, "//") == 0, "upper and lower case hex agree");

    dec("%41%42", 1, out);
    CHECK(strcmp(out, "ZB") == 0, "adjacent escapes decode in order");

    /* '+' : a space in a query value, itself in a path */
    dec("a+b", 1, out);
    CHECK(strcmp(out, "a b") == 0, "query: '+' becomes a space");

    dec("a+b", 0, out);
    CHECK(strcmp(out, "a+b") == 0, "path: '+' stays a '+'");

    dec("a%2Bb+c", 0, out);
    CHECK(strcmp(out, "a+b+c") == 0, "path: %2B and '+' both give '+'");

    dec("a%20b", 0, out);
    CHECK(strcmp(out, "a b") == 0, "path: %20 still decodes");

    /* not an escape: kept as written, the rest of the string survives */
    dec("/a%zzb", 0, out);
    CHECK(strcmp(out, "/a%zzb") == 0, "%zz is kept and does not cut the path");

    dec("/a%4zb", 0, out);
    CHECK(strcmp(out, "/a%4zb") == 0, "%4z is kept");

    dec("/a%z4b", 0, out);
    CHECK(strcmp(out, "/a%z4b") == 0, "%z4 is kept");

    dec("/a%-1b", 0, out);
    CHECK(strcmp(out, "/a%-1b") == 0, "a sign is not a hex digit");

    dec("/a% 4b", 0, out);
    CHECK(strcmp(out, "/a% 4b") == 0, "a blank is not a hex digit");

    dec("x%4", 0, out);
    CHECK(strcmp(out, "x%4") == 0, "incomplete escape at the end is kept");

    dec("a%", 0, out);
    CHECK(strcmp(out, "a%") == 0, "a trailing '%' is kept");

    dec("%", 0, out);
    CHECK(strcmp(out, "%") == 0, "a lone '%' is kept");

    /* %00 would end the string: kept as written */
    dec("/etc/x%00.html", 0, out);
    CHECK(strcmp(out, "/etc/x%00.html") == 0, "%00 is kept and does not cut the path");

    dec("%00", 1, out);
    CHECK(strcmp(out, "%00") == 0, "a lone %00 is kept");

    /* an entry the table maps to NUL is not decoded either */
    tab[0x7F] = 0;
    dec("a%7Fb", 0, out);
    CHECK(strcmp(out, "a%7Fb") == 0, "an escape the table maps to NUL is kept");

    /* the result never grows */
    dec("%41%42%43", 0, out);
    CHECK(strlen(out) == 3, "three escapes give three bytes");

    return mbt_test_summary("TSTDECX");
}
