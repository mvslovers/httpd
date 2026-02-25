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

		if (memcmp(&c->id, id, sizeof(CREDID))==0) {
			found = c;
			break;
		}
	}
	
	if (lockrc==0) unlock(array, LOCK_SHR);
	
quit:
	return found;
}
