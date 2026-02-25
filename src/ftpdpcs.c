/* FTPDPCS.C
** Process FTPD clients
*/
#include "httpd.h"

__asm__("\n&FUNC    SETC 'ftpd_process_clients'");
int ftpd_process_clients(void)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    FTPD        *ftpd   = httpd->ftpd;
    int         lockrc;
    unsigned    count;
    unsigned    n;
    FTPC        *ftpc;

    if (!ftpd) goto quit;
    if (!ftpd->ftpc) goto quit;     /* no clients */

    lockrc = lock(&ftpd, 0);
    if (lockrc==0) goto locked;     /* we have the lock */
    if (lockrc==8) goto locked;     /* we already had the lock */
    wtof("%s lock failed", __func__);
    goto quit;

locked:
    count = array_count(&ftpd->ftpc);
    for(n=0; n < count; n++) {
        ftpc = ftpd->ftpc[n];
        if (!ftpc) continue;           /* no client handle? */

        ftpd_process_client(ftpc);
    }

    if (lockrc==0) unlock(&ftpd, 0);

quit:
    return 0;
}
