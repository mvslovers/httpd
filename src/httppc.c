/* HTTPPC.C
** Process client
*/
#include "httpd.h"

static int needs_login(HTTPC *httpc, HTTPCGI *cgi);

/* http_process_client() */
int
httppc(HTTPC *httpc)
{
	HTTPD 	*httpd 		= httpc->httpd;
	CRED	*cred;
    int     rc  		= 0;
    int		needslogin;
    char    *path;
    char	*debug;

#if 0
    http_enter("http_process_client()\n");
#endif
    if (!httpc) goto quit;

    /* check for busy client */
    if (http_is_busy(httpc)) goto quit;

    /* mark this client as busy */
    if (http_set_busy(httpc)) {
        wtof("http_set_busy() failed");
        goto quit;
    }

    if (httpc->state==CSTATE_IN) {
        http_in(httpc);
    }
    if (httpc->state==CSTATE_PARSE) {
        http_parse(httpc);
		cred = httpc->cred;
    }

    /* the state should be GET, HEAD, PUT, or POST at this point */
    switch(httpc->state) {
    case CSTATE_GET:    /* process GET request          */
    case CSTATE_HEAD:   /* process HEAD request         */
    case CSTATE_PUT:    /* process PUT request          */
    case CSTATE_POST:   /* process POST request         */
    case CSTATE_DELETE: /* process DELETE request       */
        /* check this path name for CGI processing */
        path = http_get_env(httpc, "REQUEST_PATH");
#if 0
		wtof("httpc.c: path=\"%s\"", path ? path : "(null)");
#endif
        if (path) {
            HTTPCGI *cgi = http_find_cgi(httpd, path);
			char 	*cookie = http_get_env(httpc, "HTTP_Cookie-Sec-Token");
#if 0
			wtof("httpc.c: Sec-Token=\"%s\"", cookie ? cookie : "(null)");
#endif
			if (cookie) {
				CREDTOK tok;
				size_t len = strlen(cookie);
				char *buf;

				httpc->cred = cred = NULL;
				
				buf = base64_decode(cookie, strlen(cookie), &len);
				if (buf) {
					if (len > sizeof(CREDTOK)) len = sizeof(CREDTOK);
					memcpy(&tok, buf, len);
					free(buf);
					httpc->cred = cred = cred_find_by_token(&tok);
				}
			}

            if (cgi) {
				if (needs_login(httpc, cgi)) {
					/* request user login */
					rc = httpcred(httpc);
					goto check_done;
				}

                /* path needs to be processed by external program */
                rc = http_process_cgi(httpc, cgi);
                goto check_done;
            }
            
            /* process /login or /logout request */
            if (http_cmp(path, "/login")==0 || http_cmp(path, "/logout")==0) {
				rc = httpcred(httpc);
				goto check_done;
			}
        }

        /* not a CGI request */
        needslogin = needs_login(httpc, NULL);
		if (needslogin) {
			rc = httpcred(httpc);
			goto check_done;
		}
        
        if (httpc->state == CSTATE_GET) {
            /* process GET request          */
            http_get(httpc);
        }
        else if (httpc->state == CSTATE_HEAD) {
            /* process HEAD request         */
            http_head(httpc);
        }
        else if (httpc->state == CSTATE_PUT) {
            /* process PUT request          */
            http_put(httpc);
        }
        else if (httpc->state == CSTATE_POST) {
            /* process POST request         */
            http_put(httpc);
        }
        else if (httpc->state == CSTATE_DELETE) {
            /* process DELETE request       */
            http_delete(httpc);
        }
        break;
    }

check_done:
    if (httpc->state==CSTATE_DONE) {
		debug = http_get_env(httpc, "QUERY_DEBUG");
		if (debug) {
			http_debug(httpc, debug);
		}
        http_done(httpc);
    }
    if (httpc->state==CSTATE_REPORT) {
        http_report(httpc);
    }
    if (httpc->state==CSTATE_RESET) {
        http_reset(httpc);
    }

    /* remove client from busy array */
    if (http_reset_busy(httpc)) {
        http_dbgf("Error removing client from busy array\n");
    }

quit:
#if 0
    http_exit("http_process_client()\n");
#endif
    return rc;
}

static int
needs_login(HTTPC *httpc, HTTPCGI *cgi)
{
	int		needlogin 	= 0;
	HTTPD	*httpd 		= httpc->httpd;
	CRED	*cred 		= httpc->cred;
	UCHAR	*path;

    if (cgi) {
		if (httpd->login & HTTPD_LOGIN_CGI) {
			/* check for login required */
			if (cgi->login) {
				if (!cred) {
					/* no credential */
					needlogin++;
				}
				else if (cred->id.addr != httpc->addr) {
					/* credential is for a different client IP address */
					needlogin++;
				}
			}
		}
    }

	/* if we're already logged in then we're done */
	if (cred && cred->id.addr == httpc->addr) goto quit;

	path = http_get_env(httpc, "REQUEST_PATH");

	switch(httpc->state) {
        case CSTATE_GET:
            /* process GET request          */
            if (http_cmp(path, "/login")==0 || http_cmp(path, "/logout")==0) {
				/* These resources never need a login */
				break;
			}

            if (__patmat(path, "/login.*") || __patmat(path, "/favicon.*")) {
				/* These resources never need a login */
				break;
			}
			
			if (!cred && httpd->login & HTTPD_LOGIN_GET) {
				needlogin++;
			}
			break;

        case CSTATE_HEAD:
            /* process HEAD request         */
			if (!cred && httpd->login & HTTPD_LOGIN_HEAD) {
				needlogin++;
			}
			break;

        case CSTATE_PUT:
            /* process PUT request          */
			if (!cred && httpd->login & HTTPD_LOGIN_POST) {
				needlogin++;
			}
			break;

        case CSTATE_POST:
            /* process POST request         */
			if (!cred && httpd->login & HTTPD_LOGIN_POST) {
				needlogin++;
			}
			break;
    }

quit:
	return needlogin;
}
