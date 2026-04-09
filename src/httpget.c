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

extern int
httpget(HTTPC *httpc)
{
    int         rc      = 0;
    UCHAR       *path;
    UCHAR       *open_path = NULL;
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

    /* try to open path from UFS */
    mime = http_mime(path);
    open_path = path;
    fp = http_open(httpc, path, mime);
    if (fp || httpc->ufp) goto okay;

    /* If the path is for a directory, try index.html / default.html */
    len = strlen(path);
    if (path[len-1]=='/') {
        memcpy(buf, path, len);
        strcpy(&buf[len], "index.html");
        mime = http_mime(buf);
        open_path = buf;
        fp = http_open(httpc, buf, mime);
        if (fp || httpc->ufp) goto okay;

        strcpy(&buf[len], "default.html");
        mime = http_mime(buf);
        open_path = buf;
        fp = http_open(httpc, buf, mime);
        if (fp || httpc->ufp) goto okay;
    }

    /* file not found */
    rc = http_resp_not_found(httpc, path);
    httpc->state = CSTATE_DONE;
    goto quit;

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
		__patmat(path, "*.html")) {
		rc = http_printf(httpc, "Cache-Control: no-store\r\n");
		if (rc) goto die;
	}
#endif
    rc = http_printf(httpc, "Content-Type: %s\r\n", mime->type);
    if (rc) goto die;

    /* Content-Length for static UFS files, chunked for SSI */
    if (!httpc->ssi && httpc->ufp) {
        UFS *ufs = http_get_ufs(httpc);
        if (ufs) {
            UFSDLIST st;
            UCHAR ufspath[256];
            const char *dr = httpc->httpd->docroot;
            if (dr[0]) {
                snprintf((char *)ufspath, sizeof(ufspath), "%s%s",
                         dr, open_path);
            } else {
                snprintf((char *)ufspath, sizeof(ufspath), "%s", open_path);
            }
            if (ufs_stat(ufs, (const char *)ufspath, &st) == 0
                && st.filesize > 0) {
                rc = http_printf(httpc, "Content-Length: %u\r\n",
                                 st.filesize);
                if (rc) goto die;
                httpc->content_length_set = 1;
            }
        }
    }

    /* chunked transfer encoding only for HTTP/1.1 clients */
    {
        int use_chunked = 0;
        if (!httpc->content_length_set) {
            UCHAR *ver = http_get_env(httpc, "REQUEST_VERSION");
            if (ver && http_cmp(ver, "HTTP/1.1") == 0) {
                rc = http_printf(httpc, "Transfer-Encoding: chunked\r\n");
                if (rc) goto die;
                use_chunked = 1;
            }
            /* HTTP/1.0: no chunked, body delimited by Connection: close */
        }

        rc = http_printf(httpc, "\r\n");
        if (rc) goto die;

        /* enable chunk framing AFTER header-ending CRLF is sent */
        httpc->chunked = use_chunked;
    }

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
