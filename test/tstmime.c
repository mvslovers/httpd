/* TSTMIME.C
** Tests for http_mime() / httpmime() -- the file extension to MIME type
** lookup (src/httpmime.c), issue #303.
**
** httpmime() finds the extension with bsearch() in a table of HTTPM entries.
** C99 calls the comparison function as compar(key, element); libc370 up to
** 2.20.0 called it as compar(element, key) and corrects that with
** libc370#388.  A comparator written for one order reads the other order's
** key as a table entry, finds nothing, and every file falls back to
** text/plain, character mode -- an image is then sent through the codepage
** translation.  The lookups below must hold whichever order the linked
** libc.a uses.
**
** The table is sorted in EBCDIC, where digits follow the letters: "svg" comes
** before "sv4cpio".  The lookup is checked across that boundary, at both ends
** of the table, for the binary flag, for the dd:name(member) form and for an
** extension the table does not know.
**
** httpmime() is reached through httpd.h, so like TSTDECO this is an
** MVS-target test (mbt test --mvs); the ASCII collating order of a host build
** would not match the table either.  HTTPMIME resolves from the [internal]
** autocall archive, so only the test root is listed in mbt.toml.
**
** httpmime() traces through http_enter()/http_exit(), and dbgenter() reads
** the HTTPD block from grtapp1 -- the server puts it there, a test step has a
** NULL.  A zeroed block (dbg NULL: no trace) stands in for it.
**
** HTTP_PRIVATE selects the real HTTPMIME entry (called directly) instead of
** the httpx-vector macro.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <mbtcheck.h>

static int
is_type(const UCHAR *path, const char *type, int binary)
{
    const HTTPM *m = http_mime(path);

    if (!m) return 0;
    printf("  %-20s -> %s,%d\n", (const char *)path, (const char *)m->type,
        m->binary);
    return strcmp((const char *)m->type, type) == 0 && m->binary == binary;
}

int main(void)
{
    HTTPD   httpd;

    memset(&httpd, 0, sizeof(httpd));
    __grtget()->grtapp1 = &httpd;

    printf("=== HTTPD http_mime tests ===\n");

    CHECK(is_type((const UCHAR *)"/www/index.html", "text/html", 0),
          "html is text/html, character");
    CHECK(is_type((const UCHAR *)"/www/logo.png", "image/png", 1),
          "png is image/png, binary");
    CHECK(is_type((const UCHAR *)"/www/a.css", "text/css", 0),
          "css is text/css");
    CHECK(is_type((const UCHAR *)"/www/app.js", "application/x-javascript", 0),
          "js is application/x-javascript");

    /* both ends of the table */
    CHECK(is_type((const UCHAR *)"x.ai", "application/postscript", 1),
          "first entry (ai) is found");
    CHECK(is_type((const UCHAR *)"x.zip", "application/zip", 1),
          "last entry (zip) is found");

    /* EBCDIC order: letters before digits */
    CHECK(is_type((const UCHAR *)"x.svg", "image/svg+xml", 0),
          "svg is image/svg+xml");
    CHECK(is_type((const UCHAR *)"x.sv4cpio", "application/x-sv4cpio", 1),
          "sv4cpio (digit after svg) is found");

    /* the last path fragment decides, the member of a dd: name is stripped */
    CHECK(is_type((const UCHAR *)"/a.b/c.gif", "image/gif", 1),
          "extension of the last path fragment");
    CHECK(is_type((const UCHAR *)"dd:x.jpg(mem)", "image/jpeg", 1),
          "dd: name, member stripped");

    /* unknown and missing extensions fall back to text/plain */
    CHECK(is_type((const UCHAR *)"x.nosuch", "text/plain", 0),
          "unknown extension is text/plain");
    CHECK(is_type((const UCHAR *)"/www/README", "text/plain", 0),
          "no extension is text/plain");

    return mbt_test_summary("TSTMIME");
}
