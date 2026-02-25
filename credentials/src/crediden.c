#include "cred.h"

CREDID *credid_enc(CREDID *in, CREDID *out)
{
	CREDKEY	*key = credkey();
	CREDID	tmp;
	
	if (in && out) {
		tmp = *in;

		if (!(in->userflg & CREDID_USER_ENCRYPT)) {
			blowfish_encrypt(in->userid, tmp.userid, key);
			tmp.userflg |= CREDID_USER_ENCRYPT;
		}
		
		if (!(in->passflg & CREDID_PASS_ENCRYPT)) {
			blowfish_encrypt(in->password, tmp.password, key);
			tmp.passflg |= CREDID_PASS_ENCRYPT;
		}

		*out = tmp;
	}
	
	return out;
}
