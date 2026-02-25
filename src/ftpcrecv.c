/* FTPCRECV.C */
#include "httpd.h"

int
ftpcrecv(FTPC *ftpc)
{
    int             rc      = 0;
    int             iseof   = 0;
    int             len;
    int             i;
    unsigned char   *ptr;

    if (!ftpc) goto quit;

    if (!ftpc->fp) {
        ftpcmsg(ftpc, "552 Requested file action aborted");
        if (ftpc->data_socket >= 0) {
            closesocket(ftpc->data_socket);
            ftpc->data_socket = -1;
        }
        goto quit;
    }

    if (ftpc->state == FTPSTATE_IDLE) {
        /* wtof("%s FTPSTATE_IDLE", __func__); */
        rc = ftpcopen(ftpc);
        /* wtof("%s ftpcopen() rc=%d", __func__, rc); */
        if (rc < 0) {
            ftpcmsg(ftpc, "425 Can't open data connection");
            if (ftpc->fp) {
                if (ftpc->fflags & FTPC_FFLAG_MVS) {
                    fclose(ftpc->fp);
                    ftpc->fp = 0;
                }
                else if (ftpc->fflags & FTPC_FFLAG_UFS) {
                    /* UFS file system access */
                    ufs_fclose((UFSFILE **)&ftpc->fp);
                }
            }
            goto quit;
        }
        ftpc->data_socket = rc;
    }

    /* wtof("%s FTPSTATE_TRANSFER", __func__); */
    ftpc->state = FTPSTATE_TRANSFER;

    /* receive data from client */
    len = sizeof(ftpc->buf) - ftpc->len;
    /* wtof("%s sizeof(ftpc->buf)=%d, ftpc->len=%d, len=%d", __func__, sizeof(ftpc->buf), ftpc->len, len); */
    if (!len) goto saveit;

    ptr = &ftpc->buf[ftpc->len];
    len = recv(ftpc->data_socket, ptr, len, 0);
    /* wtof("%s recv(%d) len=%d", __func__, ftpc->data_socket, len); */
    if (len==0) {
        /* nothing received */
        iseof = 1;
        goto saveit;
    }
    if (len < 0) {
        /* wtof("%s errno=%d", __func__, errno); */
        if (errno == EWOULDBLOCK) {
            /* no data available */
            rc = 0;
            goto quit;
        }
        iseof = 1;
        goto saveit;
    }

    /* ptr[len] = 0; */

    if (ftpc->type == FTPC_TYPE_ASCII) {
        /* wtof("%s FTPC_TYPE_ASCII", __func__); */
        /* convert buffer to EBCDIC */
        for(i=0; i < len; i++) {
            if (ptr[i]==0x0D) {
                /* discard CR */
                int move = len - (i+1);
                if (move > 0) {
                    /* wtof("%s memcpy(%08X, %08X, %d) i=%d, len=%d",
                        __func__, &ptr[i], &ptr[i+1], move, i, len); */
                    memcpy(&ptr[i], &ptr[i+1], move);
                    /* wtof("%s memcpy() okay", __func__); */
                }
                len--;
                i--;
                continue;
            }
            if (ptr[i]==0x0A) {
                /* convert LF to newline */
                ptr[i] = '\n';
                continue;
            }
            ptr[i] = asc2ebc[ptr[i]];
        }
    }
    ftpc->len   += len;
    ftpc->count += len;

saveit:
    if (ftpc->fflags & FTPC_FFLAG_MVS) {
        /* wtof("%s FTPC_FFLAG_MVS", __func__); */
        for(; ftpc->pos < ftpc->len; ftpc->pos++) {
            rc = fputc(ftpc->buf[ftpc->pos], ftpc->fp);
            if (rc==EOF) goto eof;
        }
    }
    else if (ftpc->fflags & FTPC_FFLAG_UFS) {
        /* UFS file system access */
        UINT32 len = ftpc->len - ftpc->pos;
        UINT32 n;

        /* wtof("%s FTPC_FFLAG_UFS", __func__); */

        if (len > 0) {
            /* wtodumpf(&ftpc->buf[ftpc->pos], len, "%s write buffer", __func__); */
            n = ufs_fwrite(&ftpc->buf[ftpc->pos], 1, len, ftpc->fp);
            /* wtof("%s %u = ufs_fwrite(%08X, 1, %u, %08X)",
                __func__, n, &ftpc->buf[ftpc->pos], len, ftpc->fp); */
            /* wtof("%s len=%u, n=%u", __func__, len, n); */

            if (n!=len) goto eof;
            ftpc->pos += n;
        }
    }

    /* wtof("%s ftpc->pos=%u, ftpc->len=%u", __func__, ftpc->pos, ftpc->len); */
    if (ftpc->pos == ftpc->len) {
        /* buffer is empty */
        /* wtof("%s buffer is empty", __func__); */
        ftpc->len = 0;
        ftpc->pos = 0;
    }

    rc = 0;
    if (!iseof) goto quit;

eof:
    /* wtof("%s EOF", __func__); */
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
        /* wtof("%s FTPSTATE_IDLE", __func__); */
        ftpc->flags &= 0xFF - FTPC_FLAG_RECV;
    }

quit:
    return rc;
}
