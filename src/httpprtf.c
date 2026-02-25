/* HTTPPRTF.C
** HTTP Printf
*/
#include "httpd.h"

extern int
httpprtf(HTTPC *httpc,  const char *fmt, ... )
{
    int     rc  = 0;
    va_list args;

    va_start( args, fmt );
    rc = http_printv(httpc, fmt, args);
    va_end( args );

    return rc;
}
