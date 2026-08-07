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
#include "httpracf.h"

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
** work under the client's identity.
**
** A CGI that calls racf_auth() itself sees the RAW SAF rc, which is NOT the
** contract http_check_auth() publishes below: an unprotected resource answers
** 4, not 0, and testing rc == 0 / != 0 turns that allowed access into a denial.
** Use http_check_auth() unless the raw distinction is actually wanted, and if it
** is, accept both 0 and 4 (see httpracf()). */
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
** one of RACF_ATTR_READ/UPDATE/CONTROL/ALTER (0 -> READ).  Returns 0 when the
** access is permitted, the racf_auth() rc (8 and up) on a refusal, or -1 when
** the request is unauthenticated.
**
** SAF has two "allowed" answers: rc 0 (a profile permits the access) and rc 4
** (no profile covers the resource).  We normalize 4 to 0 here, so the contract
** this vector entry has published since it was added -- 0 == permitted -- keeps
** holding.  Three reasons that is the right call rather than the information
** hiding it looks like:
**
**   - The rc reaches CGI modules through the HTTPX vector, and modules are
**     LINKed at runtime.  Widening the contract to "0 or 4" would leave every
**     not-yet-rebuilt module denying access to unprotected resources, with no
**     build-time signal that it has to move.
**   - This function already invents -1 for "unauthenticated", so it was never a
**     SAF pass-through -- the published contract is ours, not SAF's.  Worse, the
**     natural widened idiom `rc <= 4` would read -1 as allowed and let
**     unauthenticated requests straight through the gate.
**   - rc 4 never means denied (0 permitted, 4 not protected, 8+ refused), so
**     collapsing it into 0 cannot soften a denial.
**
** Until libc370 #63 the distinction was invisible: racf_auth() set RACHECK flag
** byte 0x10 believing it meant LOG=NONE, when that bit is DSTYPE=V, and with
** that flag RAKF answered 0 where it should answer 4.  With the flag corrected
** the 4 becomes visible to every caller -- including httpd's own RES= route gate
** in auth_gate() (httppc.c), which tests != 0 and would otherwise 403 every
** request to a route whose resource has no profile.
**
** A resource with no profile passing the gate is SAF-correct, but it is also
** exactly the shape of a misconfigured RES= route, so trace it when DEBUG is on
** -- it is the only place the distinction is still visible.
**
** One caveat on that trace: dbgf() resolves HTTPD through __grtget()->grtapp1
** and writes httpd->dbg, a FILE* owned by httpd's C runtime, so reached from a
** CGI it touches another runtime's stdio state.  That is the same thing the
** vector's own http_dbgf entry does, so it is the sanctioned pattern rather than
** a new one -- but it has not been exercised from a CGI, and today it cannot be:
** no CGI calls http_check_auth(), and with DEBUG 0 (the default) dbgf() returns
** after two loads.  A first CGI consumer plus DEBUG 1 is the combination to
** verify before trusting it. */
__asm__("\n&FUNC SETC 'HTTPCKAU'");
#undef http_check_auth
int
http_check_auth(HTTPC *httpc, const char *classname, const char *resource, int attr)
{
    int rc;

    if (!httpc || !httpc->cred || !httpc->cred->acee) return -1;

    rc = racf_auth(httpc->cred->acee, classname, resource, attr);

    if (rc == HTTP_RACF_NOTPROT) {
        http_dbgf("http_check_auth(%s:%s) rc=4: resource not protected,"
                  " access allowed\n",
                  classname ? classname : "(null)",
                  resource  ? resource  : "(null)");
    }

    return httpracf(rc) ? HTTP_RACF_PERMITTED : rc;
}

/* http_logout() - end the client's session: drop the CRED (and its ACEE) from
** the credential store by token and clear httpc->cred.  Returns 0 on success,
** -1 if there was no session (or no cached store).
**
** We must NOT reach the store via credtok_logout()/cred_array() here:
** cred_array() reads the credential array out of the *current* GRT's WSA, which
** is only populated in httpd's own GRT.  A CGI reached through the HTTPX vector
** runs under its own GRT, where that slot is empty -- so the token scan finds
** nothing, the CRED stays in httpd's real store, and the (deterministic) token
** keeps resolving: logout would be a silent no-op (issue #113).  Instead we use
** the array pointer httpd cached in the HTTPD block at cred_init() time
** (httpc->httpd->credarr), the same GRT-independent pattern http_get_password()
** uses for the blowfish key (#111).  The array lives in the shared address
** space (address-keyed ENQ lock, subpool-0 storage), so mutating/freeing it
** from the CGI's GRT is safe. */
__asm__("\n&FUNC SETC 'HTTPLOUT'");
#undef http_logout
int
http_logout(HTTPC *httpc)
{
    int rc;

    if (!httpc || !httpc->cred || !httpc->httpd) return -1;

    rc = credtok_logout_arr(httpc->httpd->credarr, &httpc->cred->token);
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
