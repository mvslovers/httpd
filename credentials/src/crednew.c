#include <clibos.h>		/* __getmsp() -- subpool pin (issue #154)	*/
#include "cred.h"

CRED *
cred_new(CREDID *id, CREDTOK *token, ACEE *acee, unsigned char flags)
{
	/* Pinned to subpool 0 rather than calloc'd (issue #154).  This is the
	   unconditional half of the pin audit, not the RECLAIM=YES half: a CRED
	   is created on whichever TCB served the login -- a worker, and on a
	   RECLAIM=YES route under the module's ambient subpool -- but it is freed
	   on a DIFFERENT one, by the session reaper and by http_logout() from
	   module context (#113).  A non-zero subpool is per-task, and @@FREEM
	   issues the R form of FREEMAIN, which has no return code to refuse a
	   cross-task free with: it would abend rather than be rejected. */
	CRED	*cred = __getmsp(sizeof(CRED), 0);

	if (cred) {
		memset(cred, 0, sizeof(CRED));
		strncpy(cred->eye, CRED_EYE, sizeof(cred->eye));
		cred->last = time64(NULL);
		if (id) 	cred->id = *id;
		if (token) 	{
			*(&cred->token) = *token;
		}
		else {
			/* No token supplied -- mint a random one rather than deriving it
			   from the CREDID (#188).  The BLOWFISH key is not mixed in here:
			   cred_new() has no key to hand and must not call credkey(), which
			   is GRT-relative (#109).  A caller that wants the key's secrecy in
			   the token mints it itself and passes it in, the way cred_login()
			   does; what is left here (STCK + CREDID + a storage address) is
			   still never deterministic, which is the property that matters for
			   the in-tree callers -- all of them tests. */
			cred->token = credtok_rand(NULL, &cred->id);
		}
		if (acee)	cred->acee = acee;
		/* creation stamp for the hard max-age (#118).  Unlike cred->last it is
		   never refreshed -- that is the whole point: activity must not be able
		   to extend a session past the configured maximum. */
		cred->created = cred->last;
		cred->len   = sizeof(CRED);
		cred->version = CRED_VERSION;
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
