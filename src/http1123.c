/* HTTP1123.C
** Format RFC1123 style date
*/
#include "httpd.h"

extern UCHAR *
http1123( time64_t t, UCHAR *buf, size_t size )
{
    /* RFC1123 Date "Sat, 10 Sep 2016 13:05:57 GMT" */
    strftime( buf, size, "%a, %d %b %Y %H:%M:%S GMT", gmtime64(&t) );
    return buf;
}
