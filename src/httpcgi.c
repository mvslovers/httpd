/* HTTPCGI.C - CGI interface accessor functions */
#define HTTP_PRIVATE
#include "httpd.h"

/*
** http_get_httpx - return the HTTPX function vector from an HTTPD handle.
**
** CGI modules treat HTTPD as an opaque pointer and call this function
** to obtain the HTTPX vector.  They then use the httpcgi.h macro layer
** to call server functions without needing the full httpd.h header.
*/
HTTPX *http_get_httpx(HTTPD *httpd)                         asm("HTTPCGIX");
HTTPX *http_get_httpx(HTTPD *httpd)
{
    return httpd ? httpd->httpx : 0;
}
