/* DMCHUNK.H
** Pure helpers of the /.dm memory display (httpdm.c).
**
** Free of httpd.h so the logic unit-tests on the host as well as on MVS
** (see test/tstdmchk.c).
*/
#ifndef DMCHUNK_H
#define DMCHUNK_H

#define DM_CHUNK_DEFAULT    16      /* bytes per line when none/zero given */
#define DM_CHUNK_MAX        64      /* most bytes per line                 */
#define DM_ESC_MAX          5       /* longest replacement dmesc() writes  */

/* Bytes per dump line from the client-supplied value: 0 gives the default,
** anything above DM_CHUNK_MAX is cut to it, and the rest is rounded up to a
** whole word.  The result is always a multiple of 4 in 4..DM_CHUNK_MAX.
*/
int dmchunk(unsigned long value);

/* The text for one character of the EBCDIC/ASCII column in HTML mode: '<',
** '>' and '&' become entities, everything else stands for itself.  Writes at
** most DM_ESC_MAX characters plus a NUL to out and returns their number.
*/
int dmesc(unsigned char c, char *out);

#endif /* DMCHUNK_H */
