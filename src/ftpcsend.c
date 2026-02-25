/* FTPCSEND.C */
#include "httpd.h"

int
ftpcsend(FTPC *ftpc)
{
    int     rc      = 0;
    int     len;
    int     i;
    char    *ptr;

    if (!ftpc) goto quit;

    if (ftpc->state == FTPSTATE_IDLE) {
        rc = ftpcopen(ftpc);
        if (rc < 0) {
            ftpcmsg(ftpc, "425 Can't open data connection");
            if (ftpc->fp) {
                if (ftpc->fflags & FTPC_FFLAG_MVS) {
                    fclose(ftpc->fp);
                }
                else if (ftpc->fflags & FTPC_FFLAG_UFS) {
                    /* UFS file system access */
                    ufs_fclose((UFSFILE **)&ftpc->fp);
                }
                ftpc->fp = 0;
            }
            ftpc->flags &= 0xFF - FTPC_FLAG_SEND;
            goto quit;
        }
        ftpc->data_socket = rc;
    }

    if (!ftpc->fp) {
        ftpcmsg(ftpc, "550 Requested file unavailable");
        if (ftpc->data_socket >= 0) {
            closesocket(ftpc->data_socket);
            ftpc->data_socket = -1;
        }
        goto quit;
    }

    ftpc->state = FTPSTATE_TRANSFER;
    if (ftpc->pos == ftpc->len) {
        /* buffer is empty */
        ftpc->len = 0;
        ftpc->pos = 0;

        if (ftpc->fflags & FTPC_FFLAG_MVS) {
            if (ftpc->type == FTPC_TYPE_ASCII ||
                ftpc->type == FTPC_TYPE_EBCDIC) {
                ptr = fgets(ftpc->buf, sizeof(ftpc->buf), ftpc->fp);
                if (!ptr) goto eof;

                len = strlen(ftpc->buf);
                /* wtodumpf(ftpc->buf, len, "ftpcsend"); */

                if (ftpc->type == FTPC_TYPE_ASCII) {
                    /* convert buffer to ASCII */
                	char *p = strchr(ftpc->buf, '\n');
                	if (p) {
                		strcpy(p, "\r\n");
                		len++;
                	}
                    for(i=0; i < len; i++) {
                        if (ftpc->buf[i]=='\n') {
                            ftpc->buf[i] = 0x0A;
                            continue;
                        }
                        ftpc->buf[i] = ebc2asc[ftpc->buf[i]];
                    }
                }
            }
            else {
                len = fread(ftpc->buf, 1, sizeof(ftpc->buf), ftpc->fp);
                if (len <= 0) goto eof;
            }
            ftpc->len = len;
        }
        else if (ftpc->fflags & FTPC_FFLAG_UFS) {
            /* UFS file system access */
            if (ftpc->type == FTPC_TYPE_ASCII ||
                ftpc->type == FTPC_TYPE_EBCDIC) {
                ptr = ufs_fgets(ftpc->buf, sizeof(ftpc->buf), ftpc->fp);
                if (!ptr) goto eof;

                len = strlen(ftpc->buf);
                /* wtodumpf(ftpc->buf, len, "ftpcsend"); */

                if (ftpc->type == FTPC_TYPE_ASCII) {
                    /* convert buffer to ASCII */
                	char *p = strchr(ftpc->buf, '\n');
                	if (p) {
                		strcpy(p, "\r\n");
                		len++;
                	}
                    for(i=0; i < len; i++) {
                        if (ftpc->buf[i]=='\n') {
                            ftpc->buf[i] = 0x0A;
                            continue;
                        }
                        ftpc->buf[i] = ebc2asc[ftpc->buf[i]];
                    }
                }
            }
            else {
                len = ufs_fread(ftpc->buf, 1, sizeof(ftpc->buf), ftpc->fp);
                if (len <= 0) goto eof;
            }
            ftpc->len = len;
        }
    }

    /* send data to client */
    len = ftpc->len - ftpc->pos;
    len = send(ftpc->data_socket, &ftpc->buf[ftpc->pos], len, 0);
    if (len >= 0) {
        ftpc->pos   += len;
        ftpc->count += len;
        goto quit;
    }

eof:
    ftpc->len = 0;
    ftpc->pos = 0;
    if (ftpc->data_socket >= 0) {
        ftpcmsg(ftpc, "226 Closing data connection");
        closesocket(ftpc->data_socket);
        ftpc->data_socket = -1;
    }
    if (ftpc->fp) {
        if (ftpc->fflags & FTPC_FFLAG_MVS) {
            fclose(ftpc->fp);
        }
        else if (ftpc->fflags & FTPC_FFLAG_UFS) {
            /* UFS file system access */
            ufs_fclose((UFSFILE **)&ftpc->fp);
        }
        ftpc->fp = 0;
        ftpc->fflags = 0;
    }
    if (ftpc->state == FTPSTATE_TRANSFER) {
        /* save file transfer stats */
        ftpdsecs(&ftpc->end);
        /* ftpcstat(ftpc); */
        ftpc->state = FTPSTATE_IDLE;
        ftpc->flags &= 0xFF - FTPC_FLAG_SEND;
    }

quit:
    return rc;
}
