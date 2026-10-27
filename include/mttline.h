/* MTTLINE.H
** Pure helper of the /.dmtt Master Trace Table display (httpdmtt.c).
**
** Free of httpd.h so the logic unit-tests on the host as well as on MVS
** (see test/tstmttl.c).
*/
#ifndef MTTLINE_H
#define MTTLINE_H

#include <stddef.h>

/* The text for the start of one trace entry's data.
**
** Reads at most srclen bytes of src and stops early at a NUL; the entry data
** is not guaranteed to be NUL-terminated, so the length bounds the read.  With
** html non-zero '<', '>', '&', '"' and '\'' become entities, otherwise the
** bytes are copied as they are.  Writes a NUL-terminated string of at most
** dstsize - 1 characters to dst and returns the number of source bytes it
** took.  A replacement is written whole or not at all, so the output never
** ends in the middle of an entity; the caller calls again for the rest while
** the result is non-zero and source bytes are left.
**
** A dstsize of 7 or more (the longest entity plus the NUL) takes at least one
** byte of any non-empty entry.
*/
size_t mttline(unsigned char *dst, size_t dstsize,
               const unsigned char *src, size_t srclen, int html);

#endif /* MTTLINE_H */
