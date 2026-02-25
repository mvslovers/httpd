/* DBGF.C
**
*/
#include "httpd.h"

int
dbgf(const char *fmt, ...)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc  = 0;
    va_list     list;

    if (httpd->dbg) {
        dbgtime(NULL);
        va_start(list, fmt);
        vfprintf(httpd->dbg, fmt, list);
        va_end(list);
    }

    return rc;
}
