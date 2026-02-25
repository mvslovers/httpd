#include "cred.h"

int cred_init(void *salt, unsigned saltlen)
{
	BLOWFISH_KEY	*key = credkey();
	int				rc = 0;
	
	if (!credkey) {
		wtof("%s: unable to allocate WSA for BLOWFISH_KEY", __func__);
		return -1;
	}

	if (!salt) {
		/* we'll use the object code for blowfish_key_setup as the salt */
		salt = blowfish_key_setup;
		saltlen = 256;
	}
	
	if (!saltlen) saltlen = 256;
	
	rc = try(blowfish_key_setup, salt, key, saltlen);

quit:
	return rc;
}
