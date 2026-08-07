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

#endif /* HTTPRACF_H */
