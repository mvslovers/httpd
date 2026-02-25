#include "httpd.h"

int http_getc(HTTPC *httpc)
{
    int             rc;
    int             i;
    unsigned char   buf[4];

    i = recv(httpc->socket, buf, 1, 0);
    if (i > 0) {
        /* we read a single character */
        i = (int) buf[0];
        goto quit;
    }

    if (i == 0) {
        /* nothing was read */
        // wtof("%s: recv() i=%d errno=%d", __func__, i, errno);
        i = -1; 
    }

    if (i < 0) {
        /* socket error, make sure errno is set */
        if (!errno) errno = EIO;
        // wtof("%s: recv() i=%d errno=%d", __func__, i, errno);
        goto quit;
    }

quit:
    return i;
}
