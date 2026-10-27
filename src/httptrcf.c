/* HTTPTRCF.C
** Table logic and line format of the request trace (issue #350).
**
** Kept free of httpd.h and of the clock so it unit-tests dual (host + MVS).
** The glue is httptrc.c.
*/
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include "httptrc.h"

static const char *const mark_name[TM_COUNT] = {
    "q", "run", "line1", "req", "parse", "cred", "route", "lnk0", "lnk1",
    "snd1", "sndl", "done", "end", "close"
};

unsigned
trc_size(unsigned slots)
{
    return (unsigned)sizeof(HTTPTRC)
         + 2 * slots * (unsigned)sizeof(unsigned)
         + slots * (unsigned)sizeof(HTTPTRR);
}

void
trc_init(HTTPTRC *t, unsigned slots)
{
    t->slots = slots;
    t->hsize = 2 * slots;
    t->next  = 0;
    t->seq   = 0;
    t->lost  = 0;
    t->miss  = 0;
    t->hash  = (volatile unsigned *)(t + 1);
    t->rec   = (HTTPTRR *)(t->hash + t->hsize);
}

static unsigned
hash_of(const HTTPTRC *t, void *key)
{
    unsigned h = ((unsigned)(size_t)key >> 4) * 2654435761u;

    return (h >> 8) % t->hsize;
}

/* advance a counter by one and hand back the old value */
static unsigned
bump(volatile unsigned *ctr)
{
    unsigned old;

    do {
        old = *ctr;
    } while (!trc_cas(ctr, old, old + 1));

    return old;
}

static HTTPTRR *
claim(HTTPTRC *t)
{
    unsigned start = bump(&t->next) % t->slots;
    unsigned i;
    int      pass;

    /* a finished request that was fast goes first; a slow one is only
    ** overwritten when nothing else is left, so it survives long enough to
    ** be read.  A record in flight is never taken. */
    for (pass = 0; pass < 2; pass++) {
        for (i = 0; i < t->slots; i++) {
            HTTPTRR  *r = &t->rec[(start + i) % t->slots];
            unsigned s  = r->state;

            if (s == TRS_LIVE) continue;
            if (s == TRS_KEEP && pass == 0) continue;
            if (trc_cas(&r->state, s, TRS_LIVE)) return r;
        }
    }

    return NULL;
}

HTTPTRR *
trc_open(HTTPTRC *t, void *key, unsigned now)
{
    HTTPTRR  *r = claim(t);
    unsigned h;
    unsigned j;
    int      i;

    if (!r) {
        bump(&t->lost);
        return NULL;
    }

    r->seq    = bump(&t->seq) + 1;
    r->httpc  = key;
    r->worker = NULL;
    r->tcb    = NULL;
    r->addr   = 0;
    r->port   = 0;
    r->reqn   = 0;
    r->resp   = 0;
    r->last   = TM_OPEN;
    r->cst    = 0;
    r->polls  = 0;
    r->txn    = 0;
    r->txst   = 0;
    r->txms   = 0;
    r->sent   = 0;
    r->slot   = 0;
    r->path[0] = '\0';
    for (i = 0; i < TM_COUNT; i++) r->t[i] = TRC_UNSET;
    if (now == TRC_UNSET) now--;
    r->t[TM_OPEN] = now;

    /* register the key: the first entry that does not belong to a request in
    ** flight (the record behind an entry names that entry in slot) */
    h = hash_of(t, key);
    for (j = 0; j < TRC_PROBE; j++) {
        unsigned k = (h + j) % t->hsize;
        unsigned e = t->hash[k];

        if (e == 0 || t->rec[e - 1].state != TRS_LIVE
                   || t->rec[e - 1].slot != k + 1) {
            r->slot = (unsigned short)(k + 1);
            if (trc_cas(&t->hash[k], e, (unsigned)(r - t->rec) + 1))
                return r;
            r->slot = 0;
        }
    }

    r->state = TRS_FREE;
    bump(&t->lost);
    return NULL;
}

HTTPTRR *
trc_find(HTTPTRC *t, void *key)
{
    unsigned h = hash_of(t, key);
    unsigned j;

    for (j = 0; j < TRC_PROBE; j++) {
        unsigned e = t->hash[(h + j) % t->hsize];
        HTTPTRR  *r;

        if (e == 0) break;
        r = &t->rec[e - 1];
        if (r->httpc == key && r->state == TRS_LIVE
            && r->slot == ((h + j) % t->hsize) + 1) return r;
    }

    return NULL;
}

void
trc_set(HTTPTRR *r, int mark, unsigned now, int cst)
{
    if (now == TRC_UNSET) now--;
    r->t[mark] = now;
    r->last    = (unsigned char)mark;
    r->cst     = (unsigned char)cst;
}

void
trc_mark(HTTPTRR *r, int mark, unsigned now, int cst)
{
    if (r->t[mark] == TRC_UNSET) trc_set(r, mark, now, cst);
}

void
trc_end(HTTPTRR *r, int mark, unsigned now)
{
    unsigned base;
    int      slow = 0;

    if (r->t[mark] == TRC_UNSET) trc_set(r, mark, now, r->cst);

    /* How long the request itself took.  The first request of a connection is
    ** timed from the accept, a later one from its request line: the wait for
    ** it is the client's think time. */
    base = (r->reqn == 0) ? r->t[TM_OPEN] : r->t[TM_LINE1];
    if (base != TRC_UNSET && r->t[mark] - base >= TRC_SLOW) slow = 1;

    r->state = slow ? TRS_KEEP : TRS_DONE;
}

unsigned
trc_ms(unsigned ticks)
{
    return (ticks / 1000) * 1024 + (ticks % 1000) * 1024 / 1000;
}

/* the CSTATE_* values of httpd.h, which this file does not include */
static const char *
cst_name(int cst)
{
    switch (cst) {
    case 0:  return "IN";
    case 1:  return "PARSE";
    case 2:  return "GET";
    case 3:  return "HEAD";
    case 4:  return "PUT";
    case 5:  return "POST";
    case 6:  return "DELETE";
    case 10: return "DONE";
    case 11: return "REPORT";
    case 12: return "RESET";
    case 13: return "CLOSE";
    }
    return "?";
}

int
trc_line(char *out, int max, const HTTPTRR *r, unsigned now)
{
    int      n  = 0;
    int      i;
    unsigned t0 = r->t[TM_OPEN];
    int      live = (r->state == TRS_LIVE);
    unsigned end = live ? now : r->t[r->last];

#define ADD(...) do { if (n < max) n += snprintf(out + n, max - n, __VA_ARGS__); } while (0)

    ADD("seq=%u c=%06X w=%06X tcb=%06X ip=%u.%u.%u.%u port=%u n=%u resp=%d",
        r->seq, (unsigned)(size_t)r->httpc & 0xFFFFFF,
        (unsigned)(size_t)r->worker & 0xFFFFFF,
        (unsigned)(size_t)r->tcb & 0xFFFFFF,
        r->addr >> 24 & 0xFF, r->addr >> 16 & 0xFF,
        r->addr >> 8 & 0xFF, r->addr & 0xFF,
        (unsigned)r->port, (unsigned)r->reqn, (int)r->resp);
    ADD(" ago=%u live=%d last=%s st=%s age=%u", trc_ms(now - t0), live,
        mark_name[r->last < TM_COUNT ? r->last : 0], cst_name(r->cst),
        trc_ms(end - t0));

    for (i = TM_RUN; i < TM_COUNT; i++) {
        if (r->t[i] == TRC_UNSET) ADD(" %s=-", mark_name[i]);
        else                      ADD(" %s=%u", mark_name[i], trc_ms(r->t[i] - t0));
    }

    ADD(" polls=%u txn=%u txst=%u txms=%u sent=%u path=%s",
        (unsigned)r->polls, (unsigned)r->txn, (unsigned)r->txst,
        trc_ms(r->txms), r->sent, r->path);

#undef ADD

    if (n >= max) n = max - 1;
    return n;
}
