/* HELLO.C - CGI Sample Program, should work standalone or when linked to by HTTPD */
#include "httpd.h"

#define httpx   (httpd->httpx)

int main(int argc, char **argv)
{
    CLIBPPA     *ppa    = __ppaget();
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    int     i;

#if 0
    wtof("HELLO main PPA=%08X", ppa);
    wtof("HELLO main PPAPREV=%08X", ppa->ppaprev);

    for(i=0; i < argc; i++) {
        wtof("HELLO main argv#%d=%s", i, argv[i]);
    }
#endif

    if (httpd) {
        http_resp(httpc,200);
        http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
        http_printf(httpc, "\r\n");
    }

    printf("Hello from %s\n", argv[0]);

    return 1234;
}
