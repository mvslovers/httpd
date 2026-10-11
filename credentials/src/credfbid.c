#include "cred.h"

#undef array_count

CRED *cred_find_by_id(CREDID *id)
{
	CRED		***array = cred_array();
	CRED		*found	= NULL;
	int			lockrc;
	unsigned	count;
	unsigned	n;
	
	if (!array) goto quit;
	if (!id) goto quit;

	lockrc = lock(array, LOCK_SHR);
	
	count = array_count(array);
	for (n=1; n <= count; n++) {
		CRED *c = array_get(array, n);

		if (!c) continue;

		/* A private login session carries the same CREDID as the credential
		   it was minted from, but it belongs to its token alone (#288).  Found
		   here, a plain Basic request would hand its token out, and cred_login()'s
		   max-age branch could end it under its owner. */
		if (c->flags & CRED_FLAG_SESSION) continue;

		if (memcmp(&c->id, id, sizeof(CREDID))==0) {
			found = c;
			break;
		}
	}

	/* Refresh the last-used stamp, exactly as cred_find_by_token() does (M2).
	   Since #188 this is a PER-REQUEST lookup: cred_login() resolves the Basic
	   source through here on every request, so without the refresh an actively
	   used session would be reaped SESSION_TIMEOUT after its CREATION rather
	   than after going idle.  Written under LOCK_SHR -- a torn 8-byte write
	   between near-simultaneous readers is benign (all write ~now); the reaper
	   reads it under LOCK_EXC after these readers drain. */
	if (found) found->last = time64(NULL);

	if (lockrc==0) unlock(array, LOCK_SHR);
	
quit:
	return found;
}
