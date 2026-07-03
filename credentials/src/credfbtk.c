#include "cred.h"

#undef array_count

CRED *cred_find_by_token(CREDTOK *token)
{
	CRED		***array = cred_array();
	CRED		*found	= NULL;
	int			lockrc;
	unsigned	count;
	unsigned	n;
	
	if (!array) goto quit;
	if (!token) goto quit;

	lockrc = lock(array, LOCK_SHR);
	
	count = array_count(array);
	for (n=1; n <= count; n++) {
		CRED *c = array_get(array, n);

		if (!c) continue;

		if (memcmp(&c->token, token, sizeof(CREDTOK))==0) {
			found = c;
			break;
		}
	}

	/* refresh the last-used stamp so activity keeps the session alive past the
	   reaper's TTL (M2).  Written under LOCK_SHR -- a torn 8-byte write between
	   near-simultaneous readers is benign (all write ~now); the reaper reads it
	   under LOCK_EXC after these readers drain. */
	if (found) found->last = time64(NULL);

	if (lockrc==0) unlock(array, LOCK_SHR);
	
quit:
	return found;
}
