/* HTTPXAUTH.C
** HTTPX auth accessors — let CGI modules reach the resolved client
** credential (httpc->cred) through the function vector, without linking the
** credentials package or dereferencing CRED/ACEE layout themselves.
**
** All accessors are NULL-credential safe (an unauthenticated request has
** httpc->cred == NULL).  Exported through the HTTPX vector (append-only):
**   http_get_userid  http_get_acee  http_get_token  http_check_auth
**   http_logout  http_get_password
**
** Each function carries a >8-char C name, so it needs an explicit &FUNC CSECT
** name (matching its asm() alias) and an #undef to shed the httpx macro layer
** (HTTP_PRIVATE also suppresses it) — same pattern as httpgufs.c.
*/
#define HTTP_PRIVATE
#include "httpd.h"

/* http_get_userid() - copy the client's userid into the caller's buffer as a
** NUL-terminated string.  Returns out on success, or NULL if there is no
** authenticated credential (or no room).
**
** The userid is read straight from the RACF ACEE (aceeuser is a length-
** prefixed field: byte 0 = length, bytes 1.. = the userid) -- the same
** RACF-canonical source http_get_acee() consumers already trust.  We do NOT
** decrypt httpc->cred->id here: the credential's blowfish key lives in
** per-GRT WSA (credkey()) and is only initialized in httpd's own GRT by
** cred_init().  A CGI reached through the HTTPX vector runs under its own C
** runtime / GRT, where that key is still zero, so credid_dec() would decrypt
** with an all-zero key and return garbage (issue #109). Reading the ACEE
** needs no key and is therefore correct from any execution context. */
__asm__("\n&FUNC SETC 'HTTPGUID'");
#undef http_get_userid
UCHAR *
http_get_userid(HTTPC *httpc, UCHAR *out, unsigned outlen)
{
    ACEE     *acee;
    unsigned  len;
    unsigned  n;

    if (!out || outlen == 0) return NULL;
    out[0] = 0;

    if (!httpc || !httpc->cred || !httpc->cred->acee) return NULL;

    acee = httpc->cred->acee;
    len  = (unsigned char) acee->aceeuser[0];   /* length-prefixed userid */
    if (len > 8) len = 8;                        /* userid is at most 8 chars */
    if (len > outlen - 1) len = outlen - 1;

    for (n = 0; n < len; n++) {
        out[n] = acee->aceeuser[1 + n];
    }
    out[n] = 0;

    return out;
}

/* http_get_acee() - the client's RACF ACEE, or NULL when the request is not
** authenticated.  A CGI may hand this to racf_set_acee()/racf_auth() to run
** work under the client's identity. */
__asm__("\n&FUNC SETC 'HTTPGACE'");
#undef http_get_acee
ACEE *
http_get_acee(HTTPC *httpc)
{
    if (!httpc || !httpc->cred) return NULL;
    return httpc->cred->acee;
}

/* http_get_token() - copy the client's opaque session token into the caller's
** buffer.  Returns the number of bytes copied (0 if there is no session or the
** buffer is too small).  The caller base64-encodes exactly that many bytes
** (e.g. to hand back a z/OSMF LtpaToken2 cookie). */
__asm__("\n&FUNC SETC 'HTTPGTOK'");
#undef http_get_token
int
http_get_token(HTTPC *httpc, UCHAR *out, unsigned outlen)
{
    if (!httpc || !httpc->cred || !out) return 0;
    if (outlen < sizeof(CREDTOK)) return 0;

    memcpy(out, httpc->cred->token.c, sizeof(CREDTOK));
    return (int)sizeof(CREDTOK);
}

/* http_check_auth() - RACF resource check under the client's ACEE.  attr is
** one of RACF_ATTR_READ/UPDATE/CONTROL/ALTER (0 -> READ).  Returns the
** racf_auth() rc (0 == access permitted), or -1 when unauthenticated. */
__asm__("\n&FUNC SETC 'HTTPCKAU'");
#undef http_check_auth
int
http_check_auth(HTTPC *httpc, const char *classname, const char *resource, int attr)
{
    if (!httpc || !httpc->cred || !httpc->cred->acee) return -1;
    return racf_auth(httpc->cred->acee, classname, resource, attr);
}

/* http_logout() - end the client's session: drop the CRED (and its ACEE) from
** the credential store by token and clear httpc->cred.  Returns 0 on success,
** -1 if there was no session. */
__asm__("\n&FUNC SETC 'HTTPLOUT'");
#undef http_logout
int
http_logout(HTTPC *httpc)
{
    int rc;

    if (!httpc || !httpc->cred) return -1;

    rc = credtok_logout(&httpc->cred->token);
    httpc->cred = NULL;   /* the CRED it pointed at may now be freed */
    return rc;
}

/* http_get_password() - copy the caller's plaintext password into out as a
** NUL-terminated string.  Returns out on success, or NULL if the request is
** unauthenticated (httpc->cred == NULL) or the credential carries no password
** (e.g. a session ever established purely from a token, with no prior
** password login) or there is no room in out.
**
** Unlike http_get_userid(), the password is NOT in the ACEE -- it lives only
** in the credential, blowfish-encrypted in httpc->cred->id.password (see
** cred_login() -> credid_enc()).  cred_login() upper-cases the password before
** encrypting (RACF folds it anyway), so what is returned here is the RACF-
** canonical (upper-case) form -- correct for a JOB card, but note it is not
** necessarily byte-for-byte what the client typed.
**
** We decrypt with blowfish_decrypt(), which takes the key EXPLICITLY and so is
** a pure function of (data, key) valid in any GRT.  We must NOT call credkey()/
** __wsaget(): those read the key from the *current* GRT's WSA, which is only
** initialized in httpd's own GRT by cred_init().  A CGI reached through the
** HTTPX vector runs under its own GRT, where that slot is still zero, so the
** key would read back as garbage (issue #109/#111).  Instead we use the key
** pointer httpd cached in the HTTPD block at cred_init() time -- the key bytes
** live at a stable WSA address readable across the shared address space, so the
** cached pointer works from any execution context.
**
** Security: for job submission the CGI unavoidably needs the plaintext password
** to write it onto the JOB card.  The caller should memset() its buffer as soon
** as the JOB card is built. */
__asm__("\n&FUNC SETC 'HTTPGPWD'");
#undef http_get_password
UCHAR *
http_get_password(HTTPC *httpc, UCHAR *out, unsigned outlen)
{
    CREDKEY  *key;
    UCHAR     plain[8];             /* one blowfish block = the password */
    unsigned  len;
    unsigned  n;

    if (!out || outlen == 0) return NULL;
    out[0] = 0;

    if (!httpc || !httpc->cred || !httpc->httpd) return NULL;

    /* no password on this credential (e.g. a token-only session) */
    if (!(httpc->cred->id.passflg & CREDID_PASS_ENCRYPT)) return NULL;

    /* GRT-independent key pointer cached by cred_init() -- never credkey() */
    key = httpc->httpd->credkey;
    if (!key) return NULL;

    /* one 8-byte block; plaintext is NUL-padded when shorter than 8 chars */
    blowfish_decrypt(httpc->cred->id.password, plain, key);

    for (len = 0; len < sizeof(plain) && plain[len]; len++) ;
    if (len > outlen - 1) len = outlen - 1;

    for (n = 0; n < len; n++) {
        out[n] = plain[n];
    }
    out[n] = 0;

    memset(plain, 0, sizeof(plain));   /* scrub the local plaintext copy */

    return out;
}
