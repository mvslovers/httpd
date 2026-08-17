/* HTTPRLM.H
** Build the Basic-auth realm from the system's SMF ID.
**
** Free of httpd.h so the trimming and the fallback unit-test on the host as
** well as on MVS (see test/tstrealm.c).  The name is 8 characters so it needs
** no asm() CSECT alias -- same reason as httpstat() and httpbody().
*/
#ifndef HTTPRLM_H
#define HTTPRLM_H

/* Fallback when the system supplies no usable SMF ID.  It is what the realm
** was hard-coded to before #191, so a system that cannot name itself keeps
** behaving exactly as it always did. */
#define HTTP_REALM_FALLBACK "MVS"

/* Longest realm this produces: 4 SMF characters or the fallback, + NUL. */
#define HTTP_REALM_MAX 8

/* httprlm() - copy the realm into out and return out.
**
** smfid points at the 4-character, blank-padded, NOT NUL-terminated SMF ID as
** __smfid() hands it over, or is NULL when the system has none.  Trailing
** blanks are trimmed, because a realm is text a browser shows a human in its
** credential dialog and "MVS " reads as a typo.  A NULL, empty or all-blank id
** yields HTTP_REALM_FALLBACK, never an empty string: realm is a required
** parameter of the Basic challenge (RFC 7617), and realm="" would make every
** protection space on the origin collide.
**
** out must hold at least HTTP_REALM_MAX bytes; a shorter buffer truncates
** rather than overflowing.
*/
char *httprlm(const char *smfid, char *out, unsigned outlen);

#endif /* HTTPRLM_H */
