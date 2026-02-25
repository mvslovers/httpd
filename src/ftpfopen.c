/* FTPDSTOR.C */
#include "httpd.h"

static int ftp_mvs_open(FTPC *ftpc, const char *fn, const char *fm, void **ret);
static int ftp_ufs_open(FTPC *ftpc, const char *fn, const char *fm, void **ret);

__asm__("\n&FUNC    SETC 'ftpfopen'");
void *
ftpfopen(FTPC *ftpc, const char *fn, const char *fm, int fqnrc)
{
    int         rc          = 0;
    void        *fp         = NULL;
    unsigned    *psa        = (unsigned *)0;
    unsigned    *ascb       = (unsigned *)psa[0x224/4]; /* A(ASCB)      */
    unsigned    *asxb       = (unsigned *)ascb[0x6C/4]; /* A(ASXB)      */
    ACEE		*oldacee;
    int         lockrc;

    /* lock the ASXB (ENQ) address */
    lockrc = lock(asxb,0);

	oldacee = racf_get_acee() ;

	if (fqnrc == FTPFQN_RC_PATH) {
        rc = try(ftp_ufs_open, ftpc, fn, fm, &fp);
	}
	else {
        rc = try(ftp_mvs_open, ftpc, fn, fm, &fp);
	}

quit:
    if (rc) {
        /* restore security environment */
        racf_set_acee(oldacee);
    }

    /* unlock the asxb */
    if (lockrc==0) unlock(asxb,0);

    return fp;
}

__asm__("\n&FUNC    SETC 'ftp_ufs_open'");
static int
ftp_ufs_open(FTPC *ftpc, const char *fn, const char *fm, void **ret)
{
    UFSFILE     *fp         = NULL;

    /* open requested path */
    fp = ufs_fopen(ftpc->ufs, fn, fm);
    if (fp) {
        ftpc->fflags    = FTPC_FFLAG_UFS;       /* fp is a UFS file */
    }
    else {
        wtof("ftpfopen: ufs_fopen(\"%s\",\"%s\") failed", fn, fm);
    }

quit:
    *ret = fp;
    return 0;
}

static int
ftp_mvs_open(FTPC *ftpc, const char *fn, const char *fm, void **ret)
{
    FILE        *fp         = NULL;
    ACEE		*oldacee;

    /* set the security environment */
    oldacee = racf_set_acee(ftpc->acee);

    /* open requested dataset */
    fp = fopen(fn, fm);
    if (fp) {
        ftpc->fflags    = FTPC_FFLAG_MVS;       /* fp is a MVS dataset  */
    }
    else {
        wtof("ftpfopen: fopen(\"%s\",\"%s\") failed", fn, fm);
    }

    /* restore security environment */
    racf_set_acee(oldacee);

quit:
    *ret = fp;
    return 0;
}

#if 0
/* old code */
__asm__("\n&FUNC    SETC 'ftp_mvs_open'");
static int
ftp_mvs_open(FTPC *ftpc, const char *fn, const char *fm, void **ret)
{
    void        *fp         = NULL;
    unsigned    *psa        = (unsigned *)0;
    unsigned    *ascb       = (unsigned *)psa[0x224/4]; /* A(ASCB)      */
    unsigned    *asxb       = (unsigned *)ascb[0x6C/4]; /* A(ASXB)      */
    ACEE        *oldacee;
    int			i;
    int			len;
    char		*p;
    char		dataset[sizeof(ftpc->cwd)+12];
#if 0
    wtof("ftpfopen: ftp_mvs_open(\"%s\",\"%s\")", fn, fm);
    wtof("ftpfopen: ftpc->cwd=\"%s\"", ftpc->cwd);
#endif
    if (ftpc->flags & FTPC_FLAG_CWDDS) {
    	/* The current working directory is not a single dataset name */
    	/* Make sure the fn looks like a dataset or dataset(member) */
    	if (fn[0] == '\'') goto doit;				/* looks like 'dataset' or 'dataset(member)' */

    	if ((strchr(fn,'(')) && (strchr(fn,')'))) { /* looks like dataset(member) */
    		strcpy(dataset, ftpc->cwd);				/* put the cwd as the dataset name */
    		len = strlen(dataset);					/* get the length of the dataset name */
    		dataset[len++] = '.';					/* append a '.' to the dataset name */
    		strcpy(&dataset[len], fn);				/* copy the fn(member) to the dataset name */
    		fn = dataset;							/* use the cwd.dataset(member) as the fn */
    		goto doit;
    	}

    	if (strchr(fn,'.')) goto doit;				/* looks like some.dataset */

    	/* otherwise we need to fail the request right now */
    	wtof("ftpfopen: Current working directory is not a PDS dataset");
    	goto quit;
    }

    if (ftpc->flags & FTPC_FLAG_CWDPDS) {
    	/* The current working directory is a PDS name. */
    	/* Make sure the requested file name isn't a dataset name. */
    	if ((!strchr(fn, '\'')) && (!strchr(fn, '('))) {
    		/* we're going to assume the requested file name is a member name */
    		strcpy(dataset, ftpc->cwd);				/* put the cwd as the dataset name */
    		len = strlen(dataset);					/* get the length of the dataset name */
    		dataset[len++] = '(';					/* append a '(' for the member name */
    		for(i=0; i < 8; i++) {
    			if (!fn[i]) break;					/* break if end of string */
    			if (fn[i]=='.') break;				/* break if file extension */
    			dataset[len+i] = fn[i];				/* copy to member name */
    		}
    		len+=i;
    		dataset[len++] = ')';					/* append a ')' for the member name */
    		dataset[len] = 0;						/* terminate dataset name with 0 byte */
    		fn = dataset;							/* use the dataset(member) as the fn */
    	}
    }

doit:
    /* set the security environment */
    oldacee = racf_set_acee(ftpc->acee);

    /* open requested dataset */
#if 0
    wtof("ftpfopen: fopen(\"%s\",\"%s\")", fn, fm);
#endif
    fp = fopen(fn, fm);
    if (fp) {
        ftpc->fflags    = FTPC_FFLAG_MVS;       /* fp is a MVS dataset  */
    }
    else {
        wtof("ftpfopen: fopen(\"%s\",\"%s\") failed", fn, fm);
    }

    /* restore security environment */
    racf_set_acee(oldacee);

quit:
    *ret = fp;
    return 0;
}
#endif /* old code */
