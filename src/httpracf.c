/* HTTPRACF.C
** Decide whether a RACHECK (racf_auth()) return code allows the access.
**
** Kept free of httpd.h so the SAF decision table unit-tests dual (host + MVS).
*/
#include <ctype.h>
#include "httpracf.h"

int
httpracf(int rc)
{
    return (rc == HTTP_RACF_PERMITTED || rc == HTTP_RACF_NOTPROT);
}

static int
token_is(const char *value, const char *word)
{
    while (*word) {
        if (toupper((unsigned char)*value) != *word) return 0;
        value++;
        word++;
    }
    return *value == '\0' || *value == ' ';
}

int
httpracv(const char *value)
{
    if (!value) return -1;
    if (token_is(value, "DENY"))  return 1;
    if (token_is(value, "ALLOW")) return 0;
    return -1;
}

int
httpracr(int rc, int refuse_noprof)
{
    if (rc == HTTP_RACF_NOTPROT && refuse_noprof) return HTTP_RACF_V_NOPROF;
    return httpracf(rc) ? HTTP_RACF_V_ALLOW : HTTP_RACF_V_DENY;
}
