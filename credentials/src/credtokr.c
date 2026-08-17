/* CREDTOKR.C
** credtok_rand() - mint an unpredictable session token (#188).
**
** This replaces credtok_gen()'s CREDTOK = SHA-256(CREDID) for live sessions.
** That derivation was deterministic over a low-entropy input -- the client
** address is known, the userid is 8 characters and usually guessable, and
** cred_login() upper-case-folds the password before encrypting -- so a leaked
** LtpaToken2 was an offline verification oracle: hash candidates until one
** matches.  It also meant expiry and logout did not bind a token client, since
** re-presenting the same credentials minted a byte-identical token.
**
** The unpredictability rests on the caller's BLOWFISH key, which cred_init()
** seeds from STCK at startup and which never leaves the address space.  STCK
** alone would not be enough: it resolves to about a microsecond, and the login
** time is often observable from outside, so a +/- 10 s uncertainty is only
** ~10^7 candidates.  With the secret key in the hash an attacker cannot
** enumerate candidates at all, and the remaining attack is online -- guess a
** live token out of at most CRED_MAX sessions by sending requests to a 3.8j
** box, which is not a threat.
**
** The key is taken as a PARAMETER, never from credkey().  credkey() reads the
** current GRT's WSA, which only httpd's own GRT has initialized; a token minted
** from module context would otherwise hash an all-zero key (issue #109/#111,
** the same reason http_get_password() takes the cached pointer).
**
** Only key->p is hashed, not the whole 4168-byte BLOWFISH_KEY.  The key is
** XORed into the P-array first thing during key setup, so those 72 bytes are
** key-dependent in every Blowfish implementation -- no assumption about how
** thoroughly the S-boxes get mixed is needed.
**
** No counter is mixed in, deliberately.  A writable static would have to live
** in module storage, and these modules are RENT; STCK is monotonic and a login
** costs a RACF SVC, so two calls cannot land in the same microsecond anyway.
*/
#include "cred.h"

CREDTOK
credtok_rand(CREDKEY *key, CREDID *id)
{
	CREDTOK				tok;
	SHA256_CTX			ctx;
	unsigned long long	tod;
	void				*stack = &tok;

	memset(&tok, 0, sizeof(tok));

	__asm__("STCK\t0(%0)" : : "r" (&tod));

	sha256_init(&ctx);
	if (key) sha256_update(&ctx, (unsigned char *)key->p, sizeof(key->p));
	sha256_update(&ctx, (unsigned char *)&tod,   sizeof(tod));
	if (id)  sha256_update(&ctx, (unsigned char *)id, sizeof(CREDID));
	sha256_update(&ctx, (unsigned char *)&stack, sizeof(stack));
	sha256_final(&ctx, tok.c);

	return tok;
}
