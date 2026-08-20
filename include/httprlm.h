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

/* Longest realm the REALM keyword may configure (#193), excluding the NUL.
** Bounds the static buffer the settled realm lives in (httpprm.c) and the
** stack buffer the login form's title is composed in (httpcred.c). */
#define HTTP_REALM_CFG_MAX 64

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

/* httprlm_ok() - true if s is acceptable as a configured realm (#193).
**
** The value lands in two framings the parser cannot see from here: inside the
** quoted-string of `WWW-Authenticate: Basic realm="..."`, and in the login
** form's HTML.  Rather than escaping per consumer, the characters that would
** break either framing are refused outright -- none of them belongs in a
** human-readable system name:
**
**   - empty or NULL: realm is a required parameter (RFC 7617)
**   - longer than HTTP_REALM_CFG_MAX
**   - control characters (anything below ' ', so also CR/LF)
**   - '"' and '\' (quoted-string framing and escaping)
**   - '<', '>' and '&' (HTML)
*/
int httprlm_ok(const char *s);

#endif /* HTTPRLM_H */
