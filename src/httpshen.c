/* HTTPSHEN.C
** Set HTTP environment variable
*/
#include "httpd.h"

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
		wtof("%s: out of memory", __func__);
		return ENOMEM;
	}
	
	for(name=strtok(buf, "; "); name; name=strtok(NULL, "; ")) {
		value = strchr(name, '=');
		if (value) {
			*value++ = 0;
		}
		else {
			value = "";
		}
		set_cookie(httpc, name, value);
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
