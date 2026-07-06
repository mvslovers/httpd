#include "cred.h"

/* logout the CRED matching this CREDTOK in the supplied array.  Split out from
   credtok_logout() so a CGI can pass httpd's own array pointer: cred_array()
   reads the current GRT's WSA, which is empty/foreign from a CGI's GRT, so the
   scan there finds nothing and the session is never invalidated (issue #113,
   same GRT/WSA class as #109/#111). */
int
credtok_logout_arr(CRED ***array, CREDTOK *token)
{
	int		rc = 1;
	int		lockrc;
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

/* logout CRED for this CREDTOK, using this GRT's credential array */
int
credtok_logout(CREDTOK *token)
{
	return credtok_logout_arr(cred_array(), token);
}
