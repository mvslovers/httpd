/* HTTPTRC.C
** Request trace glue (issue #350): the clock, the HTTPD/HTTPC side of the
** hooks and the allocation of the table.  The table logic is httptrcf.c.
**
** Every function here is reached only after the caller saw httpd->trace
** non-NULL (HTTP_TRACING), except start/stop.
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include "httpdmsg.h"
#include <s370/atomic.h>

int
trc_cas(volatile unsigned *mem, unsigned expect, unsigned newv)
{
    unsigned want = expect;

    return __cas((unsigned *)mem, &want, newv) == 0;
}

/* TOD clock in ticks of 1.024 ms (bit 41 of the 64-bit value).  STCK is one
** problem-state instruction on MVS 3.8j; the clock itself ticks in
** microseconds, so the tick is the coarser of the two. */
unsigned
httptrc_now(void)
{
    unsigned long long tod;

    __asm__("STCK\t0(%0)" : : "r" (&tod));

    return (unsigned)(tod >> 22);
}

void
httptrc_start(HTTPD *httpd)
{
    unsigned    slots = httpd->cfg_trace;
    HTTPTRC     *t;

    httpd->trace = NULL;
    if (slots == 0) return;

    t = calloc(1, trc_size(slots));
    if (!t) {
        wtof(MSG_TRACE_NO_STORAGE);
        return;
    }

    trc_init(t, slots);
    wtof(MSG_TRACE_ACTIVE, slots, trc_size(slots));

    /* published last: the hooks test this pointer */
    httpd->trace = t;
}

void
httptrc_stop(HTTPD *httpd)
{
    HTTPTRC     *t = httpd->trace;

    httpd->trace = NULL;
    free(t);
}

static HTTPTRR *
find(HTTPC *httpc)
{
    HTTPTRC     *t = httpc->httpd->trace;
    HTTPTRR     *r = trc_find(t, httpc);

    if (!r) __uinc((void *)&t->miss);

    return r;
}

HTTPTRR *
httptrc_rec(HTTPC *httpc)
{
    return find(httpc);
}

void
httptrc_open(HTTPC *httpc)
{
    HTTPTRR     *r = trc_open(httpc->httpd->trace, httpc, httptrc_now());

    if (r) {
        r->addr = httpc->addr;
        r->port = (unsigned short)httpc->port;
        r->reqn = (unsigned short)httpc->request_count;
        r->cst  = (unsigned char)httpc->state;
    }
}

void
httptrc_run(HTTPC *httpc, void *work)
{
    HTTPTRR     *r = find(httpc);

    if (!r) return;

    r->worker = work;
    r->tcb    = (void *)*(unsigned *)0x21C; /* PSATOLD */
    trc_mark(r, TM_RUN, httptrc_now(), httpc->state);
}

void
httptrc_mark(HTTPC *httpc, int mark)
{
    HTTPTRR     *r = find(httpc);

    if (!r) return;

    if (mark == TM_PARSE && r->path[0] == '\0') {
        UCHAR   *path = http_get_env(httpc, "REQUEST_PATH");
        int     i;

        if (path) {
            for (i = 0; i < TRC_PATH - 1 && path[i]; i++)
                r->path[i] = (path[i] > 0x40) ? path[i] : '?';
            r->path[i] = '\0';
        }
    }

    r->resp = httpc->resp;
    trc_mark(r, mark, httptrc_now(), httpc->state);
}

void
httptrc_poll(HTTPC *httpc)
{
    HTTPTRR     *r = find(httpc);

    if (r && r->polls < 0xFFFF) r->polls++;
}

void
httptrc_tx(HTTPTRR *r, unsigned t0, unsigned t1, int rc)
{
    if (r->txn < 0xFFFF) r->txn++;
    r->txms += t1 - t0;
    if (rc > 0) {
        trc_mark(r, TM_SND1, t1, r->cst);
        trc_set(r, TM_SNDL, t1, r->cst);
    }
}

void
httptrc_stall(HTTPTRR *r)
{
    if (r->txst < 0xFFFF) r->txst++;
}

/* The request is over: close its record and, on a kept-alive connection,
** open the record of the next request in the same worker. */
void
httptrc_next(HTTPC *httpc, int keep)
{
    HTTPTRR     *r = find(httpc);
    HTTPTRR     *n;
    void        *worker;
    void        *tcb;
    unsigned    now;

    if (!r) return;

    now     = httptrc_now();
    worker  = r->worker;
    tcb     = r->tcb;
    r->sent = httpc->sent;
    r->resp = httpc->resp;
    trc_mark(r, TM_END, now, httpc->state);
    trc_end(r, TM_END, now);

    /* r may be handed out again by the open below, so it is not read past
    ** this point */
    if (!keep) return;

    n = trc_open(httpc->httpd->trace, httpc, now);
    if (n) {
        n->addr   = httpc->addr;
        n->port   = (unsigned short)httpc->port;
        n->reqn   = (unsigned short)(httpc->request_count + 1);
        n->worker = worker;
        n->tcb    = tcb;
        n->cst    = CSTATE_IN;
        trc_mark(n, TM_RUN, now, CSTATE_IN);
    }
}

/* The connection is closed: whatever record is still in flight ends here. */
void
httptrc_close(HTTPC *httpc)
{
    HTTPTRC     *t = httpc->httpd->trace;
    HTTPTRR     *r = trc_find(t, httpc);

    if (!r) return;

    r->sent = httpc->sent;
    r->resp = httpc->resp;
    trc_end(r, TM_CLOSE, httptrc_now());
}
