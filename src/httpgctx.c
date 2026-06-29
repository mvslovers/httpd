/* HTTPGCTX.C
** Generic per-CGI context registrar (find-or-create by eyecatcher)
*/
#define HTTP_PRIVATE
#include "httpd.h"
#include "mvssupa.h"                /* __getm() raw GETMAIN in subpool 0    */

__asm__("\n&FUNC SETC 'HTTPGCTX'");

#undef http_cgictx_get

/*
** http_cgictx_get() - find or create one context block per 8-byte eyecatcher.
**
** httpd knows neither the type nor the name of any CGI -- only 8 bytes to
** match and a size to allocate.  Every context block begins with the 8
** eyecatcher bytes (stamped here on create); `size` is used ONLY on create
** and ignored on a hit (caller contract: always pass the same size for the
** same eyecatcher).
**
** The block is allocated with __getm() (raw GETMAIN, subpool 0).  SP0 is
** shared address-space wide (SZERO=YES on the worker ATTACH), so the block
** outlives the LINK return that started the CGI and the worker shrink, and
** lives until the address space ends.  __getm() bypasses the malloc memmgr,
** so @@exit never reclaims it -- exactly why __getm() and not malloc().  A
** context block is therefore never freed individually.
**
** The whole find-or-create runs under one lock on the array address
** (CLIBLOCK ENQ, STEP scope), so concurrent workers cannot race and there is
** no loser-free case.
**
** Returns the existing or newly created block, or NULL (array full / no core).
*/
void *
http_cgictx_get(HTTPD *httpd, const char *eye, unsigned size)
{
    void **slot = NULL;
    void  *ctx  = NULL;
    int    i;

    if (!httpd || !httpd->cgictx) return NULL;

    lock(&httpd->cgictx, LOCK_EXC);     /* STEP-scope, serialize workers     */

    for (i = 0; i <= httpd->cfg_cgictx; i++) {
        void *c = httpd->cgictx[i];
        if (c) {
            if (memcmp(c, eye, 8) == 0) { ctx = c; goto done; }  /* existing  */
        } else if (!slot) {
            slot = &httpd->cgictx[i];   /* remember first free slot           */
        }
    }
    if (!slot) goto done;               /* array full -> NULL                 */

    ctx = __getm(size);                 /* SP0 shared -> AS lifetime          */
    if (!ctx) goto done;
    memset(ctx, 0, size);
    memcpy(ctx, eye, 8);                /* stamp eyecatcher                   */
    *slot = ctx;

done:
    unlock(&httpd->cgictx, LOCK_EXC);
    return ctx;
}
