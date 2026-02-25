/* DBGS.C
**
*/
#include "httpd.h"

int
dbgs(const char *str)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;

    if (httpd->dbg) {
        int len = str ? strlen(str) : 0;

        if (len) {
            dbgtime(NULL);
            fputs(str, httpd->dbg);
        }
    }

    return rc;
}
