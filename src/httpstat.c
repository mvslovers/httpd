/* HTTPSTAT.C
** Map an HTTP status code to its status-line text.
**
** Kept free of httpd.h (string literals only) so the table unit-tests dual
** (host + MVS).  A switch over literals needs no writable static, so the
** module stays reentrant.
**
** Every code httpd itself emits must be in here: an absent code is answered
** by httpresp() with 500, which puts a "server failed" status line in front
** of a body that says something else entirely.
*/
#include "httpstat.h"

const char *
httpstat(int code)
{
    switch (code) {
    case 200: return "200 OK";
    case 201: return "201 Created";
    case 202: return "202 Accepted";
    case 204: return "204 No Content";
    case 206: return "206 Partial Content";
    case 301: return "301 Moved Permanently";
    case 302: return "302 Moved Temporarily";
    case 303: return "303 See Other";
    case 304: return "304 Not Modified";
    case 400: return "400 Bad Request";
    case 401: return "401 Unauthorized";
    case 403: return "403 Forbidden";
    case 404: return "404 Not Found";
    case 405: return "405 Method Not Allowed";
    case 409: return "409 Conflict";
    case 410: return "410 Gone";
    case 412: return "412 Precondition Failed";
    case 413: return "413 Payload Too Large";
    case 414: return "414 URI Too Long";
    case 429: return "429 Too Many Requests";
    case 500: return "500 Internal Server Error";
    case 501: return "501 Not Implemented";
    case 502: return "502 Bad Gateway";
    case 503: return "503 Service Unavailable";
    case 505: return "505 HTTP Version Not Supported";
    case 507: return "507 Insufficient Storage";
    default : return (const char *)0;
    }
}
