/* HTTPSSI.H
** Pure helpers for Server Side Includes (httpfile.c).
**
** Free of httpd.h so the logic unit-tests on the host as well as on MVS
** (see test/tstssi.c).
*/
#ifndef HTTPSSI_H
#define HTTPSSI_H

#include <stddef.h>

/* the longest directive that may span two reads of a line, in bytes */
#define SSI_HOLDMAX 384

/* A line is read in pieces; buf[0..len) is one piece of a line that goes on.
** Returns the offset from which the piece must be kept back and joined with
** the next one, because it may be the start of a directive that this piece
** does not complete: the last "<!--#" without a following " -->", or a
** trailing part of "<!--#" itself.  Returns len when nothing is to be held,
** also when the held part would be longer than max bytes (such a directive
** is not recognised, it goes out as text).
*/
size_t httpssih(const char *buf, size_t len, size_t max);

/* Copy *src to dst as HTML text: & < > " ' become entities.  dst is always
** NUL-terminated.  Stops when src ends or when the next entity no longer
** fits in dst; *src is advanced past everything that was copied, so the
** caller loops until **src is 0.  Returns the number of bytes written to dst.
** dstsize must be at least 8.
*/
size_t httpssie(char *dst, size_t dstsize, const char **src);

#endif /* HTTPSSI_H */
