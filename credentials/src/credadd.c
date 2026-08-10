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
	{
		/* Pinned for the reason cred_new() spells out (issue #154): the
		   credential store is server-lifetime and is torn down on a TCB other
		   than the one that grew it, and @@FREEM's R-form FREEMAIN cannot be
		   refused a cross-task free -- it abends. */
		unsigned char sp = __setsp(0);

		rc = array_add(array, cred);

		__setsp(sp);
	}
	if (lockrc==0) unlock(array, LOCK_EXC);

quit:
	return rc;
}
