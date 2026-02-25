/* credidup.c - update credential id structure */
#include "cred.h"

CREDID *
credid_update_enc(CREDID *id, unsigned *addr, unsigned char *userid, unsigned char *password)
{
	CREDKEY			*key = credkey();
	unsigned char	buf[12];
	
	if (id) {
		credid_update(id, addr, userid, password);
		
		if (!(id->userflg & CREDID_USER_ENCRYPT)) {
			blowfish_encrypt(id->userid, buf, key);
			memcpy(id->userid, buf, 8);
			id->userflg |= CREDID_USER_ENCRYPT;
		}
		
		if (!(id->passflg & CREDID_PASS_ENCRYPT)) {
			blowfish_encrypt(id->password, buf, key);
			memcpy(id->password, buf, 8);
			id->passflg |= CREDID_PASS_ENCRYPT;
		}
	}

	return id;
}
