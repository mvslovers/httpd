/* HTTPNOBD.C
** Does a response carry a message body (RFC 7230 3.3.3)?
**
** Kept free of httpd.h so the decision unit-tests dual (host + MVS).
*/
#include "httpnobd.h"

int
httpnobd(int head, int status)
{
    if (head) return 1;
    if (status >= 100 && status < 200) return 1;
    return status == 204 || status == 304;
}
