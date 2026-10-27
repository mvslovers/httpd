/* DBGS.C
**
*/
#include "httpd.h"

int
dbgs(const char *str)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    FILE        *fp     = httpd->dbg;
    int         rc      = 0;

    if (fp) {
        int len = str ? strlen(str) : 0;

        if (len) {
            dbgtime(NULL);
            fputs(str, fp);
            dbgfail(httpd, fp);
        }
    }

    return rc;
}
