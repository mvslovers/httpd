/* DBGW.C
**
*/
#include "httpd.h"

int
dbgw(const char *buf, int len)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    FILE        *fp     = httpd->dbg;
    int         rc      = 0;

    if (fp) {
        if (len) {
            fwrite(buf, 1, len, fp);
            dbgfail(httpd, fp);
        }
    }

    return rc;
}
