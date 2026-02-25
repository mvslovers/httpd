#include "cred.h"

CREDID *credid_dec(CREDID *in, CREDID *out)
{
	CREDKEY	*key = credkey();
	CREDID	tmp;
	
	if (in && out) {
		tmp = *in;

		if ((in->userflg & CREDID_USER_ENCRYPT)) {
			blowfish_decrypt(in->userid, tmp.userid, key);
			tmp.userflg &= 0xFF - CREDID_USER_ENCRYPT;
		}
		
		if ((in->passflg & CREDID_PASS_ENCRYPT)) {
			blowfish_decrypt(in->password, tmp.password, key);
			tmp.passflg &= 0xFF - CREDID_PASS_ENCRYPT;
		}

		*out = tmp;
	}
	
	return out;
}
