/* HTTPCGSP.C
** Heap subpool for the CGI that is about to run (issues #154, #174)
*/
#define HTTP_PRIVATE
#include "httpd.h"

__asm__("\n&FUNC SETC 'HTTPCGSP'");

#undef http_cgi_subpool

/*
** http_cgi_subpool() - which heap subpool this request's CGI allocates from.
**
** cgistart calls this from inside the module, once per LINK, and brackets
** main() with __setsp() on the answer.  Since #174 the answer is the same for
** every module: the reclaim is the default, not a per-route option, and the
** storage contract for server modules is one sentence -- malloc() is request
** storage; storage that must outlive the request is obtained with
** __getmsp(size, 0).
**
** The function stays even though the answer is now constant, because the
** vector slot is the version handshake, and that is why it lives in a slot
** that was reserved rather than appended: a server built before #154 has 0 in
** it, so a module built after #154 finds no function to call, sets no subpool,
** and behaves as it did on that server.  Appending would have made the module
** read past the end of that server's vector.  And a future server can make
** the answer conditional again without touching a single module.
**
** Returns HTTP_CGI_SUBPOOL, or 0 for a client this server does not own
** (the module is running outside a request -- no reclaim will ever fire, so
** no subpool must be set).
*/
UCHAR
http_cgi_subpool(HTTPC *httpc)
{
    if (!httpc || !httpc->httpd) return 0;

    return HTTP_CGI_SUBPOOL;
}
