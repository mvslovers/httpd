/* JESTIME.C
** Render a JES2 job timestamp for a JSON response, as an ISO 8601 instant in
** UTC.
**
** The field used to be produced by ctime64(), which converts through
** crt->crttzoff -- the *task's* timezone, filled in from the system's CVTTZ
** when nothing else set it.  Two things were wrong with that.
**
** It applied a second, unrelated offset.  The epoch value beside it comes from
** the JES2 conversion, which uses httpd->tzoffset; running the same instant
** through ctime64() then shifted it again by the CRT's offset.  On the
** reference system (CVTTZ = -5h) a job that started at 17:25:18 UTC was
** reported as "Fri Aug  7 05:25:18 2026" -- neither UTC, nor the system's local
** time, nor the caller's.  See issue #145 for how the two offsets came to
** disagree.
**
** And a local time is the wrong thing for an API to return at all.  The server
** cannot know the caller's timezone, so any local rendering is unlabelled and
** unusable -- the caller cannot tell which zone it is in.  Returning an
** unambiguous instant and letting the client localize is what zowe and every
** browser already do, and it is the convention mvsMF settled on elsewhere:
** ussapi.c formats mtime with mgmtime64() and a literal "Z" (see
** mvsmf docs/endpoints/uss/list.md).  Those timestamps never had this class of
** bug precisely because they never touch localtime.
**
** So: gmtime64_r(), never ctime64() or localtime64().  That removes the
** dependency on per-task timezone state rather than trying to keep two copies
** of it in step.
*/
#include "jestime.h"
#include "clib64.h"
#include <stdio.h>

void
jestime(const time64_t *t, char *out, unsigned outlen)
{
    struct tm   tm;

    if (!out || outlen == 0) return;
    out[0] = 0;
    if (!t) return;

    /* a job that has not started (or has not ended) carries a zero time */
    if (__64_cmp_u32((time64_t *)t, 0) == __64_EQUAL) {
        snprintf(out, outlen, "...");
        return;
    }

    if (!gmtime64_r(t, &tm)) {
        snprintf(out, outlen, "...");
        return;
    }

    /* ".000" rather than no fraction: this is the shape real z/OSMF emits for
    ** exec-submitted / exec-started / exec-ended,
    **     "exec-started":"2018-11-03T09:05:18.010Z"
    ** and httpjes2 is the reference mvsMF's own implementation is compared
    ** against, so the two should be directly comparable.  JES2 gives us second
    ** resolution here (start_time64 is a time64_t), so the fraction is always
    ** zero -- which is what z/OSMF itself reports for exec-submitted. */
    snprintf(out, outlen, "%04d-%02d-%02dT%02d:%02d:%02d.000Z",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
             tm.tm_hour, tm.tm_min, tm.tm_sec);
}
