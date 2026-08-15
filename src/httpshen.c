/* HTTPSHEN.C
** Set HTTP environment variable
*/
#include "httpd.h"
#include "httpdmsg.h"

static int parse_cookies(HTTPC *httpc, const UCHAR *in);
static int set_cookie(HTTPC *httpc, const UCHAR *name, const UCHAR *value);

extern int
httpshen(HTTPC *httpc, const UCHAR *name, const UCHAR *value)
{
    char        newname[256];
    
    snprintf(newname, sizeof(newname), "HTTP_%s", name);

    if (http_cmp(name, "Cookie")==0) {
		return parse_cookies(httpc, value);
	}

    return http_set_env(httpc, newname, value);
}

static int
parse_cookies(HTTPC *httpc, const UCHAR *in)
{
	UCHAR	*buf = strdup(in);
	UCHAR	*name;
	UCHAR	*value;
	
	if (!buf) {
		wtof(MSG_ENV_NO_STORAGE, "HTTP_Cookie", httpc);
		return -1;      /* -1, like every other setter in this path */
	}

	for (name = buf; name && *name; ) {
		/* skip leading delimiters */
		while (*name == ';' || *name == ' ') name++;
		if (!*name) break;

		/* find end of this token */
		UCHAR *end = name;
		while (*end && *end != ';' && *end != ' ') end++;
		if (*end) *end++ = 0;

		value = strchr(name, '=');
		if (value) {
			*value++ = 0;
		}
		else {
			value = "";
		}
		/* stop at the first failure: the caller resets the connection
		   anyway, and carrying on would emit one HTTPD904E per remaining
		   cookie instead of one per request */
		if (set_cookie(httpc, name, value)) {
			free(buf);
			return -1;
		}
		name = end;
	}

	free(buf);
	return 0;
}

static int
set_cookie(HTTPC *httpc, const UCHAR *name, const UCHAR *value)
{
    char        newname[256];
    
    snprintf(newname, sizeof(newname), "HTTP_Cookie-%s", name);

    return http_set_env(httpc, newname, value);
}
