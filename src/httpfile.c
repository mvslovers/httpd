/* HTTPFILE.C
** Send file, file handle is already open.
*/
#include "httpd.h"
#include "httpdmsg.h"
#include "httpgive.h"
#include "httpssi.h"
#include <time.h>      /* time() */

typedef enum sstate     SSTATE;
enum sstate {
    SSTATE_SENDING=0,
    SSTATE_EOF,
    SSTATE_DONE,
    SSTATE_ERROR
};

/* forward reference static function prototypes */
static int file_send(HTTPC *httpc, int binary, const char *path);
static int ufs_read_failed(HTTPC *httpc, const char *path);
static int ssi_file_read(HTTPC *httpc, const char *path);
static int ssi_process(HTTPC *httpc, char *ssi);
static int ssi_echo(HTTPC *httpc, char *next);
static int ssi_include(HTTPC *httpc, char *next);
static int ssi_printenv(HTTPC *httpc);

/* output handlers */
static int ssi_printf(HTTPC *httpc,  const char *fmt, ... );
static int ssi_printv(HTTPC *httpc, const char *fmt, va_list args);
static int ssi_text(HTTPC *httpc, const char *text);
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
    return file_send(httpc, binary, NULL);
}

/* path names the file being read for HTTPD045E; NULL is the requested
 * page, a path is a file pulled in by <!--#include --> */
static int
file_send(HTTPC *httpc, int binary, const char *path)
{
    int     rc  = 0;
    int     len = 0;
    int     avail;

	if (httpc->ssi) {
		ssi_file_read(httpc, path);
		if (httpc->substate == SSTATE_ERROR) return rc;
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

            /* ufs_fread() returns what it got before the error, or 0
             * with neither EOF nor progress: either is a failed read,
             * whatever len says */
            if (httpc->ufp && ufs_read_failed(httpc, path)) return rc;

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

/* A UFS read error ends the response here, deliberately.  The status
 * line and headers are long gone by the time the first read happens, so
 * nothing can be corrected on the wire: the one honest signal left is a
 * body that does not end.  Unsent bytes are dropped, framing is switched
 * off so httpdone() writes no "0" chunk, and the connection is closed
 * (RFC 7230 3.3.3).  Without this the state never advanced and
 * serve_client() reported the stall as HTTPD901E, a client fault, 1000
 * passes later (#263).  Returns 1 when the file has failed. */
static int
ufs_read_failed(HTTPC *httpc, const char *path)
{
    HTTPD   *httpd  = httpc->httpd;
    int     err     = ufs_ferror(httpc->ufp);
    unsigned held   = 0;
    int     report;
    int     lockrc;

    if (!err) return 0;

    if (!path) path = (const char *)http_get_env(httpc, "REQUEST_PATH");

    /* One line per READERR_INTERVAL: a file on a failing volume that is
     * requested again and again must not flood the console.  The counters
     * sit in the HTTPD block, not in a static (the module may be key 0),
     * and are guarded like the give-up counters in httpsend.c. */
    lockrc = lock(&httpd->readerr_last, LOCK_EXC);
    report = httpgive(&httpd->readerr_last, &httpd->readerr_held,
                      (unsigned)time(NULL), READERR_INTERVAL, &held);
    if (lockrc == 0) unlock(&httpd->readerr_last, LOCK_EXC);

    if (report)
        wtof(MSG_UFS_READ_ERROR, err, path ? path : "", held);

    httpc->chunked   = 0;
    httpc->keepalive = 0;
    httpc->len       = 0;
    httpc->pos       = 0;
    httpc->substate  = SSTATE_ERROR;
    httpc->state     = CSTATE_DONE;
    return 1;
}

static int
ssi_file_read(HTTPC *httpc, const char *path)
{
    int     len 	= 0;
    int		ssilen  = 0;
    int		ssicnt	= 0;
    int		newline = 1;
    int     avail	= CBUFSIZE - httpc->len;
    int		keep	= 0;	/* bytes at the start of tmp held from the last piece */
    int		eol		= 0;	/* the piece ended the line */
    int		more	= 0;	/* the last piece did not end the line */
    int		lastrem	= 0;	/* text left after the last directive of that piece */
    size_t	hold;
	char	*pbuf 	= NULL;
	char	*cur	= NULL;
	char	*ssi	= NULL;
	char	*end	= NULL;
	char	*p;
	char	c;
	char	tmp[512];

	// wtof("%s: enter", __func__);
	
    if (httpc->substate != SSTATE_SENDING) goto quit;

	// wtof("%s: SSTATE_SENDING", __func__);

	/* do we have any room in our buffer? */
	if (avail <= (int)(sizeof(tmp) * 2)) {
		ssi_send_buffer(httpc);
		avail	= CBUFSIZE - httpc->len;
	}

	if (avail <= (int)(sizeof(tmp) * 2)) goto quit;

	// wtof("%s: fgets()", __func__);

readmore:
	/* the next piece of a line needs the same room as the first one. If
	 * there is none, what is held goes out as text and the rest of the
	 * line is read on the next call: a directive split there is not
	 * recognised, but nothing is lost */
	if (more) {
		avail = CBUFSIZE - httpc->len;
		if (avail <= (int)(sizeof(tmp) * 2)) {
			ssi_send_buffer(httpc);
			avail = CBUFSIZE - httpc->len;
		}
		if (avail <= (int)(sizeof(tmp) * 2)) {
			if (keep > 0) ssi_buffer(httpc, tmp, keep);
			goto quit;
		}
	}

	/* read the line, or the next piece of a line longer than tmp: a piece
	 * that does not end in a newline is followed by more of the same line */
	if (httpc->fp) {
		/* use CLIB version of fgets() */
		pbuf = fgets(&tmp[keep], sizeof(tmp)-keep, httpc->fp);
	}
	else if (httpc->ufp) {
		/* use UFS version of fgets() */
		pbuf = ufs_fgets(&tmp[keep], sizeof(tmp)-keep, httpc->ufp);
	}
			
	if (!pbuf) {
		/* what was held back is not a directive: it goes out as text,
		 * and a line that ran into the end of the file gets its newline */
		if (keep > 0) {
			ssi_buffer(httpc, tmp, keep);
			lastrem = keep;
		}
		if (more && lastrem > 0 && newline) ssi_buffer(httpc, "\n", 1);
		goto checkeof;
	}

	tmp[sizeof(tmp)-1] = 0;	/* just in case */

	/* strip the newline from the tmp buffer */
	p = strrchr(tmp, '\n');
	eol = (p != NULL);
	if (p) *p = 0;

	len = strlen(tmp);
	if (len==0) {
		/* an empty line, or the newline that ends a long one -- which, as
		 * for a line that ends in a directive, is not written when the
		 * piece before it ended with one */
		if (newline && (!more || lastrem > 0)) ssi_buffer(httpc, "\n", 1);
		goto quit;
	}

	/* a piece that does not end the line may end in the middle of a
	 * directive: keep that part back and join it with the next piece */
	hold = len;
	if (!eol) hold = httpssih(tmp, len, SSI_HOLDMAX);
	c = tmp[hold];
	tmp[hold] = 0;
	cur = tmp;
	len = (int)hold;

again:
	/* look for start of Server Side Include */
	ssilen = 0;
	end = NULL;
	ssi = strstr(cur, "<!--#");
	if (ssi) end = strstr(ssi+5, " -->");
	if (end) {
		end += 4;
		ssilen = (int)(end - ssi);
	}

	if (ssilen > 0) {
		char	sc;
		
		/* copy anything before the Server Side Include */
		len = ssi - cur;
		// wtof("%s: is SSI ssilen=%d, len=%d", __func__, ssilen, len);
		if (len > 0) {
			ssi_buffer(httpc, cur, len);
		}
				
		sc = ssi[ssilen];
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
		ssi[ssilen] = sc;

		/* see if we have any other Server Side Includes */
		cur = &ssi[ssilen];
		len = strlen(cur);
		if (!len) goto piecedone;

		// wtof("%s: goto again", __func__);
		goto again;
	}

	/* Not a Server Side Include */
	// wtof("%s: not SSI \"%s\"", __func__, cur);
	if (len > 0) ssi_buffer(httpc, cur, len);

piecedone:
	lastrem = len;
	tmp[hold] = c;
	if (eol) {
		if (len > 0 && newline) ssi_buffer(httpc, "\n", 1);
		goto quit;
	}

	/* the line goes on: move the held part to the front and read more */
	more = 1;
	keep = (int)strlen(&tmp[hold]);
	memmove(tmp, &tmp[hold], keep + 1);
	goto readmore;

checkeof:
	/* check for end-of-file */
	// wtof("%s: checkeof", __func__);
	if (httpc->fp) {
		if (feof(httpc->fp)) {
			httpc->substate = SSTATE_EOF;
		}
	}
	else if (httpc->ufp) {
		if (ufs_read_failed(httpc, path)) goto quit;
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
    int		newline		= 1;
    char	*p			= NULL;
    char	*next		= NULL;
    char	save[256];

	// wtof("%s: enter ssi=\"%s\"", __func__, ssi);

	/* on entry we *should* have ssi pointing to "<!--#.... -->" string */
	strncpy(save, ssi, sizeof(save));
	save[sizeof(save)-1] = 0;

    p = &ssi[5];				/* skip over "<!--#" */
	while (isspace((unsigned char)*p)) p++;	/* skip any white space */
	if (!*p) goto invalid;

	/* isolate the ssi request (split at first space) */
	next = p;
	while (*next && *next != ' ') next++;
	if (!*next) goto invalid;
	*next++ = 0;
	while (isspace((unsigned char)*next)) next++;
	if (!*next) goto invalid;

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

	/* the response has been abandoned (ufs_read_failed()); framing is off,
	 * so whatever is still buffered must not reach the client raw */
	if (httpc->substate == SSTATE_ERROR) return 0;
	
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

	while(isspace((unsigned char)*next)) next++;
	
	p = strchr(next, '=');
	if (!p) goto bad;
	*p++ = 0;

	if (http_cmp(next, "var")!=0) goto bad;

	while(isspace((unsigned char)*p)) p++;
	
	/* extract quoted value — skip opening quote */
	if (*p == '"' || *p == '\'') p++;
	var = p;
	while (*p && *p != '"' && *p != '\'') p++;
	*p = 0;
	if (!var || !*var) goto quit;
	
	while(isspace((unsigned char)*var)) var++;

	p = http_get_env(httpc, var);
	if (!p) p = getenv(var);
	if (p) {
		// wtof("%s: result \"%s\"", __func__, p);
		ssi_text(httpc, p);
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

/* <!--#printenv --> lists the variables of this request, nothing of the
 * server's own environment. Names and values come from the client, so
 * both are written as HTML text. */
static int 
ssi_printenv(HTTPC *httpc)
{
    unsigned    count   = array_count(&httpc->env);
    
    ssi_printf(httpc, "\n<h3>Server Variables</h3>\n");
    ssi_printf(httpc, "\n<table>\n");
    ssi_printf(httpc, " <tr><th>Variable</th><th>Value</th></tr>\n");
    for(unsigned n=0; n<count; n++) {
        const HTTPV *v = httpc->env[n];

        ssi_printf(httpc, " <tr><td>");
        ssi_text(httpc, v->name);
        ssi_printf(httpc, "</td><td>");
        ssi_text(httpc, v->value);
        ssi_printf(httpc, "</td></tr>\n");
    }
    ssi_printf(httpc, "</table>\n");

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
    int			failed		= 0;
    char		uri[UFS_PATH_MAX];

	/* an earlier include on this line already failed the response: it is
	 * unframed now, so nothing more may be written into it */
	if (httpc->substate == SSTATE_ERROR) return 0;

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

	while(isspace((unsigned char)*next)) next++;
	
	p = strchr(next, '=');
	if (!p) goto bad;
	*p++ = 0;

	if (http_cmp(next, "file")==0) 		isfile = 1;
	if (http_cmp(next, "virtual")==0) 	isvirtual = 1;

	/* we're going to treat virtual and file the same way for now */
	if (!isvirtual && !isfile) goto bad;

	while(isspace((unsigned char)*p)) p++;
	
	/* extract quoted path — skip opening quote */
	if (*p == '"' || *p == '\'') p++;
	path = p;
	while (*p && *p != '"' && *p != '\'') p++;
	*p = 0;
	if (!*path) goto bad;
	
	while(isspace((unsigned char)*path)) path++;

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

	// ssi_printf(httpc, "\n");
	
	httpc->state = CSTATE_GET;
	httpc->substate = SSTATE_SENDING;
	
	/* processing the file */
	while(httpc->state != CSTATE_DONE) {
		file_send(httpc, 0, uri);
	}
	failed = (httpc->substate == SSTATE_ERROR);
	
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

	/* a read error in the included file ends this response too; the
	 * restore above would otherwise resume the outer file */
	if (failed) {
		httpc->substate	= SSTATE_ERROR;
		httpc->state	= CSTATE_DONE;
	}

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

	/* Okay here we go. Format the string into the buffer.  vsnprintf (not
	   vsprintf): an SSI line built from an over-long env value -- e.g. an
	   attacker-controlled HTTP_* header echoed by ssi_printenv/ssi_echo --
	   could exceed buf[].  vsnprintf returns the would-be length, so clamp it
	   before ssi_buffer() copies it (mirrors http_printv() in httpprtv.c). */
    len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len >= (int)sizeof(buf))
        len = sizeof(buf) - 1;
    if (len < 0) {
		wtof(MSG_FILE_LENGTH, __func__, len, httpc->len);
        rc = -1;
    }
    else {
		ssi_buffer(httpc, buf, len);
    }

    return rc;
}

/* write a value as HTML text: it may come from the client */
static int
ssi_text(HTTPC *httpc, const char *text)
{
	char	buf[128];
	size_t	len;

	if (!text) return 0;
	while (*text) {
		len = httpssie(buf, sizeof(buf), &text);
		if (!len) break;
		ssi_buffer(httpc, buf, (int)len);
	}
	return 0;
}

static int
ssi_buffer(HTTPC *httpc, const char *buf, int len)
{
	int		avail;
	
	if ((int)httpc->len >= (CBUFSIZE - len)) {
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
