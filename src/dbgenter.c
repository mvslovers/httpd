/* DBGENTER.C
**
*/
#include "httpd.h"

int
dbgenter(const char *fmt, ...)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc  = 0;
    va_list     list;
    char        title[256];

    if (httpd->dbg) {
        va_start(list, fmt);
        vsprintf(title, fmt, list);
        va_end(list);

        dbgtime(NULL);
        fputs("Enter ", httpd->dbg);
        fputs(title, httpd->dbg);
    }

    return rc;
}
