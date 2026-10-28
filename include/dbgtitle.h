/* DBGTITLE.H
** Bounded formatting of a debug-trace title.
**
** Free of httpd.h so the logic unit-tests on the host as well as on MVS
** (see test/tstdbgt.c).
*/
#ifndef DBGTITLE_H
#define DBGTITLE_H

#include <stdarg.h>
#include <stddef.h>

/* Format fmt/args into dst[size], truncating if needed.  The result is always
** NUL-terminated and nothing is written past dst[size-1].  Returns the length
** of the text stored (0 on a formatting error or a NULL fmt).
*/
int dbgtitle(char *dst, size_t size, const char *fmt, va_list args);

#endif /* DBGTITLE_H */
