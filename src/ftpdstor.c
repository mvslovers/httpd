/* FTPDSTOR.C */
#include "httpd.h"

static int stor_error(FTPC *ftpc, char *fn);
static int stor_begin(FTPC *ftpc, char *fqn, int fqnrc);

int
ftpdstor(FTPC *ftpc)
{
	int		rc = 0;
    char    *fn;
    char    *fm;
    char	fqn[256];

    fn = strtok(ftpc->cmd, " ");
    if (fn) fn = strtok(NULL, "");
    if (!fn) {
        ftpcmsg(ftpc, "500 Syntax error, missing file name");
        goto quit;
    }

	rc = ftpfqn(ftpc, fn, fqn);
	switch (rc) {
		default:
		case FTPFQN_RC_PARMS:		/* invalid parameters 				*/
		case FTPFQN_RC_ERROR:		/* Couldn't make sense of the input */
			rc = stor_error(ftpc, fn);
			goto quit;
			
		case FTPFQN_RC_PATH:		/* fqn is a path name 				*/
		case FTPFQN_RC_HLQ:       	/* fqn is a high level qualifier 	*/
		case FTPFQN_RC_DSN:			/* fqn is a dataset name 			*/
		case FTPFQN_RC_DSNMEM:		/* fqn is a dataset(member) name 	*/
			rc = stor_begin(ftpc, fqn, rc);
			goto quit;
	}

quit:
	return rc;
}

static int 
stor_begin(FTPC *ftpc, char *fqn, int fqnrc)
{
    char    *fm;
	int		rc;

	if (ftpc->acee && (fqnrc != FTPFQN_RC_PATH)) {
		/* we have an acee and this a MVS dataset name */
		rc = racf_auth(ftpc->acee, "DATASET", fqn, RACHECK_ATTR_UPDATE);
		if (rc > 4) {
			ftpcmsg(ftpc, "552-UPDATE access to '%s' DENIED rc=%d", fqn, rc);
			ftpcmsg(ftpc, "552 Requested file action aborted");
			return 0;
		}
	}

    if (ftpc->type == FTPC_TYPE_ASCII || ftpc->type == FTPC_TYPE_EBCDIC) {
        fm = "w";
    }
    else {
        fm = "wb";
    }

    ftpc->fp = ftpfopen(ftpc, fqn, fm, fqnrc);
    if (!ftpc->fp) {
        ftpcmsg(ftpc, "552-Open of \"%s\" failed", fqn);
        ftpcmsg(ftpc, "552 Requested file action aborted");
        goto quit;
    }

    ftpc->flags     |= FTPC_FLAG_RECV;      /* data is in bound     */
    ftpc->flags     &= ~FTPC_FLAG_SEND;
    return ftpcrecv(ftpc);

quit:
    return 0;
}

static int 
stor_error(FTPC *ftpc, char *fn)
{
    ftpcmsg(ftpc, "550 invalid file name \"%s\"", fn);
    return 0;
}
