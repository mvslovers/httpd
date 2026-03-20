/* HTTPGUFS.C
** Get or create per-client UFS session (lazy init)
*/
#define HTTP_PRIVATE
#include "httpd.h"

__asm__("\n&FUNC SETC 'HTTPGUFS'");

#undef http_get_ufs
UFS *
http_get_ufs(HTTPC *httpc)
{
    if (!httpc->ufs && httpc->httpd->ufssys) {
        httpc->ufs = ufsnew();
        if (httpc->ufs) {
            CLIBCRT *crt = __crtget();
            if (crt) crt->crtufs = httpc->ufs;
        }
    }
    return httpc->ufs;
}
