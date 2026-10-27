/* TSTMTTL.C
** Tests for mttline() -- the pure helper of the /.dmtt Master Trace Table
** display (src/mttline.c, issue #426).
**
** /.dmtt wrote every trace entry into a text/html page as it stood, and the
** table holds the console output of every job, not only HTTPD's.  mttline()
** escapes an entry for the HTML mode, passes it through for the text/plain
** mode, bounds the read by the entry length, and never ends its output in the
** middle of an entity.
**
** Free of httpd.h, so this is a DUAL test.
*/
#include <stdio.h>
#include <string.h>
#include "mttline.h"
#include <mbtcheck.h>

typedef unsigned char U;

/* run the whole entry through mttline() in pieces of dstsize and return the
   concatenated text; pieces[] records how many calls it took */
static int whole(const char *src, size_t srclen, int html, size_t dstsize,
                 char *out, size_t outsize, int *calls)
{
    U       buf[64];
    size_t  left = srclen;
    const U *p = (const U *)src;
    size_t  o = 0;
    int     c = 0;

    out[0] = 0;
    while (left) {
        size_t n = mttline(buf, dstsize, p, left, html);
        size_t l = strlen((char *)buf);
        if (n == 0) break;
        if (o + l + 1 > outsize) return -1;
        memcpy(out + o, buf, l + 1);
        o += l;
        p += n;
        left -= n;
        c++;
    }
    if (calls) *calls = c;
    return (int)left;               /* 0 when the whole entry was taken */
}

int main(void)
{
    U       buf[64];
    char    out[256];
    size_t  n;
    int     left, calls;
    const char *mixed = "<<<<<<<<<<&&&&&&&&&&\"\"\"\"\"''''' end";

    printf("=== HTTPD /.dmtt helper tests ===\n");

    /* HTML mode: all five characters in one entry */
    n = mttline(buf, sizeof(buf), (const U *)"a<b>&\"'z", 8, 1);
    CHECK(n == 8, "html: all 8 source bytes taken");
    CHECK(strcmp((char *)buf, "a&lt;b&gt;&amp;&quot;&#39;z") == 0,
          "html: < > & \" ' each become their entity");

    n = mttline(buf, sizeof(buf), (const U *)"<b>x</b>", 8, 1);
    CHECK(strcmp((char *)buf, "&lt;b&gt;x&lt;/b&gt;") == 0,
          "html: <b>x</b> shows as text");

    /* plain mode: as it stands */
    n = mttline(buf, sizeof(buf), (const U *)"a<b>&\"'z", 8, 0);
    CHECK(n == 8, "plain: all 8 source bytes taken");
    CHECK(strcmp((char *)buf, "a<b>&\"'z") == 0,
          "plain: the entry is not escaped");

    /* the length bounds the read, the data need not end in a NUL */
    {
        const char raw[8] = { 'a', 'b', '<', 'c', 'X', 'X', 'X', 'X' };
        n = mttline(buf, sizeof(buf), (const U *)raw, 4, 1);
        CHECK(n == 4, "length: 4 bytes taken of 8 present");
        CHECK(strcmp((char *)buf, "ab&lt;c") == 0,
              "length: nothing behind the entry length is read");
    }

    /* a NUL ends the entry */
    n = mttline(buf, sizeof(buf), (const U *)"ab\0cd", 5, 1);
    CHECK(n == 2, "NUL: stops at the NUL");
    CHECK(strcmp((char *)buf, "ab") == 0, "NUL: text before it kept");

    /* empty */
    n = mttline(buf, sizeof(buf), (const U *)"", 0, 1);
    CHECK(n == 0 && buf[0] == 0, "empty entry: nothing, terminated");

    /* a replacement is whole or absent: dstsize 5 holds 4 characters */
    n = mttline(buf, 5, (const U *)"ab<c", 4, 1);
    CHECK(n == 2 && strcmp((char *)buf, "ab") == 0,
          "bounded: &lt; (4) does not fit behind 2 bytes in 5, left out whole");
    n = mttline(buf, 5, (const U *)"<c", 2, 1);
    CHECK(n == 1 && strcmp((char *)buf, "&lt;") == 0,
          "bounded: &lt; fits exactly in 5");
    n = mttline(buf, 6, (const U *)"\"x", 2, 1);
    CHECK(n == 0 && buf[0] == 0,
          "bounded: &quot; (6) does not fit in 6, nothing written");
    n = mttline(buf, 7, (const U *)"\"x", 2, 1);
    CHECK(n == 1 && strcmp((char *)buf, "&quot;") == 0,
          "bounded: &quot; fits exactly in 7, the next byte is left");
    n = mttline(buf, 8, (const U *)"\"x", 2, 1);
    CHECK(n == 2 && strcmp((char *)buf, "&quot;x") == 0,
          "bounded: both fit in 8");

    /* piecewise through a small buffer: no data lost, no cut entity */
    left = whole(mixed, strlen(mixed), 1, 8, out, sizeof(out), &calls);
    CHECK(left == 0, "pieces: whole entry taken through an 8-byte buffer");
    CHECK(strcmp(out, "&lt;&lt;&lt;&lt;&lt;&lt;&lt;&lt;&lt;&lt;"
                      "&amp;&amp;&amp;&amp;&amp;&amp;&amp;&amp;&amp;&amp;"
                      "&quot;&quot;&quot;&quot;&quot;"
                      "&#39;&#39;&#39;&#39;&#39; end") == 0,
          "pieces: joined text is the same as one call, no entity cut");
    CHECK(calls > 1, "pieces: it did take several calls");

    /* a buffer of 1 holds nothing, 0 does not write */
    n = mttline(buf, 1, (const U *)"a", 1, 1);
    CHECK(n == 0 && buf[0] == 0, "dstsize 1: terminator only");

    return mbt_test_summary("TSTMTTL");
}
