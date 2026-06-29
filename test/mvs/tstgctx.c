/* TSTGCTX.C
** Tests for http_cgictx_get() -- the generic per-CGI context registrar.
**
** Verifies invariants 1-4 from issue #70:
**   1. repeated calls with the same eyecatcher return the SAME pointer
**   2. different eyecatchers return DIFFERENT blocks
**   3. a full array returns NULL (no abend, no overwrite)
**   4. a freshly created block is zeroed and stamped with the eyecatcher
**
** lock()/__getm() are MVS SVCs, so this is an MVS-only test (make test-mvs).
** HTTP_PRIVATE is defined so http_cgictx_get() resolves to the real HTTPGCTX
** entry (called directly) instead of the httpx-vector macro.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include <mbtcheck.h>

int main(void)
{
    HTTPD   httpd;
    void    *p1, *p1b, *p2, *p3, *p4, *p5;
    int     zeroed;
    int     i;

    printf("=== HTTPD http_cgictx_get tests ===\n");

    /* minimal server: a small CGI context array (cfg_cgictx 3 -> 4 slots) */
    memset(&httpd, 0, sizeof(httpd));
    httpd.cfg_cgictx = 3;
    httpd.cgictx = (void **) calloc(httpd.cfg_cgictx + 1, sizeof(void *));
    CHECK(httpd.cgictx != NULL, "cgictx array allocated");
    if (!httpd.cgictx) return mbt_test_summary("TSTGCTX");

    /* create a first block */
    p1 = http_cgictx_get(&httpd, "TSTCTX01", 32);
    CHECK(p1 != NULL, "create returns a non-NULL block");

    /* invariant 4: eyecatcher stamped in bytes 0-7 */
    CHECK(p1 && memcmp(p1, "TSTCTX01", 8) == 0,
          "new block carries the eyecatcher in bytes 0-7");

    /* invariant 4: bytes past the eyecatcher are zeroed */
    zeroed = (p1 != NULL);
    for (i = 8; p1 && i < 32; i++) {
        if (((unsigned char *)p1)[i] != 0) zeroed = 0;
    }
    CHECK(zeroed, "new block is zeroed past the eyecatcher");

    /* invariant 1: same eyecatcher -> same pointer */
    p1b = http_cgictx_get(&httpd, "TSTCTX01", 32);
    CHECK(p1b == p1, "same eyecatcher returns the same pointer");

    /* invariant 2: a different eyecatcher -> a different block */
    p2 = http_cgictx_get(&httpd, "TSTCTX02", 32);
    CHECK(p2 != NULL && p2 != p1, "different eyecatcher returns a different block");

    /* fill the remaining slots (slots 2 and 3 of 0..3) */
    p3 = http_cgictx_get(&httpd, "TSTCTX03", 32);
    p4 = http_cgictx_get(&httpd, "TSTCTX04", 32);
    CHECK(p3 != NULL && p4 != NULL && p3 != p4,
          "remaining slots allocate distinct blocks");

    /* invariant 3: array full -> NULL, no abend */
    p5 = http_cgictx_get(&httpd, "TSTCTX05", 32);
    CHECK(p5 == NULL, "full array returns NULL");

    /* invariant 3: a full array must not overwrite existing entries */
    CHECK(http_cgictx_get(&httpd, "TSTCTX01", 32) == p1,
          "existing entry still found after the array filled");

    return mbt_test_summary("TSTGCTX");
}
