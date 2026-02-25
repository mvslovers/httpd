/* FTPSLIST.C */
#include "httpd.h"
#include "cliblist.h"
#include "clibary.h"

int
ftpslist(FTPC *ftpc, int brief)
{
    PDSLIST     **pdslist   = 0;
    LOADSTAT    *loadstat   = 0;
    ISPFSTAT    *ispfstat   = 0;
    DSLIST      **dslist    = 0;
    DSLIST      *d          = 0;
    int         rc          = 0;
    char        *p          = 0;
    unsigned    count       = 0;
    unsigned    n           = 0;
    char        buf[256];
    const char *cwd         = ftpc->cwd;

    if (!ftpc->ufs) goto do_open;
    if (ftpc->flags & FTPC_FLAG_CWDDS) goto do_open;
    if (ftpc->flags & FTPC_FLAG_CWDPDS) goto do_open;

    cwd = ftpc->ufs->cwd->path;

do_open:
    /* lets first try to open the data connection */
    rc = ftpcopen(ftpc);
    if (rc < 0) {
        ftpcmsg(ftpc, "425 Can't open data connection");
        goto quit;
    }
    /* wtof("%s cwd=\"%s\", data socket=%d", __func__, cwd, rc); */
    ftpc->data_socket = rc; /* data connection socket */

    /* data connection is opened--list directory information */
    p = strtok(ftpc->cmd, " ");
    if (p) p = strtok(NULL, " ");
    if (p) {
        if (strcmp(p, "*.*")==0) {
            p = 0;
        }
    }

    /* is the CWD a PDS? */
    if (ftpc->flags & FTPC_FLAG_CWDPDS) {
        /* create directory list for PDS */
        /* wtof("%s create PDS member list for cwd=\"%s\"", __func__, cwd); */
        pdslist = __listpd(cwd, p);
        if (pdslist) {
            loadstat = (LOADSTAT *)buf;
            ispfstat = (ISPFSTAT *)buf;
            count    = __arcou(&pdslist);
            for(n=0; n < count; n++) {
                PDSLIST *a = pdslist[n];
                if (!a) continue;
                if (brief) {
                    /* name list only, so no need to format the udata */
                    ftpcprtf(ftpc, "%s\r\n", a->name);
                    continue;
                }
                if (a->idc & PDSLIST_IDC_TTRS) {
                    /* most likely a load member */
                    __fmtloa(a, loadstat);
                    ftpcprtf(ftpc, "%-8.8s %s %s %-8.8s %s %s %s\r\n",
                        loadstat->name, loadstat->ttr, loadstat->size,
                        loadstat->aliasof, loadstat->ac, loadstat->ep,
                        loadstat->attr);
                }
                else {
                    /* most likely a non-load member */
                    __fmtisp(a, ispfstat);
                    ftpcprtf(ftpc, "%-8.8s %s %s %s %s %s %s %s %s\r\n",
                        ispfstat->name, ispfstat->ttr, ispfstat->ver,
                        ispfstat->created, ispfstat->changed, ispfstat->init,
                        ispfstat->size, ispfstat->mod, ispfstat->userid);
                }
            }
        }
        goto done;
    }

    /* is the CWD a DATASET level? */
    if (ftpc->flags & FTPC_FLAG_CWDDS) {
        /* create list of datasets */
        /* wtof("%s create list of datasets cwd=\"%s\"", __func__, cwd); */
        dslist = __listds(cwd, "NONVSAM VOLUME", NULL);
        count = __arcou(&dslist);
        for(n=0; n < count; n++) {
            d = dslist[n];
            if (!d) continue;
            ftpcprtf(ftpc,
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
        goto done;
    }

    if (ftpc->ufs) {
        UFSDDESC *desc = ufs_diropen(ftpc->ufs, cwd, NULL);
        UFSDLIST *list;
        struct tm *tm;
        char     fmtdate[26];

        if (desc) {
            for(list=ufs_dirread(desc); list; list=ufs_dirread(desc)) {
                count++;
                if (brief) {
                    ftpcprtf(ftpc, "%s%s\r\n", list->name, list->attr[0]=='d' ? "/" : "");
                }
                else {
                    tm = mlocaltime64((const utime64_t*)&list->mtime);
                    if (tm) {
                        strftime(fmtdate, sizeof(fmtdate), "%Y %b %d %H:%M", tm);
                    }
                    else {
                        strcpy(fmtdate, "1970 Jan 01 00:00");
                    }
                    ftpcprtf(ftpc, "%s %3u %-8.8s %-8.8s %6u %s %s\r\n",
                             list->attr, list->nlink, list->owner, list->group,
                             list->filesize, fmtdate, list->name);
                }
            }
            ufs_dirclose(&desc);
        }

        if (!count) {
            /* no directory list was returned, gripe using errno */
            ftpcmsg(ftpc, "226-%s", strerror(errno));
        }
        goto done;
    }

    /* otherwise, the contents of ftpc->cwd *should* replesent a unix like file system path */
    /* wtof("%s create directory list for cwd=\"%s\"", __func__, cwd); */

done:
    if (count) {
        ftpcmsg(ftpc, "250 Requested file action okay, completed");
    }
    else {
        ftpcmsg(ftpc, "226 Closing data connection");
    }

    closesocket(ftpc->data_socket);
    ftpc->data_socket = -1;

quit:
    if (pdslist)    __freepd(&pdslist);
    if (dslist)     __freeds(&dslist);

    return 0;
}
