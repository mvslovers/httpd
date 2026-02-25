#include "httpd.h"
#include "httpds.h"
#include "svc99.h"
#include "clibary.h"
#include "clibos.h"

static int do_xmit(HTTPDS *ds);
static int cleanup(HTTPDS *ds);
static int process_abend(HTTPDS *ds, int rc);

static int link(HTTPDS *ds, const char *pgm);

static int send_xmit(HTTPDS *ds, const char *fn);

#if 0   /* debugging */
static int list_xmitout(HTTPDS *ds, const char *title);
static int dump_dslist(unsigned count, DSLIST **dslist);
#endif

/* process "xmit" request for dsn */
int httpds_xmit(HTTPDS *ds)
{ 
    unsigned    *psa    	= (unsigned *)0;
    void        *ascb   	= (void *)psa[0x224/4];
    int         rc          = 0;
    int         lockrc      = 8;

    http_secs(&ds->start);

    if (!ds->dsn) {
        httpds_error(ds, "Missing ?dsn query variable\n");
        goto quit;
    }

    lockrc = lock(ascb, LOCK_EXC);
    do_xmit(ds);
    if (lockrc==0) unlock(ascb, LOCK_EXC);

quit:
    cleanup(ds);
    http_secs(&ds->end);
    return 0;
}

static int 
do_xmit(HTTPDS *ds)
{
    int         rc          = 0;
    FILE        *fp;
    char        buf[256];
    char        *p;

    // list_xmitout(ds, "do_xmit enter");

	/* just in case we have stale allocations */
    cleanup(ds);

    /* Make sure we can read the input dataset */
    fp = fopen(ds->dsn, "r");
    if (!fp) {
        wtof("%s: Unable to open \"%s\" for reading", __func__, ds->dsn);
        goto quit;
    }
    fclose(fp);

    // wtof("%s: SYSPRINT", __func__);
    rc = __dsalc(NULL, "dd=sysprint,dsn=&&sysprint,lrecl=121,blksize=12100,space=trk(1,1),dsorg=ps");
    if (rc) {
        wtof("%s: Unable to allocate SYSPRINT temp dataset", __func__);
        goto quit;
    }

    /* create IEPCOPY control statements DD SYSIN */
    rc = __dsalc(NULL, "dd=sysin,dsn=&&sysin,recfm=fb,"
                       "lrecl=80,blksize=3120,space=trk(1,1),dsorg=ps");
    if (rc) {
        wtof("%s: Unable to allocate SYSIN temp dataset", __func__);
        goto quit;
    }

    fp = fopen("DD:SYSIN", "w");
    if (!fp) {
        wtof("%s: Unable to open DD:SYSIN for output", __func__);
        goto quit;
    }
    
    /* add COPY statement to SYSIN */
    fprintf(fp, " COPY INDD=SYSUT1,OUTDD=SYSUT2\n");

    if (ds->member && ds->member[0]) {
        /* add SELECT statement to SYSIN */
        fprintf(fp, " SELECT MEMBER=%s\n", ds->member);
    }
    
    fclose(fp);

    // wtof("%s: SYSUT1", __func__);
    rc = __dsalcf(NULL, "dd=sysut1,dsn=%s,disp=shr", ds->dsn);
    if (rc) {
        wtof("%s: Unable to allocate \"%s\" input dataset", __func__, ds->dsn);
        goto quit;
    }

    // wtof("%s: SYSUT2", __func__);
    rc = __dsalc(NULL, "dd=sysut2,dsn=&&sysut2,space=cyl(10,10)");
    if (rc) {
        wtof("%s: Unable to allocate SYSUT2 temp dataset", __func__);
        goto quit;
    }

    // wtof("%s: XMITLOG", __func__);
    rc = __dsalc(NULL, "dd=xmitlog,dsn=&&xmitlog,recfm=fb,"
                       "lrecl=121,blksize=12100,space=trk(1,1),dsorg=ps");
    if (rc) {
        wtof("%s: Unable to allocate XMITLOG temp dataset", __func__);
        goto quit;
    }

    // wtof("%s: XMITOUT", __func__);
    rc = __dsalc(NULL, "dd=xmitout,dsn=&&xmitout,space=cyl(10,10),dsorg=ps,"
                       "recfm=fb,lrecl=80,blksize=3200");
    if (rc) {
        wtof("%s: Unable to allocate XMITOUT temp dataset", __func__);
        goto quit;
    }

    // list_xmitout(ds, "do_xmit allocated XMITOUT");

    /* link to external program */
    // wtof("%s: link(\"XMIT370\")", __func__);
    rc = link(ds, "XMIT370");
    if (rc < 0) {
        // wtof("%s: process_abend(%d)", __func__, rc);
        process_abend(ds, rc);
        goto quit;
    }

    // list_xmitout(ds, "do_xmit after link of XMIT370");

#if 0   /* debugging */
    fp = fopen("DD:SYSPRINT", "r");
    if (fp) {
        wtof("-----------------------------------------------------");
        wtof("%s: SYSPRINT", __func__);
        while(fgets(buf, sizeof(buf)-1, fp)) {
            buf[sizeof(buf)-1] = 0;
            p = strchr(buf, '\n');
            if (p) *p = 0;
            wtof("%s: %s", __func__, buf);
        }
        wtof("-----------------------------------------------------");
        fclose(fp);
    }

    fp = fopen("DD:XMITLOG", "r");
    if (fp) {
        wtof("-----------------------------------------------------");
        wtof("%s XMITLOG", __func__);
        while(fgets(buf, sizeof(buf)-1, fp)) {
            buf[sizeof(buf)-1] = 0;
            p = strchr(buf, '\n');
            if (p) *p = 0;
            wtof("%s: %s", __func__, buf);
        }
        wtof("-----------------------------------------------------");
        fclose(fp);
    }
#endif

    /* send XMITOUT dataset to web client */
    // wtof("%s: send XMITOUT", __func__);
    rc = send_xmit(ds, "DD:XMITOUT");

quit:
    // wtof("%s: quit", __func__);

    cleanup(ds);

    // list_xmitout(ds, "do_xmit exit");

    return 0;
}

static int cleanup(HTTPDS *ds)
{
	__dsfree("SYSUT1");   /* XMIT370 input dataset    */
	__dsfree("SYSUT2");   /* XMIT370 work dataset     */
	__dsfree("XMITLOG");  /* XMIT370 log file         */
	__dsfree("XMITOUT");  /* XMIT370 output dataset   */
	__dsfree("SYSPRINT"); /* IEPCOPY print dataset    */
    __dsfree("SYSIN");    /* IEPCOPY input dataset    */

    return 0;
}

static int send_xmit(HTTPDS *ds, const char *fn)
{
    FILE        *fp     = NULL;
    int         rc;
    int         len;
    int         pos;
    char        buf[88];

    fp = fopen(fn, "r,record");
    // wtof("%s: fp=%p", __func__, fp);
    if (!fp) {
        httpds_error(ds, "Unable to open %s for input.\n", fn);
        goto quit;
    }
    
    // wtof("%s: lrecl=%u blksize=%u", __func__, fp->lrecl, fp->blksize);
    
    http_resp(httpc,200);
    http_printf(httpc, "Cache-Control: no-store\r\n");
    http_printf(httpc, "Content-Type: application/octet-stream\r\n");
    http_printf(httpc, "Access-Control-Allow-Origin: *\r\n");
    if (ds->filename && ds->filename[0]) {
        http_printf(httpc, "Content-Disposition: attachment; filename=\"%s\"\r\n", ds->filename);
    }
    else if (ds->member && ds->member[0]) {
        http_printf(httpc, "Content-Disposition: attachment; filename=\"%s(%s).XMIT\"\r\n", ds->dsn, ds->member);
    }
    else {
        http_printf(httpc, "Content-Disposition: attachment; filename=\"%s.XMIT\"\r\n", ds->dsn);
    }
    http_printf(httpc, "Content-Transfer-Encoding: binary\r\n");
    http_printf(httpc, "Cache-Control: private\r\n");
    http_printf(httpc, "Pragma: private\r\n");
    http_printf(httpc, "\r\n");

    do {
        len = fread(buf, 1, 80, fp);
        // wtof("%s: fread() len=%d", __func__, len);
        if (len <= 0) break;
        // wtodumpf(buf, len, "%s buf", __func__);

        /* send buffer to web client */
        for(pos=0; pos < len; pos+=rc) {
            rc = http_send(httpc, &buf[pos], len-pos);

            /* if send error we're done */
            if (rc<0) goto quit;
        }
    } while(len > 0);

quit:
    if (fp) fclose(fp);
    return 0;
}


static int 
link(HTTPDS *ds, const char *pgm)
{
    int         rc      = -1;   /* link return code     */
    int         prc     = -1;   /* pgm return code      */
    void        *dcb    = NULL; /* no DCB for link      */
    char 		*query;
    struct {
        unsigned short  len;
        char            buf[512];
    } parms = {0, ""};
    unsigned    plist[4];

	// wtof("%s: enter", __func__);

    if (!pgm) goto quit;    /* NULL program, quit       */
    if (!*pgm) goto quit;   /* "" program name, quit    */

	// wtodumpf(&parms, sizeof(parms), "%s: parms", __func__);

    /* build parameter list for the program we're linking to */
    plist[0]    = (unsigned)&parms | 0x80000000;
    plist[1]    = 0;
    plist[2]    = 0;
    plist[3]    = 0;

	// wtodumpf(plist, sizeof(plist), "%s: plist", __func__);

    /* link to pgm, with ESTAE */
    rc = __linkds(pgm, dcb, plist, &prc);
    // wtof("%s: __linkds(\"%s\",0,0x%08X,0x%08X) rc=%d prc=%d",
	// 	__func__, pgm, plist, &prc, rc, prc);
	
	if (rc==0) rc = prc;
	
quit:
	// wtof("%s: exit rc=%d", __func__, rc);
	return rc;
}

static int 
process_abend(HTTPDS *ds, int rc)
{
	/* some kind of ABEND occurred */
	unsigned abcode = (unsigned) (rc * -1);   /* make positive again */

	/* we're running in the HTTPD server */
	if (!httpc->resp) {
		/* no response header was issued by CGI program */
		http_resp(httpc,503);   
		http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
		http_printf(httpc, "\r\n");
	}

	if (abcode > 4095) {
		/* system abend occurred */
		http_printf(httpc, "External program %s failed with S%03X ABEND", "XMIT370", abcode >> 12);
        wtof("External program %s failed with S%03X ABEND", "XMIT370", abcode >> 12);
	}
	else {
		/* user abend code */
		http_printf(httpc, "External program %s failed with U%04d ABEND", "XMIT370", abcode);
        wtof("External program %s failed with U%04d ABEND", "XMIT370", abcode);
	}
	http_printf(httpc, "\n");

	return rc;
}

#if 0   /* debugging */
static int list_xmitout(HTTPDS *ds, const char *title)
{
    ALCLIST     **alloc     = NULL;
    DSLIST      *dslist;
    unsigned    n, count;
    
    wtof("%s", title);
    
    alloc = __listal(NULL, "XMITOUT", 0);

    if (!alloc) {
        wtof("%s: XMITOUT is not allocated", __func__);
        goto quit;
    }

    count = array_count(&alloc);
    for(n=0; n < count; n++) {
        ALCLIST *l = alloc[n];
        if (!l) continue;
        
        wtof("%s: %s is allocated to %u datasets", __func__, l->ddname, l->count);
        dump_dslist(l->count, l->dslist);
    }


quit:
    if (alloc) __freeal(&alloc);

    return 0;
}

static int dump_dslist(unsigned count, DSLIST **dslist)
{
    unsigned    n;
    
    if (dslist) {
        for(n=0; n < count; n++) {
            DSLIST *d = dslist[n];
            if (!d) continue;
            
            wtof("    %-44.44s %s %s %s %u %u", 
                d->dsn, d->volser, d->dsorg, d->recfm, d->lrecl, d->blksize);
        }
    }
    
    return 0;
}
#endif
