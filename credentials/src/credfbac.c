#include "cred.h"

#undef array_count

CRED *cred_find_by_acee(ACEE *acee)
{
	CRED		***array = cred_array();
	CRED		*found	= NULL;
	int			lockrc;
	unsigned	count;
	unsigned	n;
	
	if (!array) goto quit;

	lockrc = lock(array, LOCK_SHR);
	
	count = array_count(array);
	for (n=1; n <= count; n++) {
		CRED *c = array_get(array, n);

		if (!c) continue;

		if (c->acee == acee) {
			found = c;
			break;
		}
	}
	
	if (lockrc==0) unlock(array, LOCK_SHR);
	
quit:
	return found;
}
