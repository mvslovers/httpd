/* HTTPDECX.H
** Percent-decoding of a URI component, with the code page passed in.
**
** Free of httpd.h so the decision table unit-tests on the host as well as on
** MVS (see test/tstdecx.c).
*/
#ifndef HTTPDECX_H
#define HTTPDECX_H

/* Decode str in place and return it.
**
**   "%xx"  with two hex digits becomes atoe[xx];
**   "+"    becomes a space when plus is non-zero (a query or form value) and
**          stays a "+" when it is zero (a path, where "+" is an ordinary
**          character, RFC 3986);
**   an escape that is not two hex digits ("%zz", "%4", a trailing "%") and
**   "%00" (which would end the string and cut everything behind it) are left
**   as they are.
**
** The result is never longer than the input.
*/
unsigned char *httpdecx(unsigned char *str, const unsigned char *atoe, int plus);

#endif /* HTTPDECX_H */
