#include "ftpd.h"
#include "clibary.h"        /* dynamic array prototypes     */
#include "clibdscb.h"       /* DSCB structs and prototypes  */
#include "cliblist.h"       /* __listc()                    */
#include "clibstr.h"        /* __patmat()                   */

static int  parse(void *vdata, const char *fmt, ...);

typedef struct {
    const char  *level;     /* "HLQ.TEST" */
    const char  *option;    /* NULL or "NONVSAM VOLUME" */
    const char  *filter;    /* dataset name pattern "HLQ.TEST.*DATA" */
    unsigned    count;      /* number of datasets */
    char        buf[256];   /* work buffer for parsing */
} UDATA;

unsigned
ftpc_ds_count(const char *level, const char *option, const char *filter)
{
    int     rc      = 0;
    UDATA   udata   = {0};

    udata.level     = level;
    udata.option    = option;
    udata.filter    = filter;

    rc = __listc(level, option, parse, &udata);

    return udata.count;
}

static int
parse(void *vdata, const char *fmt, ...)
{
    int     rc      = 0;
    UDATA   *udata  = vdata;
    char    *buf    = udata->buf;
    char    *p;
    va_list arg;

    /* format the record passed to us by __listc() */
    va_start(arg, fmt);
    vsprintf(buf, fmt, arg);
    va_end(arg);

	// wtof("ftpc_ds_count:parse: \"%s\"", buf);

    if (buf[0]=='1') goto quit; /* skip page headers */

    /* parse the formatted record looking for keywords */
    p = strtok(buf, " -\n");
    if (!p) goto quit;

    /* skip carriage control characters */
    if (isdigit(*p)) {
        p++;
        if (*p==0) p = strtok(NULL, " -\n");
    }

    if (stricmp(p, "NONVSAM")==0    ||
        stricmp(p, "PAGESPACE")==0  ||
        stricmp(p, "CLUSTER")==0    ||
        stricmp(p, "USERCATALOG")==0) {
        /* get next parm */
        p = strtok(NULL, " -\n");
        if (!p) goto quit;

        /* make sure name does not start with a number */
        if (isdigit(*p)) goto quit;

        if (udata->filter) {
            /* match dataset name against filter pattern */
            if (!__patmat(p, udata->filter)) goto quit;
        }

		udata->count++;
        goto quit;
    }

quit:
    return 0;
}
