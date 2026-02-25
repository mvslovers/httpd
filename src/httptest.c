/* HTTPTEST.C - CGI Program */
#include "httpd.h"

#define httpx (httpd->httpx)

int main(int argc, char **argv)
{
    int         rc          = 0;
    char        ddname[12]  = "DD:";

    rc = __dsalcf(&ddname[3], 
        "DSN=&&testing,space=cyl(1,1),dsorg=ps,lrecl=121,blksize=12100,recfm=fb");
    if (!rc) {
        FILE *fp = fopen(ddname, "w");
        if (fp) {
            wtof("Opened %s for write", ddname);
            wtof("lrecl=%u blksize=%u", fp->lrecl, fp->blksize);
            fclose(fp);
        }
        
        rc = __dsfree(&ddname[3]);
        wtof("__dsfree(\"%s\"); rc=%d", &ddname[3], rc);
    }
    
    return 0;
}

#if 0 /* CGI test program */
int main(int argc, char **argv)
{
    int         rc      = 0;
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    const char  *p;
    char        buf[2048] = {0};
    
    /* make sure we're running on the HTTPD server */
    if (!httpd) {
        wtof("This program %s must be called by the HTTPD web server%s", "");
        /* TSO callers might not see a WTO message, so we send a STDOUT message too */
        printf("This program %s must be called by the HTTPD web server%s", "\n");
        return 12;
    }

    http_resp(httpc, 200);
    http_printf(httpc, "Content: text/plain\r\n");
    http_printf(httpc, "\r\n");

    /* get the request path string */
    p = http_get_env(httpc, "REQUEST_PATH");
    if (!p) {
        http_printf(httpc, "Unable to obtain REQUEST_PATH value\n");
    }
    else {
        http_printf(httpc, "%s: REQUEST_PATH=\"%s\"\n", argv[0], p);
    }

    /* get the request method string */
    p = http_get_env(httpc, "REQUEST_METHOD");
    if (!p) {
        http_printf(httpc, "Unable to obtain REQUEST_METHOD value\n");
    }
    else {
        http_printf(httpc, "%s: REQUEST_METHOD=\"%s\"\n", argv[0], p);
    }

    if (http_cmp(p, "PUT")==0) {
        http_printf(httpc, "httpc->socket=%d\n", httpc->socket);
        rc = recv(httpc->socket, buf, sizeof(buf)-1, 0);
        http_printf(httpc, "recv() rc=%d errno=%d\n", rc, errno);
        if (rc>0) {
            wtodumpf(buf, rc, "%s recv() ASCII", argv[0]);
            http_atoe(buf, rc);
            wtodumpf(buf, rc, "%s recv() EBCDIC", argv[0]);
            buf[rc] = 0;
            http_printf(httpc, "buf=\"%s\"\n", buf);
        }
    }  

quit:
    return 0;
}

#endif /* CGI test program */
