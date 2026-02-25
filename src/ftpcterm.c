/* FTPCTERM.C */
#include "httpd.h"

void
ftpcterm(FTPC **ppftpc)
{
    FTPC        *ftpc;
    FTPD 		*ftpd;

    if (!ppftpc) goto quit;
    ftpc = *ppftpc;
    if (!ftpc) goto quit;

    if (ftpc->data_socket >= 0) {
        /* close data socket */
        closesocket(ftpc->data_socket);
        ftpc->data_socket = -1;
    }

    if (ftpc->client_socket >= 0) {
        /* close client socket */
        closesocket(ftpc->client_socket);
        ftpc->client_socket = -1;
    }

    if (ftpc->fp) {
        /* close data file */
        fclose(ftpc->fp);
        ftpc->fp = 0;
    }

	ftpd = ftpc->ftpd;
	/* this *should* always be true */
    if (ftpd) {
		if (ftpd->sys) {
			/* unix like file system exist */
            if (ftpc->ufs) {
                /* sync file system buffers to disk */
                ufs_sync(ftpc->ufs);

                /* terminate Unix File System access */
                ufsfree(&ftpc->ufs);
            }
        }
	}

    if (ftpc->acee) {
		ACEE *acee = ftpc->acee;
		char *name = acee->aceeuser;
		int len = name[0];
		
		wtof("HTTPD203I FTP connection terminated for '%*.*s'", 
			len, len, &name[1]);

        racf_logout(&ftpc->acee);
        ftpc->acee = 0;
    }

    /* free the FTPC handle */
    free(ftpc);
    *ppftpc = 0;

quit:
    return;
}
