/* HTTPRACF.H
** Decide whether a RACHECK (racf_auth()) return code allows the access.
**
** Free of httpd.h so the pure decision logic unit-tests on the host as well as
** on MVS (see test/tstracf.c).  The name is 8 characters so it needs no asm()
** CSECT alias -- same reason as httpbody().
*/
#ifndef HTTPRACF_H
#define HTTPRACF_H

/* The SAF return codes racf_auth() can answer with.  0 and 4 are both
** "allowed" -- 0 says a profile permits the access, 4 says no profile covers
** the resource at all.  8 and up are refusals. */
#define HTTP_RACF_PERMITTED     0   /* a profile permits the access         */
#define HTTP_RACF_NOTPROT       4   /* no profile covers the resource       */

/* Does this racf_auth() rc allow the access?  Returns 1 for the two SAF
** "allowed" codes, 0 for everything else.
**
** Everything else deliberately includes negative values: http_check_auth()
** answers -1 for an unauthenticated request, and the tempting shorthand for
** "0 or 4" -- rc <= 4 -- would let that through as allowed.  Testing the two
** codes explicitly is the whole point of this function existing.
*/
int httpracf(int rc);

/* The verdict of the RES= route gate (#351).  httpracf() answers "may this
** pass" and so cannot say WHY an rc 4 passed; a route that must not run on a
** missing profile needs the difference, so the gate asks this instead. */
#define HTTP_RACF_V_ALLOW   0   /* a profile permits, or none exists and   */
                                /* the server was not told to refuse that  */
#define HTTP_RACF_V_DENY    1   /* refused by a profile, or not a SAF rc   */
#define HTTP_RACF_V_NOPROF  2   /* no profile, and RES_NOPROFILE=DENY      */

/* Map a raw racf_auth() rc to HTTP_RACF_V_*.  refuse_noprof is the
** RES_NOPROFILE=DENY setting; with it 0 the answer is exactly httpracf()'s
** (rc 0 and 4 allow, everything else denies), so the default does not move.
** A refusal (8 and up) and the -1 "unauthenticated" sentinel are DENY in
** either setting: a stricter setting must never turn a denial into anything
** else. */
int httpracr(int rc, int refuse_noprof);

/* Read the value of RES_NOPROFILE= (#351): 1 for DENY, 0 for ALLOW, -1 for
** anything else.  The token ends at the first blank or at the end of the
** string, and the match is caseless, like the other Parmlib values.  -1 is a
** configuration error the caller must refuse to start on -- a misspelled
** value must not read as ALLOW, which would leave the gate open unannounced.
** Pure and free of httpd.h so that the fatal path is dual-tested. */
int httpracv(const char *value);

#endif /* HTTPRACF_H */
