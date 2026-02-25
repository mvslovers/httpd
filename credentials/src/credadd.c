#include "cred.h"

#undef array_add

int cred_add(CRED *cred)
{
	CRED 	***array = cred_array();
	int		rc	= -1;
	int		lockrc;
	
	if (!array) goto quit;
	if (!cred) goto quit;

	lockrc = lock(array, LOCK_EXC);
	rc = array_add(array, cred);
	if (lockrc==0) unlock(array, LOCK_EXC);

quit:
	return rc;
}
