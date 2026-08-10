/* HTTPCGSP.C
** Heap subpool for the CGI that is about to run (issue #154)
*/
#define HTTP_PRIVATE
#include "httpd.h"

__asm__("\n&FUNC SETC 'HTTPCGSP'");

#undef http_cgi_subpool

/*
** http_cgi_subpool() - which heap subpool this request's CGI allocates from.
**
** cgistart calls this from inside the module, once per LINK, and brackets
** main() with __setsp() on the answer.  It has to be asked rather than known
** because the decision is per route (RECLAIM=YES) and cgistart is linked into
** the module, where the route table is not reachable.
**
** It is also the version handshake, and that is why it lives in a vector slot
** that was reserved rather than appended: a server built before #154 has 0 in
** it, so a module built after #154 finds no function to call, sets no subpool,
** and behaves exactly as it does today.  Appending would have made the module
** read past the end of that server's vector.
**
** The route is re-matched from REQUEST_PATH instead of being carried in the
** HTTPC, which stays exactly 4096 bytes.  That costs one pass over the route
** table per CGI request -- the same lookup httppc() already did -- and keeps
** the flag in the one place it is configured.
**
** Returns HTTP_CGI_SUBPOOL for a RECLAIM=YES route, 0 for everything else
** (including a LOC route, a path that no longer matches, or no HTTPC).
*/
UCHAR
http_cgi_subpool(HTTPC *httpc)
{
    HTTPCGI     *cgi;
    const char  *path;

    if (!httpc || !httpc->httpd) return 0;

    path = (const char *) http_get_env(httpc, "REQUEST_PATH");
    if (!path) return 0;

    cgi = http_find_cgi(httpc->httpd, path);
    if (!cgi || !cgi->reclaim) return 0;

    return HTTP_CGI_SUBPOOL;
}
