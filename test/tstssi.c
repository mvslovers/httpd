/* TSTSSI.C
** Tests for the pure SSI helpers (src/httpssi.c), issue #401:
**
**   httpssih() -- where a piece of a long line must be held back, so that a
**                 directive split across two reads is still recognised
**   httpssie() -- HTML escaping of the values printenv and echo write
**
** Free of httpd.h, so this is a DUAL test (mbt test and mbt test --mvs).
** String literals are encoded for the target, so the checks hold under
** EBCDIC and ASCII alike.
*/
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <mbtcheck.h>
#include "httpssi.h"

static size_t hold(const char *s)
{
    return httpssih(s, strlen(s), SSI_HOLDMAX);
}

/* escape all of src with a dst of dstsize, pieces joined */
static int esc_is(const char *src, size_t dstsize, const char *want)
{
    char        piece[16];
    char        all[256];
    const char  *s = src;
    size_t      n;

    all[0] = 0;
    while (*s) {
        n = httpssie(piece, dstsize, &s);
        if (n == 0) return 0;
        strcat(all, piece);
    }
    return strcmp(all, want) == 0;
}

int main(void)
{
    char        big[SSI_HOLDMAX + 64];
    char        out[8];
    const char  *s;

    printf("=== HTTPD SSI helper tests ===\n");

    /* ---- httpssih(): nothing to hold ---- */
    CHECK_EQ((int)hold("plain text"), 10,         "plain text is not held");
    CHECK_EQ((int)hold(""), 0,                    "empty piece");
    CHECK_EQ((int)hold("a <!--#echo var=\"X\" --> b"), 25,
                                                  "a complete directive is not held");
    CHECK_EQ((int)hold("<!--#a x --> <!--#b y --> z"), 27,
                                                  "two complete directives are not held");
    CHECK_EQ((int)hold("a <!-- plain comment --> b"), 26,
                                                  "an ordinary comment is not held");

    /* ---- httpssih(): a directive that is not finished ---- */
    CHECK_EQ((int)hold("text <!--#echo var=\"X"), 5,
                                                  "an open directive is held from its start");
    CHECK_EQ((int)hold("text <!--#echo var=\"X\" --"), 5,
                                                  "a split ' -->' holds the directive");
    CHECK_EQ((int)hold("<!--#a x --> <!--#b y"), 14,
                                                  "only the open directive is held");
    CHECK_EQ((int)hold("<!--#"), 0,                "the bare start is held");

    /* ---- httpssih(): a split start ---- */
    CHECK_EQ((int)hold("abc<!"), 3,                "'<!' is held");
    CHECK_EQ((int)hold("abc<!-"), 3,               "'<!-' is held");
    CHECK_EQ((int)hold("abc<!--"), 3,              "'<!--' is held");
    CHECK_EQ((int)hold("abc<"), 3,                 "'<' is held");
    CHECK_EQ((int)hold("abc<!x"), 6,               "'<!x' is not a start");
    CHECK_EQ((int)hold("<!--#a x --> <!-"), 13,    "a split start after a directive is held");

    /* ---- httpssih(): the limit ---- */
    memset(big, 'x', sizeof(big));
    memcpy(big, "<!--#echo var=\"", 15);
    big[sizeof(big) - 1] = 0;
    CHECK_EQ((int)httpssih(big, strlen(big), SSI_HOLDMAX), (int)strlen(big),
             "a directive longer than the limit is not held");
    big[SSI_HOLDMAX] = 0;
    CHECK_EQ((int)httpssih(big, strlen(big), SSI_HOLDMAX), 0,
             "a directive of exactly the limit is held");

    /* ---- httpssie(): escaping ---- */
    CHECK(esc_is("abc", 16, "abc"),               "plain text is unchanged");
    CHECK(esc_is("a<b", 16, "a&lt;b"),            "'<' -> &lt;");
    CHECK(esc_is("a>b", 16, "a&gt;b"),            "'>' -> &gt;");
    CHECK(esc_is("a&b", 16, "a&amp;b"),           "'&' -> &amp;");
    CHECK(esc_is("a\"b", 16, "a&quot;b"),         "'\"' -> &quot;");
    CHECK(esc_is("a'b", 16, "a&#39;b"),           "'\\'' -> &#39;");
    CHECK(esc_is("<script>alert(1)</script>", 16,
                 "&lt;script&gt;alert(1)&lt;/script&gt;"),
                                                  "markup in a value is neutralised");
    CHECK(esc_is("", 16, ""),                     "empty value");
    CHECK(esc_is("<<<<<<<<<<", 8, "&lt;&lt;&lt;&lt;&lt;&lt;&lt;&lt;&lt;&lt;"),
                                                  "a small dst loses nothing");

    /* a piece never overruns dst and never ends inside an entity */
    s = "ab<cd";
    httpssie(out, 5, &s);
    CHECK(strcmp(out, "ab") == 0,                 "an entity that does not fit waits");
    CHECK(strcmp(s, "<cd") == 0,                  "src stops in front of that entity");

    return mbt_test_summary("TSTSSI");
}
