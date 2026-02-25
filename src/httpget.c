/* HTTPGET.C
** Process GET request
** Transitions to next state as needed.
*/
#include "httpd.h"

typedef enum ctype  CTYPE;
enum ctype {
    CTYPE_UNKNOWN=0,
    CTYPE_BINARY,
    CTYPE_TEXT
};

/* ddname() convert "/index.html" -> "/DD:html(index)" */
static UCHAR *
ddname(const UCHAR *in, UCHAR *out, int size)
{
    UCHAR   *ext    = strchr(in,'.');
    UCHAR   *mem    = strchr(in,'/');
    int     extlen  = ext ? strlen(ext+1) : 0;
    int     memlen  = (mem && ext) ? (int)(ext-mem) - 1 : 0;
    int     pos     = 0;
    int     len;

    /* we need space for "/DD:_ddname_(_member_)" */
    if (size < sizeof("/DD:_ddname_(_member_)")) goto quit;

    if (extlen < 0 OR memlen < 0 OR strchr(in, ':')) {
        /* doesn't look like a normal "file.ext" name */
        out = NULL;
        goto quit;
    }

    if (extlen > 8) extlen = 8; /* limit ddname to 8 characters */
    if (memlen > 8) memlen = 8; /* limit member name to 8 characters */

    strcpy(out, "/DD:");
    pos += 4;

    for(ext++;(*ext) && (extlen > 0); extlen--) {
        out[pos++]=*ext++;
    }
    out[pos++] = '(';
    for(mem++;(*mem) && (*mem!='.') && (memlen > 0); memlen--) {
        out[pos++]=*mem++;
    }
    out[pos++] = ')';
    out[pos] = 0;

quit:
    return out;
}

extern int
httpget(HTTPC *httpc)
{
    int         rc      = 0;
    UCHAR       *path;
    const HTTPM *mime;
    FILE        *fp;
    int 		len;
    UCHAR       buf[300];

    http_enter("httpget()\n");
    // wtof("%s: enter", __func__);

    if (httpc->subtype!=CTYPE_UNKNOWN) goto known;

    /* first time here, get the mime for this document */
    path = http_get_env(httpc, "REQUEST_PATH");
    http_dbgf("Path:\"%s\"\n", path);

    // wtof("%s: path=\"%s\"", __func__, path);

    if (http_cmp(path, "/abend")==0) {
        __asm__("DC\tH'0'");
    }

    if (httpc->ufs) {
        /* try to open path asis */
        mime = http_mime(path);
        fp = http_open(httpc, path, mime);
		// wtof("%s: 1 http_open(%p, \"%s\", %p) fp=%p, httpc->ufp=%p", 
		// 	__func__, httpc, path, mime, fp, httpc->ufp);
        if (fp || httpc->ufp) goto okay;   /* file was opened */
        
        /* If the path is for a directory then we need to see if a 
         * "index.html" exist in this directory and open it.
         */
        len = strlen(path);
        if (path[len-1]=='/') {
			memcpy(buf, path, len);
			strcpy(&buf[len], "index.html");

			mime = http_mime(buf);
			fp = http_open(httpc, buf, mime);
        
			if (fp || httpc->ufp) goto okay;   /* file was opened */
		}
    }

    if (http_cmpn(path,"/DD:",4)!=0 &&
        strchr(path,'.') && strlen(path) < 19) {

        /* convert the path to a "/DD:ddname(member)" */
        if (ddname(path, buf, sizeof(buf))) {
            path = buf;
            http_dbgf("ddname:\"%s\"\n", path);
        }
    }
    mime = http_mime(path);

    /* try to open the requested document */
    fp = http_open(httpc, path, mime);
	// wtof("%s: 2 http_open(%p, \"%s\", %p) fp=%p, httpc->ufp=%p", 
	// 	__func__, httpc, path, mime, fp, httpc->ufp);
    if (!fp && !httpc->ufp && http_cmp(path,"/")==0) {
        /* try to open default documents */
        path = "/index.html";
        mime = http_mime(path);
        fp = http_open(httpc, path, mime);
        if (!fp && !httpc->ufp) {
            path = "/default.html";
            mime = http_mime(path);
            fp = http_open(httpc, path, mime);
        }
        if (!fp && !httpc->ufp) {
            path = "/DD:HTML(INDEX)";
            mime = http_mime(path);
            fp = http_open(httpc, path, mime);
        }
        if (!fp && !httpc->ufp) {
            /* try another one */
            path = "/DD:HTML(DEFAULT)";
            fp = http_open(httpc, path, mime);
        }
    }

    if (!fp && !httpc->ufp) {
        rc = http_resp_not_found(httpc, path);
        httpc->state = CSTATE_DONE;
        goto quit;
    }

okay:
    httpc->fp = fp;
    httpc->len = 0;
    httpc->pos = 0;
    httpc->sent = 0;

    /* Successful request */
    rc = http_resp(httpc,200);
    if (rc) goto die;
#if 1
	/* don't allow the browser to cache our html documents */
	if (strstr(mime->type, "html") || 
		__patmat(path, "*.html") || 
		__patmat(path, "/DD:HTML(*)")) {
		rc = http_printf(httpc, "Cache-Control: no-store\r\n");
		if (rc) goto die;
	}
#endif
    rc = http_printf(httpc, "Content-Type: %s\r\n", mime->type);
    if (rc) goto die;
    rc = http_printf(httpc, "\r\n");
    if (rc) goto die;

    /* indicate type of document being sent */
    if (mime->binary) {
        httpc->subtype=CTYPE_BINARY;
    }
    else {
        httpc->subtype=CTYPE_TEXT;
    }

known:
    switch(httpc->subtype) {
    case CTYPE_BINARY:
        rc = http_send_binary(httpc);
        break;
    case CTYPE_TEXT:
        rc = http_send_text(httpc);
        break;
    default:
        wtof("Unexpected subtype %d in httpget()", httpc->subtype);
        rc = http_resp_internal_error(httpc);
        if (!rc) {
            rc = http_printf(httpc,
                "Unexpected subtype %d in httpget()", httpc->subtype);
        }
        httpc->state = CSTATE_DONE;
        break;
    }
    goto quit;

die:
    httpc->state = CSTATE_CLOSE;

quit:
    http_exit("httpget()\n");
    return rc;
}
