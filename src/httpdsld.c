#include "httpd.h"
#include "httpds.h"

/* return DSLIST record for dataset name */
DSLIST *httpds_dslist(const char *dsn)
{ 
    int         rc          = 0;
    char        volser[8]   = {0};
    LOCWORK     workarea    = {0};
    DSCB        dscbbuf     = {0};
    DSCB        *dscb       = &dscbbuf;
    DSCB1       *dscb1      = &dscb->dscb1;
    DSLIST      *dslist     = NULL;
    char        *p;
    struct tm   tm          = {0};

    if (!dsn) {
        errno = ENOENT;
        goto quit;
    }

    /* get volser for dataset */
    rc = __locate(dsn, &workarea);
    if (rc) {
        errno = ENOENT;
        goto quit;
    }
    
    /* save the volser */
    memcpyp(volser, sizeof(volser), workarea.volser, sizeof(workarea.volser), 0);

    /* get DSCB info for dataset + volser */
    rc = __dscbdv(dsn, volser, dscb);
    if (rc) {
        errno = ENOENT;
        goto quit;
    }

    dslist = (DSLIST *) calloc(1, sizeof(DSLIST));
    if (!dslist) {
        errno = ENOMEM;
        goto quit;
    }

    /* format the dslist from the dscb */
    strcpy(dslist->dsn, dsn);
    strcpy(dslist->volser, volser);

    /* extract values from DSCB for this dataset */
    p = 0;
    switch(dscb1->dsorg1 & 0x7F) {
    case DSGIS: p = "IS";   break;
    case DSGPS: p = "PS";   break;
    case DSGDA: p = "DA";   break;
    case DSGPO: p = "PO";   break;
    }
    if (dscb1->dsorg2 == ORGAM) p = "VS";
    if (p) strcpy(dslist->dsorg, p);

    p = 0;
    switch (dscb1->recfm & 0xC0) {
    case RECFF: p = "F";    break;
    case RECFV: p = "V";    break;
    case RECFU: p = "U";    break;
    }
    if (p) strcat(dslist->recfm,p);
 
    if (dscb1->recfm & RECFB) strcat(dslist->recfm, "B");
    if (dscb1->recfm & RECFS) strcat(dslist->recfm, "S");
    if (dscb1->recfm & RECFA) strcat(dslist->recfm, "A");
    if (dscb1->recfm & RECMC) strcat(dslist->recfm, "M");

    dslist->extents = dscb1->noepv;
    dslist->lrecl   = dscb1->lrecl;
    dslist->blksize = dscb1->blksz;

    dslist->cryear  = 1900 + dscb1->credt[0];
    if (dslist->cryear < 1980) {
        dslist->cryear += 100;
    }
    dslist->crjday  = *(unsigned short*)&dscb1->credt[1];
    tm.tm_year      = dslist->cryear - 1900;
    tm.tm_mday      = dslist->crjday;
    mktime(&tm);
    dslist->crmon   = tm.tm_mon + 1;
    dslist->crday   = tm.tm_mday;

    dslist->rfyear  = 1900 + dscb1->refd[0];
    if (dslist->rfyear < 1980) {
        dslist->rfyear += 100;
    }
    dslist->rfjday  = *(unsigned short *)&dscb1->refd[1];
    memset(&tm, 0, sizeof(tm));
    tm.tm_year      = dslist->rfyear - 1900;
    tm.tm_mday      = dslist->rfjday;
    mktime(&tm);
    dslist->rfmon   = tm.tm_mon + 1;
    dslist->rfday   = tm.tm_mday;

quit:
    return dslist;
}
