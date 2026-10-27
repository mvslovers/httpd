/* TSTTASK.C
** Tests for httptask() -- the operand check of SET MAXTASK / SET MINTASK
** (src/httptask.c, issue #391).
**
** httptask() is free of httpd.h, so this is a DUAL test: it runs natively via
** `mbt test` and on MVS via `mbt test --mvs`.
*/
#include <stdio.h>
#include <stddef.h>
#include "httptask.h"
#include <mbtcheck.h>

/* returns the code; *v keeps the 0xFFFF marker when the operand is refused */
static int tk(const char *s, unsigned *v)
{
    *v = 0xFFFF;
    return httptask(s, v);
}

int main(void)
{
    unsigned v;

    printf("=== HTTPD httptask (SET MAXTASK/MINTASK operand) tests ===\n");

    /* no operand: strtok(NULL, "") hands the handler a NULL */
    CHECK(tk(NULL, &v) == HTTP_TASK_USAGE && v == 0xFFFF,
          "NULL operand -> usage, value untouched");
    CHECK(tk("", &v) == HTTP_TASK_USAGE && v == 0xFFFF,
          "empty operand -> usage, value untouched");
    CHECK(tk("   ", &v) == HTTP_TASK_USAGE && v == 0xFFFF,
          "blank operand -> usage, value untouched");

    /* a negative number is refused: strtoul() would wrap it to 100 */
    CHECK(tk("-5", &v) == HTTP_TASK_USAGE && v == 0xFFFF,
          "negative number -> usage, value untouched");
    CHECK(tk("  -1", &v) == HTTP_TASK_USAGE && v == 0xFFFF,
          "negative number after blanks -> usage");

    /* in range */
    CHECK(tk("1", &v) == HTTP_TASK_OK && v == 1, "1 -> 1");
    CHECK(tk("9", &v) == HTTP_TASK_OK && v == 9, "9 -> 9");
    CHECK(tk(" 12", &v) == HTTP_TASK_OK && v == 12, "leading blank skipped");
    CHECK(tk("100", &v) == HTTP_TASK_OK && v == 100, "100 -> 100");

    /* the documented clamps stay as they were */
    CHECK(tk("0", &v) == HTTP_TASK_OK && v == 1, "0 -> 1");
    CHECK(tk("101", &v) == HTTP_TASK_OK && v == 100, "101 -> 100");
    CHECK(tk("99999", &v) == HTTP_TASK_OK && v == 100, "99999 -> 100");
    CHECK(tk("abc", &v) == HTTP_TASK_OK && v == 1,
          "a word that is not a number -> 1 (documented)");

    return mbt_test_summary("TSTTASK");
}
