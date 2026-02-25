#include "cred.h"

CRED *
cred_new(CREDID *id, CREDTOK *token, ACEE *acee, unsigned char flags)
{
	CRED	*cred = calloc(sizeof(CRED), 1);
	
	if (cred) {
		strncpy(cred->eye, CRED_EYE, sizeof(cred->eye));
		cred->last = time64(NULL);
		if (id) 	cred->id = *id;
		if (token) 	{
			*(&cred->token) = *token;
		}
		else {
			cred->token = credtok_gen(&cred->id);
		}
		if (acee)	cred->acee = acee;
		cred->len   = sizeof(CRED);
		cred->version = 1;
		cred->flags = flags;

#if 0		
		if (httpd) {
			/* save this cred in the httpd->cred array */
			int lockrc = lock(httpd, LOCK_EXC);
			array_add(&httpd->cred, cred);
		    if (lockrc==0) unlock(httpd, LOCK_EXC);
		}
#endif
	}
	
	return cred;
}
