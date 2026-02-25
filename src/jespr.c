/* JESPR.C - CGI Program, print sysout dataset from JES2 spool */
#include "httpd.h"
#include "clibjes2.h"   /* JES prototypes */

#define httpx   (httpd->httpx)

static int print_sysout(HTTPD *httpd, JES *jes, JESJOB *job, unsigned **dsid);
static int prtline(const char *line, unsigned linelen);

int main(int argc, char **argv)
{
    CLIBPPA     *ppa    = __ppaget();
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    char        *filter = NULL;
    JES         *jes    = NULL;
    JESJOB      **jobs  = NULL;
    JESFILT     jesfilt = FILTER_NONE;
    char        *p      = NULL;
    char        *buf    = NULL;
    unsigned    **dsid  = NULL;
    unsigned    count   = 0;
    unsigned    n;
    int         i;
    int         rc;

    if (httpd) {
        http_resp(httpc,200);
        http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
        http_printf(httpc, "\r\n");

        if (!filter) filter = http_get_env(httpc, "QUERY_J");
        if (!filter) filter = http_get_env(httpc, "QUERY_JO");
        if (!filter) filter = http_get_env(httpc, "QUERY_JOB");
        if (filter) {
            jesfilt = FILTER_JOBNAME;
        }
        else {
            filter = http_get_env(httpc, "QUERY_JOBID");
            if (!filter) filter = http_get_env(httpc, "QUERY_ID");
            if (!filter) filter = http_get_env(httpc, "QUERY_JID");
            if (filter) jesfilt = FILTER_JOBID;
        }

        p = http_get_env(httpc, "QUERY_DSID");
        if (p) {
            buf = calloc(1, strlen(p)+1);
            if (buf) {
                strcpy(buf, p);
                for(p=strtok(buf, " ,"); p; p = strtok(NULL, " ,")) {
                    n = (unsigned) atoi(p);
                    if (n) {
                        array_add(&dsid, (void*)n);
                    }
                }
            }
        }
    }
    else if (argc > 1) {
        filter = argv[1];
        if (filter) jesfilt = FILTER_JOBNAME;
    }

    if (!filter) {
        printf("*** No JOB specified ***\n");
        goto quit;
    }
    if (strchr(filter, '*') || strchr(filter, '?')) {
        printf("*** Wildcard * and ? characters are not allowed ***\n");
        goto quit;
    }

    /* Open the JES2 checkpoint and spool datasets */
    jes = jesopen();
    if (!jes) {
        printf("%s unable to open JES2 checkpoint and spool datasets\n", argv[0]);
        goto quit;
    }

    /* Get job info for job(s) */
    jobs = jesjob(jes, filter, jesfilt, 1);
    if (!jobs) {
        printf("*** No jobs found for filter \"%s\" ***\n", filter);
        goto quit;
    }

    count = array_count(&jobs);
    for(n=0; n < count; n++) {
        JESJOB *j = jobs[n];

        if (!j) continue;

        if (j->q_flag2 & QUEINIT) continue; /* system log or batch initiator */

        /* although the QUEINIT flag *should* cover SYSLOG and INIT jobs, it sometimes doesn't */
        if (strcmp(j->jobname, "SYSLOG")==0) continue;  /* system log */
        if (strcmp(j->jobname, "INIT")==0) continue;    /* batch initiator */

        /* print job sysout */
        print_sysout(httpd, jes, j, dsid);
    }

quit:
    if (buf) free(buf);
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

__asm__("\n&FUNC    SETC 'print_sysout'");
static int
print_sysout(HTTPD *httpd, JES *jes, JESJOB *job, unsigned **dsid)
{
    int         rc  = 0;
    unsigned    count;
    unsigned    n;

    count = array_count(&job->jesdd);
    for(n=0; n < count; n++) {
        JESDD *dd = job->jesdd[n];

        if (!dd) continue;

        if (!dd->mttr) continue;                        /* no spool data for this dd    */

        if (!ifdsid(httpd, dd->dsid, dsid)) continue;   /* skip this sysout dataset */

        if ((dd->flag & FLAG_SYSIN) && !dsid) continue;   /* don't show SYSIN data        */

        rc = jesprint(jes, job, dd->dsid, prtline);
        printf("- - - - - - - - - - - - - - - - - - - - "
               "- - - - - - - - - - - - - - - - - - - - "
               "- - - - - - - - - - - - - - - - - - - - "
               "- - - - - -\n");
    }

    return 0;
}

__asm__("\n&FUNC    SETC 'prtline'");
static int prtline(const char *line, unsigned linelen)
{
    printf("%-*.*s\n", linelen, linelen, line);
    return 0;
}
