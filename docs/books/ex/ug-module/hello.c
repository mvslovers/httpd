/* HELLO - a small HTTPD server module.
 *
 *   GET  /hello?name=X   answers "Hello, X!" (X defaults to "world")
 *   POST /hello          answers with the request body it received
 *   anything else        405 with an Allow header
 */
#include <mvs/crt.h>            /* __grtget(), CLIBGRT */
#include "httpcgi.h"

/* every http_xxx macro reaches the server through the HTTPX vector */
#define httpx   http_get_httpx(httpd)

int
main(int argc, char **argv)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;     /* the server               */
    HTTPC       *httpc  = grt->grtapp2;     /* the client being served  */
    UCHAR       *method = http_get_env(httpc, "REQUEST_METHOD");
    UCHAR       *text;
    (void)argc;
    (void)argv;

    if (method && http_cmp(method, "GET") == 0) {
        text = http_get_env(httpc, "QUERY_name");
        http_resp(httpc, 200);
        http_printf(httpc, "Content-Type: text/plain\r\n");
        http_printf(httpc, "\r\n");     /* the blank line is a call of its own */
        http_printf(httpc, "Hello, %s!\n", text ? (char *)text : "world");
    }
    else if (method && http_cmp(method, "POST") == 0) {
        text = http_get_env(httpc, "POST_STRING");
        http_resp(httpc, 200);
        http_printf(httpc, "Content-Type: text/plain\r\n");
        http_printf(httpc, "\r\n");
        http_printf(httpc, "You sent: %s\n", text ? (char *)text : "");
    }
    else {
        http_resp(httpc, 405);
        http_printf(httpc, "Allow: GET, POST\r\n");
        http_printf(httpc, "Content-Type: text/plain\r\n");
        http_printf(httpc, "\r\n");
        http_printf(httpc, "405 Method Not Allowed\n");
    }
    return 0;
}
