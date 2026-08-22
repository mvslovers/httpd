#include "httpd.h"

static int dump_route(HTTPD *httpd, HTTPC *httpc);
static int dump_help(HTTPD *httpd, HTTPC *httpc);
static int dump_vars(HTTPD *httpd, HTTPC *httpc);

static const char *auth_text(UCHAR auth);
static const char *attr_text(UCHAR attr);

int 
http_debug(HTTPC *httpc, const char *options) 
{
	HTTPD 		*httpd 		= httpc->httpd;
	int			len;
	char		*opt;
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

		if (http_cmpn(opt, "mod", len)==0) {
			dump_route(httpd, httpc);
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
	"mod      Display the server route table (MOD= and LOC= entries).",
	"help     This help text is displayed.",
	"vars     Display variables for this request.",
	NULL
};

/* auth_text() / attr_text() - short forms of the per-route auth policy. */
static const char *
auth_text(UCHAR auth)
{
	switch (auth) {
	case HTTP_AUTH_NONE:	return "NONE";
	case HTTP_AUTH_FORM:	return "FORM";
	case HTTP_AUTH_BASIC:	return "BASIC";
	case HTTP_AUTH_TOKEN:	return "TOKEN";
	default:				return "?";
	}
}

static const char *
attr_text(UCHAR attr)
{
	switch (attr) {
	case 0:					return "READ(assumed)";
	case RACF_ATTR_READ:	return "READ";
	case RACF_ATTR_UPDATE:	return "UPDATE";
	case RACF_ATTR_CONTROL:	return "CONTROL";
	case RACF_ATTR_ALTER:	return "ALTER";
	default:				return "?";
	}
}

static int
dump_route(HTTPD *httpd, HTTPC *httpc)
{
    unsigned    count;
    unsigned    n;

	http_printf(httpc, "\nRoute Table\n");
	http_printf(httpc, "   (Auth=NONE is public -- either AUTH=NONE or no"
		" AUTH= keyword at all, which since #105 mean the same thing)\n");

    count = array_count(&httpd->route);
    for (n=0; n < count; n++) {
        HTTPROUTE *p = httpd->route[n];

        if (!p) continue;

		/* MOD= carries a program, LOC= is a program-less static prefix */
		http_printf(httpc, "   %s Path=\"%s\"",
			p->pgm ? "MOD" : "LOC", p->path ? p->path : "(none)");

		if (p->pgm) {
			http_printf(httpc, " Program=\"%s\"", p->pgm);
		}

		http_printf(httpc, " Auth=%s Wild=%u", auth_text(p->auth), p->wild);

		/* RES= resource gate -- only routes that carry one */
		if (p->resclass) {
			http_printf(httpc, " Res=%s:%s Attr=%s",
				p->resclass, p->resname ? p->resname : "(none)",
				attr_text(p->resattr));
		}

		http_printf(httpc, "\n");
    }

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
