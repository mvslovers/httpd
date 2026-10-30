#ifndef CRED_H
#define CRED_H
/*
** Copyright (c) 2016 Mike Rayborn. All rights reserved.
*/

/* crent370 headers */
#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <ext/time64.h>					/* 64 bit time prototypes		*/
#include <errno.h>
#include <mvs/storage.h>					/* __setsp()/__getmsp() subpool		*/
#include <mvs/crt.h>                /* C runtime PPA, CRT and GRT   */
#include <mvs/env.h>                /* C runtime environment vars   */
#include <mvs/recovery.h>           /* recovery, try(), tryrc()     */
#include <mvs/wto.h>                /* write to operator            */
#include <mvs/lock.h>               /* lock prototypes              */
#include <ext/array.h>              /* dynamic arrays               */
#include <sha256.h>					/* SHA 256 function				*/
#include <blowfish.h>				/* Blow Fish Encryption			*/
#include "errors.h"                 /* the missing errno values     */
#include <mvs/racf.h>               /* security environment         */

/* Client credentials */
typedef struct cred 	CRED;		/* Credential					*/
typedef struct credid	CREDID;		/* Credential Identifer			*/
typedef union  credtok	CREDTOK;	/* Credential Token				*/
typedef BLOWFISH_KEY	CREDKEY;	/* Credential Key				*/

/* Client Identifier struct */
struct credid {
	unsigned		addr;			/* 00 Client IP address			*/
	unsigned char	userid[9];		/* 04 user id (8+0)				*/
	unsigned char   userflg;		/* 0D user id flags				*/
#define CREDID_USER_ENCRYPT 0x80	/* ... userid is encrypted		*/
	unsigned char	password[9];	/* 0E password (8+0)			*/
	unsigned char	passflg;		/* 17 password flags			*/
#define CREDID_PASS_ENCRYPT	0x80	/* ... password is encrypted	*/
};									/* 18 (24 bytes)				*/

union credtok {
	unsigned char	c[32];			/* 00 character array			*/
	unsigned		u[8];			/* 00 unsigned array			*/
};									/* 20 (32 bytes)				*/

/* Client Credential struct */
struct cred {
	unsigned char	eye[8];			/* 00 Eye catcher for dumps		*/
#define CRED_EYE "*CRED*"			/* ...							*/
	time64_t		last;			/* 08 last used time stamp		*/
	CREDTOK			token;			/* 10 SHA256 hash of HTTPID		*/
	CREDID			id;				/* 30 client id					*/
	ACEE			*acee;			/* 48 RACF ACEE					*/
	unsigned short	len;			/* 4C length of CRED			*/
	unsigned char	version;		/* 4E version number			*/
#define CRED_VERSION	3			/* ... 2 added created (#118)	*/
									/*     3 added stale (#367)		*/
	unsigned char   flags;			/* 4F flags						*/
#define CRED_FLAG_NO_DELETE 0x80	/* ,,, do not delete cred		*/
#define CRED_FLAG_KEEP_ACEE	0x40	/* ... do not delete cred->acee	*/
#define CRED_FLAG_SESSION	0x20	/* ... private login session	*/
									/*     (#288): never found by	*/
									/*     CREDID, only by token	*/
#define CRED_FLAG_STALE		0x10	/* ... RACF refused its password*/
									/*     (#367): never served by	*/
									/*     cred_login() again, freed	*/
									/*     CRED_STALE_GRACE later	*/
	time64_t		created;		/* 50 creation time stamp (#118)*/
	time64_t		stale;			/* 58 when CRED_FLAG_STALE was	*/
									/*    set (#367), else 0		*/
};									/* 60 (96 bytes)				*/

/* cred_init() - initialize CRED environment, generates BLOWFISH_KEY for credkey() */
extern int 
cred_init(void *salt, unsigned saltlen)														asm("CREDINIT");

/* credid_init() return initialed CREDID struct. id parm may be NULL */
extern CREDID 
credid_init(CREDID *id) 																	asm("CREDIDIN");

/* credid_update() return id parm. updates id parm with other parms */
extern CREDID *
credid_update(CREDID *id, unsigned *addr, unsigned char *userid, unsigned char *password)	asm("CREDIDUP");

/* credid_update_enc() return id parm or NULL if key error. update then encrypt userid, password */
extern CREDID *
credid_update_enc(CREDID *id, unsigned *addr, unsigned char *userid, unsigned char *password) asm("CREDIDUE");

/* credid_dec() decrypt CREDID in, returns CREDID out */
extern CREDID *
credid_dec(CREDID *in, CREDID *out)															asm("CREDIDDE");

/* credid_enc() encrypt CREDID in, returns CREDID out */
extern CREDID *
credid_enc(CREDID *in, CREDID *out)															asm("CREDIDEN");

/* credtok_gen() returns CREDTOK struct for id parm. id parm may be NULL.
 *
 * DETERMINISTIC -- the same CREDID always yields the same token.  No longer
 * used for live sessions (#188): that made a leaked token an offline oracle for
 * the password, and meant logout/expiry did not bind a token client, because
 * re-presenting the same credentials minted the identical token.  Kept for
 * tests that need a reproducible token value; use credtok_rand() for a
 * session. */
extern CREDTOK
credtok_gen(CREDID *id)																		asm("CREDTOKG");

/* credtok_rand() - mint an unpredictable session token (#188).  The key must be
 * passed in, never read via credkey(): that is GRT-relative and reads back zero
 * outside httpd's own GRT (#109/#111).  id may be NULL. */
extern CREDTOK
credtok_rand(CREDKEY *key, CREDID *id)														asm("CREDTOKR");

/* logout CRED for this CREDTOK (uses cred_array() -- caller must run in the
   GRT where the credential store was initialized, i.e. httpd's own core) */
extern int
credtok_logout(CREDTOK *token)																asm("CREDTOKL");

/* credtok_logout_arr() - like credtok_logout() but operates on the caller-
   supplied credential array instead of cred_array().  cred_array() reads the
   current GRT's WSA, so it is empty/foreign from a CGI's GRT; passing httpd's
   own array pointer (cached at cred_init() time) makes logout work from any
   execution context.  See issue #113 (same GRT/WSA class as #109/#111). */
extern int
credtok_logout_arr(CRED ***array, CREDTOK *token)											asm("CREDTKLA");

/* cred_new() - allocate a new CRED struct. all parms can be NULL */
extern CRED *
cred_new(CREDID *id, CREDTOK *token, ACEE *acee, unsigned char flags)						asm("CREDNEW");

/* cred_free() - deallocate CRED struct */
extern int 
cred_free(CRED **cred)																		asm("CREDFREE");

/* cred_free_array() - deallocate array of CRED structs */
extern unsigned 
cred_free_array(void)																		asm("CREDFREA");

/* cred_add() add credential to credential array */
extern int cred_add(CRED *cred)																asm("CREDADD");

/* cred_find_by_id() - find credential in credential array by CREDID */
extern CRED *
cred_find_by_id(CREDID *id)																	asm("CREDFBID");

/* cred_find_by_token() - find credential in credential array by CREDTOK */
extern CRED *
cred_find_by_token(CREDTOK *token)															asm("CREDFBTK");

/* cred_find_by_acee() - find credential in credential array by ACEE */
extern CRED *cred_find_by_acee(ACEE *acee)													asm("CREDFBAC");

/* cred_login() - perform racf login and allocate a cred structure.
 * note: caller must be APF authorized otherwise S047 abend.
 * returns NULL on error, check console for errors.
 * cred was added to WSA cred array if not NULL.
 *
 * This is the one find-then-authenticate chokepoint: a cached CRED is returned
 * without touching RACF, which is what keeps re-auth cheap.  maxage_secs (#118)
 * bounds that -- a cached credential older than the limit is discarded and the
 * password is checked against RACF again.  0 disables the limit.  Every caller
 * of this function must pass it, or its flow silently keeps the unbounded
 * behaviour (that is how the form-login path was missed once already).
 */
extern CRED *
cred_login(unsigned addr, unsigned char *userid, unsigned char *password,
           unsigned maxage_secs)															asm("CREDLIN");

/* overflow cap on the credential array (login flood protection, M2) */
#define CRED_MAX 256

/* cred_session() - mint a PRIVATE session for the client behind an existing
 * credential (#288): a CRED of its own, with its own random token and its own
 * ACEE, flagged CRED_FLAG_SESSION.  For an explicit login -- the z/OSMF
 * authenticate POST and the form login -- never for a plain Basic request.
 *
 * Why a session of its own: cred_login() finds the credential by CREDID
 * (address, userid, password), so every login with the same credentials from
 * the same address used to get the SAME token, and a logout by one of them
 * ended all of them.  A session CRED is never returned by cred_find_by_id(), so
 * only its own token reaches it and a logout ends only that session.
 *
 * It copies the source's encrypted CREDID unchanged, so http_get_password()
 * still works on a session (the job card needs it, #164).  The ACEE is a fresh
 * racf_login(), never shared: a CRED frees its ACEE when it goes, and the
 * source and the session go at different times.
 *
 * array and key are passed in, never taken from cred_array()/credkey(): a CGI
 * calls this through the HTTPX vector, in a GRT where both are empty (#109,
 * #111, #113).  Returns the new CRED, already in the array, or NULL; on NULL
 * *why (when why is not NULL) says which of the CRED_SESS_ reasons below it was.
 *
 * CRED_SESS_REFUSED means RACF said the userid or password is no good: RACINIT
 * return codes 4 (user not defined), 8 (wrong password), 12 (password expired)
 * and 28 (access revoked).  Those prove the password cached in `from` is no
 * longer good (changed, or the userid revoked), and `from` is then marked
 * CRED_FLAG_STALE -- never freed: the request that called this still points at
 * it, and its module may have set the task ACEE from it (#367).  Any other
 * RACINIT failure (RACF not active, an installation exit, a terminal or time
 * restriction) says nothing about the cached password: `from` is left alone
 * and the reason is CRED_SESS_NOMEM, a failure of the server, not of the
 * client.  A source that is itself a private session is not marked: a session
 * is a bearer token, not a cached password, and find_by_id never returns it. */
extern CRED *
cred_session(CRED ***array, CREDKEY *key, CRED *from, int *why)								asm("CREDSESS");

/* why cred_session() returned NULL (#367).  http_new_session() maps these onto
 * the HTTP_SESSION_ values it returns to a module; the numbers match, but that
 * mapping is spelled out there, not assumed. */
#define CRED_SESS_NONE		(-1)	/* no source, or it has no password		*/
#define CRED_SESS_REFUSED	(-2)	/* RACF refused userid or password		*/
#define CRED_SESS_BUSY		(-3)	/* client at CRED_SESSION_MAX, all in	*/
									/* use; or the store is full			*/
#define CRED_SESS_NOMEM		(-4)	/* no storage for the CRED or the array,*/
									/* or RACF failed for another reason	*/

/* sessions one client may hold at a time (#288): same CREDID, so the same
 * userid, password and address.  A login without a logout costs a slot until
 * SESSION_TIMEOUT reaps it, so a client that never logs out -- a script in a
 * loop, an editor re-authenticating -- would otherwise fill CRED_MAX within the
 * idle timeout and lock EVERY user out of logging in, Basic included.  At the
 * limit the client's own least recently used session is ended to make room
 * (if it has been idle for CRED_SESSION_IDLE, else the login is refused);
 * nobody else's is ever touched.  16 is far above what one client needs.  It
 * bounds one client, not a deliberate flood: sixteen clients at the limit
 * together fill CRED_MAX. */
#define CRED_SESSION_MAX 16

/* seconds a session must have been idle before CRED_SESSION_MAX may end it.  A
 * request holds its CRED by a plain pointer, so ending a session one of its
 * requests is still using would leave that request on freed storage.  A client
 * with all its sessions busy gets its login refused instead -- 16 logins inside
 * a minute without a logout is a loop, not a user. */
#define CRED_SESSION_IDLE 60

/* seconds a CRED_FLAG_STALE entry is kept after it was marked, before
 * cred_login() or cred_reap() frees it (#367).  The request that met the
 * refusal, and any request that resolved the entry before it, still holds it by
 * a plain pointer, possibly with the task ACEE set from it, so it cannot go at
 * once.  It is counted from the mark, not from cred->last: a lookup refreshes
 * that.  Within the grace period cred_login() does not serve the entry -- it
 * asks RACF again.  A request that is still running when the period ends can
 * lose its CRED, the same kind of bound as SESSION_MAXAGE >> longest request
 * (HTTPD032E); 10 minutes is far above any request the server is built for. */
#define CRED_STALE_GRACE 600

/* cred_reap() - free CREDs (and their ACEEs) idle longer than ttl_secs, or
 * older than maxage_secs counted from cred->created regardless of activity
 * (#118).  Either limit may be 0 to disable it; both 0 disables reaping
 * entirely.  Returns the number reaped. */
extern unsigned cred_reap(unsigned ttl_secs, unsigned maxage_secs)							asm("CREDREAP");

/* credexp() - pure expiry decision: has a credential idle for elapsed_secs
 * exceeded ttl_secs? (ttl 0 -> never; negative elapsed -> not yet; '>' bound)
 *
 * The same decision serves the hard max-age (#118) unchanged -- pass the age
 * since cred->created instead of the idle time, and the max-age instead of the
 * TTL.  The two differ in which clock they measure, not in the arithmetic, so
 * this stays one function rather than a copy that can drift. */
extern int credexp(long elapsed_secs, unsigned ttl_secs)									asm("CREDEXP");

/* credunam() - pure identity check: does the ACEE a login returned (name =
 * aceeusri, namelen = aceeusrl) belong to the userid that was asked for?
 * Case-insensitive, both cut at 8 characters, trailing blanks ignored.  An
 * empty name never matches. */
extern int credunam(const char *want, const unsigned char *name, unsigned namelen)			asm("CREDUNAM");

/* http_stcuser() - is user the started-task account (STC) of the RAKF 1.2
 * family?  STC is never a client account; the check refuses it whatever the
 * security system would answer.  The comparison is credunam()'s (case,
 * padding, 8 characters). */
extern int http_stcuser(const char *user)													asm("HTTPSTCU");

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
/* The following function prototypes are used internally.                             */
/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */
/* credkey() - retrieve the WSA pointer to the blowfish key for encryption/decryption */
extern CREDKEY *
credkey(void)																				asm("CREDKEY");

/* cred_array() - retrieve the WSA pointer to the CRED array */
extern CRED ***
cred_array(void)																			asm("CREDARR");


#endif	/* CRED_H */
