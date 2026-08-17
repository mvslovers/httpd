/* CREDREAP.C
** Reap idle credentials (M2): remove and free CREDs (and their RACF ACEEs)
** that have been idle longer than the TTL.  cred->last is refreshed on every
** cred_find_by_token() hit, so an actively used session is never reaped (given
** SESSION_TIMEOUT >> the longest request; see the M2 note in the backlog).
**
** Since #118 a second, independent limit applies: the hard max-age, measured
** from cred->created, which is never refreshed.  An actively used session does
** outlive the idle TTL, so without this a credential cached from one login
** stays valid forever -- and with it a RACF identity that may since have been
** REVOKEd, had its password changed, or had its group memberships altered
** (the ACEE is a snapshot taken at login).  The max-age bounds that lag.
*/
#include "cred.h"

#undef array_count

unsigned cred_reap(unsigned ttl_secs, unsigned maxage_secs)
{
    CRED     ***array = cred_array();
    CRED     *dead[CRED_MAX];
    unsigned ndead = 0;
    unsigned n;
    unsigned i;
    int      lockrc;
    time64_t now;

    if (!array) return 0;
    if (ttl_secs == 0 && maxage_secs == 0) return 0;    /* reaping disabled */

    now = time64(NULL);

    lockrc = lock(array, LOCK_EXC);

    /* iterate from the tail so array_del() index shifts don't skip entries */
    for (n = array_count(array); n >= 1; n--) {
        CRED *c = array_get(array, n);

        if (!c) continue;
        if (strcmp(c->eye, CRED_EYE)) continue;

        /* idle too long, OR past the hard max-age counted from creation.
           credexp() treats a 0 limit as "never", so either may be disabled
           on its own. */
        if (!credexp((long) difftime64(now, c->last),    ttl_secs) &&
            !credexp((long) difftime64(now, c->created), maxage_secs)) continue;

        /* skip anything a concurrent path is already freeing */
        if (testlock(c, LOCK_EXC) == 4) continue;

        /* unlink now; free (which does the RACF logout) AFTER releasing the
           array lock so a mass expiry doesn't stall workers in
           cred_find_by_token() */
        array_del(array, n);
        if (ndead < CRED_MAX) dead[ndead++] = c;
    }

    if (lockrc == 0) unlock(array, LOCK_EXC);

    /* the reaped CREDs are unlinked -> unreachable via cred_find_*(); free */
    for (i = 0; i < ndead; i++) {
        CRED *c = dead[i];
        cred_free(&c);
    }

    return ndead;
}
