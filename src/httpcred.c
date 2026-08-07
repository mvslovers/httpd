/* HTTPCRED.C - process login request */
#include "httpd.h"

#define httpx   (httpd->httpx)

static int process_get(HTTPD *httpd, HTTPC *httpc);
static int process_post(HTTPD *httpd, HTTPC *httpc);


int httpcred(HTTPC *httpc)
{
    HTTPD       *httpd  = httpc->httpd;
    char		*method;

	/* get the request method */
	method = http_get_env(httpc, "REQUEST_METHOD");
	if (!method) {
        http_resp(httpc,200);
        http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
        http_printf(httpc, "\r\n");
        http_printf(httpc, "missing REQUEST_METHOD\n");
        goto quit;
    }

	if (http_cmp(method, "GET")==0) {
		process_get(httpd, httpc);
	}
	else if (http_cmp(method, "POST")==0) {
		process_post(httpd, httpc);
	}
	else {
        http_resp(httpc,200);
        http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
        http_printf(httpc, "\r\n");
        http_printf(httpc, "Invalid REQUEST_METHOD \"%s\"\n", method);
	}

quit:
	httpc->state = CSTATE_DONE;
	return 0;
}


static const char *head[] = {
	"<head>",
	" <title>Login Required</title>",
	" <meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">",
	" <style>",
	"  body {font-family: Arial, Helvetica, sans-serif;}",
	"  form {",
	"   border: 3px solid #f1f1f1;",
	"  }",
	"  input[type=text], input[type=password] {",
	"   width: 100%;",
	"   padding: 12px 20px;",
	"   margin: 8px 0;",
	"   display: inline-block;",
	"   border: 1px solid #ccc;",
	"   box-sizing: border-box;",
	"  }",
	"  button {",
	"   background-color: #04AA6D;",
	"   color: white;",
	"   padding: 14px 20px;",
	"   margin: 8px 0;",
	"   border: none;",
	"   cursor: pointer;",
	"   width: 100%;",
	"  }",
	"  button:hover {",
	"   opacity: 0.8;",
	"  }",
	"  .imgcontainer {",
	"   text-align: center;",
	"   margin: 12px 0 12px 0;",
	"  }",
	"  img.avatar {",
	"   width: 20%;",
	"   border-radius: 50%;",
	"  }",
	"  .container {",
	"   padding: 16px;",
	"  }",
	"  span.psw {",
	"   float: right;",
	"   padding-top: 16px;",
	"  }",
	"  .modal {",
	"   display: none;",
	"   position: fixed;",
	"   z-index: 1;",
	"   left: 0;",
	"   top: 0;",
	"   width: 100%;",
	"   height: 100%;",
	"   overflow: auto;",
	"   background-color: rgb(0,0,0);",
	"   background-color: rgba(0,0,0,0.4);",
	"   padding-top: 60px;",
	"  }",
	"  .modal-content {",
	"   background-color: #fefefe;",
	"   margin: 5% auto 15% auto;",
	"   border: 1px solid #888;",
	"   width: 35%;",
	"  }",
	"  .message {",
	"   position: static;",
	"   right: 25px;",
	"   top: 0;",
	"	color: red;",
	"   font-size: 14px;",
	"   font-weight: bold;",
	"   text-align: center;",
#if 0
	"   border: 2px solid green;",
#endif
  	"  }",
	"  .message:hover,",
	"  .message:focus {",
	"	color: red;",
	"  }",
	"  .animate {",
	"   -webkit-animation: animatezoom 0.6s;",
	"   animation: animatezoom 0.6s",
	"  }",
	"  @-webkit-keyframes animatezoom {",
	"   from {-webkit-transform: scale(0)}",
	"   to {-webkit-transform: scale(1)}",
	"  }",
	"  @keyframes animatezoom {",
	"   from {transform: scale(0)}",
	"   to {transform: scale(1)}",
	"  }",
	"  @media screen and (max-width: 300px) {",
	"   span.psw {",
	"    display: block;",
	"    float: none;",
	"   }",
	"  }",
	" </style>",
	"</head>",
	NULL
};

static int
print_head(HTTPD *httpd, HTTPC *httpc)
{
	int		i;
	int		rc = 0;		/* head[] could be empty -> rc read unwritten */

	for(i=0; head[i]; i++) {
		if ((rc = http_printf(httpc, " %s\n", head[i]))) goto quit;
	}

quit:
	return rc;
}

static const char *body1[] = {
	"<body onload=\"showme()\">",
	" <div id=\"login\" class=\"modal\">",
	"  <form class=\"modal-content animate\" action=\"/login\" method=\"post\">",
	"   <div class=\"imgcontainer\">",
	"    <img src=\"favicon.ico\" alt=\"Avatar\" class=\"avatar\">",
	"   </div>",
	" ",
	"   <div class=\"container\">",
	NULL
};

static const char *body2[] = {
	"    <label><b>Userid</b>",
	"     <input type=\"text\" placeholder=\"Enter Userid\" name=\"userid\" maxlength=\"8\" required>",
	"    </label>",
	" ",
	"    <label><b>Password</b>",
	"     <input type=\"password\" placeholder=\"Enter Password\" name=\"password\" maxlength=\"8\" required>",
	"    </label>",
	" ",
	NULL
};

static const char *body3[] = {
	"    <button type=\"submit\">Login</button>",
	"    <p class=\"message\">This server uses cookies to store a security token when you login.</p>",
	"   </div>",
	" ",
	"  </form>",
	" </div>",
	" <script>",
	" function showme() {",
	"  var modal = document.getElementById('login');",
	"  modal.style.display=\"block\";",
	" }",
	" </script>",
	"</body>",
	NULL
};

static int
print_body(HTTPD *httpd, HTTPC *httpc, const char *msg)
{
	int		i;
	int		rc;
	char	*uri = http_get_env(httpc, "HTTP_Cookie-Sec-Uri");
	char	esc[512];

	if (!uri) uri = "/";
	
	for(i=0; body1[i]; i++) {
		if ((rc=http_printf(httpc, " %s\n", body1[i]))) goto quit;
	}

	if (msg) {
		if ((rc=http_printf(httpc, "<h2 class=\"message\">%s</h2>\n", msg))) goto quit;
	}

	for(i=0; body2[i]; i++) {
		if ((rc=http_printf(httpc, " %s\n", body2[i]))) goto quit;
	}

	/* The hidden field carries the client-controlled Sec-Uri cookie back to
	   the form.  HTML-escape it (reflected XSS) and print it directly -- never
	   sprintf an unbounded cookie value into a fixed stack buffer (overflow). */
	http_html_escape((UCHAR *)esc, sizeof(esc), (UCHAR *)uri);
	if ((rc=http_printf(httpc,
	        "    <input type=\"hidden\" name=\"uri\" value=\"%s\">\n", esc)))
		goto quit;

	for(i=0; body3[i]; i++) {
		if ((rc=http_printf(httpc, " %s\n", body3[i]))) goto quit;
	}

quit:
	return rc;
}

static int
print_login(HTTPD *httpd, HTTPC *httpc, const char *msg)
{
	int		rc = 0;

    http_resp(httpc,200);
	http_printf(httpc, "Cache-Control: no-store\r\n");
#if 0
    http_printf(httpc, "Set-Cookie: Sec-Token=deleted; path=/; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n");
#endif
    http_printf(httpc, "Content-Type: %s\r\n", "text/html");
    http_printf(httpc, "\r\n");

	http_printf(httpc, "<!DOCTYPE html>\n");
	http_printf(httpc, "<html lang=\"en-US\">\n");
	if ((rc=print_head(httpd,httpc))) goto quit;
	if ((rc=print_body(httpd,httpc,msg))) goto quit;

#if 0
	/* debugging only */
	if (rc=dump_env(httpd, httpc, 1)) goto quit;
#endif
    
    rc=http_printf(httpc, "</html>\n");

quit:
	return rc;
}

static int 
process_get(HTTPD *httpd, HTTPC *httpc)
{
	int		rc 			= 0;
	CRED 	*cred 		= httpc->cred;
	char	*path		= http_get_env(httpc, "REQUEST_PATH");
	char	*uri		= http_get_env(httpc, "REQUEST_URI");
	int		logged_out 	= 0;
	CREDID  id;
	char	*buf;
	size_t	len;
#if 0	
	wtof("httpcred.c:process_get() path=%s", path ? path : "(null)");
#endif	
	if (!path) goto send_login;
	
	if (http_cmp(path, "/login") && http_cmp(path, "/logout")) {
		/* redirect to the our "/login" path */
		http_resp(httpc,302);
		http_printf(httpc, "Cache-Control: no-store\r\n");
		http_printf(httpc, "Location: /login\r\n");

		buf = base64_encode(uri, strlen(uri), &len);
		if (buf) {
			http_printf(httpc, "Set-Cookie: Sec-Uri=%s; path=/\r\n", buf);
			free(buf);
		}
		else {
			http_printf(httpc, "Set-Cookie: Sec-Uri=%s; path=/\r\n", uri);
		}

		http_printf(httpc, "\r\n");
		goto quit;
	}
	
	if (http_cmp(path, "/logout")==0) {
		http_resp(httpc,200);
		http_printf(httpc, "Cache-Control: no-store\r\n");
		http_printf(httpc, "Set-Cookie: Sec-Uri=deleted; path=/; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n");
		http_printf(httpc, "Set-Cookie: Sec-Token=deleted; path=/; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n");
		http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
		http_printf(httpc, "\r\n");
	
		if (cred) {
			credid_dec(&cred->id, &id);
		
			if (credtok_logout(&cred->token)==0) {
				logged_out++;
			}
		}

		if (logged_out) {
			http_printf(httpc, "You have been logged out.\n");
			wtof("HTTPD008I User:%-8.8s IP:%u.%u.%u.%u Logout Successful",
				id.userid, 
				id.addr >> 24 & 0xFF,
				id.addr >> 16 & 0xFF,
				id.addr >>  8 & 0xFF,
				id.addr       & 0xFF);
		}
		else if (cred) {
			http_printf(httpc, "Logout failed.\n");
			wtof("HTTPD008W User:%-8.8s IP:%u.%u.%u.%u Logout Failed",
				id.userid, 
				id.addr >> 24 & 0xFF,
				id.addr >> 16 & 0xFF,
				id.addr >>  8 & 0xFF,
				id.addr       & 0xFF);
		}
		else {
			http_printf(httpc, "You are not logged in. No logout performed.\n");
		}

		httpc->cred = NULL;
		memset(&id, 0, sizeof(CREDID));
		goto quit;
	}

send_login:
	rc = print_login(httpd, httpc, "Login Required");

quit:    
    return rc;
}

static int
process_post(HTTPD *httpd, HTTPC *httpc)
{
	int		rc = 0;
	char	*user		= http_get_env(httpc, "POST_userid");
	char	*pass		= http_get_env(httpc, "POST_password");
	char	*uri		= http_get_env(httpc, "HTTP_Cookie-Sec-Uri");
	char	uribuf[256];
	CRED	*cred;
	CREDID  id;
	char 	*buf;
	size_t	len;
#if 0
	wtof("httpcred.c:process_post() uri=\"%s\"", uri ? uri : "(null)");
#endif
	/* decode the Sec-Uri cookie into the post-login redirect target */
	if (uri) {
		buf = base64_decode(uri, strlen(uri), &len);
		if (buf) {
			/* base64_decode returns binary and does not NUL-terminate; copy
			   exactly the decoded length (bounded) and terminate.  strncpy
			   here would over-read buf hunting for a NUL. */
			if (len >= sizeof(uribuf)) len = sizeof(uribuf) - 1;
			memcpy(uribuf, buf, len);
			uribuf[len] = 0;
			free(buf);
#if 0
			wtof("httpcred.c:process_post() uribuf=\"%s\"", uribuf);
#endif
			uri = uribuf;
		}

		/* only follow a safe, site-local target: reject CR/LF (Location
		   header injection) and off-site / protocol-relative URIs (open
		   redirect); otherwise fall back to "/" */
		if (!http_safe_redirect((UCHAR *)uri)) {
			uri = "/";
		}
		else if (http_cmpn(uri, "/login", 6)==0) {
			uri = "/";   /* don't bounce the user back to the login page */
		}
	}
	else {
		uri = "/";
	}

	cred = cred_login(httpc->addr, user, pass);
#if 0
	wtof("httpcred.c:process_post() cred=0x%08X", cred);
#endif
	if (cred) {
		/* success */
		httpc->cred = cred;

		credid_dec(&cred->id, &id);

		wtof("HTTPD007I User:%-8.8s IP:%u.%u.%u.%u Login Successful ACEE(%06X)",
			id.userid, 
			id.addr >> 24 & 0xFF,
			id.addr >> 16 & 0xFF,
			id.addr >>  8 & 0xFF,
			id.addr       & 0xFF,
			cred->acee );

		memset(&id, 0, sizeof(CREDID));

		buf = base64_encode((void*)&cred->token, sizeof(CREDTOK), &len);
		if (buf) {
			http_resp(httpc, 303);
			http_printf(httpc, "Cache-Control: no-store\r\n");
			http_printf(httpc, "Location: %s\r\n", uri);
			http_printf(httpc, "Set-Cookie: Sec-Uri=deleted; path=/; expires=Thu, 01 Jan 1970 00:00:00 GMT\r\n");
			http_printf(httpc, "Set-Cookie: Sec-Token=%s\r\n", buf);
			http_printf(httpc, "\r\n");

			free(buf);
			goto quit;
		}
	}

	rc = print_login(httpd, httpc, "Invalid userid or password");

quit:    
    return rc;
}
