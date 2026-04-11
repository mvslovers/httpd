/* HTTPJES2.C - CGI Program, REST style CGI program for access of JES2 resources */
#include "httpd.h"
#include "clibjes2.h"   /* JES prototypes */
#include "clibcp.h"     /* JES checkpoint struct */

#define httpx   (httpd->httpx)

static int do_status(HTTPD *httpd, HTTPC *httpc, const char *jobname, const char *jobid, int dd);
static int do_print(HTTPD *httpd, HTTPC *httpc, const char *jobname, const char *jobid);
static int do_cancel(HTTPD *httpd, HTTPC *httpc);
static int do_purge(HTTPD *httpd, HTTPC *httpc);
static int do_purge_all(HTTPD *httpd, HTTPC *httpc);
static int do_help(HTTPD *httpd, HTTPC *httpc);
static int getself(char *jobname, char *jobid);

int main(int argc, char **argv)
{
    int         rc      = 0;
    CLIBPPA     *ppa    = __ppaget();
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    char        *path   = NULL;
    char        *verb   = NULL;
    int         iverb   = 0;
#define VERB_STATUS     1
#define VERB_DDLIST     2   /* status for job including dataset's */
#define VERB_PRINT      3
#define VERB_VIEW       4
#define VERB_CANCEL     5
#define VERB_PURGE      6
#define VERB_PURGEALL   7
    char        *jobname= NULL;
    char        *jobid  = NULL;

    if (!httpd) {
        wtof("This program %s must be called by the HTTPD web server%s", argv[0], "");
        /* TSO callers might not see a WTO message, so we send a STDOUT message too */
        printf("This program %s must be called by the HTTPD web server%s", argv[0], "\n");
        return 12;
    }

    /* Get the query variables from the URL */
    if (!jobname) jobname = http_get_env(httpc, "QUERY_JOBNAME");
    if (!jobname) jobname = http_get_env(httpc, "QUERY_JOB");
    if (!jobname) jobname = http_get_env(httpc, "QUERY_J");

    if (!jobid) jobid = http_get_env(httpc, "QUERY_JOBID");
    if (!jobid) jobid = http_get_env(httpc, "QUERY_JID");

    /* get the request path string */
    path = http_get_env(httpc, "REQUEST_PATH");
    verb = strrchr(path, '/');
    if (http_cmp(verb, "/status")==0) {
        iverb = VERB_STATUS;
    }
    else if (http_cmp(verb, "/ddlist")==0) {
        iverb = VERB_DDLIST;
    }
    else if (http_cmp(verb, "/print")==0) {
        iverb = VERB_PRINT;
    }
    else if (http_cmp(verb, "/view")==0) {
        iverb = VERB_VIEW;
    }
    else if (http_cmp(verb, "/cancel")==0) {
        iverb = VERB_CANCEL;
    }
    else if (http_cmp(verb, "/purge")==0) {
        iverb = VERB_PURGE;
    }
    else if (http_cmp(verb, "/purgeall")==0) {
        iverb = VERB_PURGEALL;
    }

    /* process the verb */
    switch(iverb) {
    case VERB_STATUS:
        /* create status info as JSON object (no dataset list) */
        rc = do_status(httpd, httpc, jobname, jobid, 0);
        break;
    case VERB_DDLIST:
        /* create status info as JSON object (with dataset list) */
        rc = do_status(httpd, httpc, jobname, jobid, 1);
        break;
    case VERB_PRINT:
    case VERB_VIEW:
        /* print output for job (dsid==NULL) or specific dataset by id number */
        rc = do_print(httpd, httpc, jobname, jobid);
        break;
    case VERB_CANCEL:
        /* cancel the job */
        rc = do_cancel(httpd, httpc);
        break;
    case VERB_PURGE:
        /* purge job output */
        rc = do_purge(httpd, httpc);
        break;
    case VERB_PURGEALL:
        /* purge job output (all jobid's) */
        rc = do_purge_all(httpd, httpc);
        break;
    default:
        /* create help info */
        rc = do_help(httpd, httpc);
        break;
    }

quit:
    return 0;
}

static int do_status_ddlist(HTTPD *httpd, HTTPC *httpc, JESJOB *j, const char *in);

__asm__("\n&FUNC    SETC 'do_status'");
static int
do_status(HTTPD *httpd, HTTPC *httpc, const char *jobname, const char *jobid, int dd)
{
    int         rc      = 0;
	int			first   = 1;	/* first job status */
	int			tzadjust= 0;	/* time zone adjustment for local time to gmt time */
    const char  *filter = NULL;
    HASPCP      *cp     = NULL;
    JES         *jes    = NULL;
    JESJOB      **jobs  = NULL;
    JESFILT     jesfilt = FILTER_NONE;
    unsigned    count   = 0;
    double      start;
    double      end;
	__64		diff;
	__64		div;
	__64		mod;
    unsigned    n;
    int         i;
    char        jesinfo[20] = "unknown";
	time64_t	start_time;
	time64_t	end_time;
	const char  *smfid;

	httpsecs(&start);

	tzadjust = httpd->tzoffset * -1;	/* change sign as we want to convert local time to GMT */

    /* select filtering criteria */
    if (jobname && !jobid) {
        filter = jobname;
        jesfilt = FILTER_JOBNAME;
    }
    else if (jobid) {
        filter = jobid;
        jesfilt = FILTER_JOBID;
    }

    if (!filter) {
        filter = "";
        jesfilt = FILTER_NONE;
    }

    /* Open the JES2 checkpoint and spool datasets */
    jes = jesopen();
    if (!jes) {
        wtof("*** unable to open JES2 checkpoint and spool datasets ***");
        /* we don't quit here, instead we'll send back an empty JSON object */
    }

    if (jes) {
        cp = jes->cp;
        /* Get job info for all jobs */
        jobs = jesjob(jes, filter, jesfilt, dd);
    }

    if (cp && cp->buf) {
        for(i=0; i < sizeof(jesinfo); i++) {
            if (!isprint(cp->buf[i]) || cp->buf[i]=='\\' || cp->buf[i]=='"') {
                jesinfo[i] = '.';
                continue;
            }
            jesinfo[i] = cp->buf[i];
        }
        jesinfo[sizeof(jesinfo)-1]=0;
    }

    if (rc = http_resp(httpc,200) < 0) goto quit;
    if (rc = http_printf(httpc, "Cache-Control: no-store\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "Content-Type: %s\r\n", "application/json") < 0) goto quit;
    if (rc = http_printf(httpc, "Access-Control-Allow-Origin: *\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "\r\n") < 0) goto quit;

    count = array_count(&jobs);

    if (rc = http_printf(httpc, "{\n") < 0) goto quit;
    if (rc = http_printf(httpc, "  \"data\": [\n") < 0) goto quit;

    for(n=0; n < count; n++) {
        JESJOB *j = jobs[n];

        if (!j) continue;

        // if (j->q_flag2 & QUEINIT) continue; /* system log or batch initiator */

        /* although the QUEINIT flag *should* cover SYSLOG and INIT jobs, it sometimes doesn't */
        // if (strcmp(j->jobname, "SYSLOG")==0) continue;  /* system log */
        // if (strcmp(j->jobname, "INIT")==0) continue;    /* batch initiator */

        if (first) {
            /* first time we're printing this '{' so no ',' needed */
            rc = http_printf(httpc, "    {\n");
            first = 0;
        }
        else {
            /* all other times we need a ',' before the '{' */
            rc = http_printf(httpc, "   ,{\n");
        }

		/* adjust time's from local time to utc time */
#if 0		
		start_time = 0;
		end_time = 0;	
		if (j->start_time > 0) {
			start_time 	= j->start_time + tzadjust;
			if (j->end_time >= j->start_time) {
				end_time = j->end_time + tzadjust;
			}
		}
#else
		__64_init(&start_time);
		__64_init(&end_time);	
		if (__64_cmp_u32(&j->start_time64, 0) == __64_LARGER) {
			/* make start time local to start time GMT */
			__64_add_i32(&j->start_time64, tzadjust, &start_time);
			if (__64_cmp_u32(&j->end_time64, 0) == __64_LARGER) {
				/* make end time local to end time GMT */
				__64_add_i32(&j->end_time64, tzadjust, &end_time);
			}
		}
#endif
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "      \"jobname\": \"%s\",\n", j->jobname);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "      \"jobid\": \"%s\",\n", j->jobid);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "      \"owner\": \"%s\",\n", j->owner);
        if (rc < 0) goto quit;
#if 0
        if (j->eclass=='\\') {
            rc = http_printf(httpc, "      \"class\": \"\\\\\",\n");
        }
#endif
        if (j->q_type & _XEQ) {
            rc = http_printf(httpc, "      \"class\": \"XEQ\",\n");
        }
        else if (isalnum(j->eclass)) {
            rc = http_printf(httpc, "      \"class\": \"%c\",\n", j->eclass);
        }
        else {
            rc = http_printf(httpc, "      \"class\": \"0x%02X\",\n", j->eclass);
        }
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "      \"priority\": \"%u\",\n", j->priority >> 4);
        if (rc < 0) goto quit;

#if 1 /* new code - "status" and "type" */
        if (j->q_type) {
            const char *p = "UNKNOWN";
            if (j->q_type & _XEQ) {
                p = "ACTIVE";
            }
            else if (j->q_type & _INPUT) {
                p = "INPUT";
            }
            else if (j->q_type & _XMIT) {
                p = "XMIT";
            }
            else if (j->q_type & _SETUP) {
                p = "SETUP";
            }
            else if (j->q_type & _RECEIVE) {
                p = "RECEIVE";
            }
            else if (j->q_type & _OUTPUT || j->q_type & _HARDCPY) {
                p = "OUTPUT";
            }
            rc = http_printf(httpc, "      \"status\": \"%s\",\n", p);
            if (rc < 0) goto quit;
        }

        if (j->jobid) {
            const char *p = "UNKNOWN";
            if (memcmp(j->jobid, "JOB", 3)==0) {
                p = "JOB";
            }
            else if (memcmp(j->jobid, "STC", 3)==0) {
                p = "STC";
            }
            else if (memcmp(j->jobid, "TSU", 3)==0) {
                p = "TSU";
            }
            rc = http_printf(httpc, "      \"type\": \"%s\",\n", p);
            if (rc < 0) goto quit;
        }
#endif /* new code - "status" and "type" */

#if 0
        rc = http_printf(httpc, "      \"start_stamp\": \"%u\",\n", start_time);
        if (rc < 0) goto quit;
        if (start_time) {
            rc = http_printf(httpc, "      \"start_display\": \"%-24.24s\",\n", ctime(&start_time) );
        }
        else {
            rc = http_printf(httpc, "      \"start_display\": \"...\",\n");
        }
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "      \"end_stamp\": \"%u\",\n", end_time);
        if (rc < 0) goto quit;
        if (end_time) {
            rc = http_printf(httpc, "      \"end_display\": \"%-24.24s\",\n", ctime(&end_time) );
        }
        else {
            rc = http_printf(httpc, "      \"end_display\": \"...\",\n" );
        }
        if (rc < 0) goto quit;
#else
        rc = http_printf(httpc, "      \"start_stamp\": \"%llu\",\n", start_time);
        if (rc < 0) goto quit;
        if (__64_cmp_u32(&start_time, 0) != __64_EQUAL) {
            rc = http_printf(httpc, "      \"start_display\": \"%-24.24s\",\n", ctime64(&start_time) );
        }
        else {
            rc = http_printf(httpc, "      \"start_display\": \"...\",\n");
        }
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "      \"end_stamp\": \"%llu\",\n", end_time);
        if (rc < 0) goto quit;
        if (__64_cmp_u32(&end_time, 0) != __64_EQUAL) {
            rc = http_printf(httpc, "      \"end_display\": \"%-24.24s\",\n", ctime64(&end_time) );
        }
        else {
            rc = http_printf(httpc, "      \"end_display\": \"...\",\n" );
        }
        if (rc < 0) goto quit;
#endif
        rc = do_status_ddlist(httpd, httpc, j, "      ");
        if (rc < 0) goto quit;
        /* rc = http_printf(httpc, "    }%s\n", (n+1) < count ? ",":""); */
        rc = http_printf(httpc, "    }\n");
        if (rc < 0) goto quit;
    }

    rc = http_printf(httpc, "  ],\n");
    if (rc < 0) goto quit;
    rc = http_printf(httpc, "  \"debug\": {\n");
    if (rc < 0) goto quit;
    rc = http_printf(httpc, "    \"program\": \"%s\",\n", "HTTPJES2");
    if (rc < 0) goto quit;

	smfid = (const char *)__smfid();
	if (smfid) {
	    rc = http_printf(httpc, "    \"node\": \"%s\",\n", smfid);
		if (rc < 0) goto quit;
	}

    rc = http_printf(httpc, "    \"jes\": \"%s\",\n", 
        jes ? "checkpoint and spool opened" : "unable to open checkpoint and spool");
    if (rc < 0) goto quit;
    rc = http_printf(httpc, "    \"jesinfo\": \"%s\",\n", jesinfo);
    if (rc < 0) goto quit;
    rc = http_printf(httpc, "    \"results\": \"%u jobs found\",\n", count);
    if (rc < 0) goto quit;
    rc = http_printf(httpc, "    \"filter\": \"%s\",\n", filter);
    if (rc < 0) goto quit;
    rc = http_printf(httpc, "    \"jesfilt\": \"%s\",\n", 
        jesfilt==FILTER_NONE ? "NONE" : jesfilt==FILTER_JOBNAME ? "JOBNAME" : 
        jesfilt==FILTER_JOBID ? "JOBID" : "unknown");
    if (rc < 0) goto quit;
    rc = http_printf(httpc, "    \"ddlist\": \"%s\",\n", dd ? "true" : "false");
    if (rc < 0) goto quit;

	httpsecs(&end);
    rc = http_printf(httpc, "    \"elapsed\": \"%f\"\n", end - start);
    
    if (rc < 0) goto quit;
    rc = http_printf(httpc, "  }\n");
    if (rc < 0) goto quit;
    rc = http_printf(httpc, "}\n");

quit:
    if (jobs) jesjobfr(&jobs);
    if (jes) jesclose(&jes);
    return rc;
}

__asm__("\n&FUNC    SETC 'do_status_ddlist'");
static int
do_status_ddlist(HTTPD *httpd, HTTPC *httpc, JESJOB *j, const char *indent)
{
    int             rc      = 0;
    const char      *in     = indent;
    unsigned int    count   = array_count(&j->jesdd);
    unsigned int    n;
    int             i;
    char            recfm[12];

    rc = http_printf(httpc, "%s\"dd\": [\n", in);
    if (rc < 0) goto quit;

    for(n=0; n < count; n++) {
        JESDD *dd = j->jesdd[n];

        if (!dd) continue;

        if (dd->ddname[0] == ' ') dd->ddname[0] = 0;
        rc = http_printf(httpc, "%s  {\n", in);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "%s    \"ddname\": \"%s\",\n", in, dd->ddname);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "%s    \"stepname\": \"%s\",\n", in, dd->stepname);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "%s    \"procstep\": \"%s\",\n", in, dd->procstep);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "%s    \"dsid\": \"%u\",\n", in, dd->dsid);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "%s    \"dsname\": \"%s\",\n", in, dd->dsname);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "%s    \"class\": \"%c\",\n", in, dd->oclass);
        if (rc < 0) goto quit;

        i = 0;
        if ((dd->recfm & RECFM_U) == RECFM_U) {
            recfm[i++] = 'U';
        }
        else if ((dd->recfm & RECFM_F) == RECFM_F) {
            recfm[i++] = 'F';
        }
        else if ((dd->recfm & RECFM_V) == RECFM_V) {
            recfm[i++] = 'V';
        }

        if (dd->recfm & RECFM_BR) {
            recfm[i++] = 'B';
        }

        if (dd->recfm & RECFM_CA) {
            recfm[i++] = 'A';
        }
        else if (dd->recfm & RECFM_CM) {
            recfm[i++] = 'M';
        }

        if (recfm[0] == 'V' && (dd->recfm & RECFM_SB)) {
            recfm[i++] = 'S';   /* spanned records */
        }
        recfm[i] = 0;
        rc = http_printf(httpc, "%s    \"recfm\": \"%s\",\n", in, recfm);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "%s    \"records\": \"%u\",\n", in, dd->records);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "%s    \"lrecl\": \"%u\",\n", in, dd->lrecl);
        if (rc < 0) goto quit;
        rc = http_printf(httpc, "%s    \"mttr\": \"%08X\"\n", in, dd->mttr);
        if (rc < 0) goto quit;

        rc = http_printf(httpc, "%s  }%s\n", in, (n+1) < count ? "," : "");
        if (rc < 0) goto quit;
    }

    rc = http_printf(httpc, "%s]\n");
quit:
    return rc;
}

static int do_print_sysout(HTTPD *httpd, HTTPC *httpc, JES *jes, JESJOB *job, unsigned **dsid);
__asm__("\n&FUNC    SETC 'do_print'");
static int
do_print(HTTPD *httpd, HTTPC *httpc, const char *jobname, const char *jobid)
{
    const char  *filter = NULL;
    JES         *jes    = NULL;
    JESJOB      **jobs  = NULL;
    JESFILT     jesfilt = FILTER_NONE;
    char        *p      = NULL;
    char        *buf    = NULL;
    unsigned    **dsid  = NULL;
    int         download= 0;
    unsigned    count   = 0;
    unsigned    n;
    int         i;
    int         rc;

    /* we'll send all job output unless one or more dataset id's are specified */
    p = http_get_env(httpc, "QUERY_DSID");
    if (p) {
        buf = calloc(1, strlen(p)+1);
        if (buf) {
            strcpy(buf, p);
            for (p = buf; p && *p; ) {
                while (*p == ' ' || *p == ',') p++;
                if (!*p) break;
                n = (unsigned) atoi(p);
                if (n) {
                    array_add(&dsid, (void*)n);
                }
                while (*p && *p != ' ' && *p != ',') p++;
            }
        }
    }

    /* we'll send headers to download if '?download=[yes|true|number>0]' */
    p = http_get_env(httpc, "QUERY_DOWNLOAD");
    if (p) {
        if (http_cmp(p, "yes")==0 || http_cmp(p, "true")==0) {
            download = 1;
        }
        else if (isdigit(*p)) {
            download = atoi(p);
        }
    }

    /* send the headers */
    if (rc = http_resp(httpc,200) < 0) goto quit;
    if (rc = http_printf(httpc, "Cache-Control: no-store\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "Content-Type: %s\r\n", "text/plain") < 0) goto quit;
    if (!download) {
        rc = http_printf(httpc, "\r\n");
        if (rc < 0) goto quit;
    }

    /* select filtering criteria */
    if (jobname && !jobid) {
        filter = jobname;
        jesfilt = FILTER_JOBNAME;
    }
    else if (jobid) {
        filter = jobid;
        jesfilt = FILTER_JOBID;
    }

    /* we require that a job name or job id be specified */
    if (!filter) {
        if (download) http_printf(httpc, "\r\n");
        rc = http_printf(httpc, "*** No JOB specified ***\n");
        goto quit;
    }

    /* wildcard characters are not allowed for print/view */
    if (strchr(filter, '*') || strchr(filter, '?')) {
        if (download) http_printf(httpc, "\r\n");
        rc = http_printf(httpc, "*** Wildcard * and ? characters are not allowed ***\n");
        goto quit;
    }

    /* Open the JES2 checkpoint and spool datasets */
    jes = jesopen();
    if (!jes) {
        if (download) http_printf(httpc, "\r\n");
        rc = http_printf(httpc, "%s unable to open JES2 checkpoint and spool datasets\n", "HTTPJES2");
        goto quit;
    }

    /* Get job info for job(s) */
    jobs = jesjob(jes, filter, jesfilt, 1);
    if (!jobs) {
        if (download) http_printf(httpc, "\r\n");
        rc = http_printf(httpc, "*** No jobs found for filter \"%s\" ***\n", filter);
        goto quit;
    }

    /* see if we have anything to send back */
    count = array_count(&jobs);
    for(n=0; n < count; n++) {
        JESJOB *j = jobs[n];

        if (!j) continue;

        // if (j->q_flag2 & QUEINIT) continue; /* system log or batch initiator */

        /* although the QUEINIT flag *should* cover SYSLOG and INIT jobs, it sometimes doesn't */
        // if (strcmp(j->jobname, "SYSLOG")==0) continue;  /* system log */
        // if (strcmp(j->jobname, "INIT")==0) continue;    /* batch initiator */

        if (download) {
            /* send headers so browser will save the data we send to a file */
            download = 0;   /* turn off so we don't send this again */
            rc = http_printf(httpc, "Content-Disposition: attachment; filename=\"%s.%s.TXT\"\r\n", j->jobname, j->jobid);
            if (rc < 0) goto quit;
            rc = http_printf(httpc, "Content-Transfer-Encoding: binary\r\n");
            if (rc < 0) goto quit;
            rc = http_printf(httpc, "Cache-Control: private\r\n");
            if (rc < 0) goto quit;
            rc = http_printf(httpc, "Pragma: private\r\n");
            if (rc < 0) goto quit;
            rc = http_printf(httpc, "\r\n");
            if (rc < 0) goto quit;
        }

        /* print job sysout */
        rc = do_print_sysout(httpd, httpc, jes, j, dsid);
        if (rc < 0) goto quit;
    }

quit:
    if (download && rc >= 0) {
        /* we didn't send any data, so finish the header */
        http_printf(httpc, "\r\n");
    }
    if (buf) free(buf);
    if (dsid) array_free(&dsid);
    if (jobs) jesjobfr(&jobs);
    if (jes) jesclose(&jes);
    return 0;
}

__asm__("\n&FUNC    SETC 'ifdsid'");
static int
ifdsid(HTTPD *httpd, unsigned dsid, unsigned **array)
{
    int         rc = 0;
    unsigned    count;
    unsigned    n;

    if (array) {
        /* match dsid against array of DSID's */
        count = array_count(&array);
        for(n=0; n < count; n++) {
            unsigned id = (unsigned)array[n];
            if (!id) continue;

            if (dsid==id) {
                /* matched */
                rc = 1;
                break;
            }
        }
        goto quit;
    }

    /* no array of DSID's */
    if (dsid < 2) goto quit;                        /* don't show input JCL         */
    if (dsid > 4 && dsid < 100) goto quit;          /* don't show interpreted JCL   */

    /* assume this dsid is okay */
    rc = 1;

quit:
    return rc;
}

static int do_print_sysout_line(const char *line, unsigned linelen);
__asm__("\n&FUNC    SETC 'do_print_sysout'");
static int
do_print_sysout(HTTPD *httpd, HTTPC *httpc, JES *jes, JESJOB *job, unsigned **dsid)
{
    int         rc  = 0;
    unsigned    count;
    unsigned    n;

    // wtodumpf(job, sizeof(JESJOB), "%s: JESJOB", __func__);

    count = array_count(&job->jesdd);
    for(n=0; n < count; n++) {
        JESDD *dd = job->jesdd[n];

        if (!dd) continue;

        if (!dd->mttr) continue;                        /* no spool data for this dd    */

        if (!ifdsid(httpd, dd->dsid, dsid)) continue;   /* skip this sysout dataset */

        if ((dd->flag & FLAG_SYSIN) && !dsid) continue;   /* don't show SYSIN data        */

        rc = jesprint(jes, job, dd->dsid, do_print_sysout_line);
        if (rc < 0) goto quit;

        rc = http_printf(httpc, "- - - - - - - - - - - - - - - - - - - - "
                                "- - - - - - - - - - - - - - - - - - - - "
                                "- - - - - - - - - - - - - - - - - - - - "
                                "- - - - - -\r\n");
        if (rc < 0) goto quit;
    }

quit:
    return rc;
}

__asm__("\n&FUNC    SETC 'do_print_sysout_line'");
static int
do_print_sysout_line(const char *line, unsigned linelen)
{
    int         rc      = 0;
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;

    rc = http_printf(httpc, "%-*.*s\r\n", linelen, linelen, line);

    return rc;
}

typedef struct findjob {
    HTTPD   *httpd;
    char    *jobid;
    char    *jobname;
} FINDJOB;

__asm__("\n&FUNC    SETC 'findjobname'");
static int findjobname(unsigned index, void *item, void *udata)
{
    JESJOB *jesjob = item;
    FINDJOB *findjob = udata;
    HTTPD  *httpd = findjob->httpd;

    if (http_cmp(findjob->jobid, jesjob->jobid)==0) {
        findjob->jobname = jesjob->jobname;
        return 1;
    }

    return 0;
}

__asm__("\n&FUNC    SETC 'do_cancel_ex'");
static int
do_cancel_ex(HTTPD *httpd, HTTPC *httpc, const char *verb, int purge, int all)
{
    int         rc          = 0;
    char        *method     = http_get_env(httpc, "REQUEST_METHOD");
    char        *jobname    = http_get_env(httpc, "POST_JOBNAME");
    char        *jobid      = http_get_env(httpc, "POST_JOBID");
    JES         *jes        = NULL;
    char        **jobids    = NULL;
    JESJOB      **jobs      = NULL;
    JESJOB      *job        = NULL;
    unsigned    jcount      = 0;
    unsigned    jn          = 0;
    unsigned    count       = 0;
    unsigned    n           = 0;
    char        *buf        = NULL;
    char        *p;
    const char  *msg        = "";
    char        thisjob[12] = "";
    char        thisid[12]  = "";
    FINDJOB     findjob     = {httpd,NULL,NULL};

    if (!jobname) jobname = http_get_env(httpc, "POST_JOB");
    if (!jobname) jobname = http_get_env(httpc, "POST_J");
    if (!jobid) jobid = http_get_env(httpc, "POST_JID");

    /* get jobname and jobid of this address space */
    getself(thisjob, thisid);
#if 0
    wtof("thisjob=%s, thisid=%s", thisjob, thisid);
#endif
    if (!method || http_cmp(method, "POST")!=0) {
        /* we require POST method for this action */
        rc = http_resp_not_implemented(httpc);
        goto quit;
    }

    /* send the headers */
    if (rc = http_resp(httpc,200) < 0) goto quit;
    if (rc = http_printf(httpc, "Cache-Control: no-store\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "Content-Type: %s\r\n", "text/plain") < 0) goto quit;
    if (rc = http_printf(httpc, "\r\n") < 0) goto quit;

    if (!jobname && !all) {
        rc = http_printf(httpc, "Jobname: \"null\" <=== missing value\r\n");
        if (rc < 0) goto quit;
    }
    if (!jobid) {
        rc = http_printf(httpc, "Jobid: \"null\" <=== missing value\r\n");
        if (rc < 0) goto quit;
    }

    /* convert string of jobid's into array of jobid's */
    buf = calloc(1, strlen(jobid)+1);
    if (buf) {
        strcpy(buf, jobid);
        for (p = buf; p && *p; ) {
            while (*p == ',') p++;
            if (!*p) break;
            UCHAR *end = strchr(p, ',');
            if (end) *end++ = 0;
            array_add(&jobids, p);
            p = end;
        }
    }

    /* make sure we have something to do */
    count = arraycount(&jobids);
    if (!count) {
        rc = http_printf(httpc, "%s requested but no jobid's specified\r\n\r\n", verb);
        goto quit;
    }

    /* ignore jobname if we're processing all jobid's */
    if (all) {
        /* Open the JES2 checkpoint and spool datasets */
        jes = jesopen();
        if (!jes) {
            wtof("*** unable to open JES2 checkpoint and spool datasets ***");
            /* we don't quit here, instead we'll send back an empty JSON object */
        }

        if (jes) {
            /* Get job info for all jobs */
            jobs = jesjob(jes, NULL, FILTER_NONE, 0);
            jcount = arraycount(&jobs);
        }
    }

    for(n=0; n < count; n++) {
        jobid = jobids[n];
        /* is cancel/purge for this job? */
        if (http_cmp(jobid, thisid)==0) {
            /* we can't allow that */
            rc = http_printf(httpc, "%s not allowed for %s %s\r\n\r\n", verb, thisjob, thisid);
            arraydel(&jobids, n+1);
            count--;
            continue; /* skip this jobid */
        }

        if (all) {
            /* we need to find the jobname for the jobid we're about to process */
            jobname = NULL;
            findjob.jobid = jobid;
            rc = arrayeach(&jobs, findjobname, &findjob);
            if (rc) jobname = findjob.jobname;
        }

        /* cancel the job/stc/tsu */
        rc = jescanj(jobname, jobid, purge);

        switch(rc) {
        case CANJ_OK:
            msg = "CANCEL or PURGE COMPLETED";
            break;
        case CANJ_NOJB:
            msg = "JOB NAME NOT FOUND";
            break;
        case CANJ_BADI:
            msg = "INVALID JOBNAME/JOB ID COMBINATION";
            break;
        case CANJ_NCAN:
            msg = "JOB NOT CANCELLED\r\nDUPLICATE JOBNAMES AND NO JOB ID GIVEN";
            break;
        case CANJ_SMALL:
            msg = "STATUS ARRAY TOO SMALL";
            break;
        case CANJ_OUTP:
            msg = "JOB NOT CANCELLED or PURGED\r\nJOB ON OUTPUT QUEUE";
            break;
        case CANJ_SYNTX:
            msg = "JOBID WITH INVALID SYNTAX FOR SUBSYSTEM";
            break;
        case CANJ_ICAN:
            msg = "INVALID CANCEL or PURGE REQUEST\r\n"
                  "CANNOT CANCEL or PURGE AN ACTIVE TSO USER OR STARTED TASK\r\n"
                  "TSO USER MAY NOT CANCEL or PURGE THE ABOVE JOBS UNLESS THEY ARE ON AN OUTPUT QUEUE.";
            break;
        }
        rc = http_printf(httpc, "%s of job %s %s rc=%d\r\n%s\r\n\r\n", verb, jobname ? jobname : "=======>", jobid, rc, msg);
        if (rc < 0) goto quit;
    }

quit:
    if (buf)    free(buf);
    if (jobids) arrayfree(&jobids);
    if (jobs)   jesjobfr(&jobs);
    if (jes)    jesclose(&jes);

    return 0;
}

__asm__("\n&FUNC    SETC 'do_cancel'");
static int
do_cancel(HTTPD *httpd, HTTPC *httpc)
{
    return do_cancel_ex(httpd, httpc, "Cancel", 0, 0);
}

__asm__("\n&FUNC    SETC 'do_purge'");
static int
do_purge(HTTPD *httpd, HTTPC *httpc)
{
    return do_cancel_ex(httpd, httpc, "Purge", 1, 0);
}

__asm__("\n&FUNC    SETC 'do_purge_all'");
static int
do_purge_all(HTTPD *httpd, HTTPC *httpc)
{
    return do_cancel_ex(httpd, httpc, "Purge", 1, 1);
}

const char *help_text[] = {
    "Supported URL (REST) verbs are:",
    "/cancel       Cancel job (POST method only)",
    "/ddlist       Returns JSON information about job/stc/tsu including dd list",
    "/help         Display this text",
    "/print        Returns requested output as plan text",
    "/purge        Cancel job and purge output (POST method only)",
    "/purgeall     Cancel job's and purge output (POST method only)",
    "/status       Returns JSON information about job/stc/tsu (no dd list)",
    "/view         Same as /print",
    " ",
    "Supported query strings (?name=value&name2=value2&...) are:",
    "?jobname=...  Name or pattern for job/stc/tsu (also:job= or j=)",
    "?jobid=...    Job/Stc/Tsu identifier or pattern (also:jid=)",
    "?dsid=...     Dataset identifier (/print or /view only)",
    "?download=... Download boolean: yes/true/non-zero number (/print or /view only)",
    " ",
    "Example url:  \".../status?job=herc*\"\n"
    "              Return JSON status for job/stc/tsu names starting with HERC.",
    " ",
    "              \".../print?jobid=job12345\"\n"
    "              Return all job output for job id JOB12345.",
    " ",
    "              \".../print?jobid=stc45678&dsid=2,4\"\n"
    "              Return datasets 2 and 4 from started task STC45678.",
    " ",
    "              \".../print?jobid=stc45678&dsid=2,4&download=1\"\n"
    "              Return datasets 2 and 4 from started task STC45678 with your browser saving the output as a file.",
    NULL    /* must be last item in this array */
};
__asm__("\n&FUNC    SETC 'do_help'");
static int
do_help(HTTPD *httpd, HTTPC *httpc)
{
    int     rc      = 0;
    char    *path   = http_get_env(httpc, "REQUEST_PATH");
    char    *verb   = path ? strrchr(path, '/') : "";
    int     i;

    /* send HTTP headers */
    if (rc = http_resp(httpc,200) < 0) goto quit;
    if (rc = http_printf(httpc, "Cache-Control: no-store\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "Content-Type: %s\r\n", "text/plain") < 0) goto quit;
    if (rc = http_printf(httpc, "\r\n") < 0) goto quit;

    /* Tell them what we are */
    if (rc = http_printf(httpc, "HTTPJES2 Help\r\n\r\n") < 0) goto quit;
    if (http_cmp(verb, "/help")!=0) {
        /* they didn't ask for help, so tell them we don't understand the request */
        rc = http_printf(httpc,
            "The URL you entered \"%s\" is not understood by the HTTPJES2 cgi program.\r\n\r\n", path);
        if (rc < 0) goto quit;
    }

    /* send the help text */
    for(i=0; help_text[i]; i++) {
        rc = http_printf(httpc, "%s\r\n", help_text[i]);
        if (rc < 0) goto quit;
    }

quit:
    return rc;
}

__asm__("\n&FUNC    SETC 'getself'");
static int
getself(char *jobname, char *jobid)
{
    unsigned    *psa        = (unsigned*)0;
    unsigned    *tcb        = (unsigned*)psa[540/4];    /* A(current TCB) */
    unsigned    *jscb       = (unsigned*)tcb[180/4];    /* A(JSCB) */
    unsigned    *ssib       = (unsigned*)jscb[316/4];   /* A(SSIB) */

    const char  *name       = (const char*)tcb[12/4];   /* A(TIOT), but the job name is first 8 chars of TIOT so we cheat just a bit */
    const char  *id         = ((const char*)ssib) + 12; /* jobid is in SSIB at offset 12 */
    int         i;

    for(i=0; i < 8 && name[i] > ' '; i++) {
        jobname[i] = name[i];
    }
    jobname[i] = 0;

    for(i=0; i < 8 && id[i] >= ' '; i++) {
        jobid[i] = id[i];
        /* the job id may have space(s) which should be '0' */
        if (jobid[i]==' ') jobid[i] = '0';
    }
    jobid[i] = 0;

    return 0;
}
