/* TSTTHRD.C
** Tests for httpself() and httpthup() -- the start-up hand-shake between a
** server thread and its creator (src/httpthrd.c), issue #296.
**
** The failure this pins: the socket thread asked cthread_self() for its handle
** before cthread_create_ex() had stored the TCB address in it, got NULL, took
** that as fact, ended at once -- and main() wrote READY for a server that
** accepted nothing.  httpself() is the bounded wait that rides out the window;
** httpthup() is the check main() makes before READY.
**
** Both are free of httpd.h, so this is a DUAL test: it runs natively via
** `mbt test` and on MVS via `mbt test --mvs`.
*/
#include <stdio.h>
#include <stddef.h>
#include "httpthrd.h"
#include <mbtcheck.h>

/* -- a probe that answers NULL for the first `blind` calls --------------- */
static int      probes;
static int      pauses;
static int      blind;
static char     handle;

static void *probe(void)
{
    probes++;
    return (probes > blind) ? (void *)&handle : NULL;
}

static void pause_counting(void)
{
    pauses++;
}

static void reset(int blind_calls)
{
    probes = 0;
    pauses = 0;
    blind  = blind_calls;
}

/* -- a state byte the "thread" advances after `steps` pauses -------------- */
static volatile unsigned char state;
static int      steps;
static unsigned char target;

static void pause_advancing(void)
{
    pauses++;
    if (pauses >= steps) state = target;
}

static void arm(unsigned char start, unsigned char end, int after)
{
    state  = start;
    target = end;
    steps  = after;
    pauses = 0;
}

int main(void)
{
    printf("=== HTTPD httpself/httpthup (thread start-up) tests ===\n");

    /* httpself: the handle is there at once -> no pause, one probe */
    reset(0);
    CHECK(httpself(probe, 10, pause_counting) == (void *)&handle,
          "handle present: returned");
    CHECK(probes == 1 && pauses == 0,
          "handle present: one probe, no pause");

    /* the race itself: the first probes see nothing, a later one does */
    reset(3);
    CHECK(httpself(probe, 10, pause_counting) == (void *)&handle,
          "handle appears after 3 empty probes: returned");
    CHECK(probes == 4 && pauses == 3,
          "handle appears after 3 empty probes: paused between probes only");

    /* the last allowed probe still counts: tries pauses, tries + 1 probes */
    reset(5);
    CHECK(httpself(probe, 5, pause_counting) == (void *)&handle,
          "handle appears on the last probe: returned");
    CHECK(probes == 6 && pauses == 5,
          "handle appears on the last probe: tries + 1 probes");

    /* one probe too few: gives up with NULL and does not loop on */
    reset(6);
    CHECK(httpself(probe, 5, pause_counting) == NULL,
          "handle never appears in time: NULL");
    CHECK(probes == 6 && pauses == 5,
          "handle never appears in time: bounded at tries + 1 probes");

    /* tries == 0 is a single probe, and a NULL pause is allowed */
    reset(1);
    CHECK(httpself(probe, 0, NULL) == NULL && probes == 1,
          "tries 0: exactly one probe");
    reset(2);
    CHECK(httpself(probe, 4, NULL) == (void *)&handle,
          "NULL pause: probes back to back");

    /* httpthup: already running -> yes, without pausing */
    arm(HTTP_THRD_RUNNING, HTTP_THRD_RUNNING, 99);
    CHECK(httpthup(&state, 10, pause_advancing) == 1 && pauses == 0,
          "running at once: 1, no pause");

    /* still starting, comes up while main() waits -> yes */
    arm(HTTP_THRD_STARTING, HTTP_THRD_RUNNING, 3);
    CHECK(httpthup(&state, 10, pause_advancing) == 1 && pauses == 3,
          "comes up during the wait: 1");

    /* issue #296: the thread started and was gone again -> no READY */
    arm(HTTP_THRD_GONE, HTTP_THRD_GONE, 99);
    CHECK(httpthup(&state, 10, pause_advancing) == 0 && pauses == 0,
          "gone before the check: 0 at once, no wait");

    /* ... or it ends while main() is still waiting for it */
    arm(HTTP_THRD_STARTING, HTTP_THRD_GONE, 2);
    CHECK(httpthup(&state, 10, pause_advancing) == 0 && pauses == 2,
          "ends during the wait: 0 as soon as it says so");

    /* never leaves STARTING: bounded, then no */
    arm(HTTP_THRD_STARTING, HTTP_THRD_STARTING, 99);
    CHECK(httpthup(&state, 10, pause_advancing) == 0 && pauses == 10,
          "never starts: 0 after tries pauses");

    /* running on the very last look still counts */
    arm(HTTP_THRD_STARTING, HTTP_THRD_RUNNING, 10);
    CHECK(httpthup(&state, 10, pause_advancing) == 1,
          "running on the last look: 1");

    /* a byte nobody initialised is not a running thread */
    arm(0x7F, 0x7F, 99);
    CHECK(httpthup(&state, 10, pause_advancing) == 0 && pauses == 0,
          "unknown state value: 0, no wait");

    /* the states keep the values the HTTPD block's zero fill relies on */
    CHECK(HTTP_THRD_STARTING == 0,
          "STARTING is 0: a zeroed HTTPD block reads as 'not yet'");

    return mbt_test_summary("TSTTHRD");
}
