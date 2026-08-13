/* CREDEXP.C
** Expiry decision for the credential reaper (M2): given how many seconds a
** credential has been idle and the configured TTL, has it expired?
**
** Kept free of project headers (plain integers only) so the boundary logic
** unit-tests on the host as well as on MVS (see test/tstexpir.c).  The C name
** is <= 8 chars, so cc370 maps it to CSECT CREDEXP.
*/

int
credexp(long elapsed_secs, unsigned ttl_secs)
{
    if (ttl_secs == 0)    return 0;     /* reaping disabled -> never expires   */
    if (elapsed_secs < 0) return 0;     /* clock skew (last > now) -> not yet  */

    return elapsed_secs > (long) ttl_secs;   /* strict: exactly at TTL is live */
}
