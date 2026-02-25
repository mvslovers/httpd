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
#include <time64.h>					/* 64 bit time prototypes		*/
#include <errno.h>
#include "clibppa.h"                /* C runtime program properties */
#include "clibcrt.h"                /* C runtime area for each task */
#include "clibenv.h"                /* C runtime environment vars   */
#include "clibstae.h"               /* C runtime recovery routines  */
#include "clibwto.h"                /* write to operator            */
#include "cliblock.h"               /* lock prototypes              */
#include "clibary.h"                /* dynamic arrays               */
#include "sha256.h"					/* SHA 256 function				*/
#include "blowfish.h"				/* Blow Fish Encryption			*/
#include "socket.h"                 /* sockets via DYN75            */
#include "errors.h"                 /* the missing errno values     */
#include "racf.h"                   /* security environment         */

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
	unsigned char   flags;			/* 4F flags						*/
#define CRED_FLAG_NO_DELETE 0x80	/* ,,, do not delete cred		*/
#define CRED_FLAG_KEEP_ACEE	0x40	/* ... do not delete cred->acee	*/	
};									/* 50 (80 bytes)				*/

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

/* credtok_gen() returns CREDTOK struct for id parm. id parm may be NULL */
extern CREDTOK 
credtok_gen(CREDID *id)																		asm("CREDTOKG");

/* logout CRED for this CREDTOK */
extern int 
credtok_logout(CREDTOK *token)																asm("CREDTOKL");

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
 */
extern CRED * 
cred_login(unsigned addr, unsigned char *userid, unsigned char *password)					asm("CREDLIN");

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
