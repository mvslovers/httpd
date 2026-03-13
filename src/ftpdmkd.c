/* FTPDMKD.C */
#include "httpd.h"

static int mkd_error(FTPC *ftpc, char *p);
static int mkd_path(FTPC *ftpc, char *path);
static int mkd_dsn(FTPC *ftpc, char *dsn);
static int mkd_dsnmem(FTPC *ftpc, char *dsnmem);

int
ftpdmkd(FTPC *ftpc)
{
    int         rc          = 0;
    char        *p          = 0;
    char        buf[256]    = {0};

    /* get parameters */
    p = strtok(ftpc->cmd, " ");
    if (p) p = strtok(NULL, " ");

	if (p) {
		rc = ftpfqn(ftpc, p, buf);
		switch (rc) {
			default:
			case FTPFQN_RC_PARMS:	/* invalid parameters 				*/
			case FTPFQN_RC_ERROR:	/* Couldn't make sense of the input */
				rc = mkd_error(ftpc, p);
				goto quit;

			case FTPFQN_RC_PATH:	/* out is a path name 				*/
				rc = mkd_path(ftpc, buf);
				goto quit;
				
			case FTPFQN_RC_HLQ:     /* out is a high level qualifier 	*/
			case FTPFQN_RC_DSN:		/* out is a dataset name 			*/
				rc = mkd_dsn(ftpc, buf);
				goto quit;
				
			case FTPFQN_RC_DSNMEM:	/* out is a dataset(member) name 	*/
				rc = mkd_dsnmem(ftpc, buf);
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

static int mkd_error(FTPC *ftpc, char *p)
{
	ftpcmsg(ftpc, "550 Invalid path name '%s'", p);
	
	return 0;
}

static int mkd_path_tree(UFS *ufs, char *path)
{
	int		rc = ufs_mkdir(ufs, path);
	char	*p;
	
	if (rc==ENOENT && path[0] == '/') {
		p = strrchr(path+1, '/');
		if (p) {
			*p = 0;
			rc = mkd_path_tree(ufs, path);
			*p = '/';

			if (rc==EPERM) goto quit;

			rc = ufs_mkdir(ufs, path);
		}
	}

quit:
	return rc;
}

static int mkd_path(FTPC *ftpc, char *path)
{
    int 	rc = 0;

	if (!ftpc->ufs) {
		ftpcmsg(ftpc, "550 Unix File System is not available. Requested action not taken");
		goto quit;
	}

	/* try to make the path */
    rc = mkd_path_tree(ftpc->ufs, path);

    if (rc) {
        ftpcmsg(ftpc, "550-%s", strerror(rc));
        ftpcmsg(ftpc, "550 Requested action not taken");
        rc = 0;
    }
    else {
        ftpcmsg(ftpc, "257 \"%s\" created", path);
    }

quit:
	return rc;
}

static int mkd_dsn(FTPC *ftpc, char *dsn)
{
	ACEE 	*acee	= ftpc->acee;
    char    *p		= dsn;
    int     rc      = 0;
    int     i;

    i = strlen(dsn);
    if (!i) {
        ftpcmsg(ftpc, "550 Missing dataset name");
        goto quit;
    }

	if (!acee) {
		ftpcmsg(ftpc, "550 Login required for access to dataset '%s'", dsn);
		goto quit;
	}

	/* check for access to this dataset name */
	rc = racf_auth(ftpc->acee, "DATASET", dsn, RACF_ATTR_ALTER);
	if (rc > 4) {
		ftpcmsg(ftpc, "550 ALTER Access to dataset '%s' DENIED rc=%d", dsn, rc);
		goto quit;
	}

	/* we don't currently have code for allocating a new dataset "directory" */
	ftpcmsg(ftpc, "550 Unable to create a 'directory' dataset for '%s'", dsn);

quit:
    return rc;
}

static int mkd_dsnmem(FTPC *ftpc, char *dsnmem)
{
    ftpcmsg(ftpc, "550 Invalid dataset name '%s'", dsnmem);

	return 0;
}


#if 0
/* old code */
int
ftpdmkd(FTPC *ftpc)
{
    int         rc          = 0;
    int         i           = 0;
    char        *p          = 0;
    char        buf[256]    = {0};
    const char *cwd         = ftpc->cwd;

    /* get parameters */
    p = strtok(ftpc->cmd, " ");
    if (p) p = strtok(NULL, " ");

    /* are we doing file system or dataset processing? */
    if (!ftpc->ufs) goto do_dataset;
    if (ftpc->flags & FTPC_FLAG_CWDDS) goto do_dataset;
    if (ftpc->flags & FTPC_FLAG_CWDPDS) goto do_dataset;

    /* file system it is */
    cwd = ftpc->ufs->cwd.path;

    if (p) {
        /* skip quotes */
        while(*p=='\'') p++;
        strtok(p, "\'");

        rc = ufs_mkdir(ftpc->ufs, p);
        if (rc) {
            ftpcmsg(ftpc, "550-%s", strerror(rc));
            ftpcmsg(ftpc, "550 Requested action not taken");
            rc = 0;
        }
        else {
            ftpcmsg(ftpc, "257 \"%s\" created", p);
        }
        goto quit;
    }

do_dataset:
    if (p) {
        /* skip quotes and periods */
        while(*p=='\'' || *p=='.') p++;

        /* copy to buf in upper case */
        for(i=0; i < sizeof(buf) && p[i] > ' ' && p[i] != '\''; i++) {
            buf[i] = toupper(p[i]);
        }
        buf[i] = 0;
    }

    if (strlen(buf)==0) {
        ftpcmsg(ftpc, "501 Syntax error in parameters");
        goto quit;
    }

    /* create directory */
    rc = EPERM; /* fail until we add some logic for dataset creation */

    if (rc) {
        ftpcmsg(ftpc, "550 Requested action not taken");
        goto quit;
    }

    ftpcmsg(ftpc, "257 \"%s\" created", buf);

quit:
    return rc;
}
#endif /* old code */
