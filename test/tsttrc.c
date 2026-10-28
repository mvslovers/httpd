/* TSTTRC.C
** Tests for the request trace table and its line format (issue #350).
**
** httptrcf.c is free of httpd.h and of the clock, so this is a DUAL test.
** trc_cas() is the one thing it leaves to the caller; here a plain compare,
** which is all one thread needs.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "httptrc.h"
#include <mbtcheck.h>

int
trc_cas(volatile unsigned *mem, unsigned expect, unsigned newv)
{
    if (*mem != expect) return 0;
    *mem = newv;
    return 1;
}

/* CHECK does not stop the test: a missing record must fail here, not fault below */
#define NEED(p, msg) \
    do { \
        if ((p) == NULL) { \
            CHECK(0, msg); \
            return mbt_test_summary("TSTTRC"); \
        } \
    } while (0)

#define K(n) ((void *)(size_t)(0x100000 + (n) * 0x1000))

static HTTPTRC *
table(unsigned slots)
{
    HTTPTRC *t = calloc(1, trc_size(slots));

    if (t) trc_init(t, slots);
    return t;
}

int main(void)
{
    HTTPTRC  *t;
    HTTPTRR  *a;
    HTTPTRR  *b;
    HTTPTRR  *r;
    char     line[512];
    unsigned i;
    unsigned slow_seq;
    int      n;

    printf("=== HTTPD request trace (httptrcf) tests ===\n");

    /* ticks to milliseconds: 1.024 ms per tick */
    CHECK_EQ((int)trc_ms(0), 0, "0 ticks are 0 ms");
    CHECK_EQ((int)trc_ms(1), 1, "1 tick rounds down to 1 ms");
    CHECK_EQ((int)trc_ms(977), 1000, "977 ticks are 1000 ms");
    CHECK_EQ((int)trc_ms(1000), 1024, "1000 ticks are 1024 ms");
    CHECK_EQ((int)trc_ms(58000), 59392, "58000 ticks do not overflow");

    t = table(16);
    NEED(t, "the table is allocated");
    CHECK_EQ((int)t->slots, 16, "slots");
    CHECK_EQ((int)t->hsize, 32, "two hash entries per record");

    /* open and find by key */
    a = trc_open(t, K(0), 1000);
    CHECK(a != NULL, "a record is claimed");
    NEED(a, "a record is claimed");
    CHECK_EQ((int)a->seq, 1, "first request is number 1");
    CHECK_EQ((int)a->state, TRS_LIVE, "the record is in flight");
    CHECK_EQ((int)a->t[TM_OPEN], 1000, "open stamps TM_OPEN");
    CHECK_EQ((int)(a->t[TM_RUN] == TRC_UNSET), 1, "other marks are unset");
    CHECK(trc_find(t, K(0)) == a, "the key finds its record");
    CHECK(trc_find(t, K(8)) == NULL, "another key finds nothing");

    /* a mark is set once, a stamp always */
    trc_mark(a, TM_RUN, 1010, 0);
    trc_mark(a, TM_RUN, 1099, 0);
    CHECK_EQ((int)a->t[TM_RUN], 1010, "trc_mark keeps the first tick");
    trc_set(a, TM_SNDL, 1020, 2);
    trc_set(a, TM_SNDL, 1030, 2);
    CHECK_EQ((int)a->t[TM_SNDL], 1030, "trc_set takes the last tick");
    CHECK_EQ((int)a->last, TM_SNDL, "the last mark is remembered");
    CHECK_EQ((int)a->cst, 2, "and the client state with it");

    /* a fast request is free for reuse once it ends */
    trc_end(a, TM_END, 1100);
    CHECK_EQ((int)a->state, TRS_DONE, "a fast request ends as DONE");
    CHECK(trc_find(t, K(0)) == NULL, "a finished record is not found");

    /* keep-alive: the same key opens a new record */
    b = trc_open(t, K(0), 1200);
    CHECK(b != NULL, "the next request of the connection opens");
    NEED(b, "the next request opens");
    CHECK_EQ((int)b->seq, 2, "with the next number");
    CHECK(trc_find(t, K(0)) == b, "and the key finds the new one");
    trc_end(b, TM_CLOSE, 1210);

    /* a slow first request is kept: timed from the accept */
    r = trc_open(t, K(1), 5000);
    NEED(r, "a record is claimed");
    slow_seq = r->seq;
    trc_end(r, TM_END, 5000 + TRC_SLOW);
    CHECK_EQ((int)r->state, TRS_KEEP, "a request of TRC_SLOW ticks is kept");
    r = trc_open(t, K(2), 5000);
    NEED(r, "a record is claimed");
    trc_end(r, TM_END, 5000 + TRC_SLOW - 1);
    CHECK_EQ((int)r->state, TRS_DONE, "one tick less is not");

    /* a later request on a connection is timed from its request line, not
    ** from the wait for it */
    r = trc_open(t, K(3), 8000);
    NEED(r, "a record is claimed");
    r->reqn = 4;
    trc_mark(r, TM_LINE1, 8000 + 50000, 0);
    trc_end(r, TM_END, 8000 + 50010);
    CHECK_EQ((int)r->state, TRS_DONE, "client think time does not make it slow");
    r = trc_open(t, K(4), 8000);
    NEED(r, "a record is claimed");
    r->reqn = 4;
    trc_end(r, TM_CLOSE, 8000 + 50000);
    CHECK_EQ((int)r->state, TRS_DONE, "an idle close is not slow either");

    /* the slow record survives a long run of fast requests */
    for (i = 0; i < 200; i++) {
        r = trc_open(t, K(5 + i % 10), 9000 + i);
        NEED(r, "a record is claimed");
        trc_end(r, TM_END, 9000 + i + 1);
    }
    for (i = 0, r = NULL; i < t->slots; i++)
        if (t->rec[i].seq == slow_seq) r = &t->rec[i];
    CHECK(r != NULL && r->state == TRS_KEEP, "the slow record is still there");

    /* in-flight records are never taken, a full table loses the request */
    free(t);
    t = table(16);
    NEED(t, "the table is allocated");
    for (i = 0; i < 16; i++)
        CHECK(trc_open(t, K(i), 1) != NULL, "fill the table");
    CHECK(trc_open(t, K(100), 1) == NULL, "a full table has no record");
    CHECK_EQ((int)t->lost, 1, "and counts the loss");
    r = trc_find(t, K(5));
    NEED(r, "a live key is found");
    trc_end(r, TM_END, 2);
    CHECK(trc_open(t, K(100), 1) != NULL, "a finished record is reused");

    /* every live key is found again, also with the hash filling up */
    free(t);
    t = table(64);
    NEED(t, "the table is allocated");
    for (i = 0; i < 64; i++)
        CHECK(trc_open(t, K(i), 1) != NULL, "open");
    n = 0;
    for (i = 0; i < 64; i++)
        if (trc_find(t, K(i)) != NULL) n++;
    CHECK_EQ(n, 64, "all 64 live keys are found");

    /* only kept records left: the oldest of them is overwritten, not refused */
    free(t);
    t = table(16);
    NEED(t, "the table is allocated");
    for (i = 0; i < 16; i++) {
        r = trc_open(t, K(i), 100);
        NEED(r, "a record is claimed");
        trc_end(r, TM_END, 100 + 2 * TRC_SLOW);
    }
    r = trc_open(t, K(100), 100);
    CHECK(r != NULL, "a table of kept records still takes a request");
    CHECK_EQ((int)t->lost, 0, "without a loss");

    /* the line */
    free(t);
    t = table(16);
    NEED(t, "the table is allocated");
    a = trc_open(t, (void *)(size_t)0x00A3F2B0, 0xFFFFFFF0u);
    NEED(a, "a record is claimed");
    a->worker = (void *)(size_t)0x00A1C2D0;
    a->tcb    = (void *)(size_t)0x008C1234;
    a->addr   = (10u << 24) | (0u << 16) | (0u << 8) | 2;
    a->port   = 51234;
    a->reqn   = 3;
    strcpy(a->path, "/zosmf/info");
    trc_mark(a, TM_RUN, 0xFFFFFFF0u + 2, 0);
    trc_mark(a, TM_LINK0, 0x00000010u, 2);   /* the tick counter wrapped */
    n = trc_line(line, sizeof(line), a, 0x00000020u);
    CHECK_EQ(n, (int)strlen(line), "the length is the length");
    CHECK(strstr(line, "seq=1 c=A3F2B0 w=A1C2D0 tcb=8C1234 ip=10.0.0.2 port=51234 n=3") == line,
          "identity fields");
    CHECK(strstr(line, " ago=49 live=1 last=lnk0 st=GET age=") != NULL, "live and last");
    CHECK(strstr(line, " run=2 ") != NULL, "an offset in ms");
    CHECK(strstr(line, " lnk0=32 ") != NULL, "the offset survives the wrap");
    CHECK(strstr(line, " age=49 ") != NULL, "a live record ages up to the readout");
    CHECK(strstr(line, " snd1=- ") != NULL, "a mark not reached is a dash");
    CHECK(strstr(line, " path=/zosmf/info") != NULL, "the path closes the line");

    trc_end(a, TM_END, 0x00000030u);
    trc_line(line, sizeof(line), a, 0x00000FFFu);
    CHECK(strstr(line, " live=0 last=end ") != NULL, "a finished record is not live");
    CHECK(strstr(line, " age=65 ") != NULL, "and ages up to its last mark");

    /* a short buffer is cut, never overrun */
    memset(line, 'x', sizeof(line));
    n = trc_line(line, 40, a, 0x00000FFFu);
    CHECK_EQ(n, 39, "a short buffer is filled to its end");
    CHECK_EQ((int)line[39], 0, "and terminated");
    CHECK_EQ((int)line[40], 'x', "without a byte past it");

    free(t);
    return mbt_test_summary("TSTTRC");
}
