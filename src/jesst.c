/* JESST.C - CGI Program, display JOE from JES checkpoint */
#include "httpd.h"
#include "clibjes2.h"   /* JES prototypes */

#define httpx   (httpd->httpx)

static int print_dd(HTTPD *httpd, JESJOB *j, const char *in);
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
    int         dd      = 0;
    JESFILT     jesfilt = FILTER_NONE;
    char        *p      = NULL;
    unsigned    count   = 0;
    double      start   = 0.0;
    double      end     = 0.0;
    unsigned    n;
    int         i;
    int         rc;
#if 0
    struct {
        unsigned short  len;
        char            text[0];
    }           *parm   = grt->grtptrs[0];
#endif

    if (httpd) {
        httpsecs(&start);

        if (!filter) filter = http_get_env(httpc, "QUERY_J");
        if (!filter) filter = http_get_env(httpc, "QUERY_JO");
        if (!filter) filter = http_get_env(httpc, "QUERY_JOB");
        if (!filter) filter = http_get_env(httpc, "QUERY_JOBS");
        if (filter) {
            jesfilt = FILTER_JOBNAME;
        }
        else {
            if (!filter) filter = http_get_env(httpc, "QUERY_ID");
            if (!filter) filter = http_get_env(httpc, "QUERY_JOBID");
            if (filter) {
                jesfilt = FILTER_JOBID;
            }
        }

        p = http_get_env(httpc, "QUERY_DD");
        if (!p) p = http_get_env(httpc, "QUERY_DDLIST");
        if (p) {
            if (http_cmp(p, "yes")==0) dd = 1;
            else if (http_cmp(p, "true")==0) dd=1;
            else if (isdigit(*p)) dd = atoi(p);
        }
    }
    else if (argc > 1) {
        filter = argv[1];
        jesfilt = FILTER_JOBNAME;
    }
    if (!filter) filter = "";

    /* Open the JES2 checkpoint and spool datasets */
    jes = jesopen();
    if (!jes) {
        wtof("%s unable to open JES2 checkpoint and spool datasets", argv[0]);
        /* we don't quit here, instead we'll send back an empty object */
    }

    if (jes) {
        /* Get job info for all jobs */
        jobs = jesjob(jes, filter, jesfilt, dd);
    }

    if (httpd) {
        http_resp(httpc,200);
        http_printf(httpc, "Content-Type: %s\r\n", "application/json");
        http_printf(httpc, "Access-Control-Allow-Origin: *\r\n");
        http_printf(httpc, "\r\n");
    }

    count = array_count(&jobs);

    printf("{\n");
    printf("  \"data\": [\n");

    for(n=0; n < count; n++) {
        JESJOB *j = jobs[n];

        if (!j) continue;

        if (j->q_flag2 & QUEINIT) continue; /* system log or batch initiator */

        /* although the QUEINIT flag *should* cover SYSLOG and INIT jobs, it sometimes doesn't */
        if (strcmp(j->jobname, "SYSLOG")==0) continue;  /* system log */
        if (strcmp(j->jobname, "INIT")==0) continue;    /* batch initiator */

        printf("    {\n");
        printf("      \"jobname\": \"%s\",\n", j->jobname);
        printf("      \"jobid\": \"%s\",\n", j->jobid);
        printf("      \"owner\": \"%s\",\n", j->owner);
        if (j->eclass=='\\') {
            printf("      \"class\": \"\\\\\",\n");
        }
        else {
            printf("      \"class\": \"%c\",\n", j->eclass);
        }
        printf("      \"priority\": \"%u\",\n", j->priority >> 4);
#if 0
        printf("      \"start_stamp\": \"%u\",\n", j->start_time );
        if (j->start_time) {
            printf("      \"start_display\": \"%-24.24s\",\n", ctime(&j->start_time) );
        }
        else {
            printf("      \"start_display\": \"...\",\n");
        }
        printf("      \"end_stamp\": \"%u\",\n", j->end_time);
        if (j->end_time) {
            printf("      \"end_display\": \"%-24.24s\",\n", ctime(&j->end_time) );
        }
        else {
            printf("      \"end_display\": \"...\",\n" );
        }
#else
        printf("      \"start_stamp\": \"%llu\",\n", j->start_time64 );
        if (__64_cmp_u32(&j->start_time64, 0) != __64_EQUAL) {
            printf("      \"start_display\": \"%-24.24s\",\n", ctime64(&j->start_time64) );
        }
        else {
            printf("      \"start_display\": \"...\",\n");
        }
        printf("      \"end_stamp\": \"%llu\",\n", j->end_time64);
        if (__64_cmp_u32(&j->end_time64, 0) != __64_EQUAL) {
            printf("      \"end_display\": \"%-24.24s\",\n", ctime64(&j->end_time64) );
        }
        else {
            printf("      \"end_display\": \"...\",\n" );
        }
#endif
        print_dd(httpd, j, "      ");
        printf("    }%s\n", (n+1) < count ? ",":"");
    }

    printf("  ],\n");
    printf("  \"debug\": {\n");
    printf("    \"program\": \"%s\",\n", argv[0]);
    printf("    \"jes\": \"%s\",\n", jes ? "checkpoint and spool opened" : "unable to open checkpoint and spool");
    printf("    \"results\": \"%u jobs found\",\n", count);
    printf("    \"filter\": \"%s\",\n", filter);
    printf("    \"jesfilt\": \"%s\",\n", jesfilt==FILTER_NONE ? "NONE" : jesfilt==FILTER_JOBNAME ? "JOBNAME" : jesfilt==FILTER_JOBID ? "JOBID" : "unknown");
    printf("    \"ddlist\": \"%s\",\n", dd ? "true" : "false");
    if (httpd) httpsecs(&end);
    printf("    \"elapsed\": \"%f\"\n", end - start);
    printf("  }\n");
    printf("}\n");

#if 0
    for(n=0; n < count; n++) {
        JESJOB *j = jobs[n];

        if (!j) continue;
        if (strcmp(j->jobname, "BSPPILOT")==0) {
            rc = jesprint(jes, j, 1, prtline);
            rc = jesprint(jes, j, 2, prtline);
            rc = jesprint(jes, j, 3, prtline);
            rc = jesprint(jes, j, 4, prtline);
            rc = jesprint(jes, j, 5, prtline);
            break;
        }
    }
#endif

quit:
    if (jobs) jesjobfr(&jobs);
    if (jes) jesclose(&jes);
    return 0;
}
#if 0
static int prtline(const char *line, unsigned linelen)
{
    printf("%-*.*s\n", linelen, linelen, line);
    return 0;
}
#endif
static int
print_dd(HTTPD *httpd, JESJOB *j, const char *in)
{
    unsigned int count = array_count(&j->jesdd);
    unsigned int n;
    int          i;
    char         recfm[12];

    printf("%s\"dd\": [\n", in);

    for(n=0; n < count; n++) {
        JESDD *dd = j->jesdd[n];

        if (!dd) continue;

        if (dd->ddname[0] == ' ') dd->ddname[0] = 0;
        printf("%s  {\n", in);
        printf("%s    \"ddname\": \"%s\",\n", in, dd->ddname);
        printf("%s    \"stepname\": \"%s\",\n", in, dd->stepname);
        printf("%s    \"procstep\": \"%s\",\n", in, dd->procstep);
        printf("%s    \"dsid\": \"%u\",\n", in, dd->dsid);
        printf("%s    \"dsname\": \"%s\",\n", in, dd->dsname);
        printf("%s    \"class\": \"%c\",\n", in, dd->oclass);

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
        printf("%s    \"recfm\": \"%s\",\n", in, recfm);
        printf("%s    \"records\": \"%u\",\n", in, dd->records);
        printf("%s    \"lrecl\": \"%u\",\n", in, dd->lrecl);
        printf("%s    \"mttr\": \"%08X\"\n", in, dd->mttr);

        printf("%s  }%s\n", in, (n+1) < count ? "," : "");
    }

    printf("%s]\n");
    return 0;
}
