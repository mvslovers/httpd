/* HTTPDMTT.C - CGI Program, Display Master Trace Table */
#include "httpd.h"
#include "clibmtt.h"

#define httpx   (httpd->httpx)

static int getself(char *jobname, char *jobid);
static int display_mtt(HTTPD *httpd, HTTPC *httpc, int data);

int main(int argc, char **argv)
{
    int         rc      = 0;
    CLIBPPA     *ppa    = __ppaget();
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    char        *data   = NULL;

    if (!httpd) {
        wtof("This program %s must be called by the HTTPD web server%s", argv[0], "");
        /* TSO callers might not see a WTO message, so we send a STDOUT message too */
        printf("This program %s must be called by the HTTPD web server%s", argv[0], "\n");
        return 12;
    }

    if (!data) data = http_get_env(httpc, "QUERY_DATA");
    if (!data) data = http_get_env(httpc, "QUERY_D");
    if (!data) data = "0";

    display_mtt(httpd, httpc, atoi(data));

quit:
    return 0;
}

static int
display_mtt(HTTPD *httpd, HTTPC *httpc, int data)
{
    int         rc      = 0;
    CMTT        *cmtt   = NULL;
    MTENTRY     **mtentry;
    unsigned    n, count;

    if (rc = http_resp(httpc,200) < 0) goto quit;
    if (rc = http_printf(httpc, "Cache-Control: no-store\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "Content-Type: %s\r\n", data ? "text/plain" : "text/html") < 0) goto quit;
    if (rc = http_printf(httpc, "Access-Control-Allow-Origin: *\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "\r\n") < 0) goto quit;

    if (!data) {
        if (rc = http_printf(httpc, "<!DOCTYPE html>\n") < 0) goto quit;
        if (rc = http_printf(httpc, "<html>\n") < 0) goto quit;

        if (rc = http_printf(httpc, "<head>\n") < 0) goto quit;
        if (rc = http_printf(httpc, "<title>HTTPDMTT</title>\n") < 0) goto quit;
        if (rc = http_printf(httpc, "</head>\n") < 0) goto quit;

        if (rc = http_printf(httpc, "<body>\n") < 0) goto quit;
    }

    /* allocate Master Trace Table */
    cmtt = cmtt_new();
    if (!cmtt) {
        const char *errmsg = "Unable to allocate Master Trace Table.";
        if (data) {
            http_printf(httpc, "%s\n", errmsg);
            goto quit;
        }
        
        http_printf(httpc, "<h2>%s</h2>\n", errmsg);
        http_printf(httpc, "</body>\n");
        http_printf(httpc, "</html>\n");
        goto quit;
    }

    if (!data) {
        http_printf(httpc, "<h2>Master Trace Table</h2>\n");
        http_printf(httpc, "<pre>\n");
    }

    /* allocate array of entries */
    mtentry = cmtt_get_array(cmtt);
    
    count = array_count(&mtentry);
    for(n=0; n < count; n++) {
        MTENTRY *mt = mtentry[n];
        
        if (!mt) continue;
        
        http_printf(httpc, "%s\n", mt->mtentdat);
    }

    if (!data) {
        http_printf(httpc, "</pre>\n");
        http_printf(httpc, "</body>\n");
        http_printf(httpc, "</html>\n");
    }
	
quit:
    cmtt_free(&cmtt);
	return 0;
}

static int
getself(char *jobname, char *jobid)
{
    unsigned    *psa        = (unsigned*)0;
    unsigned    *tcb        = (unsigned*)psa[540/4];    /* A(current TCB) */
    unsigned    *jscb       = (unsigned*)tcb[180/4];    /* A(JSCB) */
    unsigned    *ssib       = (unsigned*)jscb[316/4];   /* A(SSIB) */

    const char  *name       = (const char*)tcb[12/4];   /* A(TIOT), but the job name is first 8 chars of TIOT so we cheat just a bit */
    const char  *id         = ((const char*)ssib) + 12; /* jobid is in SSIB at offset 12 */
    int         i;

    for(i=0; i < 8 && name[i] > ' '; i++) {
        jobname[i] = name[i];
    }
    jobname[i] = 0;

    for(i=0; i < 8 && id[i] >= ' '; i++) {
        jobid[i] = id[i];
        /* the job id may have space(s) which should be '0' */
        if (jobid[i]==' ') jobid[i] = '0';
    }
    jobid[i] = 0;

    return 0;
}
