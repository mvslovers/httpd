/* TSTSHFT.C
** Tests for httpshft() -- the send-buffer shift of file_send() after a
** partial send (issue #440).
**
** httpshft() is free of httpd.h, so this is a DUAL test.
*/
#include <stdio.h>
#include <string.h>
#include "httpshft.h"
#include <mbtcheck.h>

#define CBUF    4000
#define GUARD   16

static unsigned char    area[GUARD + CBUF + GUARD];

static unsigned char *
fill(unsigned len)
{
    unsigned char  *buf = &area[GUARD];
    unsigned        i;

    memset(area, 0xEE, sizeof(area));
    memset(buf, 0, CBUF);
    for (i = 0; i < len; i++) buf[i] = (unsigned char)('a' + i % 26);
    return buf;
}

static int
guards_intact(void)
{
    unsigned    i;

    for (i = 0; i < GUARD; i++) {
        if (area[i] != 0xEE) return 0;
        if (area[GUARD + CBUF + i] != 0xEE) return 0;
    }
    return 1;
}

/* the arithmetic file_send() had before #440, kept to show what it gave */
static void
old_shift(unsigned char *buf, unsigned *pos, unsigned *len)
{
    unsigned    n = *len - *pos;

    if (*pos > 0) {
        if ((int)n > 0) {
            memcpy(buf, &buf[*pos], n);
            memset(&buf[n], 0, CBUF - *len);
            *pos -= n;
            *len -= n;
        }
    }
}

int main(void)
{
    unsigned char  *buf;
    unsigned        pos;
    unsigned        len;
    unsigned        left;
    unsigned        i;
    int             ok;

    printf("=== HTTPD httpshft (send buffer shift) tests ===\n");

    /* the case from the report: a full buffer, 880 bytes went out */
    buf = fill(CBUF);
    pos = 880;
    len = CBUF;
    old_shift(buf, &pos, &len);
    printf("before the fix: len %u pos %u (should be 3120 and 0)\n", len, pos);
    CHECK(pos != 0 && len != 3120, "the old arithmetic left pos/len wrong");

    buf = fill(CBUF);
    left = httpshft(buf, CBUF, 880, CBUF);
    printf("after the fix:  len %u pos 0\n", left);
    CHECK_EQ((int)left, 3120, "3120 bytes are left");
    ok = 1;
    for (i = 0; i < left; i++) {
        if (buf[i] != (unsigned char)('a' + (880 + i) % 26)) ok = 0;
    }
    CHECK(ok, "the unsent bytes moved to the front in order");
    ok = 1;
    for (i = left; i < CBUF; i++) if (buf[i] != 0) ok = 0;
    CHECK(ok, "everything behind them is cleared");
    CHECK(guards_intact(), "nothing outside the buffer was written");

    /* a partial send of a partly filled buffer (last block of a file) */
    buf = fill(1000);
    left = httpshft(buf, CBUF, 250, 1000);
    CHECK_EQ((int)left, 750, "partly filled buffer: 750 left");
    CHECK_EQ((int)buf[0], (int)(unsigned char)('a' + 250 % 26),
             "partly filled buffer: first byte is the first unsent one");
    CHECK_EQ((int)buf[750], 0, "partly filled buffer: tail cleared");
    CHECK(guards_intact(), "partly filled: guards intact");

    /* one byte left */
    buf = fill(CBUF);
    left = httpshft(buf, CBUF, CBUF - 1, CBUF);
    CHECK_EQ((int)left, 1, "one byte left");
    CHECK_EQ((int)buf[0], (int)(unsigned char)('a' + (CBUF - 1) % 26),
             "the last byte is now first");
    CHECK_EQ((int)buf[1], 0, "and the rest is clear");

    /* everything went out */
    buf = fill(CBUF);
    left = httpshft(buf, CBUF, CBUF, CBUF);
    CHECK_EQ((int)left, 0, "pos == len: nothing left");
    ok = 1;
    for (i = 0; i < CBUF; i++) if (buf[i] != 0) ok = 0;
    CHECK(ok, "pos == len: buffer cleared");
    CHECK(guards_intact(), "pos == len: guards intact");

    /* pos beyond len must not read or write outside the buffer */
    buf = fill(100);
    left = httpshft(buf, CBUF, 500, 100);
    CHECK_EQ((int)left, 0, "pos > len: nothing left");
    CHECK(guards_intact(), "pos > len: guards intact");

    /* a len beyond the capacity is taken as the capacity */
    buf = fill(CBUF);
    left = httpshft(buf, CBUF, 100, CBUF + 500);
    CHECK_EQ((int)left, CBUF - 100, "len > size: clamped");
    CHECK(guards_intact(), "len > size: guards intact");

    /* pos 0 keeps the data where it is */
    buf = fill(CBUF);
    left = httpshft(buf, CBUF, 0, CBUF);
    CHECK_EQ((int)left, CBUF, "pos 0: all bytes kept");
    CHECK_EQ((int)buf[0], (int)'a', "pos 0: unchanged");

    /* the result feeds the next pass: shift, then refill behind it */
    buf = fill(CBUF);
    left = httpshft(buf, CBUF, 3999, CBUF);
    CHECK_EQ((int)left + (CBUF - (int)left), CBUF,
             "room for the refill is size - left");

    return mbt_test_summary("TSTSHFT");
}
