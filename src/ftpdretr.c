/* FTPDRETR.C */
#include "ftpd.h"

static int retr_error(FTPC *ftpc, char *fn);
static int retr_begin(FTPC *ftpc, char *fqn, int fqnrc);

int
ftpdretr(FTPC *ftpc)
{
	int			rc	= 0;
    char        *fn;
    char        fqn[256];

    fn = strtok(ftpc->cmd, " ");
    if (fn) fn = strtok(NULL, " ");
    if (!fn) {
        ftpcmsg(ftpc, "501 missing file name");
        goto quit;
    }

	/* transform fn to fully qualifed name */
	rc = ftpfqn(ftpc, fn, fqn);
	
	/* process the request */
	switch (rc) {
		default:
		case FTPFQN_RC_PARMS:		/* invalid parameters 				*/
		case FTPFQN_RC_ERROR:		/* Couldn't make sense of the input */
			rc = retr_error(ftpc, fn);
			goto quit;
			
		case FTPFQN_RC_PATH:		/* fqn is a path name 				*/
		case FTPFQN_RC_HLQ:       	/* fqn is a high level qualifier 	*/
		case FTPFQN_RC_DSN:			/* fqn is a dataset name 			*/
		case FTPFQN_RC_DSNMEM:		/* fqn is a dataset(member) name 	*/
			rc = retr_begin(ftpc, fqn, rc);
			goto quit;
	}

quit:
	return rc;
}

static int 
retr_begin(FTPC *ftpc, char *fqn, int fqnrc)
{
    FILE        *fp     = 0;
    char		*fm;
    int			rc;

	if (ftpc->acee && (fqnrc != FTPFQN_RC_PATH)) {
		/* we have an acee and this a MVS dataset name */
		rc = racf_auth(ftpc->acee, "DATASET", fqn, RACHECK_ATTR_READ);
		if (rc > 4) {
			ftpcmsg(ftpc, "550-READ access to '%s' DENIED rc=%d", fqn, rc);
			ftpcmsg(ftpc, "550 Requested file action aborted");
			return 0;
		}
	}
 
    if (ftpc->type==FTPC_TYPE_ASCII || ftpc->type==FTPC_TYPE_EBCDIC) {
        fm = "r";
    }
    else {
        fm = "rb";
    }

    fp = ftpfopen(ftpc, fqn, fm, fqnrc);
    if (!fp) {
        ftpcmsg(ftpc, "550 open of \"%s\" failed", fqn);
        return 0;
    }

    ftpc->fp        = fp;                       /* this file handle     */
    ftpc->flags     |= FTPC_FLAG_SEND;          /* data is out bound    */
    ftpc->flags     &= ~FTPC_FLAG_RECV;   		/* not receiving data   */
    ftpc->len       = 0;                        /* buffer is empty      */
    ftpc->pos       = 0;                        /* buffer is empty      */
    return ftpcsend(ftpc);
}

static int 
retr_error(FTPC *ftpc, char *fn)
{
    ftpcmsg(ftpc, "550 invalid file name \"%s\"", fn);
    return 0;
}


#if 0
/* old code */
int
ftpdretr(FTPC *ftpc)
{
    FILE        *fp     = 0;
    char        *fn;
    int         i;
    int         bin;
    char        dataset[56] = "";

    fn = strtok(ftpc->cmd, " ");
    if (fn) fn = strtok(NULL, " ");
    if (!fn) {
        ftpcmsg(ftpc, "501 missing file name");
        goto quit;
    }

    if ((ftpc->flags & FTPC_FLAG_CWDDS) || (ftpc->flags & FTPC_FLAG_CWDPDS)) {
        /* cwd is dataset level or pds */
        if (ftpc->flags & FTPC_FLAG_CWDPDS) {
            /* pds, so make full dataset name with member */
            if (strlen(fn) > 8) {
                ftpcmsg(ftpc, "501 CWD is PDS and requested member name is too long");
                goto quit;
            }
            snprintf(dataset, sizeof(dataset)-1, "%s(%s)", ftpc->cwd, fn);
        }
        else if (ftpc->flags & FTPC_FLAG_CWDDS) {
            strcpyp(dataset, sizeof(dataset)-1, fn, 0);
        }
        dataset[sizeof(dataset)-1] = 0;
        fn = dataset;

        /* make dataset name upper case */
        for(i=0; fn[i]; i++) {
            fn[i] = toupper(fn[i]);
        }

        if (ftpc->type==FTPC_TYPE_ASCII || ftpc->type==FTPC_TYPE_EBCDIC) {
            bin = 0;
        }
        else {
            bin = 1;
        }
        fp = ftpfopen(ftpc, fn, bin ? "rb" :"r");

        if (!fp) {
            ftpcmsg(ftpc, "550 open of \"%s\" failed", fn);
            goto quit;
        }

        ftpc->fp        = fp;                       /* this file handle     */
        ftpc->flags     |= FTPC_FLAG_SEND;          /* data is out bound    */
        ftpc->flags     &= 0xFF - FTPC_FLAG_RECV;   /* not receiving data   */
        ftpc->len       = 0;                        /* buffer is empty      */
        ftpc->pos       = 0;                        /* buffer is empty      */
        return ftpcsend(ftpc);
    }

    /* must be UFS resource */
    if (ftpc->type==FTPC_TYPE_ASCII || ftpc->type==FTPC_TYPE_EBCDIC) {
        bin = 0;
    }
    else {
        bin = 1;
    }
    ftpc->fp = ftpfopen(ftpc, fn, bin ? "rb" : "r");
    if (!ftpc->fp) {
        ftpcmsg(ftpc, "550 open of \"%s\" failed", fn);
        goto quit;
    }

    ftpc->flags     |= FTPC_FLAG_SEND;          /* data is out bound    */
    ftpc->flags     &= 0xFF - FTPC_FLAG_RECV;   /* not receiving data   */
    ftpc->len       = 0;                        /* buffer is empty      */
    ftpc->pos       = 0;                        /* buffer is empty      */
    return ftpcsend(ftpc);

quit:
    return 0;
}
#endif	/* old-code */
