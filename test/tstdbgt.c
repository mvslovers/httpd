/* TSTDBGT.C
** Tests for dbgtitle() -- the bounded title formatter behind dbgdump(),
** dbgenter() and dbgexit() (src/dbgtitle.c, issue #394).
**
** Those three used vsprintf() into an 80- or 256-byte title, so a long
** caller-supplied string wrote past it.  A guard area behind the buffer makes
** an overrun visible.
**
** dbgtitle() is free of httpd.h, so this is a DUAL test.
*/
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "dbgtitle.h"
#include <mbtcheck.h>

#define GUARD 16

static int fmt(char *dst, size_t size, const char *f, ...)
{
    va_list ap;
    int     n;

    va_start(ap, f);
    n = dbgtitle(dst, size, f, ap);
    va_end(ap);
    return n;
}

static int guard_intact(const char *g)
{
    int i;

    for (i = 0; i < GUARD; i++) {
        if (g[i] != 'G') return 0;
    }
    return 1;
}

int main(void)
{
    struct {
        char    title[80];
        char    guard[GUARD];
    } b;
    char longarg[300];
    int  n;

    printf("=== HTTPD dbgtitle tests ===\n");

    memset(longarg, 'x', sizeof(longarg) - 1);
    longarg[sizeof(longarg) - 1] = 0;

    /* a title that fits comes out whole */
    memset(&b, 'G', sizeof(b));
    n = fmt(b.title, sizeof(b.title), "Route #%u (%s)", 7u, "MOD");
    CHECK(strcmp(b.title, "Route #7 (MOD)") == 0, "short title is formatted");
    CHECK(n == (int)strlen("Route #7 (MOD)"), "length of a short title");
    CHECK(guard_intact(b.guard), "short title leaves the guard alone");

    /* a title of exactly size-1 characters fits */
    memset(&b, 'G', sizeof(b));
    n = fmt(b.title, sizeof(b.title), "%.79s", longarg);
    CHECK(n == 79 && strlen(b.title) == 79, "79 characters fit an 80-byte title");
    CHECK(guard_intact(b.guard), "full title leaves the guard alone");

    /* a title that does not fit is cut, terminated, and stays inside */
    memset(&b, 'G', sizeof(b));
    n = fmt(b.title, sizeof(b.title), "%s", longarg);
    CHECK(n == 79, "long title is cut to size-1");
    CHECK(strlen(b.title) == 79, "long title is NUL-terminated inside the buffer");
    CHECK(guard_intact(b.guard), "long title does not write past the buffer");

    memset(&b, 'G', sizeof(b));
    n = fmt(b.title, sizeof(b.title), "%0100d", 1);
    CHECK(n == 79 && guard_intact(b.guard), "wide conversion stays inside");

    /* degenerate sizes */
    memset(&b, 'G', sizeof(b));
    n = fmt(b.title, 1, "%s", "abc");
    CHECK(n == 0 && b.title[0] == 0, "size 1 holds just the NUL");
    CHECK(b.title[1] == 'G', "size 1 writes nothing else");

    memset(&b, 'G', sizeof(b));
    n = fmt(b.title, 0, "%s", "abc");
    CHECK(n == 0 && b.title[0] == 'G', "size 0 writes nothing");

    memset(&b, 'G', sizeof(b));
    n = fmt(b.title, sizeof(b.title), NULL);
    CHECK(n == 0 && b.title[0] == 0, "NULL format gives an empty title");

    return mbt_test_summary("TSTDBGT");
}
