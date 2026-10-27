/* DBGF.C
**
*/
#include "httpd.h"

int
dbgf(const char *fmt, ...)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    FILE        *fp     = httpd->dbg;
    int         rc  = 0;
    va_list     list;

    if (fp) {
        dbgtime(NULL);
        va_start(list, fmt);
        vfprintf(fp, fmt, list);
        va_end(list);
        dbgfail(httpd, fp);
    }

    return rc;
}
