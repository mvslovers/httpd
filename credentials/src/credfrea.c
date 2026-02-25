#include "cred.h"

#undef array_add
#undef array_free
#undef array_count

unsigned cred_free_array(void)
{
	CRED	 ***array = cred_array();
	int		 lockrc;
	unsigned count;
	int		 rc;
	
	if (!array) 	return 0;	/* no pointer to array of CRED */
	
	lockrc = lock(array, LOCK_EXC);

	for(count = array_count(array); count; count--) {
		CRED *c = array_del(array, count);

		/* make sure this is in fact a CRED structure */
		if (!c) continue;
		if (strcmp(c->eye, CRED_EYE)) continue;
		
		/* is this CRED in use? */
		rc = testlock(c, LOCK_EXC);
		if (rc==4) {
			/* Oops, complain */
			wtof("%s: CRED is still locked", __func__);
			wtodumpf(c, sizeof(CRED), "CRED");

			/* put CRED back into array */
			array_add(array, c);

			/* let the caller sort it out */
			goto quit;
		}

		/* deallocate this CRED struct */
		rc = cred_free(&c);
		if (rc && c) {
			/* can't deallocate this credential */
			wtof("%s: cred_free() failed. rc=%d", __func__, rc);
			wtodumpf(c, sizeof(CRED), "CRED");
			array_add(array, c);
			goto quit;
		}
	}
	
	if (*array) array_free(array);

quit:
	count = array_count(array);
	
	if (lockrc==0) unlock(array, LOCK_EXC);
	
	return count;
}
