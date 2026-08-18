/* HTTPIN.C
** Get request headers from client.
** Transitions to next state as needed.
*/
#define HTTP_PRIVATE
#include "httpd.h"

/* RFC 7231 5.1.1: a client that sends "Expect: 100-continue" holds its
** request body back until the server says it will read one.  Nothing here
** answered that expectation, so such a client waited out its own timeout
** while the body reader -- the CGI's, or the POST reader in httppars() --
** called recv() on a socket that was empty *because* the client was waiting.
** Every accepted socket is non-blocking (FIONBIO in httpd.c), so that recv()
** returned EWOULDBLOCK at once and the request failed in under a tenth of a
** second, with no byte of the body ever sent (mvsmf#247).
**
** Answered here, at the end of header parsing, because this is the one point
** that precedes every body read in the server: httppars() consumes a POST
** body under 4000 bytes itself, and a CGI reads a PUT body straight off the
** socket.  A single call before both cannot be forgotten by a route type
** added later.  It does mean the expectation is answered before the auth
** gate runs, so a client whose request is about to be refused still uploads
** its body -- exactly what it does today, having waited for its timeout
** first, so nothing gets worse.
**
** Sent as ONE http_printf().  httpresp() would be wrong twice over: it looks
** the phrase up in httpstat(), which carries none for 100 and would put
** "500 Internal Server Error" on the wire; and it sets httpc->resp, which is
** what httpprtv() tests when it decides whether a bare "\r\n" ends the final
** response's headers.  Emitting the interim response as a single 25-byte
** write touches neither.
*/
static void
expect_continue(HTTPC *httpc)
{
    UCHAR *p;

    /* HTTP/1.0 has no Expect (RFC 7231 5.1.1) -- ignore it there */
    p = http_get_env(httpc, "REQUEST_VERSION");
    if (!p || http_cmp(p, "HTTP/1.1") != 0) return;

    /* the only expectation defined; anything else is not ours to satisfy */
    p = http_get_env(httpc, "HTTP_EXPECT");
    if (!p || http_cmp(p, "100-continue") != 0) return;

    /* no body announced -> nothing is being held back */
    if (!http_get_env(httpc, "HTTP_CONTENT-LENGTH") &&
        !http_get_env(httpc, "HTTP_TRANSFER-ENCODING")) return;

    http_printf(httpc, "HTTP/1.1 100 Continue\r\n\r\n");
}

int http_in(HTTPC *httpc)
{
    int         rc      = 0;
    UCHAR       *buf    = httpc->buf;
    unsigned    len     = CBUFSIZE-1;
    UCHAR       *p;
    int         host_count = 0;

    // wtof("%s: Enter", __func__);
    
    /* read the request string "method uri version" */
    rc = http_gets(httpc, buf, len);
    if (rc < 0) {
        if (errno == EINVAL) goto badrequest;
        goto failed;
    }
    if (rc == 0) {
        /* request line exceeds buffer — URI too long */
        http_resp(httpc, 414);
        http_printf(httpc, "Content-Type: text/plain\r\n\r\n");
        http_printf(httpc, "414 URI Too Long\r\n");
        httpc->state = CSTATE_DONE;
        goto quit;
    }

    /* "GET path/to/resource HTTP/1.0\n" */

    /* remove newline character from request */
    p = strchr(buf, '\n');
    if (p) *p = 0;

    // wtof("%s: \"%s\"", __func__, buf);

    /* save the original request string */
    if (http_set_env(httpc, "HTTP_REQUEST", buf)) goto failed;

    /* parse request "method uri version" */
    p = strtok(buf, " ");
    if (!p) goto failed;
    if (http_set_env(httpc, "REQUEST_METHOD", p)) goto failed;

    p = strtok(NULL, " ");
    if (!p) goto failed;
    if (http_set_env(httpc, "REQUEST_URI", p)) goto failed;

    p = strtok(NULL, " ");
    if (!p) goto badrequest; /* missing HTTP version */
    if (http_set_env(httpc, "REQUEST_VERSION", p)) goto failed;

    /* validate HTTP version — RFC 7230 §2.6 */
    if (http_cmp(p, "HTTP/1.0") != 0 && http_cmp(p, "HTTP/1.1") != 0) {
        http_resp(httpc, 505);
        http_printf(httpc, "Content-Type: text/plain\r\n\r\n");
        http_printf(httpc, "505 HTTP Version Not Supported\r\n");
        httpc->state = CSTATE_DONE;
        goto quit;
    }

    /* create "HTTP_..." environment variables from HTTP headers */
    do {
        /* get HTTP header string */
        rc = http_gets(httpc, buf, len);
        if (rc < 0) {
            if (errno == EINVAL) goto badrequest;
            goto failed;
        }
        if (rc == 0) goto badrequest; /* header line too long */

        /* check for end of HTTP headers */
        if (buf[0]=='\n') break;

        /* remove newline character */
        p = strrchr(buf, '\n');
        if (p) *p = 0;

        /* find keyword delimiter */
        p = strchr(buf, ':');
        if (!p) continue;

        /* replace delimiter with 0 byte */
        *p++ = 0;

        /* validate header name — RFC 7230 §3.2.6 token chars only
           allowlist approach avoids EBCDIC code page mismatches */
        {
            UCHAR *s;
            for (s = buf; *s; s++) {
                if (isalnum(*s)) continue;
                if (*s == '!' || *s == '#' || *s == '$' || *s == '%' ||
                    *s == '&' || *s == '\'' || *s == '*' || *s == '+' ||
                    *s == '-' || *s == '.' || *s == '^' || *s == '_' ||
                    *s == '`' || *s == '|' || *s == '~')
                    continue;
                goto badrequest;
            }
        }

        /* skip any spaces */
        while(*p==' ') p++;

        /* validate header value — no control chars (RFC 7230 §3.2.6) */
        {
            UCHAR *s;
            for (s = p; *s; s++) {
                if (*s < ' ' && *s != '\t')
                    goto badrequest;
            }
        }

        /* track Host header for duplicate detection */
        if (http_cmp(buf, "Host") == 0)
            host_count++;

        /* create "HTTP_..." environment variable */
        if (http_set_http_env(httpc, buf, p)) goto failed;
    } while(rc > 0);

    /* reject multiple Host headers — RFC 7230 §5.4 */
    if (host_count > 1) goto badrequest;

    /* validate Content-Length — RFC 7230 §3.3.2 (digits only) */
    {
        UCHAR *cl = http_get_env(httpc, "HTTP_CONTENT-LENGTH");
        if (cl) {
            UCHAR *s;
            if (!*cl) goto badrequest;
            for (s = cl; *s; s++) {
                if (*s < '0' || *s > '9')
                    goto badrequest;
            }
        }
    }

    /* HTTP/1.1 requires a Host header */
    {
        UCHAR *ver = http_get_env(httpc, "REQUEST_VERSION");
        UCHAR *conn = http_get_env(httpc, "HTTP_CONNECTION");

        if (ver && http_cmp(ver, "HTTP/1.1") == 0) {
            UCHAR *host = http_get_env(httpc, "HTTP_HOST");
            if (!host || !host[0]) {
                http_resp(httpc, 400);
                http_printf(httpc, "Content-Type: text/plain\r\n\r\n");
                http_printf(httpc, "400 Bad Request: Missing Host header\r\n");
                httpc->state = CSTATE_DONE;
                goto quit;
            }
            /* HTTP/1.1: default keep-alive */
            httpc->keepalive = 1;
            if (conn && http_cmp(conn, "close") == 0)
                httpc->keepalive = 0;
        } else {
            /* HTTP/1.0: default close */
            httpc->keepalive = 0;
            if (conn && http_cmp(conn, "keep-alive") == 0)
                httpc->keepalive = 1;
        }

        /* enforce max requests per connection */
        if (httpc->request_count >= httpc->httpd->cfg_keepalive_max)
            httpc->keepalive = 0;
    }

    /* release a client that is waiting for permission to send its body */
    expect_continue(httpc);

    /* next step will parse and do any additional processing */
    rc = 0;
    httpc->state = CSTATE_PARSE;
    goto quit;

badrequest:
    http_resp(httpc, 400);
    http_printf(httpc, "Content-Type: text/plain\r\n\r\n");
    http_printf(httpc, "400 Bad Request\r\n");
    httpc->state = CSTATE_DONE;
    goto quit;

failed:
    if (httpc->request_count > 0) {
        /* keep-alive: idle timeout or client disconnect — just close */
        httpc->state = CSTATE_CLOSE;
    } else {
        /* first request: bad request, reset the connection */
        httpc->state = CSTATE_RESET;
    }

quit:
    // wtof("%s: exit rc=%d", __func__, rc);
    return rc;
}

#if 0   /* old code */

/* http_in() - client state CSTATE_IN
** Read socket data into our clients buffer until we encounter
** a header sperator sequence, LF+LF or CRLF+CRLF.
** Once we detect the header seperator sequence we transtion to the
** next client state (CSTATE_PARSE).
*/
extern int
http_in(HTTPC *httpc)
{
    int     rc      = 0;
    HTTPD 	*httpd	= httpc->httpd;
    UCHAR   *buf    = &httpc->buf[httpc->len];  /* put data here */
    int     len     = CBUFSIZE-httpc->len;      /* space available in buffer */
    double  timeout;
    double  now;
#if 0
    http_enter("httpin(), len=%d\n", len);
#endif

	// wtof("%s: httpd=%p", __func__, httpd);
	// wtof("%s: httpd->cfg_client_timeout=%u", __func__, httpd->cfg_client_timeout);
	// wtof("%s: httpd->client=%02X", __func__, httpd->client);
	if (!httpd->cfg_client_timeout) {
		timeout = 10.0;	// default value
	}
	else {
		timeout = (httpd->cfg_client_timeout * 1.0);
	}
 	if (timeout < 1.0) timeout = 1.0;	// let's keep is reasonable
 	// wtof("%s: timeout=%g", __func__, timeout);

    len--;  /* leave room for 0 byte */
    if (len <= 0) {
        /* no room in buffer */
        httpc->state = CSTATE_CLOSE;
        goto quit;
    }

    /* try to read some data from client */
#if 0
    http_dbgf("calling recv(%d,%08X,%d,0)\n", httpc->socket, buf, len);
#endif
    rc = recv(httpc->socket, buf, len, 0);
#if 0
    http_dbgf("recv() rc=%d\n", rc);
#endif
    if (rc<0) {
        /* client closed connection */
        rc = 0;
        /* check for EWOULDBLOCK */
        if (errno == EWOULDBLOCK) goto check;
        httpc->state = CSTATE_CLOSE;
        goto quit;
    }

    httpc->len += rc;   /* update length of data in buffer */

    if (strstr(httpc->buf, "\x0A\x0A") ||           /* ASCII LF LF */
        strstr(httpc->buf, "\x0D\x0A\x0D\x0A")) {   /* ASCII CRLF CRLF */
        /* we have a complete request in ASCII */
        httpc->state = CSTATE_PARSE;
        goto quit;
    }

check:
    /* check for inactive/broken client */
    httpsecs(&now);
    if ((now - httpc->start) > timeout) {	/* if more than timeout seconds 		*/
        char *fmt = "client %08X on socket %d timed out after %g seconds\n";

		if (httpc->len > 0) {
			if (httpd->client & HTTPD_CLIENT_INMSG) {
				wtof(fmt, httpc, httpc->socket, timeout);
			}
			if (httpd->client & HTTPD_CLIENT_INDUMP) {
				wtodumpf(httpc->buf, httpc->len, "Receive Buffer");
			}
        
			http_dbgf(fmt, httpc, httpc->socket, timeout);
		}
        httpc->state = CSTATE_CLOSE;
    }

quit:
#if 0
    http_exit("httpin(), rc=%d\n", rc);
#endif
    return rc;
}

#endif  /* old code */
