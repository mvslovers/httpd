#ifndef HTTPDS_H
#define HTTPDS_H

/* httpds.h - dataset services CGI program HTTPDSL */
#include "cliblist.h"
#include "clibary.h"
#include "clibstr.h"
#include "osdcb.h"
#include "clibdscb.h"       /* DSCB structs and prototypes  */

typedef struct httpds       HTTPDS;

struct httpds {             /* handle passed to httpds... functions */
    CLIBPPA     *_ppa;
    CLIBGRT     *_grt;
    CLIBCRT     *_crt;
    HTTPD       *_httpd;
    HTTPC       *_httpc;
    const char  *pgm;       /* name of this program */
    const char  *path;      /* request path name */
    const char  *verb;      /* request verb (last name in path) */
    const char  *hlq;       /* high level dataset qualifier */
    const char  *dsn;       /* dataset name */
    const char  *dstype;    /* dataset type */
    const char  *member;    /* member name */
    const char  *format;    /* desired response format */
    const char  *filename;  /* download as file name */
    const char  *unused1;   /* unused / available */
    const char  *unused2;   /* unused / available */
    const char  *unused3;   /* unused / available */
    const char  *unused4;   /* unused / available */
    int         binary;     /* binary data */
    int         e2a;        /* perform EBCDIC to ASCII conversion */
    int         type;       /* type of dataset or high level qualifer */
#define TYPE_UNKNOWN    0   /* we don't know what this is */
#define TYPE_HLQ        1   /* this is a single level qualifer */
#define TYPE_DSN        2   /* this is a dataset name */
#define TYPE_DSNMEM     3   /* this is a dataset with member */
    int         org;        /* dataset organization */
#define ORG_UNKNOWN     0   /* we don't know what this is */
#define ORG_PS          1   /* dataset is sequential */
#define ORG_PDS         2   /* dataset is partitioned */
    double      start;      /* processing start time */
    double      end;        /* processing end time */
};

#define ppa (ds->_ppa)
#define grt (ds->_grt)
#define crt (ds->_crt)
#define httpd (ds->_httpd)
#define httpc (ds->_httpc)
#define httpx (ds->_httpd->httpx)

/* return DSLIST record for dataset name */
DSLIST *httpds_dslist(const char *dsn)                  asm("HTTPDSLD");

/* httpds_error()  - send error response */
int httpds_error(HTTPDS *ds, const char *fmt, ...)      asm("HTTPDSLE");

/* httpds_info() - process "info" request for dsn */
int httpds_info(HTTPDS *ds)                             asm("HTTPDSLI");

/* httpds_list() - process "list" request for hlq */
int httpds_list(HTTPDS *ds)                             asm("HTTPDSLL");

/* httpds_pds() - process "pds" request for dsn */
int httpds_pds(HTTPDS *ds)                              asm("HTTPDSLP");

/* httpds_xmit() - process "xmit" request for dsn */
int httpds_xmit(HTTPDS *ds)                             asm("HTTPDSLX");

#endif
