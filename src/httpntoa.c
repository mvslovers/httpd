/* HTTPNTOA.C
** Convert network address to character string
*/
#include "httpd.h"

char *
httpntoa(struct in_addr in)
{
    CLIBCRT     *crt    = __crtget();
    char        *p  = (char *)&in.s_addr;

    sprintf(crt->crtntoa, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);

    return crt->crtntoa;
}
