/* DBGDUMP.C
**
*/
#include "httpd.h"

__asm__("\n&FUNC    SETC 'dump'");
static void
dump(FILE *fp, const char *pName, const void *pvArea, int iSize, int iChunk)
{
    int     indent  = 0;
    int     i, j;
    int     iHex    = 0;
    int     ie      = 0;
    int     ia      = 0;
    int     x       = (iChunk * 2) + ((iChunk / 4) - 1);
    UCHAR   *pArea  = (UCHAR*)pvArea;
    UCHAR   e;
    UCHAR   a;
    char    sHex[80];
    char    eChar[80];
    char    aChar[80];

    fprintf(fp, "%*.*sDump of %08X \"%s\" (%d bytes)\n",
        indent, indent, "",
        pArea, pName, iSize);

    for (i=0, j=0; iSize > 0;i++ ) {
        if ( i==iChunk ) {
            fprintf(fp, "%*.*s+%05X %-*.*s :%-*.*s: :%-*.*s:\n",
                indent, indent, "",
                j, x, x, sHex,
                iChunk, iChunk, eChar, iChunk, iChunk, aChar);
            j += i;
            ia = ie = iHex = i = 0;
        }
        iHex += sprintf(&sHex[iHex],"%02X",*pArea);
        if ((i & 3) == 3) iHex += sprintf(&sHex[iHex]," ");

        e = *pArea;
        a = asc2ebc[e];

        ie += sprintf(&eChar[ie],"%c", isgraph(e)?e:e==' '?e:'.');
        ia += sprintf(&aChar[ia],"%c", isgraph(a)?a:a==' '?a:'.');
        pArea++;
        iSize--;
    }

    if ( iHex ) {
        fprintf(fp, "%*.*s+%05X %-*.*s :%-*.*s: :%-*.*s:\n",
            indent, indent, "",
            j, x, x, sHex,
            iChunk, iChunk, eChar, iChunk, iChunk, aChar);
    }

quit:
    return;
}

__asm__("\n&FUNC    SETC 'dbgdump'");
int
dbgdump(void *buf, int len, const char *fmt, ...)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc  = 0;
    va_list     list;
    char        title[80];

    if (httpd->dbg) {
        dbgtime(NULL);
        va_start(list, fmt);
        vsprintf(title, fmt, list);
        va_end(list);

        dump(httpd->dbg, title, buf, len, 16);
    }

    return rc;
}
