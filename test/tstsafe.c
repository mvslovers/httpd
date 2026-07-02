/* TSTSAFE.C
** Tests for the string-safety helpers used by the login/redirect path:
**   httpesc()  -- HTML-escape into a bounded buffer (src/httpesc.c)   [S2]
**   httpsrdr() -- safe site-local redirect predicate (src/httpsrdr.c) [S3]
**
** Both helpers are free of httpd.h, so this is a DUAL test: it runs natively
** via `make test-host` (real execution of the edge-case tables) and on MVS via
** `make test-mvs`.  Assertions use character literals / literal strings, which
** the compiler encodes for the target, so they hold under EBCDIC and ASCII.
**
** The real C symbols (httpesc/httpsrdr) are declared directly here so the same
** name resolves on both the host (symbol `httpesc`) and MVS (CSECT `HTTPESC`).
*/
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <mbtcheck.h>

extern unsigned char *httpesc(unsigned char *, size_t, const unsigned char *);
extern int            httpsrdr(const unsigned char *);

/* escape src into a fresh buffer and compare against want */
static int esc_is(const char *src, const char *want)
{
    unsigned char buf[64];
    httpesc(buf, sizeof(buf), (const unsigned char *)src);
    return strcmp((char *)buf, want) == 0;
}

int main(void)
{
    unsigned char small[5];

    printf("=== HTTPD string-safety tests ===\n");

    /* ---- httpesc(): HTML escaping (S2) ---- */
    CHECK(esc_is("abc", "abc"),                 "plain text is unchanged");
    CHECK(esc_is("a<b", "a&lt;b"),              "'<' -> &lt;");
    CHECK(esc_is("a>b", "a&gt;b"),              "'>' -> &gt;");
    CHECK(esc_is("a&b", "a&amp;b"),             "'&' -> &amp;");
    CHECK(esc_is("a\"b", "a&quot;b"),           "'\"' -> &quot;");
    CHECK(esc_is("a'b", "a&#39;b"),             "'\\'' -> &#39;");
    CHECK(esc_is("<>&\"'", "&lt;&gt;&amp;&quot;&#39;"),
                                                "all five escape together");
    CHECK(esc_is("\"><script>", "&quot;&gt;&lt;script&gt;"),
                                                "an XSS breakout is neutralised");
    CHECK(esc_is("", ""),                       "empty input -> empty output");

    /* NULL source is treated as empty, never dereferenced */
    httpesc(small, sizeof(small), NULL);
    CHECK(small[0] == 0,                         "NULL source -> empty, no crash");

    /* truncation is safe: never writes past the buffer, always terminated,
       and never emits a partial entity */
    httpesc(small, sizeof(small), (const unsigned char *)"aaaaaaaa");
    CHECK(strlen((char *)small) < sizeof(small), "over-long plain input truncates safely");
    httpesc(small, sizeof(small), (const unsigned char *)"<<<<");
    CHECK(strcmp((char *)small, "&lt;") == 0,    "entity that fits is emitted whole");
    httpesc(small, 4, (const unsigned char *)"<x");
    CHECK(small[0] == 0,                          "entity that would overflow is dropped, not split");

    /* ---- httpsrdr(): safe site-local redirect (S3) ---- */
    CHECK(httpsrdr((const unsigned char *)"/") == 1,            "\"/\" is safe");
    CHECK(httpsrdr((const unsigned char *)"/foo") == 1,         "\"/foo\" is safe");
    CHECK(httpsrdr((const unsigned char *)"/a/b?x=1") == 1,     "path with query is safe");
    CHECK(httpsrdr((const unsigned char *)"/login") == 1,       "\"/login\" is a safe path");

    CHECK(httpsrdr(NULL) == 0,                                  "NULL is rejected");
    CHECK(httpsrdr((const unsigned char *)"") == 0,             "empty is rejected");
    CHECK(httpsrdr((const unsigned char *)"foo") == 0,          "no leading '/' is rejected");
    CHECK(httpsrdr((const unsigned char *)"http://evil") == 0,  "absolute URL is rejected");
    CHECK(httpsrdr((const unsigned char *)"//evil.com") == 0,   "'//host' (protocol-relative) is rejected");
    CHECK(httpsrdr((const unsigned char *)"/\\evil.com") == 0,  "'/\\host' (backslash) is rejected");
    CHECK(httpsrdr((const unsigned char *)"/a\r\nSet-Cookie: x") == 0,
                                                                "CRLF (header injection) is rejected");
    CHECK(httpsrdr((const unsigned char *)"/a\nb") == 0,        "bare LF is rejected");
    CHECK(httpsrdr((const unsigned char *)"/a\rb") == 0,        "bare CR is rejected");

    return mbt_test_summary("TSTSAFE");
}
