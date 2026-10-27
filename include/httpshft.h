/* HTTPSHFT.H
** Drop the part of a send buffer that has gone out (issue #440).
**
** Free of httpd.h so the arithmetic unit-tests on the host as well as on MVS
** (see test/tstshft.c).
*/
#ifndef HTTPSHFT_H
#define HTTPSHFT_H

/* buf[0..len) holds data, buf[0..pos) of it has been sent.  Move the unsent
** rest to the front of buf and clear what is left behind.
**   buf    the buffer
**   size   its capacity; a len above it is taken as size
**   pos    bytes sent so far
**   len    bytes in the buffer
**
** Returns the number of unsent bytes, which is the new len; the new pos is
** always 0.  When nothing is left the whole buffer is cleared. */
unsigned httpshft(unsigned char *buf, unsigned size,
                  unsigned pos, unsigned len);

#endif /* HTTPSHFT_H */
