/* HTTPPC.C
** Process client
*/
#include "httpd.h"
#include "httpdmsg.h"

/* Basic-auth realm advertised in the WWW-Authenticate challenge (fixed for now;
   a per-route / Parmlib realm is a later step of the auth redesign). */
#define HTTP_AUTH_REALM "MVS"

static int  needs_login(HTTPC *httpc, HTTPCGI *cgi);
static void resolve_credential(HTTPC *httpc);
static void auth_gate(HTTPC *httpc, HTTPCGI *route);
static int  auth_required_response(HTTPC *httpc, UCHAR mode);
static int  forbidden_response(HTTPC *httpc);
static int  is_asset_exempt(const char *path);

/* http_process_client() */
int
httppc(HTTPC *httpc)
{
	HTTPD 	*httpd 		= httpc->httpd;
    int     rc  		= 0;
    char    *path;
    char	*debug;
    HTTPCGI	*route;

#if 0
    http_enter("http_process_client()\n");
#endif
    if (!httpc) goto quit;

    /* check for busy client */
    if (http_is_busy(httpc)) {
#if HTTPD_DEBUG_217
        /* This exit returns without advancing httpc->state, and serve_client()
           has no wait -- so whoever reaches it is about to spin.  Name the
           client and its state so the wedge reports itself (#159). */
        wtof(MSG_DBG_BUSY_EXIT,
            httpc, (int)httpc->state, httpc->socket, httpc->request_count);
#endif
        goto quit;
    }

    /* mark this client as busy */
    if (http_set_busy(httpc)) {
        wtof(MSG_SET_BUSY_FAILED);
        goto quit;
    }

    if (httpc->state==CSTATE_IN) {
        http_in(httpc);
    }
    if (httpc->state==CSTATE_PARSE) {
        http_parse(httpc);
    }

    /* the state should be GET, HEAD, PUT, or POST at this point */
    switch(httpc->state) {
    case CSTATE_GET:    /* process GET request          */
    case CSTATE_HEAD:   /* process HEAD request         */
    case CSTATE_PUT:    /* process PUT request          */
    case CSTATE_POST:   /* process POST request         */
    case CSTATE_DELETE: /* process DELETE request       */
        path = http_get_env(httpc, "REQUEST_PATH");
#if 0
		wtof("httpc.c: path=\"%s\"", path ? path : "(null)");
#endif
        /* resolve the client credential for this request into httpc->cred:
           Sec-Token / LtpaToken2 cookie, Bearer, or Basic (see
           resolve_credential) */
        resolve_credential(httpc);

        /* find the route (MOD= CGI or LOC= static prefix) owning this path */
        route = path ? http_find_cgi(httpd, path) : NULL;

        /* /login and /logout are the auth UI endpoints: always dispatch them,
           in every mode, so an AUTH=FORM challenge can never lock itself out */
        if (path && (http_cmp(path, "/login")==0 ||
                     http_cmp(path, "/logout")==0)) {
            rc = httpcred(httpc);
            goto check_done;
        }

        /* 2-stage gate (authenticate -> 401, authorize -> 403), applied
           uniformly to CGI and static routes before anything is served.
           Login-page assets (/login.*, /favicon.*) stay reachable on a read
           request so an AUTH=FORM challenge can load its own page. */
        {
            int exempt = is_asset_exempt(path) &&
                         (httpc->state == CSTATE_GET ||
                          httpc->state == CSTATE_HEAD);
            if (!exempt) {
                auth_gate(httpc, route);
                if (httpc->state == CSTATE_DONE) goto check_done;
            }
        }

        /* CGI dispatch -- only routes that carry a program (pgm != NULL).
           A LOC= route has pgm == NULL and falls through to static serving. */
        if (route && route->pgm) {
            /* extension-based CGI (pattern starts with "*."):
               set SCRIPT_FILENAME = docroot + request path */
            if (route->path[0] == '*' && route->path[1] == '.') {
                char scriptfile[384];
                snprintf(scriptfile, sizeof(scriptfile), "%s%s",
                         httpd->docroot, path);
                /* every other http_set_env() caller checks; dispatching the
                   CGI without SCRIPT_FILENAME would run it against no file at
                   all, so fail the request instead of guessing (#162) */
                if (http_set_env(httpc, "SCRIPT_FILENAME", scriptfile)) {
                    http_resp_internal_error(httpc);
                    httpc->state = CSTATE_DONE;
                    goto check_done;
                }
            }

            /* path needs to be processed by external program */
            rc = http_process_cgi(httpc, route);
            goto check_done;
        }

        /* not a CGI: serve statically (route was NULL or a LOC policy route,
           already gated above) */
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

/*
** token_to_cred() - decode a base64 session-token string into a CREDTOK and
** resolve the session (NULL on a miss).  Shared by the Sec-Token / LtpaToken2
** cookies and the Bearer header; cred_find_by_token() refreshes cred->last (M2).
*/
static CRED *
token_to_cred(const char *b64)
{
	CREDTOK	tok;
	size_t	len;
	char	*buf;

	if (!b64 || !*b64) return NULL;

	buf = base64_decode(b64, strlen(b64), &len);
	if (!buf) return NULL;

	memset(&tok, 0, sizeof(tok));
	if (len > sizeof(tok)) len = sizeof(tok);
	memcpy(&tok, buf, len);
	free(buf);

	return cred_find_by_token(&tok);
}

/*
** resolve_credential() - set httpc->cred from the request, trying each
** credential source in turn (cred_find_by_token()/cred_login() refresh
** cred->last, M2, so activity keeps the session alive):
**   1. Sec-Token cookie                       (form / POST login)
**   2. LtpaToken2 cookie                       (z/OSMF token; mvsMF's authenticate
**                                               endpoint hands back our token)
**   3. Authorization: Bearer <token>           (optional; the z/OSMF standard is
**                                               the LtpaToken2 cookie above)
**   4. Authorization: Basic <b64(user:pass)>   (find-or-create; a token hit
**      reuses the cached CRED+ACEE, so no per-request racf_login)
*/
static void
resolve_credential(HTTPC *httpc)
{
	char	*authhdr;

	httpc->cred = NULL;

	/* 1. Sec-Token cookie */
	httpc->cred = token_to_cred(http_get_env(httpc, "HTTP_Cookie-Sec-Token"));
	if (httpc->cred) return;

	/* 2. LtpaToken2 cookie (our token under the z/OSMF cookie name) */
	httpc->cred = token_to_cred(http_get_env(httpc, "HTTP_Cookie-LtpaToken2"));
	if (httpc->cred) return;

	authhdr = http_get_env(httpc, "HTTP_Authorization");
	if (!authhdr) return;

	/* 3. Authorization: Bearer <token> */
	if (http_cmpn(authhdr, "Bearer ", 7) == 0) {
		httpc->cred = token_to_cred(authhdr + 7);
		return;
	}

	/* 4. HTTP Basic auth: Authorization: Basic <base64(user:pass)> */
	if (http_cmpn(authhdr, "Basic ", 6) == 0) {
		size_t	dlen = 0;
		char	*dec = base64_decode(authhdr + 6, strlen(authhdr + 6), &dlen);

		if (dec) {
			char	creds[256];
			char	*colon;
			size_t	n = dlen;

			/* base64_decode() does not NUL-terminate; copy bounded + terminate.
			   The decoded user:pass is ASCII on the wire -> translate to EBCDIC. */
			if (n >= sizeof(creds)) n = sizeof(creds) - 1;
			memcpy(creds, dec, n);
			creds[n] = 0;
			free(dec);
			http_atoe(creds, n);

			colon = strchr(creds, ':');
			if (colon) {
				*colon = 0;
				httpc->cred = cred_login(httpc->addr,
				                         (UCHAR *)creds, (UCHAR *)(colon + 1));
			}
			memset(creds, 0, sizeof(creds));   /* scrub the password */
		}
	}
}

/*
** is_asset_exempt() - login-page assets that must load even when a wildcard
** route would otherwise gate them, so an AUTH=FORM challenge can render its
** page.  (/login and /logout themselves are dispatched before the gate.)
*/
static int
is_asset_exempt(const char *path)
{
	if (!path) return 0;
	if (__patmat(path, "/login.*"))   return 1;
	if (__patmat(path, "/favicon.*")) return 1;
	return 0;
}

/*
** auth_gate() - per-route 2-stage authorization gate, run in the pipeline
** before a file is served or a CGI is dispatched (uniform for both):
**   Stage 1 (authenticate): does the route require an identity?  If so and the
**     client is not authenticated -> 401 (challenge per the route's AUTH mode).
**   Stage 2 (authorize): if the route sets RES=class:resource, RACHECK it under
**     the client's ACEE (http_check_auth) -> 403 on deny.
** On 401/403 the response is emitted and httpc->state is set to CSTATE_DONE;
** the caller detects that and stops.  route == NULL (a plain static path)
** falls back to the legacy global LOGIN default.
*/
static void
auth_gate(HTTPC *httpc, HTTPCGI *route)
{
	UCHAR	mode   = route ? route->auth : HTTP_AUTH_DEFAULT;
	int		authed = (httpc->cred && httpc->cred->id.addr == httpc->addr);
	int		need_authn;

	/* Stage 1: does this route require authentication? */
	if (mode == HTTP_AUTH_NONE) {
		need_authn = 0;
	}
	else if (mode == HTTP_AUTH_FORM || mode == HTTP_AUTH_BASIC) {
		need_authn = 1;
	}
	else {
		/* HTTP_AUTH_DEFAULT: inherit the legacy global LOGIN policy.  A LOC=
		   route (program-less) is treated like a plain static path (NULL). */
		need_authn = needs_login(httpc, (route && route->pgm) ? route : NULL);
	}

	/* a RACF resource gate (RES=) always needs an identity to check against */
	if (route && route->resclass && mode != HTTP_AUTH_NONE) {
		need_authn = 1;
	}

	if (need_authn && !authed) {
		auth_required_response(httpc, mode);
		return;
	}

	/* Stage 2: authorize the resource under the client's ACEE.  Skipped for
	   AUTH=NONE (a public route has no identity to check -> NONE wins).

	   != 0 is the whole denial test: http_check_auth() already normalizes the
	   second SAF "allowed" code (rc 4, resource not protected) to 0, and it
	   answers -1 for an unauthenticated request.  Do NOT relax this to <= 4 --
	   that reads -1 as allowed.  See httpxauth.c for why the normalization
	   lives there and not here. */
	if (route && route->resclass && route->resname && authed &&
	    mode != HTTP_AUTH_NONE) {
		if (http_check_auth(httpc, route->resclass, route->resname,
		                    route->resattr) != 0) {
			forbidden_response(httpc);
		}
	}
}

/*
** auth_required_response() - the request needs a login but isn't authenticated.
** The challenge follows the route's AUTH mode:
**   BASIC   -> 401 + WWW-Authenticate: Basic (suppressed for XHR clients that
**              send the z/OSMF CSRF marker -- see the emit below)
**   FORM    -> the interactive HTML login form
**   DEFAULT -> legacy heuristic: a client that sent an Authorization header is
**              a REST/Basic client (401); a browser gets the form.
** Always ends with httpc->state == CSTATE_DONE so the pipeline stops.
*/
static int
auth_required_response(HTTPC *httpc, UCHAR mode)
{
	int	use_basic;

	if (mode == HTTP_AUTH_FORM) {
		return httpcred(httpc);              /* renders the form, sets DONE */
	}
	else if (mode == HTTP_AUTH_BASIC) {
		use_basic = 1;
	}
	else {
		/* DEFAULT: REST client (sent Authorization) -> 401, else the form */
		use_basic = (http_get_env(httpc, "HTTP_Authorization") != NULL);
	}

	if (!use_basic) {
		return httpcred(httpc);              /* interactive client -> form */
	}

	http_resp(httpc, 401);
	/* Omit the Basic challenge for XHR/API clients that identify via the
	   z/OSMF CSRF marker.  A WWW-Authenticate makes the browser pop its native
	   credential dialog, and the Basic credentials it then caches outlive the
	   token session -- defeating token logout (#119).  A challenge-less 401
	   lets the SPA's own "session expired" handling see the 401.  Genuine
	   Basic clients (curl -u, Zowe) send credentials preemptively and do not
	   need the challenge; a plain browser navigation (no marker) still gets it. */
	if (http_get_env(httpc, "HTTP_X-CSRF-ZOSMF-HEADER") == NULL) {
		http_printf(httpc, "WWW-Authenticate: Basic realm=\"%s\"\r\n",
		            HTTP_AUTH_REALM);
	}
	http_printf(httpc, "Cache-Control: no-store\r\n");
	http_printf(httpc, "Content-Type: text/plain\r\n");
	http_printf(httpc, "\r\n");
	http_printf(httpc, "401 Unauthorized\n");
	httpc->state = CSTATE_DONE;
	return 0;
}

/*
** forbidden_response() - the client is authenticated but the route's RES=
** resource check denied access.  403 + short body; ends at CSTATE_DONE.
*/
static int
forbidden_response(HTTPC *httpc)
{
	http_resp(httpc, 403);
	http_printf(httpc, "Cache-Control: no-store\r\n");
	http_printf(httpc, "Content-Type: text/plain\r\n");
	http_printf(httpc, "\r\n");
	http_printf(httpc, "403 Forbidden\n");
	httpc->state = CSTATE_DONE;
	return 0;
}
