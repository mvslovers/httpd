/* DBGTITLE.C
** Bounded formatting of a debug-trace title.
**
** dbgdump(), dbgenter() and dbgexit() format a caller-supplied printf string
** into a fixed title buffer; this is the one place that does it without
** running off the end of that buffer.  Kept free of httpd.h so it unit-tests
** on the host.
*/
#include <stdio.h>
#include "dbgtitle.h"

int
dbgtitle(char *dst, size_t size, const char *fmt, va_list args)
{
    int     n;

    if (!dst || size == 0) return 0;
    dst[0] = 0;
    if (!fmt) return 0;

    n = vsnprintf(dst, size, fmt, args);
    dst[size - 1] = 0;
    if (n < 0) {
        dst[0] = 0;
        return 0;
    }
    if ((size_t)n >= size) n = (int)(size - 1);

    return n;
}
