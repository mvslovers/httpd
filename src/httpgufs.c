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
        /* Pin the session to subpool 0 (issue #154).  A CGI reaches this
           through the httpx vector, so during a module window ufsnew() would
           allocate under the module's ambient subpool -- but the session hangs
           off httpc->ufs and is released by http_close(), after the module has
           returned or abended.  libufs allocates it, so the bracket has to sit
           here rather than at the allocation. */
        UCHAR sp = __setsp(0);

        httpc->ufs = ufsnew();
        if (httpc->ufs) {
            CLIBCRT *crt = __crtget();
            if (crt) crt->crtufs = httpc->ufs;
        }

        __setsp(sp);
    }
    return httpc->ufs;
}
