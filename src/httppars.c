/* HTTPPARS.C
** Parse received headers from client.
** Transitions to next state as needed.
*/
#include "httpd.h"

static int http_set_post_env(HTTPC *httpc, const UCHAR *name, const UCHAR *value);

/* http_parse() - client state CSTATE_PARSE
** Parse header in the client buffer and create variables from
** the values we encounter.
** Transition to the next state depending on the HTTP method
** we find in the request headers (GET, HEAD, POST, ...).
*/
extern int
httppars(HTTPC *httpc)
{
    int     rc      = 0;
    UCHAR   *buf    = httpc->buf;
    int     len     = CBUFSIZE-1;
    int     pos     = 0;
    UCHAR   *data   = NULL;
    int     datalen = 0;
    int     i;
    UCHAR   *p;
    int     salen;
    struct sockaddr sa;
    UCHAR   tmp[80];

    http_enter("httppars()\n");

    /* parse the REQUEST_URI */
    p = http_get_env(httpc, "REQUEST_URI");
    if (!p) goto failed;

    // wtof("%s: REQUEST_URI=\"%s\"", __func__, p);

    /* prepare for query variables */
    strcpyp(buf, CBUFSIZE, p, 0);
    // wtof("%s: buf=\"%s\"", __func__, buf);

    p = strchr(buf, '?');   /* any query variables? */
    if (p) {
        /* we have query variables */
        /* create "QUERY_STRING" environment variable */
        if (http_set_env(httpc, "QUERY_STRING", p+1)) goto failed;

        /* parse the query string */
        if (strlen(buf) < CBUFSIZE - 2)
            strcat(buf, "&");   /* append a final "&" to buffer */
        *p++ = 0;           /* "?" -> 0 byte */
        buf = p;            /* start of query variables */
        for (p=strchr(buf,'&'); p; buf=&buf[pos], p=strchr(buf,'&')) {
            *p++ = 0;
            pos = (int)(p - buf);
            if (!buf[0]) continue;  /* "&&" or trailing "&" */

            p = strchr(buf,'=');
            if (!p) {
                if (http_set_query_env(httpc, buf, "")) goto failed;
            }
            else {
                *p++=0;     /* "=" -> 0 byte */
				p = http_decode(p);
                if (http_set_query_env(httpc, buf, p)) goto failed;
            }
        }
    }

    /* decode the uri and create REQUEST_PATH */
    buf = http_decode(httpc->buf);
    
    // wtof("%s: REQUEST_PATH=\"%s\"", __func__, buf);
    
    if (http_set_env(httpc, "REQUEST_PATH", buf)) goto failed;

    /* create server variables for this client */
    salen = sizeof(sa);
    memset(&sa, 0, sizeof(sa));
    if (getsockname(httpc->socket, &sa, &salen)==0) {

        p = sa.sa_data;
        sprintf(tmp, "%d.%d.%d.%d", p[2], p[3], p[4], p[5]);
        if (http_set_env(httpc, "SERVER_ADDR", tmp)) goto failed;

        sprintf(tmp, "%d", ((p[0] << 8) + p[1]));
        if (http_set_env(httpc, "SERVER_PORT", tmp)) goto failed;
    }

    if (http_set_env(httpc, "SERVER_PROTOCOL", "HTTP/1.1")) goto failed;
    
    snprintf(tmp, sizeof(tmp), "HTTPD/%s", httpc->httpd->version);
    if (http_set_env(httpc, "SERVER_SOFTWARE", tmp)) goto failed;

    /* select next state based on request method */
    p = http_get_env(httpc, "REQUEST_METHOD");
    if (!p) goto failed;

    if (http_cmp(p, "DELETE")==0) {
        httpc->state = CSTATE_DELETE;
        goto nodata;
    }
    if (http_cmp(p, "GET")==0) {
        httpc->state = CSTATE_GET;
        goto nodata;
    }
    if (http_cmp(p, "HEAD")==0) {
        httpc->state = CSTATE_HEAD;
        goto nodata;
    }
    if (http_cmp(p, "PUT")==0) {
        httpc->state = CSTATE_PUT;
        /* Technically there is more data but we don't process that 
         * data in the HTTPD server. Instead we allow CGI programs
         * that allow the PUT method to processes the remaining
         * data still unread from the socket.
         */
        goto nodata;
    }
    if (http_cmp(p, "POST")==0) {
        httpc->state = CSTATE_POST;
        goto postdata;
    }

    /* method not allowed — RFC 7231 §6.5.5 */
    httpc->keepalive = 0; /* unread body would poison next request */
    http_resp(httpc, 405);
    http_printf(httpc, "Cache-Control: no-store\r\n");
    http_printf(httpc, "Content-Type: text/plain\r\n");
    http_printf(httpc, "\r\n");
    http_printf(httpc, "405 Method Not Allowed\n");
    httpc->state = CSTATE_DONE;
    goto quit;

failed:
    /* most likely a bad request, reset the connection */
    httpc->state = CSTATE_RESET;
    goto quit;

postdata:
    /* classify the request body (RFC 7230 §3.3.3) before reading it */
    {
        const UCHAR *cl = http_get_env(httpc, "HTTP_CONTENT-LENGTH");
        int          te = (http_get_env(httpc, "HTTP_TRANSFER-ENCODING") != NULL);
        long         clen = 0;

        switch (httpbody(cl, te, CBUFSIZE-1, &clen)) {
        case HTTP_BODY_TOO_LARGE:
            /* body larger than we buffer: reject, leave it unread, close */
            httpc->keepalive = 0;
            http_resp(httpc, 413);
            http_printf(httpc, "Cache-Control: no-store\r\n");
            http_printf(httpc, "Content-Type: text/plain\r\n");
            http_printf(httpc, "\r\n");
            http_printf(httpc, "413 Payload Too Large\n");
            httpc->state = CSTATE_DONE;
            goto quit;
        case HTTP_BODY_BAD:
            /* malformed Content-Length */
            httpc->keepalive = 0;
            http_resp(httpc, 400);
            http_printf(httpc, "Cache-Control: no-store\r\n");
            http_printf(httpc, "Content-Type: text/plain\r\n");
            http_printf(httpc, "\r\n");
            http_printf(httpc, "400 Bad Request\n");
            httpc->state = CSTATE_DONE;
            goto quit;
        case HTTP_BODY_NONE:
            /* no body to read (no CL & no TE, CL 0, or a chunked body we do
               not decode here) -- don't block on a recv that won't complete */
            goto nodata;
        default:                    /* HTTP_BODY_READ */
            datalen = (int)clen;    /* bytes to read (1..CBUFSIZE-1) */
            break;
        }
    }

    /* what type of POST data do we have */
    p = http_get_env(httpc, "HTTP_CONTENT-TYPE");
    if (!p) goto nodata;

    // wtof("%s: HTTP_CONTENT-TYPE=\"%s\"", __func__, p);

    /* if not form data then leav it alone */
    if (http_cmp(p, "application/x-www-form-urlencoded")!=0) goto postother;

    /* retrieve the form data and decode into "POST_..." variables.  Read at
       most the declared Content-Length so we never slurp bytes belonging to a
       pipelined next request (single non-blocking recv -- see S7 scope). */
    memset(httpc->buf, 0, CBUFSIZE);
    buf = httpc->buf;
    len = datalen;
    rc = recv(httpc->socket, buf, len, 0);
    if (rc < 0) goto failed;
    
    // wtodumpf(buf, rc, "%s recv()", __func__);
    
    http_atoe(buf, rc);
    buf[rc] = 0;

    // wtof("%s: recv() rc=%d buf=\"%s\"", __func__, rc, buf);
    
    /* create "POST_STRING" environment variable */
    if (http_set_env(httpc, "POST_STRING", buf)) goto failed;

    /* parse string into "POST_..." variables */
    if (strlen(buf) < CBUFSIZE - 2)
        strcat(buf, "&");   /* append a final "&" to buffer */
    for (p=strchr(buf,'&'); p; buf=&buf[pos], p=strchr(buf,'&')) {
        *p++ = 0;
        pos = (int)(p - buf);
        if (!buf[0]) continue;  /* "&&" or trailing "&" */

        p = strchr(buf,'=');
        if (!p) {
            if (http_set_post_env(httpc, buf, "")) goto failed;
        }
        else {
            *p++=0;     /* "=" -> 0 byte */
            http_decode(p);
            if (http_set_post_env(httpc, buf, p)) goto failed;
        }
    }
    goto nodata;

postother:
    /* some other POST data to be received -- read at most Content-Length */
    memset(httpc->buf, 0, CBUFSIZE);
    buf = httpc->buf;
    len = datalen;
    rc = recv(httpc->socket, buf, len, 0);
    if (rc < 0) goto failed;
    
    // wtodumpf(buf, rc, "%s recv()", __func__);
    
    http_atoe(buf, rc);
    buf[rc] = 0;

    // wtof("%s: recv() rc=%d buf=\"%s\"", __func__, rc, buf);
    
    /* create "POST_STRING" environment variable */
    if (http_set_env(httpc, "POST_STRING", buf)) goto failed;

nodata:
    /* if the request had a body we didn't read, disable keep-alive
       to prevent body data from poisoning the next request */
    if (http_get_env(httpc, "HTTP_CONTENT-LENGTH") ||
        http_get_env(httpc, "HTTP_TRANSFER-ENCODING"))
        httpc->keepalive = 0;

    /* no more expected data from client */
    memset(httpc->buf, 0, CBUFSIZE);
    httpc->len = 0;
    httpc->pos = 0;
    rc = 0;

quit:
    httpc->subtype = 0;
    httpc->substate = 0;
    http_exit("httppars(), rc=%d\n", rc);
    return rc;
}


static int
http_set_post_env(HTTPC *httpc, const UCHAR *name, const UCHAR *value)
{
    char        newname[256];

    snprintf(newname, sizeof(newname), "POST_%s", name);

    return http_set_env(httpc, newname, value);
}
