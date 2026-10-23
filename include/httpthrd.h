/* HTTPTHRD.H
** Start-up hand-shake between a server thread and the thread that created it
** (issue #296).
**
** Free of httpd.h so the decisions unit-test on the host as well as on MVS
** (see test/tstthrd.c).  The names are 8 characters or less, so no asm()
** CSECT alias is needed -- same reason as httpbody().
**
** Why this exists.  cthread_create_ex() ATTACHes the new subtask and only
** afterwards, in the creating task, stores the subtask's TCB address into its
** CTHDTASK handle.  A subtask that is dispatched first calls cthread_self(),
** finds no handle carrying its TCB and gets NULL -- for a few microseconds, and
** then only now and again.  The socket thread used to take NULL as a fact: it
** logged garbage (TASK(000000), a STACKSIZE read from low storage), skipped its
** accept loop and ended, while main() went on to write READY for a server that
** accepted nothing.
*/
#ifndef HTTPTHRD_H
#define HTTPTHRD_H

/* The state byte a thread keeps for its creator.  Only the thread writes it
** (a single-byte store); the creator only reads. */
#define HTTP_THRD_STARTING  0   /* created, not yet at its accept loop      */
#define HTTP_THRD_RUNNING   1   /* in its loop                              */
#define HTTP_THRD_GONE      2   /* ended, or never got as far as the loop   */

/* Call probe() until it answers something other than NULL, at most tries + 1
** times, calling pause() between two probes.  Returns that answer, or NULL when
** every probe came back empty.  The first probe is made without pausing, so the
** usual case costs nothing.
**
** For a thread to find its own CTHDTASK while its creator is still storing the
** TCB address (see above).  A NULL return after the full wait means the handle
** is really not there, and the thread must not go on as if it were.
*/
void *httpself(void *(*probe)(void), unsigned tries, void (*pause)(void));

/* Has the thread whose state byte this is reached its loop?  Returns 1 as soon
** as the byte reads HTTP_THRD_RUNNING and 0 as soon as it reads HTTP_THRD_GONE.
** While it reads HTTP_THRD_STARTING, pause() is called and the byte read again,
** up to tries times; 0 if it never left STARTING.  Any other value is a block
** that was never initialised and counts as not running.
**
** main() asks this before it writes READY: a server whose socket thread is not
** running accepts nothing, and READY would be a lie (issue #296).
*/
int httpthup(const volatile unsigned char *state, unsigned tries,
             void (*pause)(void));

#endif /* HTTPTHRD_H */
