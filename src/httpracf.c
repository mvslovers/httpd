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
