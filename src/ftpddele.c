/* FTPDDELE.C */
#include "httpd.h"

static int isdataset(const char *name);
static int ismember(const char *name);

__asm__("\n&FUNC    SETC 'ftpddele'");
int
ftpddele(FTPC *ftpc)
{
    int     rc;
    int		dataset = 0;
    int		member  = 0;
    char    *p;
    char	buf[256];

	/* isolate the file/dataset/member name in the delete command */
    p = strtok(ftpc->cmd, " ");
    if (!p) goto nope;
    p = strtok(NULL, " ");
    if (!p) goto nope;

#if 1
	/* new code using ftpfqn() */
	rc = ftpfqn(ftpc, p, buf);
	switch (rc) {
		case FTPFQN_RC_PARMS:		/* invalid parameters 				*/
		case FTPFQN_RC_ERROR:		/* Couldn't make sense of the input */
			goto nope;

		case FTPFQN_RC_PATH:		/* buf is a path name 				*/
			if (ftpc->ufs) {
				/* must be UFS resource */
				rc = ufs_remove(ftpc->ufs, buf);
				if (rc==0) goto okay;
			}
			goto nope;

		case FTPFQN_RC_HLQ:			/* buf is a high level qualifier 	*/
			goto nope;

		case FTPFQN_RC_DSN:			/* buf is a dataset name 			*/
		case FTPFQN_RC_DSNMEM:		/* buf is a dataset(member) name 	*/
			rc = remove(buf);		/* try to delete the dataset asis 	*/
			if (rc==0) goto okay;
			goto nope;

		default:
			goto nope;
	}

#else
	/* old code - brute force */
	if (*p == '\'') {
		char *e = strrchr(p+1,'\'');

		if (e) *e = 0;

		strcpy(p, p+1);
		dataset = isdataset(p);
	}
	else if (ftpc->flags & FTPC_FLAG_CWDDS) {
		sprintf(buf, "%s.%s", ftpc->cwd, p);
		dataset = isdataset(buf);
		if (dataset) p = buf;
	}
	else if (ftpc->flags & FTPC_FLAG_CWDPDS) {
		member = ismember(p);
	}

	/* if cwd is not a dataset OR this name looks like a dataset */
    if (dataset) {
        rc = remove(p);		/* try to delete the name asis */
        if (rc==0) goto okay;
    }

	/* if the cwd is a PDS treat the name as a member of that PDS */
    if (member) {
		sprintf(buf, "%s(%s)", ftpc->cwd, p);
        rc = remove(buf);
        if (rc==0) goto okay;
    }

	/* otherwise, if we have a UFS, try to delete this file name */
    if (ftpc->ufs) {
        /* must be UFS resource */
        rc = ufs_remove(ftpc->ufs, p);
        if (rc==0) goto okay;
    }
#endif

nope:
    ftpcmsg(ftpc, "550 Requested action not taken");
    goto quit;

okay:
    ftpcmsg(ftpc, "250 Requested file action okay, completed");

quit:
   return 0;
}

#if 0 /* old code */
static int 
isdataset(const char *name)
{
	int		dataset = 0;
	int		qualifiers = 0;
	char	buf[256];
	char	*p;
	
	if (name[0]=='\'') {
		dataset=1;
		goto quit;
	}

	strcpy(buf, name);
	for(p = strtok(buf, ".()"); p; p = strtok(NULL, ".()")) {
		if (p[0]=='/') goto quit;		/* not a dataset */
		if (strlen(p) > 8) goto quit;	/* member or dataset level name too long */
		if (!ismember(p)) goto quit;	/* bad character in name */
		qualifiers++;					/* dataset name qualifiers */
	}
	
	if (qualifiers > 1) dataset=1;
	
quit:
	/* wtof("ftpddele->%s(\"%s\"): dataset=%d", __func__, name, dataset); */
	return dataset;
}

static int 
ismember(const char *name)
{
	int		member = 0;
	int		len = strlen(name);
	int		i;

	if (len < 1) goto quit;				/* invalid member name */

	if (len <= 8) {
		member = 1;	/* assume the name is a valid member name */
		for(i=0; name[i]; i++) {
			if (name[i]=='@') continue;
			if (name[i]=='#') continue;
			if (name[i]=='$') continue;
			if (isalnum(name[i])) continue;

			/* not a valid character for a member name */
			member = 0;
			break;
		}
	}
	
quit:
	return member;
}
#endif /* old code */

#if 0
__asm__("STOW\tDCB,LIST,D\n"
"DCB\tDC\tF'0'\n"
"LIST\tDC\tF'0");
#endif
