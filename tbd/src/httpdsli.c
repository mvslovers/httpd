#include "httpd.h"
#include "httpds.h"

static int dslist_json(HTTPDS *ds, DSLIST *dslist);
static int dslist_print(HTTPDS *ds, DSLIST *dslist);

/* process "info" request for dsn */
int httpds_info(HTTPDS *ds)
{ 
    int         rc          = 0;
    DSLIST      *dslist     = NULL;

    http_secs(&ds->start);

    if (!ds->dsn) {
        httpds_error(ds, "Missing ?dsn query variable\n");
        goto quit;
    }

    /* get datset information */
    dslist = httpds_dslist(ds->dsn);
    if (!dslist) {
        rc = errno;
        switch(rc) {
        case ENOENT:
            httpds_error(ds, "Dataset %s not found.\n", ds->dsn);
            break;
        case ENOMEM:
            httpds_error(ds, "Out of memory.\n");
            break;
        default:
            httpds_error(ds, "Unexpected error=%d \"%s\"\n", rc, strerror(rc));
            break;
        }
        goto quit;
    }
    
    if (http_cmp(ds->format, "json")==0) {
        rc = dslist_json(ds, dslist);
        goto quit;
    }
    
    if (http_cmp(ds->format, "print")==0) {
        rc = dslist_print(ds, dslist);
        goto quit;
    }
    
    httpds_error(ds, "Invalid ?format=%s value.\nUse ?format=json or ?format=print\n", ds->format);

quit:
    if (dslist) free(dslist);
    http_secs(&ds->end);
    return 0;
}

static int dslist_json(HTTPDS *ds, DSLIST *dslist)
{
    http_resp(httpc,200);
    http_printf(httpc, "Cache-Control: no-store\r\n");
    http_printf(httpc, "Content-Type: application/json\r\n");
    http_printf(httpc, "Access-Control-Allow-Origin: *\r\n");
    http_printf(httpc, "\r\n");

    http_printf(httpc, "{\n");
        
    http_printf(httpc, "  \"dsname\": \"%s\",\n", dslist->dsn);
    http_printf(httpc, "  \"volser\": \"%s\",\n", dslist->volser);
    http_printf(httpc, "  \"lrecl\": %u,\n", dslist->lrecl);
    http_printf(httpc, "  \"blksize\": %u,\n", dslist->blksize);
    http_printf(httpc, "  \"dsorg\": \"%s\",\n", dslist->dsorg);
    http_printf(httpc, "  \"recfm\": \"%s\",\n", dslist->recfm);
    http_printf(httpc, "  \"create_date\": \"%u/%02u/%02u\",\n", 
        dslist->cryear, dslist->crmon, dslist->crday);
    http_printf(httpc, "  \"reference_date\": \"%u/%02u/%02u\"\n", 
        dslist->rfyear, dslist->rfmon, dslist->rfday);

    /* end of JSON object */
    http_printf(httpc, "}\n");

    return 0;
}

static int dslist_print(HTTPDS *ds, DSLIST *dslist)
{
    http_resp(httpc,200);
    http_printf(httpc, "Content-Type: text/plain\r\n");
    http_printf(httpc, "\r\n");

    http_printf(httpc, 
        "%-44.44s "			/* dsn */
        "%-6.6s "			/* volser */
        "%5u "				/* lrecl */
        "%5u "				/* blksize */
        "%s "				/* dsorg */
        "%-3s "				/* recfm */
        "%u/%02u/%02u "		/* create date */
        "%u/%02u/%02u\n",	/* reference date */
        dslist->dsn, 
        dslist->volser, 
        dslist->lrecl, 
        dslist->blksize, 
        dslist->dsorg,
        dslist->recfm,
        dslist->cryear, dslist->crmon, dslist->crday,
        dslist->rfyear, dslist->rfmon, dslist->rfday);

    return 0;
}
