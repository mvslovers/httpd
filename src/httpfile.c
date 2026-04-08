/* HTTPFILE.C
** Send file, file handle is already open.
*/
#include "httpd.h"

typedef enum sstate     SSTATE;
enum sstate {
    SSTATE_SENDING=0,
    SSTATE_EOF,
    SSTATE_DONE
};

/* forward reference static function prototypes */
static int ssi_file_read(HTTPC *httpc);
static int ssi_process(HTTPC *httpc, char *ssi);
static int ssi_echo(HTTPC *httpc, char *next);
static int ssi_include(HTTPC *httpc, char *next);
static int ssi_printenv(HTTPC *httpc);
static int ssi_system_env(HTTPC *httpc);

/* output handlers */
static int ssi_printf(HTTPC *httpc,  const char *fmt, ... );
static int ssi_printv(HTTPC *httpc, const char *fmt, va_list args);
static int ssi_send_buffer(HTTPC *httpc);
static int ssi_buffer(HTTPC *httpc, const char *buf, int len);

/* http_printf() uses it own buffer and not the httpc->buf 
 * so we have our own ssi_printf() function. If by chance
 * we forget not to use http_printf() we'll use this macro
 * to force the ssi_printf() to be used instead.
 */
#undef http_printf
#define http_printf(httpc,fmt,...) \
    ((ssi_printf)((httpc),(fmt),## __VA_ARGS__))

#undef http_send_file
int
http_send_file(HTTPC *httpc, int binary)
{
    int     rc  = 0;
    int     rdw = binary ? httpc->rdw : 0;
    int     len = 0;
    int     avail;

	if (httpc->ssi) {
		ssi_file_read(httpc);
		goto sendit;
	}

    switch (httpc->substate) {
    case SSTATE_SENDING:
        /* do we have any room in our buffer? */
        avail = CBUFSIZE - httpc->len;
        if (avail > 0) {
			if (httpc->fp) {
				/* use CLIB version of fread() */
				len = fread(&httpc->buf[httpc->len], 1, avail, httpc->fp);
			}
			else if (httpc->ufp) {
				/* use UFS version of fread() */
				len = ufs_fread(&httpc->buf[httpc->len], 1, avail, httpc->ufp);
			}

            if (len>0) {
                /* we read some data */
                if (!binary) {
                    /* convert EBCDIC to ASCII */
                    if (httpc->ufp) {
                        /* UFS files are stored in IBM-1047 */
                        http_xlate(&httpc->buf[httpc->len], len,
                                   httpx->xlate_1047->etoa);
                    } else {
                        /* MVS datasets use server-default codepage */
                        http_etoa(&httpc->buf[httpc->len], len);
                    }
                }
                httpc->len += len;
            }

            /* check for end-of-file */
            if (httpc->fp) {
                if (feof(httpc->fp)) {
                    httpc->substate = SSTATE_EOF;
                }
            }
            else if (httpc->ufp) {
                if (ufs_feof(httpc->ufp)) {
                    /* wtof("%s EOF", __func__); */
                    httpc->substate = SSTATE_EOF;
                }
            }
        }
    }

sendit:
    if (httpc->pos < httpc->len) {
        /* we have unsent data in buffer */
        len = httpc->len - httpc->pos;
        if (len > 0) {
            len = http_send(httpc, &httpc->buf[httpc->pos], len);
            if (len>0) {
                httpc->pos += len;
            }
        }
    }

    if (httpc->pos>0) {
        len = httpc->len - httpc->pos;
        if (len>0) {
            /* shift data left */
            memcpy(httpc->buf, &httpc->buf[httpc->pos], len);
            /* clear remaining buffer */
            avail = CBUFSIZE - httpc->len;
            memset(&httpc->buf[len], 0, avail);
            /* update data position and length */
            httpc->pos -= len;
            httpc->len -= len;
        }
        else {
            /* reset buffer */
            memset(httpc->buf, 0, CBUFSIZE);
            httpc->pos = 0;
            httpc->len = 0;
        }
    }

    if (httpc->substate==SSTATE_EOF) {
        if (httpc->pos==0 && httpc->len==0) {
            httpc->state = CSTATE_DONE;
            httpc->substate = SSTATE_DONE;
        }
    }

    return rc;
}

static int
ssi_file_read(HTTPC *httpc) 
{
    int     len 	= 0;
    int		ssilen  = 0;
    int		ssicnt	= 0;
    int		newline = 1;
    int     avail	= CBUFSIZE - httpc->len;
	char	*pbuf 	= NULL;
	char	*ssi	= NULL;
	char	*end	= NULL;
	char	*p;
	char	tmp[256];

	// wtof("%s: enter", __func__);
	
    if (httpc->substate != SSTATE_SENDING) goto quit;

	// wtof("%s: SSTATE_SENDING", __func__);

	/* do we have any room in our buffer? */
	if (avail <= (sizeof(tmp) * 2)) {
		ssi_send_buffer(httpc);
		avail	= CBUFSIZE - httpc->len;
	}

	if (avail <= (sizeof(tmp) * 2)) goto quit;

	// wtof("%s: fgets()", __func__);
	
	/* read one line from the file */
	if (httpc->fp) {
		/* use CLIB version of fgets() */
		pbuf = fgets(tmp, sizeof(tmp)-1, httpc->fp);
	}
	else if (httpc->ufp) {
		/* use UFS version of fgets() */
		pbuf = ufs_fgets(tmp, sizeof(tmp)-1, httpc->ufp);
	}
			
	if (!pbuf) goto checkeof;

	tmp[sizeof(tmp)-1] = 0;	/* just in case */

	/* strip the newline from the tmp buffer */
	p = strrchr(tmp, '\n');
	if (p) *p = 0;

	len = strlen(tmp);
	if (len==0) {
		ssi_buffer(httpc, "\n", 1);
		goto quit;
	}

again:
	// wtodumpf(tmp, len, "%s: again", __func__);

	/* look for start of Server Side Include */
	ssilen = 0;
	end = NULL;
	ssi = strstr(tmp, "<!--#");
	if (ssi) end = strstr(ssi+5, " -->");
	if (end) {
		end += 4;
		ssilen = (int)(end - ssi);
	}

	if (ssilen > 0) {
		char	c;
		
		/* copy anything before the Server Side Include */
		len = ssi - tmp;
		// wtof("%s: is SSI ssilen=%d, len=%d", __func__, ssilen, len);
		if (len > 0) {
			ssi_buffer(httpc, tmp, len);
		}
				
		c = ssi[ssilen];
		ssi[ssilen] = 0;
		// wtodumpf(ssi, ssilen, "%s: ssi", __func__);
		/* at this point ssi *should* look like:
		 * "<!--#echo var="DOCUMENT_NAME" -->" or
		 * "<!--#include virtual="some path name" -->" or
		 *  some other "<!--#xxxx ... -->" style string.
		 */
		// wtof("%s: ssi=\"%s\"", __func__, ssi);
		len = ssi_process(httpc, ssi);
		if (!ssicnt) newline = len;
		ssicnt++;
		ssi[ssilen] = c;

		/* see if we have any other Server Side Includes */
		end = &ssi[ssilen];
		strcpy(tmp, end);
		len = strlen(tmp);
		if (!len) goto quit;

		// wtof("%s: goto again", __func__);
		goto again;
	}

	/* Not a Server Side Include */
	// wtof("%s: not SSI \"%s\"", __func__, tmp);
	if (newline) strcat(tmp, "\n");
	if (strlen(tmp)) {
		ssi_printf(httpc, "%s", tmp);
	}
	goto quit;

checkeof:
	/* check for end-of-file */
	// wtof("%s: checkeof", __func__);
	if (httpc->fp) {
		if (feof(httpc->fp)) {
			httpc->substate = SSTATE_EOF;
		}
	}
	else if (httpc->ufp) {
		if (ufs_feof(httpc->ufp)) {
			httpc->substate = SSTATE_EOF;
		}
	}

quit:
	// wtof("%s: exit", __func__);
	return 0;
}

/* process a Server Side Include request */
static int
ssi_process(HTTPC *httpc, char *ssi)
{
    int     rc  		= 0;
    int     len 		= 0;
    int		newline		= 1;
    char	*p			= NULL;
    char	*next		= NULL;
    char	save[256];

	// wtof("%s: enter ssi=\"%s\"", __func__, ssi);

	/* on entry we *should* have ssi pointing to "<!--#.... -->" string */
	strncpy(save, ssi, sizeof(save));
	save[sizeof(save)-1] = 0;

    p = &ssi[5];				/* skip over "<!--#" */
	while (isspace(*p)) p++;	/* skip any white space */
	if (!*p) goto invalid;

	p = strtok(p, " ");			/* isolate the ssi request */
	if (!p) goto invalid;
	
	next = strtok(NULL,"");		/* remember whatever follows the request */
	if (!next) goto invalid;

	if (http_cmp(p, "echo")==0) {
		/* next *should* point to whatever follows "<!--#echo " */
		ssi_echo(httpc, next);
	}
	else if (http_cmp(p, "include")==0) {
		/* next *should* point to whatever follows "<!--#include " */
		ssi_include(httpc, next);
		newline = 0;
	}
	else if (http_cmp(p, "printenv")==0) {
		/* next *should* point to whatever follows "<!--#printenv " */
		ssi_printenv(httpc);
	}
	else {
		/* unknown request */
		goto invalid;
	}

	goto quit;

invalid:
	/* put the saved inout into the output buffer */
	ssi_printf(httpc, "%s", save);

quit:

	return newline;
}

static int
ssi_send_buffer(HTTPC *httpc)
{
	int		len		= 0;
	int		avail;
	
	// wtof("%s: enter pos=%d len=%d", __func__, httpc->pos, httpc->len);
	
    if (httpc->pos < httpc->len) {
        /* we have unsent data in buffer */
        len = httpc->len - httpc->pos;
        if (len > 0) {
            len = http_send(httpc, &httpc->buf[httpc->pos], len);
            if (len>0) {
                httpc->pos += len;
            }
        }
    }

    if (httpc->pos>0) {
        len = httpc->len - httpc->pos;
        if (len>0) {
            /* shift data left */
            memcpy(httpc->buf, &httpc->buf[httpc->pos], len);
            /* clear remaining buffer */
            avail = CBUFSIZE - httpc->len;
            memset(&httpc->buf[len], 0, avail);
            /* update data position and length */
            httpc->pos -= len;
            httpc->len -= len;
        }
        else {
            /* reset buffer */
            memset(httpc->buf, 0, CBUFSIZE);
            httpc->pos = 0;
            httpc->len = 0;
        }
    }

	// wtof("%s: exit httpc->len=%d", __func__, httpc->len);
	
	return len;
}

/* <!--#echo var="DOCUMENT_NAME" --> */
static int 
ssi_echo(HTTPC *httpc, char *next)
{
	char	*p		= NULL;
	char	*var	= NULL;

	// wtof("%s: enter next=\"%s\"", __func__, next);

	while(isspace(*next)) next++;
	
	p = strchr(next, '=');
	if (!p) goto bad;
	*p++ = 0;

	if (http_cmp(next, "var")!=0) goto bad;

	while(isspace(*p)) p++;
	
	var = strtok(p, "\"\'");
	if (!var) goto quit;
	
	while(isspace(*var)) var++;

	p = http_get_env(httpc, var);
	if (!p) p = getenv(var);
	if (p) {
		// wtof("%s: result \"%s\"", __func__, p);
		ssi_printf(httpc, "%s", p);
		goto quit;
	}

	ssi_printf(httpc, "<!-- Oops: we didn't find var=\"%s\" -->\n", var);
	goto quit;

bad:
	ssi_printf(httpc, "<!-- Unable to parse 'echo' request -->\n");

quit:
	// wtof("%s: exit rc=0", __func__);
	return 0;
}

static int 
ssi_printenv(HTTPC *httpc)
{
    unsigned    indx    = 0;
    HTTPV       *v      = NULL;
    unsigned    count   = array_count(&httpc->env);
    unsigned    n;
    
    ssi_printf(httpc, "\n<h3>Server Variables</h3>\n");
    ssi_printf(httpc, "\n<table>\n");
    ssi_printf(httpc, " <tr><th>Variable</th><th>Value</th></tr>\n");
    for(n=0; n<count; n++) {
        v = httpc->env[n];
        if (!v) continue;
        
        ssi_printf(httpc, " <tr><td>%s</td><td>%s</td></tr>\n", v->name, v->value);
    }
    ssi_printf(httpc, "</table>\n");

	ssi_system_env(httpc);
	
	return 0;
}

static int
ssi_system_env(HTTPC *httpc)
{
    CLIBGRT     *grt    = __grtget();
    int			lockrc	= -1;
    unsigned	n;
    unsigned	count;
	__ENVVAR    *v;

	if (!grt) goto quit;
	
    lockrc = lock(&grt->grtenv,LOCK_SHR);

    ssi_printf(httpc, "\n<h3>System Variables</h3>\n");
    ssi_printf(httpc, "<table>\n");
    ssi_printf(httpc, " <tr><th>Variable</th><th>Value</th></tr>\n");

	count = array_count(&grt->grtenv);
	for(n=0; n < count; n++) {
		v = grt->grtenv[n];
		if (!v) continue;
		
        ssi_printf(httpc, " <tr><td>%s</td><td>%s</td></tr>\n", v->name, v->value);
	}

    ssi_printf(httpc, "</table>\n");

quit:
    if (lockrc==0) unlock(&grt->grtenv,0);
    return 0;

}

static int
ssi_include(HTTPC *httpc, char *next)
{
	int			isfile		= 0;
	int			isvirtual	= 0;
	char		*p			= NULL;
	char		*path		= NULL;
    const HTTPM	*mime		= NULL;
    void		*oldfp		= httpc->fp;
    void		*oldufp 	= httpc->ufp;
    short   	state		= httpc->state;
    UCHAR   	subtype		= httpc->subtype;
    UCHAR		substate	= httpc->substate;
    UCHAR		ssi			= httpc->ssi;
    char		uri[UFS_PATH_MAX];

	// wtof("%s: enter next=\"%s\"", __func__, next);

	httpc->fp	= NULL;
	httpc->ufp	= NULL;

	if (httpc->ssilevel < SSI_LEVEL_MAX) {
		httpc->ssilevel++;
	}
	else {
		ssi_printf(httpc, "<!-- Oops: Maximum Server Side Include levels exceeded -->\n");
		goto quit;
	}

	while(isspace(*next)) next++;
	
	p = strchr(next, '=');
	if (!p) goto bad;
	*p++ = 0;

	if (http_cmp(next, "file")==0) 		isfile = 1;
	if (http_cmp(next, "virtual")==0) 	isvirtual = 1;

	/* we're going to treat virtual and file the same way for now */
	if (!isvirtual && !isfile) goto bad;

	while(isspace(*p)) p++;
	
	path = strtok(p, "\"\'");
	if (!path) goto bad;
	
	while(isspace(*path)) path++;

	/* save the path as uri in case of failure to open path */
	strncpy(uri, path, sizeof(uri));
	uri[sizeof(uri)-1] = 0;

    mime = http_mime(path);
    if (mime->binary) {
        ssi_printf(httpc, "<!-- Oops: we can't include a binary file \"%s\" -->\n", uri);
        goto quit;
    }

    httpc->fp = http_open(httpc, path, mime);
    if (!httpc->fp && !httpc->ufp) {
        ssi_printf(httpc, "<!-- Oops: we didn't find \"%s\" -->\n", uri);
        goto quit;
    }

okay:
	// ssi_printf(httpc, "\n");
	
	httpc->state = CSTATE_GET;
	httpc->substate = SSTATE_SENDING;
	
	/* processing the file */
	while(httpc->state != CSTATE_DONE) {
		http_send_file(httpc, 0);
	}
	
	goto quit;

bad:
	ssi_printf(httpc, "<!-- Unable to parse 'include' request -->\n");

quit:
	if (httpc->fp) {
		fclose(httpc->fp);
		httpc->fp = NULL;
	}
	if (httpc->ufp) {
		ufs_fclose(&httpc->ufp);
		httpc->ufp = NULL;
	}
	
    httpc->fp			= oldfp;
    httpc->ufp			= oldufp;
    httpc->state		= state;
    httpc->subtype		= subtype;
    httpc->substate		= substate;
    httpc->ssi			= ssi;
    if (httpc->ssilevel > 0) httpc->ssilevel--;

	// wtof("%s: exit rc=0", __func__);
	return 0;
}

static int
ssi_printf(HTTPC *httpc,  const char *fmt, ... )
{
    int     rc  = 0;
    va_list args;

    va_start( args, fmt );
    rc = ssi_printv(httpc, fmt, args);
    va_end( args );

    return rc;
}

static int
ssi_printv(HTTPC *httpc, const char *fmt, va_list args)
{
    int     rc      = 0;
    int     len;
    char	buf[4096];

	/* Okay here we go. Format the string into the buffer */
    len = vsprintf(buf, fmt, args);
    if (len < 0) {
		wtof("%s: looks like a bug to me, len=%d, httpc->len=%d", 
			__func__, len, httpc->len);
        rc = -1;
    }
    else {
		ssi_buffer(httpc, buf, len);
    }

quit:
    return rc;
}

static int
ssi_buffer(HTTPC *httpc, const char *buf, int len)
{
	int		avail;
	
	if (httpc->len >= (CBUFSIZE - len)) {
		/* try to send what we have in the buffer */
		ssi_send_buffer(httpc);
	}

	if (len > 0) {
		avail = CBUFSIZE - httpc->len;
		if (len < avail) {
			memcpy(&httpc->buf[httpc->len], buf, len);
			/* SSI files are served from UFS (IBM-1047) */
			http_xlate(&httpc->buf[httpc->len], len,
			           httpx->xlate_1047->etoa);
			httpc->len += len;
		}
	}
	
	return 0;
}
