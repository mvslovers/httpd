/* HTTPSTAT.H
** Map an HTTP status code to its status-line text ("<code> <reason-phrase>").
**
** Free of httpd.h so the table unit-tests on the host as well as on MVS (see
** test/tststat.c).  The name is 8 characters so it needs no asm() CSECT alias
** -- same reason as httpbody().
*/
#ifndef HTTPSTAT_H
#define HTTPSTAT_H

/* Return the status line text for code -- the decimal code, one blank, and the
** reason phrase, e.g. "404 Not Found" -- or NULL when the code is not in the
** table.
**
** NULL means "this server has no phrase for that code", not "invalid code".
** httpresp() answers it with 500 and a WTO rather than putting a status line
** it cannot spell on the wire; see src/httpresp.c.
*/
const char *httpstat(int code);

#endif /* HTTPSTAT_H */
