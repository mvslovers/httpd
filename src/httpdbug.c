#include "httpd.h"

static int dump_cgi(HTTPD *httpd, HTTPC *httpc);
static int dump_help(HTTPD *httpd, HTTPC *httpc);
static int dump_vars(HTTPD *httpd, HTTPC *httpc);

int 
http_debug(HTTPC *httpc, const char *options) 
{
	HTTPD 		*httpd 		= httpc->httpd;
	int			len;
	char		*opt;
	char		*next;
	char		opts[256];
	
	if (!options) goto quit;

	strncpy(opts, options, sizeof(opts));
	opts[sizeof(opts)-1] = 0;
	
	http_printf(httpc, "<!--\n");

	for (opt = opts; opt && *opt; ) {
		/* skip leading commas */
		while (*opt == ',') opt++;
		if (!*opt) break;

		/* find end of this option */
		char *end = strchr(opt, ',');
		if (end) *end++ = 0;

		len = strlen(opt);

		if (http_cmpn(opt, "cgi", len)==0) {
			dump_cgi(httpd, httpc);
		}
		else if (http_cmpn(opt, "help", len)==0 || http_cmp(opt, "?")==0) {
			dump_help(httpd, httpc);
		}
		else if (http_cmpn(opt, "vars", len)==0) {
			dump_vars(httpd, httpc);
		}

		opt = end;
	}

	http_printf(httpc, "-->\n");

quit:
	return 0;
}

static char *help_text[] = {
	"cgi      Display server CGI Table.",
	"help     This help text is displayed.",
	"vars     Display variables for this request.",
	NULL
};

static int 
dump_cgi(HTTPD *httpd, HTTPC *httpc)
{
    unsigned    count;
    unsigned    n;

	http_printf(httpc, "\nCGI Table\n");
	
    count = array_count(&httpd->httpcgi);
    for (n=0; n < count; n++) {
        HTTPCGI *p = httpd->httpcgi[n];

        if (!p) continue;
        
        http_printf(httpc, "   Path=\"%s\" Program=\"%s\" Login=%u Wild=%u\n",
			p->path, p->pgm, p->login, p->wild);
    }

quit:
    return 0;
}


static int 
dump_help(HTTPD *httpd, HTTPC *httpc)
{
	int		rc = 0;
	int		i;

	http_printf(httpc, "\nHelp for debug=... query variable values\n");

    for(i=0; help_text[i]; i++) {
		http_printf(httpc, "   %s\n", help_text[i]);
	}
	
quit:
	return rc;
}

static int 
dump_vars(HTTPD *httpd, HTTPC *httpc)
{
	int		rc = 0;

	http_printf(httpc, "\nVariables\n");
    if (httpc->env) {
        unsigned count = array_count(&httpc->env);
        unsigned n;
        for(n=0;n<count;n++) {
			HTTPV *env = httpc->env[n];
			
			if (!env) continue;

			rc = http_printf(httpc, "   \"%s\"=\"%s\"\n", env->name, env->value);
			if (rc) goto quit;
        }
    }
    
quit:
	return rc;
}
