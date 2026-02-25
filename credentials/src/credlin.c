#include "cred.h"

CRED * 
cred_login(unsigned addr, unsigned char *userid, unsigned char *password)
{
	CRED	***array= cred_array();
	CREDKEY	*key	= credkey();
	CRED	*cred 	= NULL;
	CREDID	id 		= credid_init(NULL);
	CREDTOK	tok		= credtok_gen(&id);
	ACEE	*acee	= NULL;
	int		rc		= 0;
	int		lockrc	= 0;
	int		i;
	char	user[12];
	char	pass[12];

	if (!array) {
		wtof("%s: cred_array() returned NULL", __func__);
		goto quit;
	}
	
	if (!key) {
		wtof("%s: credkey() returned NULL", __func__);
		goto quit;
	}

	/* required parameters */
	if (!userid) {
		/* wtof("%s: %s parameter is NULL", __func__, "userid"); */
		goto quit;
	}
	
	if (!password) {
		/* wtof("%s: %s parameter is NULL", __func__, "password"); */
		goto quit;
	}

	for(i=0; i < 8; i++) {
		user[i] = toupper(userid[i]);
		if (!userid[i]) break;
	}
	user[8] = 0;
	for(i=0; i < 8; i++) {
		pass[i] = toupper(password[i]);
		if (!password[i]) break;
	}
	pass[8] = 0;

	/* update CREDID with addr, userid, password */
	credid_update(&id, &addr, user, pass);
	
	/* encrypt the CREDID info */
	credid_enc(&id, &id);

	/* generate CREDTOK for this CREDID */
	tok = credtok_gen(&id);

	/* lock the array for exclusive access */
	lockrc = lock(array, LOCK_EXC);
	
	/* do we already have a CRED for this CREDTOK? */
	cred = cred_find_by_token(&tok);
	if (cred) goto cleanup;

	/* attempt to login via racf interface */
	acee = racf_login(user, pass, 0, &rc);
	if (!acee) {
		wtof("%s: racf_login(\"%s\", \"%s\") failed, rc=%d", 
			__func__, user, pass, rc);
		goto cleanup;
	}

	/* allocate new CRED structure */
	cred = cred_new(&id, &tok, acee, 0);
	if (!cred) {
		wtof("%s: cred_new() failed", __func__);
		racf_logout(&acee);
		goto cleanup;
	}

	/* save the cred in our array */
	rc = array_add(array, cred);
	if (rc) {
		wtof("%s: array_add() failed, rc=%d", __func__, rc);
		cred_free(&cred);
	}

cleanup:
	if (lockrc==0) unlock(array, LOCK_EXC);
	memset(user, 0, 8);
	memset(pass, 0, 8);
	
quit:
	return cred;
}
