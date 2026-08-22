/* HTTPPC.C
** Process client
*/
#include "httpd.h"
#include "httpdmsg.h"

static void resolve_credential(HTTPC *httpc);
static void auth_gate(HTTPC *httpc, HTTPROUTE *route);
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
    HTTPROUTE	*route;

#if 0
    http_enter("http_process_client()\n");
#endif
    if (!httpc) goto quit;

    /* check for busy client */
    if (http_is_busy(httpc)) {
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
        route = path ? http_find_route(httpd, path) : NULL;

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
            rc = http_process_route(httpc, route);
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

/*
** maxage_secs() - the configured hard credential max-age in seconds, 0 when
** it is switched off (#118).  Parmlib carries it in minutes.
*/
static unsigned
maxage_secs(HTTPD *httpd)
{
	return httpd ? (unsigned) httpd->cfg_session_maxage * 60 : 0;
}

/*
** cred_overage() - has this credential passed that limit?  The age is measured
** from cred->created, which -- unlike cred->last -- is never refreshed, so
** activity cannot extend a session past the maximum.  credexp() treats a 0
** limit as "never", which is what switches the check off.
*/
static int
cred_overage(HTTPD *httpd, CRED *cred)
{
	if (!cred) return 0;

	return credexp((long) difftime64(time64(NULL), cred->created),
	               maxage_secs(httpd));
}

/*
** token_to_cred() - decode a base64 session-token string into a CREDTOK and
** resolve the session (NULL on a miss).  Shared by the Sec-Token / LtpaToken2
** cookies and the Bearer header; cred_find_by_token() refreshes cred->last (M2).
**
** An over-age credential is rejected HERE and not only by the reaper: the sweep
** runs every ~60 s (httpd.c), so a reaper-only max-age would keep answering
** with an expired session for up to a minute.  Enforcement is immediate;
** reclamation is left to the sweep on purpose.  Freeing it on this path would
** open a borrow window the idle design does not have -- a second worker holding
** the same CRED for an in-flight request would be left with freed storage (the
** M2 note in docs/refactoring-backlog.md: testlock catches a concurrent free,
** not a borrow).  Leaving it linked costs one dead CRED for up to a minute and
** resolves nothing in the meantime, because every lookup re-checks the age.
*/
static CRED *
token_to_cred(HTTPC *httpc, const char *b64)
{
	CREDTOK	tok;
	CRED	*cred;
	size_t	len;
	char	*buf;

	if (!b64 || !*b64) return NULL;

	buf = base64_decode(b64, strlen(b64), &len);
	if (!buf) return NULL;

	memset(&tok, 0, sizeof(tok));
	if (len > sizeof(tok)) len = sizeof(tok);
	memcpy(&tok, buf, len);
	free(buf);

	cred = cred_find_by_token(&tok);
	if (cred && cred_overage(httpc->httpd, cred)) cred = NULL;

	return cred;
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
	httpc->cred = token_to_cred(httpc, http_get_env(httpc, "HTTP_Cookie-Sec-Token"));
	if (httpc->cred) return;

	/* 2. LtpaToken2 cookie (our token under the z/OSMF cookie name) */
	httpc->cred = token_to_cred(httpc, http_get_env(httpc, "HTTP_Cookie-LtpaToken2"));
	if (httpc->cred) return;

	authhdr = http_get_env(httpc, "HTTP_Authorization");
	if (!authhdr) return;

	/* 3. Authorization: Bearer <token> */
	if (http_cmpn(authhdr, "Bearer ", 7) == 0) {
		httpc->cred = token_to_cred(httpc, authhdr + 7);
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

				/* A Basic client carries its password on every request, so the
				   max-age cannot log it out -- what the limit buys here is
				   REVALIDATION, and cred_login() applies it: past the limit it
				   drops the cached credential and checks the password against
				   RACF again (#118). */
				httpc->cred = cred_login(httpc->addr,
				                         (UCHAR *)creds, (UCHAR *)(colon + 1),
				                         maxage_secs(httpc->httpd));
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
** the caller detects that and stops.
**
** route == NULL is a path no MOD=/LOC= line claimed, and it is public: since
** #105 a request is gated because a route says so and for no other reason.
** There is no global policy left to fall back to, and nothing to fall back
** FROM either -- an unmatched path never carried a policy to lose.  Serving a
** docroot that should not be public is a LOC= line with a wildcard path and an
** AUTH= mode, which is a route and therefore visible in /.dsrv, unlike the
** LOGIN bitmask it replaces.
*/
static void
auth_gate(HTTPC *httpc, HTTPROUTE *route)
{
	UCHAR	mode   = route ? route->auth : HTTP_AUTH_NONE;
	int		authed = (httpc->cred && httpc->cred->id.addr == httpc->addr);
	int		need_authn;

	/* Stage 1: does this route require authentication?  Four concrete modes,
	   no "unset" -- httpprm resolves a line that named no AUTH= before the
	   policy ever reaches the route (see ROUTE_POLICY.has_auth). */
	need_authn = (mode != HTTP_AUTH_NONE);

	/* a RACF resource gate (RES=) always needs an identity to check against.
	   Redundant for a route the Parmlib built -- parse_kv_tail() already gave
	   a RES=-without-AUTH= route a challenge mode -- but kept because a route
	   can also be registered through the httpx vector, where nothing does. */
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
**   TOKEN   -> bare 401, never a challenge, whatever the client sent (#121)
**   BASIC   -> 401 + WWW-Authenticate: Basic (suppressed for XHR clients that
**              send the z/OSMF CSRF marker -- see the emit below)
**   FORM    -> the interactive HTML login form
** Always ends with httpc->state == CSTATE_DONE so the pipeline stops.
**
** None of this decides which credentials are ACCEPTED -- resolve_credential()
** has already tried every source before the route was even looked up.  A
** TOKEN route still authenticates a plain `curl -u user:pass` exactly like a
** BASIC one; it just never advertises that it would.
*/
static int
auth_required_response(HTTPC *httpc, UCHAR mode)
{
	int	challenge;

	/* FORM is the only mode that answers with a page instead of a 401.  BASIC
	   and TOKEN are the only others that can arrive -- NONE never sets
	   need_authn, and since #105 there is no unset mode to guess for. */
	if (mode == HTTP_AUTH_FORM) {
		return httpcred(httpc);              /* renders the form, sets DONE */
	}

	/* Whether to advertise a challenge at all.
	   TOKEN is the server-declared API marker (#121): never challenge, and do
	   NOT consult the request -- an operator marked this route as machine-facing
	   and that must not depend on what a client happens to send.
	   For BASIC the older heuristic from #120 still applies: omit the
	   challenge for XHR clients identifying via the z/OSMF CSRF marker.  A
	   WWW-Authenticate makes the browser pop its native credential dialog, and
	   the Basic credentials it then caches outlive the token session, defeating
	   token logout (#119).  Genuine Basic clients (curl -u, Zowe) send
	   credentials preemptively and never needed the challenge; a plain browser
	   navigation (no marker) still gets it. */
	challenge = (mode != HTTP_AUTH_TOKEN) &&
	            (http_get_env(httpc, "HTTP_X-CSRF-ZOSMF-HEADER") == NULL);

	http_resp(httpc, 401);
	if (challenge) {
		/* The realm names the system, not the product: the SMF ID by default
		   (#191), or the Parmlib's REALM value (#193) -- settled once in
		   http_config(), so it is always non-NULL here.  It is what the
		   browser shows the user, and together with the origin it is the key it
		   caches the credentials under -- so a constant would make every httpd
		   on the network one shared protection space. */
		http_printf(httpc, "WWW-Authenticate: Basic realm=\"%s\"\r\n",
		            httpc->httpd->cfg_realm);
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
