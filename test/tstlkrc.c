/* TSTLKRC.C
** Tests for httplkrc() -- the decision that turns the outcome of a LINK
** (__linkdsx() return value, program rc, LINK completion code) into the one
** code http_link() returns (issue #320).
**
** Before #320 a module that was not found and any other LINK failure were the
** same answer (HTTP_LINK_ENOLOAD).  The completion code S806 now sets the
** missing module apart; every other failure keeps ENOLOAD.
**
** httplkrc() is free of httpd.h, so this is a DUAL test.
*/
#include <stdio.h>
#include "httplkrc.h"
#include <mbtcheck.h>

int main(void)
{
    printf("=== HTTPD httplkrc (LINK outcome) tests ===\n");

    /* the new answer: LINK took its error exit with S806 */
    CHECK_EQ(httplkrc(0, -1, 0x806), HTTP_LINK_ENOMOD,
             "S806 -> module not found");

    /* every other LINK failure stays distinguishable from it */
    CHECK_EQ(httplkrc(0, -1, 0x106), HTTP_LINK_ENOLOAD,
             "S106 (fetch error) -> ENOLOAD, not ENOMOD");
    CHECK_EQ(httplkrc(0, -1, 0x80A), HTTP_LINK_ENOLOAD,
             "S80A (no storage) -> ENOLOAD");
    CHECK_EQ(httplkrc(0, -1, 0), HTTP_LINK_ENOLOAD,
             "failure without a completion code -> ENOLOAD");
    CHECK(HTTP_LINK_ENOMOD != HTTP_LINK_ENOLOAD, "the two codes differ");

    /* the S806 completion code only counts when LINK failed */
    CHECK_EQ(httplkrc(0, 0, 0x806), 0, "module rc 0 passes through");
    CHECK_EQ(httplkrc(0, 4, 0), 4, "module rc 4 passes through");

    /* abend: the negated 0x00sssuuu code, whatever else is set */
    CHECK_EQ(httplkrc(0x0000C4000, -1, 0), -0x0000C4000,
             "abend -> negated abend code");
    CHECK_EQ(httplkrc(0x000005, 0, 0x806), -5, "user abend U0005");

    /* __linkdsx() could not set its ESTAE */
    CHECK_EQ(httplkrc(-1, 0, 0), HTTP_LINK_EESTAE, "negative rc -> EESTAE");

    /* a module's own negative rc is folded, not read as an abend */
    CHECK_EQ(httplkrc(0, -5, 0), HTTP_LINK_EPGMRC(-5), "module rc -5 folded");
    CHECK(HTTP_LINK_IS_PGMRC(httplkrc(0, -5, 0)), "folded rc is recognised");
    CHECK_EQ(HTTP_LINK_PGMRC(httplkrc(0, -5, 0)), -5, "fold round-trips");
    CHECK_EQ(httplkrc(0, -0x7FFFFFFF, 0), HTTP_LINK_EPGMRC(-0x00FFFFFF),
             "huge negative rc is clamped");

    /* no sentinel collides with a real abend code (0..0x00FFFFFF) */
    CHECK(HTTP_LINK_ENOMOD < -0x00FFFFFF, "ENOMOD is outside the abend range");

    return mbt_test_summary("TSTLKRC");
}
