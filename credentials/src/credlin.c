#include "cred.h"

#undef array_count

CRED *
cred_login(unsigned addr, unsigned char *userid, unsigned char *password,
           unsigned maxage_secs)
{
	CRED	***array= cred_array();
	CREDKEY	*key	= credkey();
	CRED	*cred 	= NULL;
	CREDID	id 		= credid_init(NULL);
	CREDTOK	tok;
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

	/* lock the array for exclusive access */
	lockrc = lock(array, LOCK_EXC);

	/* Do we already have a CRED for these credentials?  The lookup is by
	   CREDID, not by token (#188).  It used to derive CREDTOK = SHA-256(CREDID)
	   and look that up, which is what made the token deterministic and turned a
	   leaked token into an offline oracle for the password.  The CREDID compare
	   keeps both properties that mattered: the cache hit still costs no RACF
	   call, and the session is still bound to the client address, because addr
	   is CREDID+0.

	   cred_find_by_id() compares the ENCRYPTED CREDID, so it relies on Blowfish
	   ECB being deterministic under a fixed key.  It is, and this was already
	   load-bearing: the old token was a hash over these same encrypted bytes, so
	   an unstable byte would have made every cache hit miss. */
	cred = cred_find_by_id(&id);
	if (cred) {
		/* A cached hit answers without touching RACF, so the ACEE stays the
		   snapshot taken at the original login.  Past the hard max-age (#118)
		   that is exactly what must not happen: drop the credential and fall
		   through to racf_login(), which is where a REVOKE, a changed password
		   or an altered group membership finally takes effect.

		   Every entry point that authenticates arrives here -- the Basic source
		   in httppc.c and the form POST in httpcred.c -- so the limit lives at
		   this one chokepoint rather than at each caller.

		   Freeing on the request path is the reaper's known borrow window (the
		   M2 note in docs/refactoring-backlog.md: testlock catches a concurrent
		   free, not a borrow), guarded by the same invariant -- SESSION_MAXAGE
		   >> the longest request, which HTTPD032E warns about at startup.
		   Merely rejecting it is not an option here: cred_login() would keep
		   returning the same entry, and the client would loop between login and
		   rejection until the next sweep.  credtok_logout_arr() re-locks the
		   array, which is the same nesting cred_find_by_id() above already
		   relies on -- and it is given the credential's OWN token, which since
		   #188 is a random value we cannot re-derive from the CREDID. */
		if (credexp((long) difftime64(time64(NULL), cred->created), maxage_secs)) {
			CREDTOK dead = cred->token;

			credtok_logout_arr(array, &dead);
			cred = NULL;
		}
		else {
			goto cleanup;
		}
	}

	/* attempt to login via racf interface.  On failure RAKF already writes its
	   own RAKF0004 audit message, so we do NOT log here -- an httpd message
	   would only duplicate it, and logging the attempted password (pass) would
	   leak a plaintext credential to the console/SYSLOG (issue #116). */
	acee = racf_login(user, pass, 0, &rc);
	if (!acee) {
		goto cleanup;
	}

	/* Mint the session token only now that RACF has accepted the credentials.
	   Random, not derived from the CREDID (#188), so it carries no information
	   about the password and cannot be re-created by presenting the same
	   credentials again -- which is what makes logout and SESSION_MAXAGE bind a
	   token-presenting client.  The key comes from our own credkey() above, not
	   from inside credtok_rand(), which must stay GRT-independent. */
	tok = credtok_rand(key, &id);

	/* allocate new CRED structure */
	cred = cred_new(&id, &tok, acee, 0);
	if (!cred) {
		wtof("%s: cred_new() failed", __func__);
		racf_logout(&acee);
		goto cleanup;
	}

	/* overflow protection (M2): reject the new session when the array is full
	   rather than evict a live one (login flood).  cred_free() logs the ACEE
	   back out and NULLs cred, so cred_login() returns NULL. */
	if (array_count(array) >= CRED_MAX) {
		wtof("%s: credential array full (%u); login rejected", __func__, CRED_MAX);
		cred_free(&cred);
		goto cleanup;
	}

	/* save the cred in our array.  Pinned like cred_new()'s CRED itself
	   (issue #154): cred_login() is reached from the auth path, which a module
	   can enter through http_check_auth(), and the store is emptied on another
	   TCB. */
	{
		unsigned char sp = __setsp(0);

		rc = array_add(array, cred);

		__setsp(sp);
	}
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
