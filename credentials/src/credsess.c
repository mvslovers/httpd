/* CREDSESS.C
** cred_session() - mint a private login session (#288).  The contract and the
** reasons are in cred.h; this file is the mechanics.
**
** Everything here has to work from a CGI's GRT, because mvsMF reaches it
** through the HTTPX vector (http_new_session()):
**   - the array and the blowfish key are parameters, never cred_array() or
**     credkey(), which read the current GRT's WSA (#109, #111, #113);
**   - the CRED comes from cred_new(), pinned to subpool 0, and the array grows
**     under __setsp(0): a CGI's ambient subpool is freed when its request ends
**     (#154), and the session outlives the request by design.
*/
#include <mvs/storage.h>		/* __setsp()						*/
#include "cred.h"

#undef array_count
#undef array_add

CRED *
cred_session(CRED ***array, CREDKEY *key, CRED *from)
{
	CREDID		id;
	CREDTOK		tok;
	CRED		*cred		= NULL;
	CRED		*evict		= NULL;
	ACEE		*acee		= NULL;
	time64_t	oldest;
	time64_t	now;
	unsigned	count;
	unsigned	mine		= 0;
	unsigned	victim		= 0;
	unsigned	n;
	int			lockrc;
	int			rc			= 0;
	unsigned char	user[9];
	unsigned char	pass[9];

	if (!array || !key || !from) goto quit;

	/* a session is minted from a password login; a source without one (a CRED
	   built from a token alone) has nothing to log on again with */
	id = from->id;
	if (!(id.userflg & CREDID_USER_ENCRYPT)) goto quit;
	if (!(id.passflg & CREDID_PASS_ENCRYPT)) goto quit;

	/* one 8-byte blowfish block each, NUL-padded plaintext -- the same
	   explicit-key decrypt http_get_password() does, for the same reason */
	blowfish_decrypt(id.userid, user, key);
	blowfish_decrypt(id.password, pass, key);
	user[8] = 0;
	pass[8] = 0;

	/* the session's own ACEE.  RACF is asked again although the source was
	   just authenticated: an ACEE is freed with the CRED that holds it, and
	   the source and the session end at different times.  No log on refusal --
	   RAKF writes its own RAKF0004, and the password must not reach a console
	   (#116). */
	acee = racf_login((char *)user, (char *)pass, 0, &rc);
	memset(user, 0, sizeof(user));
	memset(pass, 0, sizeof(pass));
	if (!acee) goto quit;

	tok  = credtok_rand(key, &id);
	cred = cred_new(&id, &tok, acee, CRED_FLAG_SESSION);
	if (!cred) {
		racf_logout(&acee);
		goto quit;
	}

	now    = time64(NULL);
	lockrc = lock(array, LOCK_EXC);

	/* CRED_SESSION_MAX: count this client's sessions, and remember its least
	   recently used one that may be ended.  Two rules, both the reaper's: not
	   one somebody is freeing right now (testlock), and not one a request may
	   still hold -- httpc->cred is a borrowed pointer, nothing pins it, so
	   freeing a session in use leaves that request on freed storage.  The
	   reaper is safe because it frees only what was idle for SESSION_TIMEOUT;
	   here the floor is CRED_SESSION_IDLE. */
	count = array_count(array);
	for (n = 1; n <= count; n++) {
		CRED *c = array_get(array, n);

		if (!c) continue;
		if (!(c->flags & CRED_FLAG_SESSION)) continue;
		if (memcmp(&c->id, &id, sizeof(CREDID)) != 0) continue;

		mine++;
		if (testlock(c, LOCK_EXC) == 4) continue;
		if (difftime64(now, c->last) < CRED_SESSION_IDLE) continue;
		if (!victim || difftime64(c->last, oldest) < 0) {
			victim = n;
			oldest = c->last;
		}
	}

	if (mine >= CRED_SESSION_MAX) {
		/* unlinked now, freed after the unlock -- cred_free() does a RACF
		   logout, which must not run under the array lock */
		if (victim) evict = array_del(array, victim);
		if (!evict) {
			/* every one of them in use: refuse rather than exceed the limit
			   or pull a session out from under a request */
			if (lockrc == 0) unlock(array, LOCK_EXC);
			cred_free(&cred);
			goto quit;
		}
	}

	if (array_count(array) >= CRED_MAX) {
		if (lockrc == 0) unlock(array, LOCK_EXC);
		wtof("%s: credential array full (%u); login rejected", __func__, CRED_MAX);
		cred_free(&cred);
		goto free_evicted;
	}

	{
		unsigned char sp = __setsp(0);

		rc = array_add(array, cred);

		__setsp(sp);
	}

	if (lockrc == 0) unlock(array, LOCK_EXC);

	if (rc) {
		wtof("%s: array_add() failed, rc=%d", __func__, rc);
		cred_free(&cred);
	}

free_evicted:
	if (evict) cred_free(&evict);

quit:
	return cred;
}
