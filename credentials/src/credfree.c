#include "cred.h"

int cred_free(CRED **cred)
{
	int		rc	= -1;
	CRED	*c	= NULL;
	int		lockrc;
	
	if (cred) c = *cred;

	/* check for NULL parm */
	if (!c) goto quit;

	/* check for valid eye catcher */
	if (strcmp(c->eye, CRED_EYE)) goto quit;
	
	/* is delete allowed? */
	if (c->flags & CRED_FLAG_NO_DELETE) goto quit;

	/* try to lock this CRED structure */
	lockrc = trylock(c, LOCK_EXC);
	if (lockrc==4) {
		/* some other thread has it locked */
		rc = lockrc;
		goto quit;
	}

	/* do we have an ACEE? */
	if (c->acee) {
		/* should we free the ACEE? */
		if (!(c->flags & CRED_FLAG_KEEP_ACEE)) {
			/* Yes, do logout now */
	        racf_logout(&c->acee);
	        c->acee = NULL;
		}
	}

	/* wipe the credential storage */
	memset(c, 0xFE, sizeof(CRED));
	
	/* release the credential storage */
	free(c);

	/* unlock this address */
	unlock(c, LOCK_EXC);

	/* clear the pointer to the credential storage */
	*cred = NULL;
	
	/* success */
	rc = 0;

quit:
	return rc;
}
