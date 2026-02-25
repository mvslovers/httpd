/* HTTPJES2.C - CGI Program, REST style CGI program for access of JES2 resources */
#include "httpd.h"
#include "httpds.h"         /* dataset services common */

static int do_list(HTTPDS *ds);
static int do_list_hlq_json(HTTPDS *ds, DSLIST **dslist);
static int do_list_hlq_print(HTTPDS *ds, DSLIST **dslist);

static char *strupper(char *buf);

int httpds_list(HTTPDS *ds)
{ 
    int         rc          = 0;
    FILE    	*fp     	= 0;
    int         len;
    DSLIST      **dslist    = 0;

	httpsecs(&ds->start);

    if (!ds->hlq) {
        httpds_error(ds, "Missing ?hlq query variable.\nUse ?hlq=SYS1 or other high level qualifier.\n");
        goto quit;
    }

    /* get list of datasets for this hlq name */
    dslist = __listds(ds->hlq, "NONVSAM VOLUME", NULL);
    if (!dslist) {
        httpds_error(ds, "No datasets found for hlq=%s\n", ds->hlq);
        goto quit;
    }
    
    len = strlen(ds->format);
    if (http_cmpn(ds->format, "json", len)==0) {
        do_list_hlq_json(ds, dslist);
    }
    else if (http_cmpn(ds->format, "print", len)==0) {
        do_list_hlq_print(ds, dslist);
    }
    else {
        httpds_error(ds, "Invalid format value \%s\".\n"
            "Use format=print or format=json query variables.\n", ds->format);
    }

quit:
    if (dslist)     __freeds(&dslist);
	httpsecs(&ds->end);
    return rc;
}

static int 
do_list_hlq_json(HTTPDS *ds, DSLIST **dslist)
{
    DSLIST      *d          = 0;
    unsigned    count       = 0;
    unsigned    n           = 0;
    int         first       = 1;
    const char  *smfid;

    http_resp(httpc,200);
    http_printf(httpc, "Cache-Control: no-store\r\n");
    http_printf(httpc, "Content-Type: %s\r\n", "application/json");
    http_printf(httpc, "Access-Control-Allow-Origin: *\r\n");
    http_printf(httpc, "\r\n");

    /* start of JSON object */
    http_printf(httpc, "{\n");
    
    /* start of JSON data array */
    http_printf(httpc, "  \"data\": [\n");

    count = array_count(&dslist);

    for(n=0; n < count; n++) {
        d = dslist[n];
        if (!d) continue;

        /* start of JSON object */
        if (first) {
            /* first time we're printing this '{' so no ',' needed */
            http_printf(httpc, "    {\n");
            first = 0;
        }
        else {
            /* all other times we need a ',' before the '{' */
            http_printf(httpc, "   ,{\n");
        }
        
        http_printf(httpc, "      \"dsname\": \"%s\",\n", d->dsn);
        http_printf(httpc, "      \"volser\": \"%s\",\n", d->volser);
        http_printf(httpc, "      \"lrecl\": %u,\n", d->lrecl);
        http_printf(httpc, "      \"blksize\": %u,\n", d->blksize);
        http_printf(httpc, "      \"dsorg\": \"%s\",\n", d->dsorg);
        http_printf(httpc, "      \"recfm\": \"%s\",\n", d->recfm);
        http_printf(httpc, "      \"create_date\": \"%u/%02u/%02u\",\n", d->cryear, d->crmon, d->crday);
        http_printf(httpc, "      \"reference_date\": \"%u/%02u/%02u\"\n", d->rfyear, d->rfmon, d->rfday);

        /* end of JSON object */
        http_printf(httpc, "    }\n");

    }

    /* end of JSON data array */
    http_printf(httpc, "  ],\n");

    /* start of JSON debug object */
    http_printf(httpc, "  \"debug\": {\n");
    
    http_printf(httpc, "    \"program\": \"%s\",\n", ds->pgm);

	smfid = (const char *)__smfid();
	if (smfid) http_printf(httpc, "    \"node\": \"%s\",\n", smfid);

	httpsecs(&ds->end);
    http_printf(httpc, "    \"elapsed\": \"%f\"\n", ds->end - ds->start);
    
    /* end of JSON debug object */
    http_printf(httpc, "  }\n");

    /* end of JSON object */
    http_printf(httpc, "}\n");

quit:
    return 0;
}

static int 
do_list_hlq_print(HTTPDS *ds, DSLIST **dslist)
{
    DSLIST      *d          = 0;
    unsigned    count       = 0;
    unsigned    n           = 0;

    count = array_count(&dslist);
    if (!count) {
        http_resp(httpc, 404);  /* not found */
        http_printf(httpc, "Content: text/plain\r\n");
        http_printf(httpc, "\r\n");
        http_printf(httpc, "No datasets found for hlq=%s\n", ds->hlq);
        goto quit;
    }

    http_resp(httpc, 200);  /* not found */
    http_printf(httpc, "Content: text/plain\r\n");
    http_printf(httpc, "\r\n");

    for(n=0; n < count; n++) {
        d = dslist[n];
        if (!d) continue;
            
        http_printf(httpc, 
            "%-44.44s "			/* dsn */
            "%-6.6s "			/* volser */
            "%5u "				/* lrecl */
            "%5u "				/* blksize */
            "%s "				/* dsorg */
            "%-3s "				/* recfm */
            "%u/%02u/%02u "		/* create date */
            "%u/%02u/%02u\r\n",	/* reference date */
            d->dsn, 
            d->volser, 
            d->lrecl, 
            d->blksize, 
            d->dsorg,
            d->recfm,
            d->cryear, d->crmon, d->crday,
            d->rfyear, d->rfmon, d->rfday);
    }

	httpsecs(&ds->end);
    http_printf(httpc, "\nElapsed: %f seconds\n", ds->end - ds->start);

quit:
    return 0;
}
