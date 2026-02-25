/* credtok_gen() returns CREDTOK struct for id parm. id parm may be NULL */
#include "cred.h"

CREDTOK 
credtok_gen(CREDID *id)
{
	CREDTOK		tok;
	CREDID		tmp;
	SHA256_CTX	ctx;
	
	if (!id) {
		id = &tmp;
		tmp = credid_init(NULL);
	}

	sha256_init(&ctx);
	sha256_update(&ctx, (unsigned char*)id, sizeof(CREDID));
	sha256_final(&ctx, tok.c);

	return tok;
}
