#include "cred.h"
#include <clibb64.h>	/* base64 encode/decode */
#include <clibssib.h>	/* __jobid() */
#include <clibtiot.h>	/* __jobname() */

#undef array_count

int main(int argc, char **argv)
{
	int			rc = 0;
	CREDID		id = credid_init(NULL);
	CREDTOK		tok = credtok_gen(&id);
	ACEE		*acee = (ACEE*)0x12345678;
	CRED		*cred;
	CRED		***array;
	unsigned	addr;
	CREDKEY		*key;
	char		*salt = "This is the salt phrase for our key";
	unsigned	saltlen = strlen(salt);
	unsigned 	count;
	unsigned	n;
	utime64_t	before;
	utime64_t	after;
	__64		usec;
	
	wtof("jobname=\"%8.8s\"", __jobname());
	wtof("jobid=\"%8.8s\"", __jobid());

    rc = __autask();    /* APF authorize this task  */
    wtof("%s: __autask() rc=%d", __func__, rc);
    if (rc) goto quit;

    before = utime64(NULL);
	after = utime64(NULL);
	__64_sub(&after, &before, &usec);
	wtof("before=%llu, after=%llu, usec=%llu", before, after, usec);
	
    before = utime64(NULL);
	rc = cred_init(salt, saltlen);
	after = utime64(NULL);
	__64_sub(&after, &before, &usec);
	wtof("cred_init(\"%s\",%u) rc=%d usec=%llu", salt, saltlen, rc, usec);
	if (rc==0) {
		key = credkey();
		// wtodumpf(key, sizeof(CREDKEY), "CREDKEY");
	}
	
	inet_aton("192.168.1.123", (in_addr_t*)&addr);

	cred = cred_login(addr, "HERC01", "CUL8TR");
	if (cred) {
		wtodumpf(cred, sizeof(CRED), "%s: CRED created", __func__);
	}

	array = cred_array();
	count = array_count(array);
	wtof("cred_array() count=%u", count);
	if (count) {
		wtodumpf(array, sizeof(CRED*)*count, "CRED array pointers");
		for(n=1; n <= count; n++) {
			cred = array_get(array, n);
			if (!cred) continue;
			wtodumpf(cred, sizeof(CRED), "CRED#%u", n);
			credid_dec(&cred->id, &id);
			wtodumpf(&id, sizeof(CREDID), "CRED#%u ID", n);
			wtodumpf(&cred->token, sizeof(CREDTOK), "CRED#%u TOKEN", n);
			{
				unsigned char *enc, *dec;
				size_t len = 0;
				enc = base64_encode((unsigned char*)&cred->token, sizeof(CREDTOK), &len);
				if (enc) {
					wtof("base64 TOKEN=\"%s\" (%u)", enc, len);
					dec = base64_decode(enc, len, &len);
					if (dec) {
						wtodumpf(dec, len, "CRED#u TOKEN", n);
						free(dec);
					}
					free(enc);
				}
			}
		}
	}


quit:	
	return 0;
}
