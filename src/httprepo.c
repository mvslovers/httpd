/* HTTPREPO.C
** Report results of request — update counters and write SMF records
** based on configured SMF level.
** Transitions to next client state.
*/
#include "httpd.h"

static void httpsmf_write_request(HTTPD *httpd, HTTPC *httpc);

extern int
httprepo(HTTPC *httpc)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;

    http_enter("httprepo()\n");

    // Counters always increment regardless of SMF level
    httpd->total_requests++;
    if (httpc->resp >= 400)
        httpd->total_errors++;
    httpd->total_bytes_sent += httpc->sent;

    // SMF level check
    if (httpd->smf_level == SMF_LEVEL_NONE)
        goto done;

    if (httpd->smf_level == SMF_LEVEL_ERROR && httpc->resp < 400)
        goto done;

    if (httpd->smf_level == SMF_LEVEL_AUTH) {
        // Write on auth events (401/403) and errors (>= 400)
        if (httpc->resp != 401 && httpc->resp != 403 && httpc->resp < 400)
            goto done;
    }

    // SMF_LEVEL_ALL: always write
    httpsmf_write_request(httpd, httpc);

done:
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
httpsmf_write_request(HTTPD *httpd, HTTPC *httpc)
{
    SMF_HTTPD_REQ rec;
    const char *p;
    unsigned len;
    double elapsed;

    if (!smf_active())
        return;

    memset(&rec, 0, sizeof(rec));
    smf_init(&rec, sizeof(rec), httpd->smf_type);

    memcpy(rec.subsys, "HTTPD   ", 8);
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

/* Write SMF session record (subtype 2) on connection close.
** Called from httpclos.c only when smf_level == SMF_LEVEL_ALL. */
__asm__("\n&FUNC    SETC 'HTTPSMFS'");
void
httpsmf_session(HTTPD *httpd, HTTPC *httpc)
{
    SMF_HTTPD_SESS rec;
    double elapsed;

    if (!smf_active())
        return;

    memset(&rec, 0, sizeof(rec));
    smf_init(&rec, sizeof(rec), httpd->smf_type);

    memcpy(rec.subsys, "HTTPD   ", 8);
    rec.subtype = SMF_HTTPD_SUBTYPE_SESS;

    // Last userid from credential
    if (httpc->cred) {
        CREDID id;
        credid_dec(&httpc->cred->id, &id);
        memcpy(rec.userid, id.userid, 8);
    }
    else {
        memset(rec.userid, ' ', 8);
    }

    rec.client_addr   = httpc->addr;
    rec.request_count = httpc->request_count;
    rec.total_bytes   = httpc->sent;

    // Session duration in milliseconds
    elapsed = httpc->end - httpc->start;
    if (elapsed < 0.0) elapsed = 0.0;
    rec.duration_ms = (unsigned)(elapsed * 1000.0);

    smf_write(&rec);
}
