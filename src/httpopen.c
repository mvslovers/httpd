/* HTTPOPEN.C
** Open path for reading
*/
#include "httpd.h"

#undef http_open
FILE *
http_open(HTTPC *httpc, const UCHAR *path, const HTTPM *mime)
{
    FILE    *fp     = NULL;
    UCHAR   *mode;
    UCHAR   *p;
    int     len;
    int     lock;
    UCHAR   ddname[12] = "";
    UCHAR   member[12] = "";
    UCHAR   buf[256];

	// wtof("%s: enter path=\"%s\"", __func__, path);

    if (!path) goto quit;

    /* reject path traversal attempts */
    if (strstr(path, "..")) goto quit;

    len = strlen(path);
    if (len >= sizeof(buf)) len = sizeof(buf)-1;

    memcpy(buf, path, len);
    buf[len]=0;

    /* wtof("%s buf=\"%s\"", __func__, buf); */

    if (http_cmpn(buf, "/DD:", 4)==0) {
        /* open by DD name */
        strcpy(buf, buf+1); /* strip leading '/' */

        /* Ideally we would expect the path to be "/DD:EXT(MEMBER)"
        ** but we'd also like to handle "/DD:MEMBER.EXT" and convert
        ** that internally to "/DD:EXT(MEMBER)".
        **
        ** The EXT part of the name will be used to lookup the MIME
        ** info and would also be used as the DDNAME to open.
        */

        p = strchr(buf, '(');
        if (!p) {
            /* We likely have "DD:MEMBER.EXT" */
            p = strrchr(buf, '.'); /* find the extension */
            if (p) {
                /* We have "DD:MEMBER.EXT"
                ** Use the EXT as the DDNAME
                */
                p++;
                len = strlen(p);
                if (len>=sizeof(ddname)) len = sizeof(ddname)-1;
                memcpy(ddname, p, len);
                ddname[len] = 0;
            }
            else {
                /* We don't want to allow "DD:MEMBER"
                ** They should use "DD:MEMBER.EXT"
                */
                goto quit;
            }

            len = strlen(&buf[3]);
            if (len>=sizeof(member)) len = sizeof(member)-1;
            memcpy(member, &buf[3], len);
            member[len] = 0;
            /* convert to "DD:DDNAME(MEMBER)" */
            sprintf(buf, "DD:%s(%s)", ddname, member);
        }
    }

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
			/* enable Server Side Include processing of this file */
			httpc->ssi = 1;
			// wtof("%s: enabling SSI processing for \"%s\" as \"%s\"", __func__, buf, mime->type);
		}
		else {
			/* disable Server Side Processing of this file */
			httpc->ssi = 0;
			// wtof("%s: disabling SSI processing for \"%s\" as \"%s\"", __func__, buf, mime->type);
		}
	}


    if (httpc->ufs) {
        UCHAR ufspath[256];
        const char *dr = httpc->httpd->docroot;
        if (dr[0] && http_cmpn(buf, "/DD:", 4) != 0) {
            /* prepend document root to UFS path */
            snprintf((char *)ufspath, sizeof(ufspath), "%s%s", dr, buf);
            httpc->ufp = ufs_fopen(httpc->ufs, ufspath, mode);
        } else {
            httpc->ufp = ufs_fopen(httpc->ufs, buf, mode);
        }
        if (httpc->ufp) {
            goto quit;  /* success, return NULL to caller */
        }
    }

    fp = fopen(buf, mode);

quit:
	// wtof("%s: exit fp=%p", __func__, fp);
    return fp;
}
