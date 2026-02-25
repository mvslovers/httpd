/* credidup.c - update credential id structure */
#include "cred.h"

CREDID *
credid_update(CREDID *id, unsigned *addr, unsigned char *userid, unsigned char *password)
{
	if (id) {
		if (addr) {
			id->addr = *addr;
		}
		if (userid) {
			strncpy(id->userid, userid, sizeof(id->userid));
			id->userid[sizeof(id->userid)-1] = 0;
			id->userflg &= 0xFF - CREDID_USER_ENCRYPT;	/* reset encrypted flag */
		}
		if (password) {
			strncpy(id->password, password, sizeof(id->password));
			id->password[sizeof(id->password)-1] = 0;
			id->passflg &= 0xFF - CREDID_PASS_ENCRYPT;	/* reset encrypted flag */
		}
	}

	return id;
}
