#ifndef HTTPTRC_H
#define HTTPTRC_H
/* HTTPTRC.H -- per-request server trace (issue #350)
**
** A small fixed ring of records, one per request, filled while the request
** runs.  Each record holds the worker that serves it, a tick per phase and a
** few counters, so a request that stalls can be attributed to a worker and a
** phase afterwards.  Off unless the Parmlib says TRACE=n; then HTTPD->trace
** is NULL and every hook costs one load and a branch.
**
** This header and httptrcf.c are free of httpd.h so the table logic and the
** line format unit-test dual (host + MVS).  The glue that reads the clock and
** knows HTTPC is httptrc.c.
**
** Nothing in here is writable module storage (#197): the table is one block
** from the heap, anchored in the HTTPD block.
**
** Tick: 2**10 microseconds = 1.024 ms, the TOD clock shifted right by 22 bits
** and kept in 32 bits (wraps after about 51 days; only differences are used).
*/

#define TRC_SLOTS_MIN   16          /* smallest TRACE=n                     */
#define TRC_SLOTS_MAX   1024        /* largest TRACE=n                      */
#define TRC_PROBE       8           /* hash probes before a request is lost */
#define TRC_UNSET       0xFFFFFFFFu /* a mark the request never reached     */
#define TRC_SLOW        977         /* ticks, about 1 s: a finished request */
                                    /* ... this slow is kept longer         */
#define TRC_PATH        24          /* bytes of the request path kept       */

/* marks, in the order a request normally reaches them */
enum trcmark {
    TM_OPEN=0,                      /* accepted and queued / next request   */
    TM_RUN,                         /* a worker took the connection         */
    TM_LINE1,                       /* request line read                    */
    TM_REQ,                         /* request headers read                 */
    TM_PARSE,                       /* request parsed (body of a POST read) */
    TM_CRED,                        /* credential resolved (RACF logon)     */
    TM_ROUTE,                       /* route found and auth gate passed     */
    TM_LINK0,                       /* about to LINK the module             */
    TM_LINK1,                       /* back from the module                 */
    TM_SND1,                        /* first byte went to the stack         */
    TM_SNDL,                        /* last byte went to the stack          */
    TM_DONE,                        /* http_done(): response complete       */
    TM_END,                         /* reported, about to reset             */
    TM_CLOSE,                       /* connection closed under the record   */
    TM_COUNT
};

/* record state; claimed with compare-and-swap */
enum trcstate {
    TRS_FREE=0,                     /* never used                           */
    TRS_LIVE,                       /* request in flight                    */
    TRS_DONE,                       /* finished, may be reused at once      */
    TRS_KEEP                        /* finished slow, reused last           */
};

typedef struct httptrr HTTPTRR;
struct httptrr {
    volatile unsigned state;        /* 00 TRS_*                             */
    unsigned    seq;                /* 04 request number, from 1            */
    void        *httpc;             /* 08 key: the client block             */
    void        *worker;            /* 0C CTHDWORK, as in WORKER(xxxxxx)    */
    void        *tcb;               /* 10 TCB of that worker                */
    unsigned    addr;               /* 14 client IP address                 */
    unsigned short port;            /* 18 client port                       */
    unsigned short reqn;            /* 1A request number on the connection  */
    short       resp;               /* 1C response code at the last mark    */
    unsigned char last;             /* 1E last mark reached (TM_*)          */
    unsigned char cst;              /* 1F CSTATE_* at that mark             */
    unsigned short polls;           /* 20 0.5 s waits for request data      */
    unsigned short txn;             /* 22 send() calls                      */
    unsigned short txst;            /* 24 pauses on a closed window         */
    unsigned short slot;            /* 26 hash entry of the key, index + 1  */
    unsigned    txms;               /* 28 ticks spent inside send()         */
    unsigned    sent;               /* 2C bytes sent for the request        */
    unsigned    t[TM_COUNT];        /* 30 tick per mark, TRC_UNSET = none   */
    char        path[TRC_PATH];     /* request path, cut                    */
};

typedef struct httptrc HTTPTRC;
struct httptrc {
    unsigned    slots;              /* 00 records                           */
    unsigned    hsize;              /* 04 hash entries                      */
    volatile unsigned next;         /* 08 where the next claim starts       */
    volatile unsigned seq;          /* 0C last request number given         */
    volatile unsigned lost;         /* 10 requests that got no record       */
    volatile unsigned miss;         /* 14 hooks that found no record (glue) */
    HTTPTRR     *rec;               /* 18 the records                       */
    volatile unsigned *hash;        /* 1C record index + 1 per entry        */
};                                  /* 20 (followed by hash and records)    */

/* trc_cas() -- compare and swap of one word: 1 if *mem was expect and now is
** newv, else 0.  Not in httptrcf.c: the target has CS (httptrc.c), the host
** test a plain compare. */
extern int trc_cas(volatile unsigned *mem, unsigned expect, unsigned newv);

/* bytes trc_init() needs for a table of this many records */
extern unsigned trc_size(unsigned slots);

/* lay out a zeroed block of trc_size(slots) bytes */
extern void trc_init(HTTPTRC *t, unsigned slots);

/* claim a record for the key, stamp TM_OPEN; NULL (and lost++) when the table
** has no room */
extern HTTPTRR *trc_open(HTTPTRC *t, void *key, unsigned now);

/* the live record of the key; NULL when there is none */
extern HTTPTRR *trc_find(HTTPTRC *t, void *key);

/* stamp a mark only if it is not set yet / always; both remember the mark and
** the client state as the place the request is at */
extern void trc_mark(HTTPTRR *r, int mark, unsigned now, int cst);
extern void trc_set(HTTPTRR *r, int mark, unsigned now, int cst);

/* finish the record at this mark; a slow one is kept longer */
extern void trc_end(HTTPTRR *r, int mark, unsigned now);

/* ticks to whole milliseconds (1.024 ms per tick) */
extern unsigned trc_ms(unsigned ticks);

/* one text line (no newline) for the record, offsets in ms from TM_OPEN.
** now is the tick of the readout; returns the length. */
extern int trc_line(char *out, int max, const HTTPTRR *r, unsigned now);

#endif /* HTTPTRC_H */
