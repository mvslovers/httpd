/* HTTPDECP.C
** Decode an escaped URI path.
**
** Like http_decode(), but a "+" is an ordinary character in a path (RFC 3986);
** it means a space only in a query or form value.  Not part of the httpx
** vector: only the request parser decodes the path.
*/
#include "httpd.h"
#include "httpdecx.h"

extern UCHAR *
httpdecp(UCHAR *str)
{
    return httpdecx(str, http_codepage()->atoe, 0);
}
