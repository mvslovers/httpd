/* HTTPJES2.C - CGI Program, REST style CGI program for access of JES2 resources */
#include "httpd.h"

#include "httpds.h"         /* dataset services common */

static int do_pds_json(HTTPDS *ds, DSLIST *dslist, PDSLIST **pdslist);
static int do_pds_print(HTTPDS *ds, DSLIST *dslist, PDSLIST **pdslist);

int 
httpds_pds(HTTPDS *ds)
{ 
    DSLIST      *dslist     = 0;
    PDSLIST     **pdslist   = 0;
    int         len;

	httpsecs(&ds->start);

    if (!ds->dsn) {
        httpds_error(ds, "Missing ?dsn query variable.\nUse ?dsn=your.pds.dataset.name\n");
        goto quit;
    }

    /* get dataset info */
    dslist = httpds_dslist(ds->dsn);
    if (!dslist) {
        httpds_error(ds, "Dataset %s does not exist.\n", ds->dsn);
        goto quit;
    }
    
    /* make sure this dataset is a PDS (dsorg PO) */
    if (strcmp(dslist->dsorg, "PO") != 0 && 
        strcmp(dslist->dsorg, "POU")!= 0) {
        /* dataset is not a PDS */
        httpds_error(ds, "Dataset %s is NOT a PDS.\n", ds->dsn);
        goto quit;
    }

    pdslist = __listpd(ds->dsn, NULL);
    
    len = strlen(ds->format);
    if (http_cmpn(ds->format, "json", len)==0) {
        do_pds_json(ds, dslist, pdslist);
    }
    else if (http_cmpn(ds->format, "print", len)==0) {
        do_pds_print(ds, dslist, pdslist);
    }
    else {
        httpds_error(ds, "Invalid format value \%s\".\n"
            "Use format=print or format=json query variables.\n", ds->format);
    }

quit:    
    if (dslist)     free(dslist);
    if (pdslist)    __freepd(&pdslist);
    http_secs(&ds->end);
    return 0;
}

static int
do_pds_json(HTTPDS *ds, DSLIST *dslist, PDSLIST **pdslist)
{
    char        buf[256];
    LOADSTAT    *loadstat   = (LOADSTAT *)buf;
    ISPFSTAT    *ispfstat   = (ISPFSTAT *)buf;
    int         first       = 1;
    unsigned    n, count;
    const char  *smfid;


    ds->dstype = NULL;

    http_resp(httpc,200);
    http_printf(httpc, "Cache-Control: no-store\r\n");
    http_printf(httpc, "Content-Type: application/json\r\n");
    http_printf(httpc, "Access-Control-Allow-Origin: *\r\n");
    http_printf(httpc, "\r\n");

    /* start of JSON object */
    http_printf(httpc, "{\n");
    
    /* start of JSON data array */
    http_printf(httpc, "  \"data\": [\n");

    count    = __arcou(&pdslist);

    if (strcmp(dslist->recfm, "U")!=0) {
        /* not a LOAD/LINK type PDS dataset */
        goto text_members;
    }

load_members:
    for(n=0; n < count; n++) {
        PDSLIST *a = pdslist[n];
        if (!a) continue;

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
        
        __fmtloa(a, loadstat);

        http_printf(httpc, "      \"member\": \"%s\",\n", loadstat->name);
        http_printf(httpc, "      \"ttr\": \"%s\",\n", loadstat->ttr);
        http_printf(httpc, "      \"size\": \"%s\",\n", loadstat->size);
        http_printf(httpc, "      \"aliasof\": \"%s\",\n", loadstat->aliasof);
        http_printf(httpc, "      \"ac\": \"%s\",\n", loadstat->ac);
        http_printf(httpc, "      \"ep\": \"%s\",\n", loadstat->ep);
        http_printf(httpc, "      \"attr\": \"%s\"\n", loadstat->attr);

        /* end of JSON object */
        http_printf(httpc, "    }\n");
    }
    goto all_members;

text_members:
    for(n=0; n < count; n++) {
        PDSLIST *a = pdslist[n];
        if (!a) continue;

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
        
        __fmtisp(a, ispfstat);

        http_printf(httpc, "      \"member\": \"%s\",\n", ispfstat->name);
        http_printf(httpc, "      \"ttr\": \"%s\",\n", ispfstat->ttr);
        http_printf(httpc, "      \"ver\": \"%s\",\n", ispfstat->ver);
        http_printf(httpc, "      \"created\": \"%s\",\n", ispfstat->created);
        http_printf(httpc, "      \"changed\": \"%s\",\n", ispfstat->changed);
        http_printf(httpc, "      \"init\": \"%s\",\n", ispfstat->init);
        http_printf(httpc, "      \"size\": \"%s\",\n", ispfstat->size);
        http_printf(httpc, "      \"mod\": \"%s\",\n", ispfstat->mod);
        http_printf(httpc, "      \"userid\": \"%s\"\n", ispfstat->userid);

        /* end of JSON object */
        http_printf(httpc, "    }\n");
    }

all_members:
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
do_pds_print(HTTPDS *ds, DSLIST *dslist, PDSLIST **pdslist)
{
    char        buf[256];
    LOADSTAT    *loadstat   = (LOADSTAT *)buf;
    ISPFSTAT    *ispfstat   = (ISPFSTAT *)buf;
    unsigned    n, count;

    count    = __arcou(&pdslist);
    if (!count) {
        http_resp(httpc, 404);  /* not found */
        http_printf(httpc, "Content-Type: text/plain\r\n");
        http_printf(httpc, "\r\n");
        http_printf(httpc, "No members found for dsn=%s\n", ds->dsn);
        goto quit;
    }

    http_resp(httpc, 200);  /* not found */
    http_printf(httpc, "Content-Type: text/plain\r\n");
    http_printf(httpc, "\r\n");

    for(n=0; n < count; n++) {
        PDSLIST *a = pdslist[n];
        if (!a) continue;

        if (a->idc & PDSLIST_IDC_TTRS) {
            /* most likely a load member */
            __fmtloa(a, loadstat);
            http_printf(httpc, "%-8.8s %s %s %-8.8s %s %s %s\n",
                loadstat->name, loadstat->ttr, loadstat->size,
                loadstat->aliasof, loadstat->ac, loadstat->ep,
                loadstat->attr);
        }
        else {
            /* most likely a non-load member */
            __fmtisp(a, ispfstat);
            http_printf(httpc, "%-8.8s %s %s %s %s %s %s %s %s\n",
                ispfstat->name, ispfstat->ttr, ispfstat->ver,
                ispfstat->created, ispfstat->changed, ispfstat->init,
                ispfstat->size, ispfstat->mod, ispfstat->userid);
        }
    }

	httpsecs(&ds->end);
    http_printf(httpc, "\nElapsed: %f seconds\n", ds->end - ds->start);

quit:
    return 0;
}
