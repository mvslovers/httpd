/* DBGOPEN.C
** Open the debug/trace FILE when DEBUG=1.  A DD that cannot be opened falls
** back to stderr, so the operator still gets the trace he asked for.
*/
#include "httpd.h"
#include "httpdmsg.h"

void
dbgopen(HTTPD *httpd)
{
    if (httpd->dbg_enabled && !httpd->dbg) {
        httpd->dbg = fopen("DD:HTTPDBG", "w");
        if (!httpd->dbg) {
            wtof(MSG_CFG_DBG_FAILED, errno);
            wtof(MSG_CFG_DBG_STDERR);
            httpd->dbg = stderr;
        }
    }
}
