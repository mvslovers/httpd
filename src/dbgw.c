/* DBGW.C
**
*/
#include "httpd.h"

int
dbgw(const char *buf, int len)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;

    if (httpd->dbg) {
        if (len) {
            fwrite(buf, 1, len, httpd->dbg);
        }
    }

    return rc;
}
