/* TSTGIVE.C
** Tests for httpgive() -- the flood guard in front of the operator message
** that send_raw_all() writes when it abandons a client whose receive window
** stayed closed for the whole no-progress budget (issue #347).
**
** httpgive() is free of httpd.h, so this is a DUAL test.
*/
#include <stdio.h>
#include "httpgive.h"
#include <mbtcheck.h>

#define IVL 60

int main(void)
{
    unsigned last = 0;
    unsigned held = 0;
    unsigned sup  = 99;
    int      i;
    int      written;

    printf("=== HTTPD httpgive (give-up flood guard) tests ===\n");

    /* the first give-up of a quiet server is written at once */
    CHECK_EQ(httpgive(&last, &held, 1000, IVL, &sup), 1,
             "first give-up is written");
    CHECK_EQ((int)sup, 0, "nothing was suppressed before the first");
    CHECK_EQ((int)last, 1000, "last moves to now");

    /* within the interval every further give-up is only counted */
    CHECK_EQ(httpgive(&last, &held, 1001, IVL, &sup), 0,
             "second give-up one second later is held back");
    CHECK_EQ(httpgive(&last, &held, 1059, IVL, &sup), 0,
             "give-up at interval-1 is held back");
    CHECK_EQ((int)held, 2, "both are counted");
    CHECK_EQ((int)last, 1000, "a held-back give-up does not move last");

    /* the interval is measured from the last message written, so the
    ** boundary itself is open */
    CHECK_EQ(httpgive(&last, &held, 1060, IVL, &sup), 1,
             "give-up at exactly the interval is written");
    CHECK_EQ((int)sup, 2, "and reports the two held back");
    CHECK_EQ((int)held, 0, "the count starts again");
    CHECK_EQ((int)last, 1060, "last moves to now");

    /* a flood: 1000 give-ups inside one interval write nothing more */
    written = 0;
    for (i = 0; i < 1000; i++)
        written += httpgive(&last, &held, 1061 + i % 50, IVL, &sup);
    CHECK_EQ(written, 0, "a flood inside one interval writes nothing");
    CHECK_EQ((int)held, 1000, "all 1000 are counted");

    CHECK_EQ(httpgive(&last, &held, 1200, IVL, &sup), 1,
             "the next interval writes again");
    CHECK_EQ((int)sup, 1000, "with the count of the flood");

    /* a clock set back must not silence the message for good */
    CHECK_EQ(httpgive(&last, &held, 500, IVL, &sup), 1,
             "now before last is treated as an elapsed interval");

    /* the same code on a server that was just started at time zero-ish */
    last = 0; held = 0;
    CHECK_EQ(httpgive(&last, &held, 5, IVL, &sup), 1,
             "a small now still counts as the first give-up");

    return mbt_test_summary("TSTGIVE");
}
