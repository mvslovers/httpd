#include "httpd.h"

static int isdataset(const char *name);
static int ismember(const char *name);
static int ispattern(const char *name, const char *pattern);
static char *resolve_path(char *path);
static char *strupper(char *buf);

/* uses ftpc and in parameters to construct a fully qualified name in out */
int 
ftpfqn(FTPC *ftpc, const char *in, char *out)
{
	int		rc		= FTPFQN_RC_PARMS;
	int		dataset = 0;
	int		member  = 0;
	int		path    = 0;
	int		i;
	char	buf[256]= "";

	if (!ftpc) goto quit;
	if (!in) goto quit;
	if (!out) goto quit;

	// wtof("%s: in=\"%s\"", __func__, in);
	
	strncpy(buf, in, sizeof(buf));
	buf[sizeof(buf)-1] = 0;
	
	if (buf[0] == '\'') {
		/* in is a dataset name */
		char *e = strrchr(&buf[1], '\'');
		
		if (e) *e = 0;	/* strip trailing single quote */
		
		strcpy(buf, &buf[1]);
		strupper(buf);
		
		dataset = isdataset(buf);
		if (dataset) {
			if (ismember(buf)) {
				/* name is a single level high level qualifier */
				rc = FTPFQN_RC_HLQ;
			}
			else if (ispattern(buf, "*.*(*)")) {
				/* name looks like a datset with member */
				rc = FTPFQN_RC_DSNMEM;
			}
			else {
				/* name is a dataset */
				rc = FTPFQN_RC_DSN;
			}
			strcpy(out, buf);
			goto quit;
		}

		rc = FTPFQN_RC_ERROR;
		goto quit;
	}

	if (buf[0] == '/') {
		/* in is a path name */
		strcpy(out, buf);
		resolve_path(out);
		rc = FTPFQN_RC_PATH;
		goto quit;
	}

	if (strcmp(in, "..")==0) {
		/* resove cwd to higher level */
		if ((ftpc->flags & FTPC_FLAG_CWDDS) || (ftpc->flags & FTPC_FLAG_CWDPDS)) {
			char *dot;
			
			strcpy(out, ftpc->cwd);
			dot = strrchr(out, '.');
			if (dot) *dot = 0;

			if (strchr(out, '.')) {
				rc = FTPFQN_RC_DSN;
			}
			else {
				rc = FTPFQN_RC_HLQ;
			}
			goto quit;
		}
		
		if (ftpc->ufs && ftpc->ufs->cwd.path[0]) {
			char *dir;
			
			strcpy(out, ftpc->ufs->cwd.path);
			dir = strrchr(out+1, '/');
			if (dir) {
				/* truncate path by one level */
				*dir = 0;
			}
			else {
				/* truncate at root name "/" */
				out[1] = 0;
			}
			rc = FTPFQN_RC_PATH;
			goto quit;
		} 
	}

	if (ftpc->flags & FTPC_FLAG_CWDDS) {
		/* The cwd is a dataset list prefix */
		if (in[0]=='(') {
			/* the input name looks like a "(member)" */
			sprintf(buf, "%s%s", ftpc->cwd, in);
		}
		else {
			/* The input name *should* be "names" or "name(member)"*/
			sprintf(buf, "%s.%s", ftpc->cwd, in);
		}
	}
	
	if (ftpc->flags & FTPC_FLAG_CWDPDS) {
		/* The cwd is the PDS name */
		if (in[0]=='(') {
			/* the input name looks like a "(member)" */
			sprintf(buf, "%s%s", ftpc->cwd, in);
		}
		else {
			/* The input name *should* be "member" */
			sprintf(buf, "%s(%s)", ftpc->cwd, in);
		}
	}
	
	if ((ftpc->flags & FTPC_FLAG_CWDDS) || (ftpc->flags & FTPC_FLAG_CWDPDS)) {
		strupper(buf);
		dataset = isdataset(buf);
		if (dataset) {
			if (ispattern(buf, "*(*)")) {
				/* name looks like a datset with member */
				rc = FTPFQN_RC_DSNMEM;
			}
			else {
				/* name is a dataset */
				rc = FTPFQN_RC_DSN;
			}
			strcpy(out, buf);
			goto quit;
		}
		
		rc = FTPFQN_RC_ERROR;
		goto quit;
	}

	if (ftpc->ufs) {
		if (ftpc->ufs->cwd.path[0]) {
			sprintf(out, "%s/%s", ftpc->ufs->cwd.path, buf);
		}
		else {
			sprintf(out, "%s/%s", ftpc->cwd, buf);
		}
		resolve_path(out);

		rc = FTPFQN_RC_PATH;
		goto quit;
	}
	
	rc = FTPFQN_RC_ERROR;
	
quit:
#if 0
	wtof("%s: out=\"%s\" rc=%d %s", __func__, out, rc,
		rc==FTPFQN_RC_PARMS ? "PARMS" :
		rc==FTPFQN_RC_ERROR ? "ERROR" :
		rc==FTPFQN_RC_PATH  ? "PATH"  :
		rc==FTPFQN_RC_HLQ   ? "HLQ"   :
		rc==FTPFQN_RC_DSN   ? "DSN"   :
		rc==FTPFQN_RC_DSNMEM? "DSNMEM": "???");
#endif
	return rc;
}

static char *
resolve_path(char *path)
{
	char	*p;
	int		len;
	
	if (!path) goto quit;

	// wtof("   %s: in path=\"%s\"", __func__, path);

	if (*path != '/') goto quit;

	/* transform "//" to "/" */
	while(p=strstr(path, "//")) {
		strcpy(p, p+1);
	}
	
	/* transform "/some/name/../bob" to "/some/bob" */
	/* transform "/some/.." to "/" */
	while(p=strstr(path, "/..")) {
		char *slash;
		
		if (strcmp(path, "/..") == 0) {
			strcpy(path, "/");
			continue;
		}
#if 1
		/* did we find "/.." at the start of the path name? */
		if (p==path) {
			/* Yes, do we have anything following the "/.." ? */
			// wtof("   %s: p==path: path+1=\"%s\" p+3=\"%s\"", __func__, path+1, p+3);
			if (*(p+3)=='/') {
				/* Yes, copy it to the start of the path */
				strcpy(path, p+3);
			}
			else {
				/* No, copy it after the "/" */
				strcpy(path+1, p+3);
			}
			continue;
		}
#endif
		
		*p = 0;
		slash = strrchr(path, '/');
		if (slash) {
			// wtof("   %s: slash: slash=\"%s\" p+3=\"%s\"", __func__, slash, p+3);
			if (*(p+3)=='/') {
				strcpy(slash, p+3);
			}
			else {
				strcpy(slash+1, p+3);
			}
			continue;
		}
		break;
	}

	/* transform "/some/name/./blivit" to "/some/name/blivit" */
	while(p=strstr(path, "/.")) {
		if (p[2]=='/') {
			strcpy(p, p+2);
		}
		else {
			strcpy(p+1, p+2);
		}
	}

	/* remove any trailing '/' from path name */
	for(len = strlen(path); len > 1; len--) {
		p = &path[len-1];
		if (*p!='/') break;
		*p = 0;
	}

quit:
	// wtof("   %s: out path=\"%s\"", __func__, path);
	return path;
}

static int 
isdataset(const char *name)
{
	int		dataset = 0;
	int		levels  = 0;
	int		member  = 0;
	int		len;
	char	buf[256];
	char	*p;

	strcpy(buf, name);
	
	if (ismember(buf)) {
		/* looks like a single high level qualifier */
		if (!isdigit(buf[0])) {
			dataset = 1;
			goto quit;
		}
	}

	if (strstr(buf, "..")) goto quit;	/* can't have and empty dataset level name */
	
	for(p = strtok(buf, "."); p; p = strtok(NULL, ".")) {
		char *lparen = strchr(p, '(');
		char *rparen = strrchr(p, ')');
	
		// wtof("   %s: p=\"%s\"", __func__, p ? p : "(null)");
		if (lparen && rparen) {
			if (member) goto quit;		/* we've already seen a member so die */
			member++;
			*lparen = 0;
			*rparen = 0;
		}

		levels++;
		len = strlen(p);
		
		if (len < 1) goto quit;			/* dataset level name too short */
		if (len > 8) goto quit;			/* dataset level name too long */
		if (isdigit(p[0])) goto quit;	/* dataset level name can not start with a number */
		if (!ismember(p)) goto quit;	/* bad character in name */

		if (lparen && rparen) {
			if (strlen(lparen+1) > 8) goto quit;	/* dataset level name too long */
			if (!ismember(lparen+1)) goto quit;	/* bad character in name */

			*lparen = '(';
			*rparen = ')';
		}

	}

	/* name could be a dataset */
	if (levels <= 22) {
		int maxlen = 44;
		
		if (member > 1) goto quit;	/* only 1 member allowed */
		
		if (member) {
			maxlen += 10;	/* maxlen += strlen("(12345678)") */
		}

		if (strlen(name) <= maxlen) {
			dataset = 1;
		}
	}

quit:
	// wtof("   %s(\"%s\") %s", __func__, name, dataset ? "SUCCESS" : "FAILED");
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
	// wtof("   %s(\"%s\") %s", __func__, name, member ? "SUCCESS" : "FAILED");
	return member;
}

static char *
strupper(char *buf)
{
	int		i;
	
	if (!buf) goto quit;
	
	for(i=0; buf[i]; i++) {
		if (islower(buf[i])) {
			buf[i] = (char) toupper(buf[i]);
		}
	}

quit:
	return buf;
}

static int 
ispattern(const char *name, const char *pattern)
{
	int	rc = __patmat(name, pattern);
	
	// wtof("   %s(\"%s\",\"%s\") %s", __func__, name, pattern, rc ? "MATCHED" : "NOMATCH");
	
	return rc;
}

