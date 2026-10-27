/* DBGENTER.C
**
*/
#include "httpd.h"

int
dbgenter(const char *fmt, ...)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    FILE        *fp     = httpd->dbg;
    int         rc  = 0;
    va_list     list;
    char        title[256];

    if (fp) {
        va_start(list, fmt);
        vsprintf(title, fmt, list);
        va_end(list);

        dbgtime(NULL);
        fputs("Enter ", fp);
        fputs(title, fp);
        dbgfail(httpd, fp);
    }

    return rc;
}
