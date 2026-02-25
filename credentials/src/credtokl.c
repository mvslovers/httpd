#include "cred.h"

/* logout CRED for this CREDTOK */
int 
credtok_logout(CREDTOK *token)
{
	int		rc = 1;
	int		lockrc;
	CRED	***array = cred_array();
	unsigned count, n;
	
	if (!token) goto quit;
	if (!array) goto quit;
	
	lockrc = lock(array, LOCK_EXC);
	
	count = array_count(array);
	for (n=1; n <= count; n++) {
		CRED *cred = array_get(array, n);

		if (!cred) continue;
		
		if (memcmp(&cred->token, token, sizeof(CREDTOK))==0) {
			cred = array_del(array, n);
			if (cred) {
				rc = cred_free(&cred);
			}
			break;
		}
	}
	
	if (lockrc==0) unlock(array, LOCK_EXC);
	
quit:
	return rc;
}
