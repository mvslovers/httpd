/* TSTUNAM.C
** Tests for credunam() -- the check that the ACEE a security system hands
** back belongs to the userid that was asked for -- and for http_stcuser(),
** which refuses the started-task account (both in credentials/src/credunam.c).
**
** credunam.c is free of project headers, so this is a DUAL test: it runs
** natively via `mbt test` and on MVS via `mbt test --mvs`.
*/
#include <stdio.h>
#include <mbtcheck.h>

extern int credunam(const char *want, const unsigned char *name, unsigned namelen);
extern int http_stcuser(const char *user) asm("HTTPSTCU");

#define NAME(s)  ((const unsigned char *)(s)), (sizeof(s) - 1)

int main(void)
{
    printf("=== HTTPD credunam (ACEE identity) tests ===\n");

    /* the ACEE names the user that was asked for */
    CHECK(credunam("ALICE", NAME("ALICE   ")) == 1,    "same name, blank padded -> match");
    CHECK(credunam("HERC01", NAME("HERC01")) == 1,     "same name, no padding -> match");
    CHECK(credunam("ALICE", NAME("ALICE")) == 1,       "length byte without padding -> match");
    CHECK(credunam("alice", NAME("ALICE   ")) == 1,    "requested id in lower case -> match");
    CHECK(credunam("Alice", NAME("ALICE   ")) == 1,    "requested id in mixed case -> match");
    CHECK(credunam("ALICE   ", NAME("ALICE   ")) == 1, "requested id blank padded -> match");
    CHECK(credunam("A", NAME("A       ")) == 1,        "one-character id -> match");

    /* the ACEE names somebody else: the open security system answering for STC */
    CHECK(credunam("ALICE", NAME("STC     ")) == 0,    "STC for ALICE -> mismatch");
    CHECK(credunam("NOSUCH", NAME("STC     ")) == 0,   "STC for unknown id -> mismatch");
    CHECK(credunam("STC", NAME("STCGROUP")) == 0,      "prefix of the ACEE name -> mismatch");
    CHECK(credunam("STCGROUP", NAME("STC     ")) == 0, "ACEE name is a prefix of the id -> mismatch");
    CHECK(credunam("ALICE", NAME("ALICF   ")) == 0,    "one character differs -> mismatch");
    CHECK(credunam("ALICE", NAME("XALICE  ")) == 0,    "leading character added -> mismatch");

    /* only the first eight characters of the requested id count, as in racf_login() */
    CHECK(credunam("ALICEBOBX", NAME("ALICEBOB")) == 1, "id longer than 8 is cut at 8 -> match");
    CHECK(credunam("ALICEBOB", NAME("ALICEBOB")) == 1,  "id of exactly 8 -> match");
    CHECK(credunam("ALICEBOB", NAME("ALICEBOC")) == 0,  "8th character differs -> mismatch");

    /* nothing to compare: never a match, so an empty answer cannot pass */
    CHECK(credunam("", NAME("        ")) == 0,         "empty id, blank ACEE name -> mismatch");
    CHECK(credunam("ALICE", NAME("        ")) == 0,    "blank ACEE name -> mismatch");
    CHECK(credunam("ALICE", (const unsigned char *)"", 0) == 0, "zero-length ACEE name -> mismatch");
    CHECK(credunam("", NAME("ALICE   ")) == 0,         "empty id -> mismatch");
    CHECK(credunam(0, NAME("ALICE   ")) == 0,          "NULL id -> mismatch");
    CHECK(credunam("ALICE", 0, 8) == 0,                "NULL ACEE name -> mismatch");

    /* a length byte above 8 must not read past the 8-byte field */
    CHECK(credunam("ALICE", NAME("ALICE   XYZ")) == 1, "length above 8 -> only 8 characters count");

    /* http_stcuser(): the started-task account is refused (1), nobody else */
    CHECK(http_stcuser("STC") == 1,                    "id is STC -> refused");
    CHECK(http_stcuser("stc") == 1,                    "id is stc -> refused");
    CHECK(http_stcuser("Stc     ") == 1,               "mixed case, blank padded -> refused");
    CHECK(http_stcuser("STCGROUP") == 0,               "STCGROUP is not STC -> not refused");
    CHECK(http_stcuser("STCX") == 0,                   "longer id with the same prefix -> not refused");
    CHECK(http_stcuser("ST") == 0,                     "shorter id -> not refused");
    CHECK(http_stcuser("HERC01") == 0,                 "other id -> not refused");
    CHECK(http_stcuser("") == 0,                       "empty id -> not refused");
    CHECK(http_stcuser(0) == 0,                        "NULL id -> not refused");

    return mbt_test_summary("TSTUNAM");
}
