/* FTPDCWD.C */
#include "httpd.h"
#include "cliblist.h"
#include "clibary.h"
#include "clibstr.h"
#include "osdcb.h"
#include "clibdscb.h"       /* DSCB structs and prototypes  */

static int cwd_error(FTPC *ftpc, char *p);
static int cwd_path(FTPC *ftpc, char *path);
static int cwd_hlq(FTPC *ftpc, char *hlq);
static int cwd_dsn(FTPC *ftpc, char *dsn);
static int cwd_dsnmem(FTPC *ftpc, char *dsnmem);

int
ftpdcwd(FTPC *ftpc)
{
    int     rc      = 0;
    char    *p;
    char    buf[256];

    buf[0] = 0;
    p = strtok(ftpc->cmd, " ");
    if (p) p = strtok(NULL, " ");

	if (p) {
		rc = ftpfqn(ftpc, p, buf);
		switch (rc) {
			default:
			case FTPFQN_RC_PARMS:	/* invalid parameters 				*/
			case FTPFQN_RC_ERROR:	/* Couldn't make sense of the input */
				rc = cwd_error(ftpc, p);
				goto quit;

			case FTPFQN_RC_PATH:	/* out is a path name 				*/
				rc = cwd_path(ftpc, buf);
				goto quit;
				
			case FTPFQN_RC_HLQ:     /* out is a high level qualifier 	*/
				rc = cwd_hlq(ftpc, buf);
				goto quit;
				
			case FTPFQN_RC_DSN:		/* out is a dataset name 			*/
				rc = cwd_dsn(ftpc, buf);
				goto quit;
				
			case FTPFQN_RC_DSNMEM:	/* out is a dataset(member) name 	*/
				rc = cwd_dsnmem(ftpc, buf);
				goto quit;
		}
	}

	if (!p) {
		ftpcmsg(ftpc, "550 Missing path or dataset name");
	}
	else {
		ftpcmsg(ftpc, "550 Invalid path name '%s'", p);
	}

quit:
	return rc;
}

static int cwd_error(FTPC *ftpc, char *p)
{
	ftpcmsg(ftpc, "550 Invalid path name '%s'", p);
	
	return 0;
}

static int cwd_path(FTPC *ftpc, char *path)
{
    int 	rc = ufs_chgdir(ftpc->ufs, path);

    if (rc==0) {
        ftpc->flags &= ~FTPC_FLAG_CWDDS;
        ftpc->flags &= ~FTPC_FLAG_CWDPDS;
        ftpcmsg(ftpc, "250 Working directory is '%s'", ftpc->ufs->cwd->path);
    }
    else {
        ftpcmsg(ftpc, "550 Invalid path name '%s'", path);
    }

	return rc;
}

static int cwd_hlq(FTPC *ftpc, char *hlq)
{
	ACEE 		*acee		= ftpc->acee;
    int     	rc      	= 0;
    int     	i;
    unsigned	count;

    i = strlen(hlq);
    if (!i) {
        ftpcmsg(ftpc, "550 Requested action not taken. Missing dataset name.");
        goto quit;
    }

	if (!acee) {
		ftpcmsg(ftpc, "550 Requested action not taken. Login required.");
		goto quit;
	}

	rc = racf_auth(ftpc->acee, "DATASET", hlq, RACF_ATTR_READ);
	if (rc > 4) {
		ftpcmsg(ftpc, "550 Requested action not taken. READ Access DENIED rc=%d", rc);
		goto quit;
	}
	
	count = ftpc_ds_count(hlq, "NONVSAM", NULL);
    if (!count) {
		ftpcmsg(ftpc, "550 Requested action not taken. No Datasets.");
		goto quit;
	}

    strcpyp(ftpc->cwd, sizeof(ftpc->cwd), hlq, 0);
    ftpc->flags &= ~FTPC_FLAG_CWDPDS;
    ftpc->flags |= FTPC_FLAG_CWDDS;
	ftpcmsg(ftpc, "250 Dataset level is '%s'", ftpc->cwd);

quit:
    return rc;
}

static int 
cwd_dsn(FTPC *ftpc, char *dsn)
{
	ACEE 		*acee		= ftpc->acee;
	char    	*p			= dsn;
    int     	rc      	= 0;
    FILE    	*fp     	= 0;
    int     	i;
    unsigned 	count;

    i = strlen(dsn);
    if (!i) {
        ftpcmsg(ftpc, "550 Requested action not taken. Missing dataset name.");
        goto quit;
    }

    if (dsn[0] != '.' && dsn[i-1]=='.') {
        /* "some.dataset.level." -> "some.dataset.level" */
        i--;
        dsn[i] = 0;
    }

	if (!acee) {
		ftpcmsg(ftpc, "550 Requested action not taken. Login required.");
		goto quit;
	}

	// wtof("%s: Userid:%*.*s", __func__, acee->aceeuser[0], acee->aceeuser[0], &acee->aceeuser[1]);

	/* check for access to this dataset name */
	rc = racf_auth(ftpc->acee, "DATASET", dsn, RACF_ATTR_READ);

	//  wtof("%s: racf_auth(0x%06X, \"DATASET\", \"%s\", RACF_ATTR_READ) rc=%d",
	//		__func__, acee, dsn, rc);

	if (rc > 4) {
		ftpcmsg(ftpc, "550 Requested action not taken. READ Access DENIED rc=%d", rc);
		goto quit;
	}
	
    /* can we open this dataset name? */
    fp = fopen(dsn, "r,record");
    if (fp) {
        /* yes, is this dataset a PDS? */
        DCB *dcb = fp->dcb;
        JFCB jfcb = {0};
        DSCB dscb = {0};
        DSCB1 *dscb1  = &dscb.dscb1;

        /* read the job file control block to get the volser */
        i = __rdjfcb(dcb, &jfcb);
        if (i) goto level;

        /* get the DSCB for this dataset and volser */
        i = __dscbdv(fp->dataset, jfcb.jfcbvols, &dscb);
        if (i) goto level;

        /* is this dataset as PDS? */
        if ((dscb1->dsorg1 & 0x7F) == DSGPO) {
            /* yes, make this dataset the CWD */
            strcpyp(ftpc->cwd, sizeof(ftpc->cwd), dsn, 0);
            ftpc->flags &= ~FTPC_FLAG_CWDDS;
            ftpc->flags |= FTPC_FLAG_CWDPDS;
            ftpcmsg(ftpc, "250 PDS '%s' is current directory", ftpc->cwd);
            goto quit;
        }
    }

level:
	count = ftpc_ds_count(dsn, "NONVSAM", NULL);
    if (!count) {
		ftpcmsg(ftpc, "550 Requested action not taken. No Datasets.");
		goto quit;
	}

    /* assume this is a dataset level */
    strcpyp(ftpc->cwd, sizeof(ftpc->cwd), dsn, 0);
    ftpc->flags &= ~FTPC_FLAG_CWDPDS;
    ftpc->flags |= FTPC_FLAG_CWDDS;
	ftpcmsg(ftpc, "250 Dataset level is '%s'", ftpc->cwd);

quit:
    if (fp) fclose(fp);
    return rc;
}

static int cwd_dsnmem(FTPC *ftpc, char *dsnmem)
{
	char	*lparen = strchr(dsnmem, '(');
	char	*rparen = strchr(dsnmem, ')');
	
	if (lparen && rparen) {
		*lparen = '.';
		*rparen = 0;
		return cwd_dsn(ftpc, dsnmem);
	}
	
    ftpcmsg(ftpc, "550 Invalid dataset name '%s'", dsnmem);

	return 0;
}

#if 0	
/* old code */
int
ftpdcwd(FTPC *ftpc)
{
    int     rc      = 0;
    FILE    *fp     = 0;
    int     i;
    char    *p;
    char    buf[256];

    buf[0] = 0;
    p = strtok(ftpc->cmd, " ");
    if (p) p = strtok(NULL, " ");

    if (!ftpc->ufs) goto do_dataset;

    if (p) {
    	if (p[0]=='/') goto do_path;		/* CWD /some/path/name		*/
    	if (p[0]=='\'') goto do_dataset;	/* CWD 'some.dataset.name'	*/
    }

    if (ftpc->flags & FTPC_FLAG_CWDDS) goto do_dataset;
    if (ftpc->flags & FTPC_FLAG_CWDPDS) goto do_dataset;

do_path:
    if (p) {
        if (p[0]=='\'') {
            /* skip quotes */
            while(*p=='\'') p++;
            strtok(p, "\'");
        }
        /* wtof("%s cwd=\"%s\"", __func__, ftpc->ufs->cwd->path); */
        rc = ufs_chgdir(ftpc->ufs, p);
        /* wtof("%s ufs_chgdir(\"%s\"), rc=%d", __func__, p, rc); */
        if (rc==0) {
            ftpc->flags &= ~FTPC_FLAG_CWDDS;
            ftpc->flags &= ~FTPC_FLAG_CWDPDS;
            ftpcmsg(ftpc, "250 Working directory is '%s'", ftpc->ufs->cwd->path);
        }
        else {
            ftpcmsg(ftpc, "550 Invalid path name '%s'", p);
        }
        goto quit;
    }

do_dataset:
    if (p) {
		/* change dataset level? */
		if (strcmp(p, "..")==0) {
			/* yes, do we have a '.' in the cwd? */
			p = strrchr(ftpc->cwd, '.');
			if (p) {
				/* yes, truncate cwd at last period */
				*p = 0;
			}
			/* use cwd as buf */
			strcpy(buf, ftpc->cwd);
		}
		else {
			/* skip quotes and periods */
			while(*p=='\'' || *p=='.') p++;

			/* copy to buf in upper case */
			for(i=0; i < sizeof(buf) && p[i] > ' ' && p[i] != '\''; i++) {
				buf[i] = toupper(p[i]);
			}
			buf[i] = 0;
		}
    }

    /* remove any member name */
    strtok(buf, "()");

    if (buf[0]=='/') {
        strcpy(buf, &buf[1]);
        p = strchr(buf,'/');
        if (p) *p = 0;
    }

    i = strlen(buf);
    if (!i) {
        ftpcmsg(ftpc, "550 Missing dataset name");
        goto quit;
    }

    if (buf[0] != '.' && buf[i-1]=='.') {
        /* "some.dataset.level." -> "some.dataset.level" */
        i--;
        buf[i] = 0;
    }

    p = strrchr(buf, '/');
    if (p > buf) goto dir;

    p = strchr(buf, '.');
    if (!p) goto level;

    /* can we open this dataset name? */
    fp = fopen(buf, "r,record");
    if (fp) {
        /* yes, is this dataset a PDS? */
        DCB *dcb = fp->dcb;
        JFCB jfcb = {0};
        DSCB dscb = {0};
        DSCB1 *dscb1  = &dscb.dscb1;

        /* read the job file control block to get the volser */
        i = __rdjfcb(dcb, &jfcb);
        if (i) goto level;

        /* get the DSCB for this dataset and volser */
        i = __dscbdv(fp->dataset, jfcb.jfcbvols, &dscb);
        if (i) goto level;

        /* is this dataset as PDS? */
        if ((dscb1->dsorg1 & 0x7F) == DSGPO) {
            /* yes, make this dataset the CWD */
            strcpyp(ftpc->cwd, sizeof(ftpc->cwd), buf, 0);
            ftpc->flags &= 0xFF - FTPC_FLAG_CWDDS;
            ftpc->flags |= FTPC_FLAG_CWDPDS;
            ftpcmsg(ftpc, "250 PDS '%s' is current directory", ftpc->cwd);
            goto quit;
        }
    }

level:
    /* assume this is a dataset level */
    strcpyp(ftpc->cwd, sizeof(ftpc->cwd), buf, 0);
    ftpc->flags &= 0xFF - FTPC_FLAG_CWDPDS;
    ftpc->flags |= FTPC_FLAG_CWDDS;
    ftpcmsg(ftpc, "250 Dataset level is '%s'", ftpc->cwd);
    goto quit;

dir:
    /* assume this is file system path */
    strcpyp(ftpc->cwd, sizeof(ftpc->cwd), buf, 0);
    ftpc->flags &= 0xFF - FTPC_FLAG_CWDPDS;
    ftpc->flags &= 0xFF - FTPC_FLAG_CWDDS;
    ftpcmsg(ftpc, "250 Working directory is '%s'", ftpc->cwd);

quit:
    if (fp) fclose(fp);
    return rc;
}
#endif /* old code */
