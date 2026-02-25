/* HTTPREXX.C - CGI Program, REST style CGI program to link BREXX and execute script */
#include "httpd.h"
#include "svc99.h"
#include "clibary.h"
#include "clibos.h"

#define httpx   (httpd->httpx)

static int link_rexx(HTTPD *httpd, HTTPC *httpc, const char *script);

int main(int argc, char **argv)
{
    int         rc      = 0;
    CLIBPPA     *ppa    = __ppaget();
    CLIBGRT     *grt    = __grtget();
    CLIBCRT		*crt	= __crtget();
    void		*crtapp1= NULL;
    void		*crtapp2= NULL;
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    char        *path   = NULL;
    char        *script = NULL;

    if (!httpd) {
        wtof("This program %s must be called by the HTTPD web server%s", argv[0], "");
        /* TSO callers might not see a WTO message, so we send a STDOUT message too */
        printf("This program %s must be called by the HTTPD web server%s", argv[0], "\n");
        return 12;
    }

	/* save for our exit/exterbnal programs */
	if (crt) {
		crtapp1			= crt->crtapp1;
		crtapp2			= crt->crtapp2;
		crt->crtapp1	= httpd;
		crt->crtapp2	= httpc;
	}
	
	// wtof("%s: enter", argv[0]);

	// wtof("%s: ppa=0x%08X grt=0x%08X httpd=0x%08X httpc=0x%08X",
	// 	argv[0], ppa, grt, httpd, httpc);

    /* get the request path string */
    path = http_get_env(httpc, "REQUEST_PATH");
    script = strrchr(path, '/');
    
    // wtof("%s: path=\"%s\"", argv[0], path);
    // wtof("%s: script=\"%s\"", argv[0], path);

	if (script) {
		rc = link_rexx(httpd, httpc, script);
	}

quit:
	if (crt) {
		/* restore crt values */
		crt->crtapp1	= crtapp1;
		crt->crtapp2	= crtapp2;
	}

	// wtof("%s: exit rc=%d", argv[0], rc);
    return rc;
}

static int link(HTTPD *httpd, HTTPC *httpc, const char *pgm, const char *script);
static int alloc_dummy(const char *ddanme);
static int alloc_temp(const char *ddname);
static int alloc_temp_vars(const char *ddname);
static int free_alloc(const char *ddname);
static int process_stdout(HTTPD *httpd, HTTPC *httpc);
static int process_stderr(HTTPD *httpd, HTTPC *httpc, const char *script);
static int process_abend(HTTPD *httpd, HTTPC *httpc, int rc);
static int create_vars(HTTPD *httpd, HTTPC *httpc);

static int 
link_rexx(HTTPD *httpd, HTTPC *httpc, const char *script)
{
	int			rc			= 0;
	int			lockrc		= 0;
	int			lines		= 0;
	int			errors		= 0;
	void 		*httpsay	= NULL;
    void        *dcb    	= 0;
    unsigned    size    	= 0;
    char        ac      	= 0;
    CDE         *cde;

	// wtof("%s: enter", __func__);

    /* get the steplib DCB */
    dcb 	= __steplb();
    /* load the HTTPSAY module into storage */
	httpsay = __load(dcb, "HTTPSAY ", &size, &ac);
	
	// wtof("%s: HTTPSAY ep=%08X size=%u, ac=%u", __func__, httpsay, size, ac);

	/* Serialize (lock) the address of this function so that 
	 * only one CGI instance (for BREXX) runs at a time.
	 */
	lockrc = lock(link_rexx, LOCK_EXC);

    while(*script=='/') script++;

	/* just in case we have stale allocations */
	free_alloc("STDIN");
	free_alloc("STDOUT");
	free_alloc("STDERR");

	/* allocate dataset for BREXX */
    rc = alloc_dummy("STDIN");
    if (rc) goto quit;
    
    rc = alloc_temp("STDOUT");
    if (rc) goto quit;
    
    rc = alloc_temp("STDERR");
    if (rc) goto quit;
    
    /* write server/client variables to "DD:HTTPVARS" */
	rc = create_vars(httpd, httpc);
	if (rc) goto quit;
    
    /* link to external program */
    rc = link(httpd, httpc, "BREXX", script);
    if (rc < 0) process_abend(httpd, httpc, rc);

	/* create response using STDOUT dataseet */
	lines = process_stdout(httpd, httpc);

	/* process any errors found in STDERR dataset */
	errors = process_stderr(httpd, httpc, script);

deallocate:
	/* release allocations */
	free_alloc("STDIN");
	free_alloc("STDOUT");
	free_alloc("STDERR");
	free_alloc("HTTPVARS");

quit:
	if (lockrc==0) 	unlock(link_rexx, LOCK_EXC);

	if (httpsay) __delete("HTTPSAY ");

	if (!httpc->resp) {
		http_resp(httpc,503);   
		http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
		http_printf(httpc, "\r\n");
		http_printf(httpc, "One or more errors occurred processing your request\n");
	}

    httpc->state = CSTATE_DONE;
	// wtof("%s: exit rc=%d", __func__, rc);
	return rc;
}

static int 
link(HTTPD *httpd, HTTPC *httpc, const char *pgm, const char *script)
{
    void        *ppa    = NULL;
    int         rc      = -1;   /* link return code     */
    int         prc     = -1;   /* pgm return code      */
    void        *dcb    = NULL; /* no DCB for link      */
    char 		*query;
    struct {
        unsigned short  len;
        char            buf[512];
    } parms = {0, ""};
    unsigned    plist[4];

	// wtof("%s: enter", __func__);

    if (!pgm) goto quit;    /* NULL program, quit       */
    if (!*pgm) goto quit;   /* "" program name, quit    */

    query = http_get_env(httpc, "QUERY_STRING");
	if (!query) query = "";

    if (script) {
        /* put request in quotes as parameter string */
        snprintf(parms.buf, sizeof(parms.buf)-1, "%s \"%s\"", script, query);
        parms.buf[sizeof(parms.buf)-1] = 0;
        parms.len = strlen(parms.buf);
    }

	// wtodumpf(&parms, sizeof(parms), "%s: parms", __func__);

    /* build parameter list for the program we're linking to */
    plist[0]    = (unsigned)&parms | 0x80000000;
    plist[1]    = 0;
    plist[2]    = 0;
    plist[3]    = 0;

	// wtodumpf(plist, sizeof(plist), "%s: plist", __func__);

    /* link to pgm, with ESTAE */
    rc = __linkds(pgm, dcb, plist, &prc);
    // wtof("%s: __linkds(\"%s\",0,0x%08X,0x%08X) rc=%d prc=%d",
	// 	__func__, pgm, plist, &prc, rc, prc);
	
	if (rc==0) rc = prc;
	
quit:
	// wtof("%s: exit rc=%d", __func__, rc);
	return rc;
}

static int 
create_vars(HTTPD *httpd, HTTPC *httpc)
{
	int			rc		= 0;
	FILE		*fp		= NULL;
	char 		*p;
	char 		name[256];

	free_alloc("HTTPVARS");

    rc = alloc_temp_vars("HTTPVARS");
    if (rc) goto quit;

	/* create dataset containing client variables */
	fp = fopen("DD:HTTPVARS", "w");
	if (!fp) goto quit;

    if (httpc->env) {
        unsigned count = array_count(&httpc->env);
        unsigned n;
        for(n=0;n<count;n++) {
			HTTPV *env = httpc->env[n];
			
			if (!env) continue;

			/* BREXX doesn't permit variable names with '-' in the name
			 * so we'll transform those to underscore characters.
			 */
			strcpy(name, env->name);
			for(p=strchr(name,'-'); p; p=strchr(name,'-')) *p = '_'; 
			fprintf(fp, "%s=\"%s\"\n", name, env->value);
			// wtof("%s: %s=\"%s\"", __func__, name, env->value);
        }
    }

quit:
	if (fp) fclose(fp);
	return rc;
}

static int 
process_stdout(HTTPD *httpd, HTTPC *httpc)
{
	int			lines	= 0;
	FILE		*fp		= NULL;
	char		*p		= NULL;
	char 		buf[256];

	/* create response */
	fp = fopen("DD:STDOUT", "r");
	// wtof("%s: fopen(\"DD:STDOUT\",\"r\") fp=0x%08X", __func__, fp);
	if (!fp) goto quit;

	while(p=fgets(buf, sizeof(buf), fp)) {
		if (!lines) {
			if (!httpc->resp && __patmat(buf, "HTTP/?.? *")) {
				/* looks like a HTTP response header */
				char *tmp = strdup(buf);
				if (tmp) {
					char *p1 = strtok(tmp, " ");	/* HTTP/1.0 */
					char *p2 = strtok(NULL, " ");	/* nnn */
					char *p3 = strtok(NULL, "");	/* OK or whatever */
					
					if (p2) httpc->resp = atoi(p2);
					free(tmp);
				}
			}

			if (!httpc->resp) {
				/* we don't have a HTTP response */
				http_resp(httpc,200);
				while(*p==' ') p++;
				if (p[0]=='<') {
					/* looks like a HTML markup tag */
					http_printf(httpc, "Content-Type: %s\r\n", "text/html");
				}
				else {
					/* anything else */
					http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
				}
				http_printf(httpc, "\r\n");
			}
		}
		http_printf(httpc, "%s", buf);
		lines++;
	}

quit:
	if (fp) fclose(fp);

	return lines;
}

static int
process_stderr(HTTPD *httpd, HTTPC *httpc, const char *script)
{
	int			errors	= 0;
	FILE		*fp		= NULL;
	char		*p		= NULL;
	char 		buf[256];

	fp = fopen("DD:STDERR", "r");
	// wtof("%s: fopen(\"DD:STDERR\",\"r\") fp=0x%08X", __func__, fp);
	if (!fp) goto quit;

	while(p=fgets(buf, sizeof(buf), fp)) {
		if (!errors) {
			wtof("HTTPD500I CGI Program BREXX script \"%s\"", script);
		}
		// wtodumpf(buf, strlen(buf), "%s: stderr", __func__);
		/* strip trailing newline */
		p = strrchr(buf, '\n');
		if (p) *p = 0;

		wtof("HTTPD501I %s", buf);
		errors++;
	}

quit:
	if (fp) fclose(fp);
	return errors;
}

static int 
process_abend(HTTPD *httpd, HTTPC *httpc, int rc)
{
	/* some kind of ABEND occurred */
	unsigned abcode = (unsigned) (rc * -1);   /* make positive again */

	/* we're running in the HTTPD server */
	if (!httpc->resp) {
		/* no response header was issued by CGI program */
		http_resp(httpc,503);   
		http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
		http_printf(httpc, "\r\n");
	}

	if (abcode > 4095) {
		/* system abend occurred */
		http_printf(httpc, "External program %s failed with S%03X ABEND", "BREXX", abcode >> 12);
        wtof("External program %s failed with S%03X ABEND", "BREXX", abcode >> 12);
	}
	else {
		/* user abend code */
		http_printf(httpc, "External program %s failed with U%04d ABEND", "BREXX", abcode);
        wtof("External program %s failed with U%04d ABEND", "BREXX", abcode);
	}
	http_printf(httpc, "\n");

	return rc;
}

static int 
alloc_dummy(const char *ddname)
{
    int         err     = 1;
    unsigned    count   = 0;
    TXT99       **txt99 = NULL;
    RB99        rb99    = {0};

	// wtof("%s: enter ddname=\"%s\"", __func__, ddname);

	/* allocate this dd name */
	err = __txddn(&txt99, ddname);
    if (err) goto quit;

    /* allocate dummy dataset */
    err = __txdmy(&txt99, NULL);
    if (err) goto quit;

    count = arraycount(&txt99);
    if (!count) goto quit;

    /* Set high order bit to mark end of list */
    count--;
    txt99[count]    = (TXT99*)((unsigned)txt99[count] | 0x80000000);

    /* construct the request block for dynamic allocation */
    rb99.len        = sizeof(RB99);
    rb99.request    = S99VRBAL;
    rb99.flag1      = S99NOCNV;
    rb99.txtptr     = txt99;

    /* SVC 99 */
    err = __svc99(&rb99);
    if (err) goto quit;

quit:
    if (txt99) FreeTXT99Array(&txt99);

	// wtof("%s: exit rc=%d", __func__, err);
    return err;
}

static int alloc_temp(const char *ddname)
{
    int         err     = 1;
    unsigned    count   = 0;
    TXT99       **txt99 = NULL;
    RB99        rb99    = {0};
    char		tempname[40];

	// wtof("%s: enter ddname=\"%s\"", __func__, ddname);

	snprintf(tempname, sizeof(tempname), "&%s", ddname);
	tempname[sizeof(tempname)-1] = 0;

	/* allocate this dd name */
	err = __txddn(&txt99, ddname);
    if (err) goto quit;

    /* allocate this dataset */
    err = __txdsn(&txt99, tempname);
    if (err) goto quit;

    /* DISP=NEW */
    err = __txnew(&txt99, NULL);
    if (err) goto quit;

	/* BLKSIZE=27998 */
	err = __txbksz(&txt99, "27998");
    if (err) goto quit;

	/* LRECL=255 */
	err = __txlrec(&txt99, "255");
    if (err) goto quit;

	/* SPACE=...(pri,sec) */
	err = __txspac(&txt99, "15,15");
    if (err) goto quit;

	/* SPACE=CYLS */
	err = __txcyl(&txt99, NULL);
	if (err) goto quit;

	/* RECFM=VB */
    err = __txrecf(&txt99, "VB");
    if (err) goto quit;

	/* DSORG=PS */
	err = __txorg(&txt99, "PS");
    if (err) goto quit;
    
    count = arraycount(&txt99);
    if (!count) goto quit;

    /* Set high order bit to mark end of list */
    count--;
    txt99[count]    = (TXT99*)((unsigned)txt99[count] | 0x80000000);

    /* construct the request block for dynamic allocation */
    rb99.len        = sizeof(RB99);
    rb99.request    = S99VRBAL;
    rb99.flag1      = S99NOCNV;
    rb99.txtptr     = txt99;

    /* SVC 99 */
    err = __svc99(&rb99);
    if (err) goto quit;

quit:
    if (txt99) FreeTXT99Array(&txt99);

	// wtof("%s: exit rc=%d", __func__, err);
    return err;
}

static int alloc_temp_vars(const char *ddname)
{
    int         err     = 1;
    unsigned    count   = 0;
    TXT99       **txt99 = NULL;
    RB99        rb99    = {0};
    char		tempname[40];

	// wtof("%s: enter ddname=\"%s\"", __func__, ddname);

	snprintf(tempname, sizeof(tempname), "&%s", ddname);
	tempname[sizeof(tempname)-1] = 0;

	/* allocate this dd name */
	err = __txddn(&txt99, ddname);
    if (err) goto quit;

    /* allocate this dataset */
    err = __txdsn(&txt99, tempname);
    if (err) goto quit;

    /* DISP=NEW */
    err = __txnew(&txt99, NULL);
    if (err) goto quit;

	/* BLKSIZE=27998 */
	err = __txbksz(&txt99, "27998");
    if (err) goto quit;

	/* LRECL=8232 */
	err = __txlrec(&txt99, "8232");
    if (err) goto quit;

	/* SPACE=...(pri,sec) */
	err = __txspac(&txt99, "15,15");
    if (err) goto quit;

	/* SPACE=TRACKS */
	err = __txtrk(&txt99, NULL);
	if (err) goto quit;

	/* RECFM=FB */
    err = __txrecf(&txt99, "VB");
    if (err) goto quit;

	/* DSORG=PS */
	err = __txorg(&txt99, "PS");
    if (err) goto quit;
    
    count = arraycount(&txt99);
    if (!count) goto quit;

    /* Set high order bit to mark end of list */
    count--;
    txt99[count]    = (TXT99*)((unsigned)txt99[count] | 0x80000000);

    /* construct the request block for dynamic allocation */
    rb99.len        = sizeof(RB99);
    rb99.request    = S99VRBAL;
    rb99.flag1      = S99NOCNV;
    rb99.txtptr     = txt99;

    /* SVC 99 */
    err = __svc99(&rb99);
    if (err) goto quit;

quit:
    if (txt99) FreeTXT99Array(&txt99);

	// wtof("%s: exit rc=%d", __func__, err);
    return err;
}

static int free_alloc(const char *ddname)
{
    int         err     = 1;
    unsigned    count   = 0;
    TXT99       **txt99 = NULL;
    RB99        rb99    = {0};

	// wtof("%s: enter ddname=\"%s\"", __func__, ddname);

    /* we want to unallocate the DDNAME */
    err = __txddn(&txt99, ddname);
    if (err) goto quit;

    count = arraycount(&txt99);
    if (!count) goto quit;

    /* Set high order bit to mark end of list */
    count--;
    txt99[count]    = (TXT99*)((unsigned)txt99[count] | 0x80000000);

    /* construct the request block for dynamic allocation */
    rb99.len        = sizeof(RB99);
    rb99.request    = S99VRBUN;
    rb99.flag1      = S99NOCNV;
    rb99.txtptr     = txt99;

    /* SVC 99 */
    err = __svc99(&rb99);
    if (err) {
		// wtof("%s: err=%d error=0x%04X info=0x%04X", __func__, err, rb99.error, rb99.info);
		goto quit;
	}

quit:
    if (txt99) FreeTXT99Array(&txt99);

	// wtof("%s: exit rc=%d", __func__, err);
    return err;
}
