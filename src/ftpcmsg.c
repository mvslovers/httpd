/* FTPCMSG.C */
#include "httpd.h"

/* Send message */
int
ftpcmsg(FTPC *ftpc, char *msg, ...)
{
    int         rc;
    int         len;
    int         i;
    va_list     args;
    char        *p;
#define BUFLEN  256
    char        buf[BUFLEN+4];

    /* print text and args to client */
    va_start(args, msg);
    len = vsnprintf(buf, BUFLEN, msg, args);
    va_end(args);

#if 0
    buf[BUFLEN]=0;
    wtof("%s \"%s\"", __func__, buf);
#endif
    if (len > BUFLEN) {
		len = BUFLEN;
		buf[len] = 0;
	}

    if (!strchr(buf, '\r')) {
		buf[len++] = '\r';
		buf[len] = 0;
	}

    if (!strchr(buf, '\n')) {
		buf[len++] = '\n';
		buf[len]=0;
	}
	
    for(i=0; i < len; i++) {
        if (buf[i]=='\n') {
            buf[i] = 0x0A;
            continue;
        }
        buf[i] = ebc2asc[buf[i]];
    }

    for(p=buf; len > 0; len-=rc, p+=rc) {
        rc = send(ftpc->client_socket, p, len, 0);
        if (rc <= 0) {
            /* abort transfer process */
            ftpc->state = FTPSTATE_TERMINATE;
            break;
        }
    }

    return 0;
}
