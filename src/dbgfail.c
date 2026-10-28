/* DBGFAIL.C
** The trace FILE failed a write: report it once and stop tracing.
**
** Since libc370 1.0.6 the first uncorrectable write error leaves the FILE
** in error and every later write on it is refused without touching the DCB.
** Left alone that is a trace that silently stops (#264), so the writers in
** dbg*.c ask here after every write.
**
** Many threads write the trace.  The pointer is taken out of httpd->dbg with
** an atomic exchange: exactly one caller gets the FILE back and issues the
** message, everyone else gets NULL.  The FILE is not closed -- a thread that
** read the pointer before the exchange may still be writing to it, and a
** write to a FILE in error is refused harmlessly.
*/
#include "httpd.h"
#include "httpdmsg.h"

#include <s370/atomic.h>

int
dbgfail(HTTPD *httpd, FILE *fp)
{
    if (!fp || !ferror(fp)) return 0;

    if (__swap((unsigned *)&httpd->dbg, 0) == 0) return 0;

    wtof(MSG_DBG_WRITE_FAILED);
    return 1;
}
