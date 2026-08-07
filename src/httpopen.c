/* HTTPOPEN.C
** Open path for reading
*/
#include "httpd.h"

#undef http_open
FILE *
http_open(HTTPC *httpc, const UCHAR *path, const HTTPM *mime)
{
    /* Initialized because mode is only assigned inside the if (mime) below and
       -Wall cannot prove that branch always runs.  It does: http_mime() never
       answers NULL -- an extension it does not know falls back to default_mime
       {"txt","text/plain",0}, i.e. exactly this "r".  Defensive, not a fix. */
    UCHAR   *mode = "r";
    int     len;
    UCHAR   buf[256];

    if (!path) goto quit;

    /* reject path traversal attempts */
    if (strstr(path, "..")) goto quit;

    len = strlen(path);
    if (len >= sizeof(buf)) len = sizeof(buf)-1;

    memcpy(buf, path, len);
    buf[len]=0;

    if (!mime) mime = (const HTTPM *) http_mime(buf);

    if (mime) {
		if (mime->binary) {
			mode = "rb";
		}
		else {
			mode = "r";
		}

		if (http_cmp(mime->type, "text/html")==0 ||
			http_cmp(mime->type, "text/x-server-parsed-html")==0) {
			httpc->ssi = 1;
		}
		else {
			httpc->ssi = 0;
		}
	}

    {
        UFS *ufs = http_get_ufs(httpc);
        if (ufs) {
            UCHAR ufspath[256];
            const char *dr = httpc->httpd->docroot;
            if (dr[0]) {
                snprintf((char *)ufspath, sizeof(ufspath), "%s%s", dr, buf);
                httpc->ufp = ufs_fopen(ufs, ufspath, mode);
            } else {
                httpc->ufp = ufs_fopen(ufs, buf, mode);
            }
        }
    }

quit:
    return NULL;
}
