/* HTTPGIVE.H
** Flood guard for the "client abandoned" operator message (issue #347).
**
** Free of httpd.h so the decision unit-tests on the host as well as on MVS
** (see test/tstgive.c).
*/
#ifndef HTTPGIVE_H
#define HTTPGIVE_H

/* Decide whether one more give-up may be written to the console.
**   last        time of the last message written, 0 = none yet
**   held        give-ups counted since then and not yet reported
**   now         current time in seconds
**   interval    minimum seconds between two messages
**   suppressed  out: give-ups held back since the previous message; only
**               set when 1 is returned
**
** Returns 1 when the caller writes the message now (last is moved to now and
** held is cleared), 0 when this give-up is only counted.  The caller
** serialises access to last and held: this function does no locking. */
int httpgive(unsigned *last, unsigned *held, unsigned now,
             unsigned interval, unsigned *suppressed);

#endif /* HTTPGIVE_H */
