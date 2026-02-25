/* ABEND0C1.C - CGI Sample Program, should ABEND S0C1 */
#include "httpd.h"

#define httpx   (httpd->httpx)

int main(int argc, char **argv)
{
    CLIBPPA     *ppa    = __ppaget();
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    int     i;

    wtof("ABEND0C1 main PPA=%08X", ppa);

    __asm("DC\tH'0'");

    return 1234;
}

