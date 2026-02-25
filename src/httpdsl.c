/* HTTPJES2.C - CGI Program, REST style CGI program for access of JES2 resources */
#include "httpd.h"

#include "httpds.h"         /* dataset services common */

static int do_help(HTTPDS *ds);

static int do_print(HTTPDS *ds);
static int do_print_binary(HTTPDS *ds);
static int do_print_text(HTTPDS *ds);

static int isdataset(const char *name);
static int ismember(const char *name);
static int ispattern(const char *name, const char *pattern);
static char *strupper(char *buf);
static int dump_ds(HTTPDS *ds);

int main(int argc, char **argv)
{
    int         rc      = 0;
    HTTPDS      httpds  = {0};
    HTTPDS      *ds     = &httpds;
    const char  *p;
    char        hlqname[56] = {0};
    char        dsname[56] = {0};
    
    /* build common struct (passed to all functions) */
    ds->_ppa            = __ppaget();
    ds->_grt            = __grtget();
    ds->_crt            = __crtget();
    ds->_httpd          = grt->grtapp1;
    ds->_httpc          = grt->grtapp2;
    ds->pgm             = argv[0];

    ds->start           = 0.0;
    ds->end             = 0.0;


    /* make sure we're running on the HTTPD server */
    if (!httpd) {
        wtof("This program %s must be called by the HTTPD web server%s", "");
        /* TSO callers might not see a WTO message, so we send a STDOUT message too */
        printf("This program %s must be called by the HTTPD web server%s", "\n");
        return 12;
    }

    /* get the request path string */
    ds->path = http_get_env(httpc, "REQUEST_PATH");
    if (!ds->path) {
        wtof("Unable to obtain REQUEST_PATH value");
        return 12;
    }

    /* get the last name in the request path string */
    ds->verb = strrchr(ds->path, '/');
    if (!ds->verb) {
        wtof("Unable to obtain verb from REQUEST_PATH value \"%s\"", ds->path);
        return 12;
    }
    
    /* skip over the path seperator */
    while(*ds->verb=='/') ds->verb++;
    if (!ds->verb[0]) ds->verb = "list";

    /* get the query variables */
    ds->hlq = http_get_env(httpc, "QUERY_HLQ");
    if (ds->hlq) {
        /* copy hlq to our dsname buffer and fold to upper case */
        strncpy(hlqname, ds->hlq, sizeof(hlqname));
        hlqname[sizeof(hlqname)-1] = 0;
        strupper(hlqname);
        ds->hlq = hlqname;
    }

    ds->dsn = http_get_env(httpc, "QUERY_DSNAME");
    if (!ds->dsn) ds->dsn = http_get_env(httpc, "QUERY_DSN");
    if (ds->dsn) {
        /* copy hlq to our dsname buffer and fold to upper case */
        strncpy(dsname, ds->dsn, sizeof(dsname));
        dsname[sizeof(dsname)-1] = 0;
        strupper(dsname);
        ds->dsn = dsname;
    }

    ds->dstype = http_get_env(httpc, "QUERY_DSTYPE");

    ds->member = http_get_env(httpc, "QUERY_MEMBER");
    if (!ds->member) ds->member = http_get_env(httpc, "QUERY_MEM");

    ds->format = http_get_env(httpc, "QUERY_FORMAT");
    if (!ds->format) ds->format = http_get_env(httpc, "QUERY_FMT");
    if (!ds->format) ds->format = "print";

    p = http_get_env(httpc, "QUERY_BINARY");
    if (!p) p = http_get_env(httpc, "QUERY_BIN");
    if (!p) p = "0";
    ds->binary = atoi(p);
    
    p = http_get_env(httpc, "QUERY_TEXT");
    if (!p) p = http_get_env(httpc, "QUERY_PRINT");
    if (!p) p = http_get_env(httpc, "QUERY_E2A");
    if (p) {
        ds->e2a = atoi(p);
    }
    else if (ds->binary) {
        ds->e2a = 0;   /* assume data is binary */
    }
    else {
        ds->e2a = 1;   /* assume data is text or print */
    }
    
    ds->filename = http_get_env(httpc, "QUERY_FILENAME");
    if (!ds->filename) ds->filename = http_get_env(httpc, "QUERY_FN");

    /* execute the appropriate function for this verb */
    if (http_cmp(ds->verb, "info")==0) {
        /* returns information for a dataset */
        rc = httpds_info(ds);
    }
    else if (http_cmp(ds->verb, "list")==0) {
        /* returns information for datasets for hlq */
        rc = httpds_list(ds);
    }
    else if (http_cmp(ds->verb, "pds")==0) {
        /* returns directory information for PDS dataset */
        rc = httpds_pds(ds);
    }
    else if (http_cmp(ds->verb, "print")==0) {
        rc = do_print(ds);
    }
    else if (http_cmp(ds->verb, "xmit")==0) {
        rc = httpds_xmit(ds);
    }
    else {
        /* create help info */
        rc = do_help(ds);
    }

#if 0
    dump_ds(ds);
#endif

    if (!httpc->resp) {
        /* we screwed up someplace, tell the end user */
        http_resp(httpc, 404);
        http_printf(httpc, "Content: text/plain\r\n");
        http_printf(httpc, "\r\n");
        http_printf(httpc, "%s: unexpected processing error.\n\n", ds->pgm);
        http_printf(httpc, "The requested document \"%s\" was not found.\n", ds->path);
    }

quit:
    return 0;
}

static int 
do_print(HTTPDS *ds)
{
    int         rc;
    
    if (ds->binary) {
        rc = do_print_binary(ds);
    }
    else {
        rc = do_print_text(ds);
    }
    
quit:
    return rc;
}

static int 
do_print_binary(HTTPDS *ds)
{
    FILE        *fp         = NULL;
    int         i;
    int         pos;
    int         len;
    int         rc;
    char        dataset[256]= {0};
    char        *buf;

    if (!ds->dsn) {
        httpds_error(ds, "Missing ?dsn= query variable.\n");
        goto quit;
    }

    if (ds->member) {
        snprintf(dataset, sizeof(dataset)-1, "%s(%s)", ds->dsn, ds->member);
    }
    else {
        snprintf(dataset, sizeof(dataset)-1, "%s", ds->dsn);
    }
    dataset[sizeof(dataset)-1] = 0;
    
    fp = fopen(dataset, "rb");
    if (!fp) {
        httpds_error(ds, "Unable to open dataset=\"%s\" for read.\n", dataset);
        goto quit;
    }

    len = fp->lrecl;
    if (!len) len = fp->blksize;
    
    buf = calloc(1, len + 8);
    if (!buf) {
        httpds_error(ds, "Unable to allocate buffer of %u bytes.\n", len + 8);
        goto quit;
    }
    
    http_resp(httpc, 200);
    http_printf(httpc, "Content-Type: application/octet-stream\r\n");
    http_printf(httpc, "Access-Control-Allow-Origin: *\r\n");
    if (ds->filename && ds->filename[0]) {
        http_printf(httpc, "Content-Disposition: attachment; filename=\"%s\"\r\n", ds->filename);
    }
    else if (ds->member && ds->member[0]) {
        http_printf(httpc, "Content-Disposition: attachment; filename=\"%s(%s)\"\r\n", ds->dsn, ds->member);
    }
    else {
        http_printf(httpc, "Content-Disposition: attachment; filename=\"%s\"\r\n", ds->dsn);
    }
    http_printf(httpc, "Content-Transfer-Encoding: binary\r\n");
    http_printf(httpc, "Cache-Control: private\r\n");
    http_printf(httpc, "Pragma: private\r\n");
    http_printf(httpc, "\r\n");
    
    while(i=fread(buf, 1, len, fp)) {
        // wtof("%s: fread() rc=%d", __func__, i);
        
        /* send buffer to web client */
        for(pos=0; pos < i; pos+=rc) {
            rc = http_send(httpc, &buf[pos], i-pos);

            /* if send error we're done */
            if (rc<0) goto quit;
        }
    }

quit:
    if (buf) free(buf);
    if (fp) fclose(fp);
    return 0;
}

static int 
do_print_text(HTTPDS *ds)
{
    FILE        *fp         = NULL;
    int         i;
    int         pos;
    int         len;
    int         rc;
    char        dataset[256]= {0};
    char        *buf;

    if (!ds->dsn) {
        httpds_error(ds, "Missing ?dsn= query variable.\n");
        goto quit;
    }

    if (ds->member) {
        snprintf(dataset, sizeof(dataset)-1, "%s(%s)", ds->dsn, ds->member);
    }
    else {
        snprintf(dataset, sizeof(dataset)-1, "%s", ds->dsn);
    }
    dataset[sizeof(dataset)-1] = 0;
    
    fp = fopen(dataset, "r");
    if (!fp) {
        httpds_error(ds, "Unable to open dataset=\"%s\" for read.\n", dataset);
        goto quit;
    }

    len = fp->lrecl;
    if (!len) len = fp->blksize;
    
    buf = calloc(1, len + 8);
    if (!buf) {
        httpds_error(ds, "Unable to allocate buffer of %u bytes.\n", len + 8);
        goto quit;
    }
    
    len++; // space to trailing newline character
    
    http_resp(httpc, 200);
    http_printf(httpc, "Content-Type: text/plain\r\n");
    if (ds->filename && ds->filename[0]) {
        http_printf(httpc, "Content-Disposition: attachment; filename=\"%s\"\r\n", ds->filename);
        http_printf(httpc, "Cache-Control: private\r\n");
        http_printf(httpc, "Pragma: private\r\n");
    }
    http_printf(httpc, "\r\n");
    
    while(fgets(buf, len, fp)) {
        /* map any non-printable characters to spaces */
        for(i=0; i < len; i++) {
            if (isprint(buf[i])) continue;
            if (buf[i]=='\n' && buf[i+1]==0) {
                i++;
                break;
            }
            buf[i] = ' ';
        }

        if (ds->e2a) {
            /* convert buffer to ASCII */
            http_etoa(buf, i);
        }
        
        /* send buffer to web client */
        for(pos=0; pos < i; pos+=rc) {
            rc = http_send(httpc, &buf[pos], i-pos);

            /* if send error we're done */
            if (rc<0) goto quit;
        }
    }

quit:
    if (buf) free(buf);
    if (fp) fclose(fp);
    return 0;
}

static int 
do_help(HTTPDS *ds)
{
    http_resp(httpc, 200);
    http_printf(httpc, "Content: text/plain\r\n");
    http_printf(httpc, "\r\n");
    http_printf(httpc, "%s Usage:\n", ds->pgm);
    http_printf(httpc, "This CGI module uses a verb name (last name in URL path) to select\n"
        "the appropriate processing function for this request.\n\n"
        "The supported verb names are:\n");
    http_printf(httpc, "help     returns this help text.\n\n");
    http_printf(httpc, "list     returns lines representing one or more\n"
                       "         datasets based on the hlq query variable.\n"
                       "         Example: /dsl/list?hlq=HTTPD\n\n");
    http_printf(httpc, "pds      returns lines representing one or more\n"
                       "         members based on the dsn query variable.\n"
                       "         Example: /dsl/pds?dsn=HTTP.HTML\n\n");
    http_printf(httpc, "print    returns the datasest contents based on the dsname and member\n"
                       "         query variables.\n"
                       "         Example: /dsl/print?dsn=HTTPD.HTML&member=INDEX\n\n");
    http_printf(httpc, "You can also add the debug query variable to your URL request with these options\n"
                       "specified after the equal sign '=' seperated by comma ',' characters:\n");
    http_printf(httpc, "help     displays the debug query variable help text.\n");
    http_printf(httpc, "vars     displays the request variables.\n");
    http_printf(httpc, "cgi      displays the CGI table.\n");
    http_printf(httpc, "         Example: /dsl/help?hlq=HTTPD&debug=help,vars,cgi\n\n");

    return 0;
}

static int 
isdataset(const char *name)
{
	int		dataset = 0;
	int		levels  = 0;
	int		member  = 0;
	int		len;
	char	buf[256];
	char	*p;

	strcpy(buf, name);
	
	if (ismember(buf)) {
		/* looks like a single high level qualifier */
		if (!isdigit(buf[0])) {
			dataset = 1;
			goto quit;
		}
	}

	if (strstr(buf, "..")) goto quit;	/* can't have and empty dataset level name */
	
	for(p = strtok(buf, "."); p; p = strtok(NULL, ".")) {
		char *lparen = strchr(p, '(');
		char *rparen = strrchr(p, ')');
	
		// wtof("   %s: p=\"%s\"", __func__, p ? p : "(null)");
		if (lparen && rparen) {
			if (member) goto quit;		/* we've already seen a member so die */
			member++;
			*lparen = 0;
			*rparen = 0;
		}

		levels++;
		len = strlen(p);
		
		if (len < 1) goto quit;			/* dataset level name too short */
		if (len > 8) goto quit;			/* dataset level name too long */
		if (isdigit(p[0])) goto quit;	/* dataset level name can not start with a number */
		if (!ismember(p)) goto quit;	/* bad character in name */

		if (lparen && rparen) {
			if (strlen(lparen+1) > 8) goto quit;	/* dataset level name too long */
			if (!ismember(lparen+1)) goto quit;	/* bad character in name */

			*lparen = '(';
			*rparen = ')';
		}

	}

	/* name could be a dataset */
	if (levels <= 22) {
		int maxlen = 44;
		
		if (member > 1) goto quit;	/* only 1 member allowed */
		
		if (member) {
			maxlen += 10;	/* maxlen += strlen("(12345678)") */
		}

		if (strlen(name) <= maxlen) {
			dataset = 1;
		}
	}

quit:
	// wtof("   %s(\"%s\") %s", __func__, name, dataset ? "SUCCESS" : "FAILED");
	return dataset;
}

static int 
ismember(const char *name)
{
	int		member = 0;
	int		len = strlen(name);
	int		i;

	if (len < 1) goto quit;				/* invalid member name */

	if (len <= 8) {
		member = 1;	/* assume the name is a valid member name */
		for(i=0; name[i]; i++) {
			if (name[i]=='@') continue;
			if (name[i]=='#') continue;
			if (name[i]=='$') continue;
			if (isalnum(name[i])) continue;

			/* not a valid character for a member name */
			member = 0;
			break;
		}
	}
	
quit:
	// wtof("   %s(\"%s\") %s", __func__, name, member ? "SUCCESS" : "FAILED");
	return member;
}

static int 
ispattern(const char *name, const char *pattern)
{
	int	rc = __patmat(name, pattern);
	
	// wtof("   %s(\"%s\",\"%s\") %s", __func__, name, pattern, rc ? "MATCHED" : "NOMATCH");
	
	return rc;
}

static char *
strupper(char *buf)
{
	int		i;
	
	if (!buf) goto quit;
	
	for(i=0; buf[i]; i++) {
		if (islower(buf[i])) {
			buf[i] = (char) toupper(buf[i]);
		}
	}

quit:
	return buf;
}

static int 
dump_ds(HTTPDS *ds)
{
    const char *p;
    
    wtodumpf(ds, sizeof(HTTPDS), "%s HTTPDS", __func__);
    wtof("ppa           %p", ds->_ppa);
    wtof("grt           %p", ds->_grt);
    wtof("crt           %p", ds->_crt);
    wtof("httpd         %p", ds->_httpd);
    wtof("httpc         %p", ds->_httpc);
    wtof("pgm           %s", ds->pgm);
    wtof("path          %s", ds->path);
    wtof("verb          %s", ds->verb);
    wtof("hlq           %s", ds->hlq);
    wtof("dsn           %s", ds->dsn);
    wtof("dstype        %s", ds->dstype);
    wtof("member        %s", ds->member);
    wtof("format        %s", ds->format);
    wtof("binary        %d", ds->binary);
    wtof("e2a           %d", ds->e2a);
    switch(ds->type) {
    default:
    case TYPE_UNKNOWN:  p = "UNKNOWN";  break;
    case TYPE_HLQ:      p = "HLQ";      break;
    case TYPE_DSN:      p = "DSN";      break;
    case TYPE_DSNMEM:   p = "DSNMEM";   break;
    }
        
    wtof("type          %d %s", ds->type, p);

    switch(ds->org) {
    default:
    case ORG_UNKNOWN:   p = "UNKNOWN";  break;
    case ORG_PS:        p = "PS";       break;
    case ORG_PDS:       p = "PDS";      break;
    }
    wtof("org           %d %s", ds->org, p);

    wtof("start         %f", ds->start);
    wtof("end           %f", ds->end);
    if (ds->start > 0.0 && ds->end > 0.0) {
        double elapsed = ds->end - ds->start;
        wtof("elapsed       %f", elapsed);
    }

    return 0;
}

