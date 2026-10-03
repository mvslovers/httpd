/* HTTPGSNA.C
** Get server name
** Transitions to next state as needed.
*/
#include "httpd.h"

extern UCHAR *
httpgsna(HTTPD *httpd)
{
    (void)httpd;
    return "HTTPD Server";
}
