/* ABEND0C1.C
** Acceptance vehicle for issue #154: allocate, then abend S0C1.
**
** Before #154 an abending CGI kept everything it held for the life of the
** address space -- nothing runs its @@EXITA, the worker TCB survives by design,
** and libc370's malloc has no memmgr to reclaim from.  Enough of those and
** http_link() can no longer load ANY module: every request answers S80A until
** the server is restarted.
**
** This module makes that measurable with httpd's own code, so the acceptance
** test needs neither mvsMF nor MVSMF_ABEND_TEST=1 on a live server.  It
** allocates a known amount of storage, reports it, and then abends without
** freeing any of it -- the exact shape of the defect:
**
**     MOD=ABEND0C1  /.abend
**
** Drive it a few hundred times and then load any other CGI.  With the
** reclaim (unconditional since #174) the server is as fast as it was on the
** first request; a regression shows up as climbing response times and
** eventually S80A on every module load.
**
** ?kb=n picks the amount (default 128 KB, capped at 1 MB).  A single request
** cannot take the server down at the default size, which is deliberate: this
** is a probe an operator can point at a running system, not a demolition tool.
**
** Never registered unless a Parmlib route says so -- like every module since
** 4.0.0.
*/
#include "httpd.h"

#define httpx   (httpd->httpx)

#define CHUNK       (16 * 1024)     /* one allocation                       */
#define DEFAULT_KB  128             /* ... total when ?kb= is absent        */
#define MAX_KB      1024            /* ... and the ceiling it is capped to  */
#define MAXCHUNKS   (MAX_KB * 1024 / CHUNK)

static unsigned wanted_kb(HTTPD *httpd, HTTPC *httpc);

int main(int argc, char **argv)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    void        *block[MAXCHUNKS];
    unsigned    kb      = wanted_kb(httpd, httpc);
    unsigned    want    = (kb * 1024) / CHUNK;
    unsigned    got     = 0;
    unsigned    i;

    (void) argc;
    (void) argv;

    if (want > MAXCHUNKS) want = MAXCHUNKS;

    /* Allocate and TOUCH each block -- an untouched GETMAIN would still be
       real storage here, but writing to it makes the cost visible in a dump
       and proves the block is usable rather than merely handed out. */
    for (i = 0; i < want; i++) {
        block[i] = malloc(CHUNK);
        if (!block[i]) break;
        memset(block[i], 0xC1, CHUNK);      /* 'A' in EBCDIC                */
        got++;
    }

    wtof("ABEND0C1 allocated %u KB in %u blocks, abending without freeing",
         (got * CHUNK) / 1024, got);

    /* Nothing is freed on purpose: this is what an abending CGI leaves
       behind.  httppcgi() releases the subpool it all came from (#174);
       before that existed, it was gone until the address space ended. */
    __asm("DC\tH'0'");                      /* S0C1                         */

    return 1234;                            /* not reached                  */
}

/* wanted_kb() - ?kb=n from the query string, defaulted and capped. */
static unsigned
wanted_kb(HTTPD *httpd, HTTPC *httpc)
{
    const char  *q;
    unsigned    kb = 0;

    if (!httpd || !httpc) return DEFAULT_KB;

    q = (const char *) http_get_env(httpc, "QUERY_STRING");
    if (!q) return DEFAULT_KB;

    while (*q && http_cmpn(q, "kb=", 3) != 0) q++;
    if (!*q) return DEFAULT_KB;

    q += 3;
    while (*q >= '0' && *q <= '9') {
        kb = kb * 10 + (unsigned) (*q - '0');
        if (kb > MAX_KB) return MAX_KB;
        q++;
    }

    return kb ? kb : DEFAULT_KB;
}
