/* HTTPREPO.C
** Report results of request — write SMF Type 243 record and
** update in-memory counters.
** Transitions to next client state.
*/
#include "httpd.h"

static void httpsmf_write(HTTPD *httpd, HTTPC *httpc);

extern int
httprepo(HTTPC *httpc)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;

    http_enter("httprepo()\n");

    // Update simple counters
    httpd->total_requests++;
    if (httpc->resp >= 400)
        httpd->total_errors++;
    httpd->total_bytes_sent += httpc->sent;

    // Write SMF record if stats recording is enabled
    if (httpd->client & HTTPD_CLIENT_STATS) {
        httpsmf_write(httpd, httpc);
    }

    // Transition to next state
    httpc->state = CSTATE_RESET;
    http_exit("httprepo(), rc=%d\n", 0);
    return 0;
}

__asm__("\n&FUNC    SETC 'HTTPSMF'");
void
httpsmf(HTTPC *httpc)
{
    httprepo(httpc);
}

static void
httpsmf_write(HTTPD *httpd, HTTPC *httpc)
{
    SMF_HTTPD_REQ rec;
    const char *p;
    unsigned len;
    double elapsed;

    if (!smf_active())
        return;

    memset(&rec, 0, sizeof(rec));
    smf_init(&rec, sizeof(rec), SMF_TYPE_HTTPD);

    memcpy(rec.ssi, "HTTP", 4);
    rec.subtype = SMF_HTTPD_SUBTYPE_REQ;

    // Userid from credential
    if (httpc->cred) {
        CREDID id;
        credid_dec(&httpc->cred->id, &id);
        memcpy(rec.userid, id.userid, 8);
    }
    else {
        memset(rec.userid, ' ', 8);
    }

    rec.client_addr = httpc->addr;
    rec.resp_code   = httpc->resp;
    rec.bytes_sent  = httpc->sent;

    // Duration in milliseconds
    elapsed = httpc->end - httpc->start;
    if (elapsed < 0.0) elapsed = 0.0;
    rec.duration_ms = (unsigned)(elapsed * 1000.0);

    // Method
    p = http_get_env(httpc, "REQUEST_METHOD");
    if (p) {
        len = strlen(p);
        if (len > sizeof(rec.method)) len = sizeof(rec.method);
        memcpy(rec.method, p, len);
    }

    // URI
    p = http_get_env(httpc, "REQUEST_PATH");
    if (p) {
        len = strlen(p);
        if (len > sizeof(rec.uri)) len = sizeof(rec.uri);
        memcpy(rec.uri, p, len);
    }

    smf_write(&rec);
}
