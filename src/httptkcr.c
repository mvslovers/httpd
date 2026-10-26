/* HTTPTKCR.C
** Resolve a session-token string to a credential (httppc.c's token_to_cred()).
** A file of its own so the decision unit-tests without the request pipeline
** (test/mvs/tstsess.c).
*/
#include "httpd.h"

/*
** cred_overage() - has this credential passed the hard max-age?  The age is
** measured from cred->created, which -- unlike cred->last -- is never
** refreshed, so activity cannot extend a session past the maximum.  credexp()
** treats a 0 limit as "never", which is what switches the check off.
*/
static int
cred_overage(CRED *cred, unsigned maxage)
{
	return credexp((long) difftime64(time64(NULL), cred->created), maxage);
}

/*
** httptkcr() - decode a base64 session-token string into a CREDTOK and resolve
** the session (NULL on a miss).  Shared by the Sec-Token / LtpaToken2 cookies
** and the Bearer header; cred_find_by_token() refreshes cred->last (M2).
** maxage is the hard credential max-age in seconds, 0 for none.
**
** An over-age credential is rejected HERE and not only by the reaper: the sweep
** runs every ~60 s (httpd.c), so a reaper-only max-age would keep answering
** with an expired session for up to a minute.  Enforcement is immediate;
** reclamation is left to the sweep on purpose.  Freeing it on this path would
** open a borrow window the idle design does not have -- a second worker holding
** the same CRED for an in-flight request would be left with freed storage (the
** M2 note in internals/refactoring-backlog.md: testlock catches a concurrent free,
** not a borrow).  Leaving it linked costs one dead CRED for up to a minute and
** resolves nothing in the meantime, because every lookup re-checks the age.
**
** An entry marked CRED_FLAG_STALE (#367, #373: RACF refused its password) is
** rejected the same way, and for the same reason it is not freed: the request
** that met the refusal still points at it.  Without this a token that
** resolves such an entry would keep working until the grace period ends.  A
** private session (CRED_FLAG_SESSION) is never marked, so it is not affected.
*/
CRED *
httptkcr(const char *b64, unsigned maxage)
{
	CREDTOK	tok;
	CRED	*cred;
	size_t	len;
	char	*buf;

	if (!b64 || !*b64) return NULL;

	buf = base64_decode(b64, strlen(b64), &len);
	if (!buf) return NULL;

	memset(&tok, 0, sizeof(tok));
	if (len > sizeof(tok)) len = sizeof(tok);
	memcpy(&tok, buf, len);
	free(buf);

	cred = cred_find_by_token(&tok);
	if (cred && (cred->flags & CRED_FLAG_STALE)) cred = NULL;
	if (cred && cred_overage(cred, maxage)) cred = NULL;

	return cred;
}
