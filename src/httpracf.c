/* HTTPRACF.C
** Decide whether a RACHECK (racf_auth()) return code allows the access.
**
** Kept free of httpd.h so the SAF decision table unit-tests dual (host + MVS).
*/
#include "httpracf.h"

int
httpracf(int rc)
{
    return (rc == HTTP_RACF_PERMITTED || rc == HTTP_RACF_NOTPROT);
}

int
httpracr(int rc, int refuse_noprof)
{
    if (rc == HTTP_RACF_NOTPROT && refuse_noprof) return HTTP_RACF_V_NOPROF;
    return httpracf(rc) ? HTTP_RACF_V_ALLOW : HTTP_RACF_V_DENY;
}
