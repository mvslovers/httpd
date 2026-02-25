/* DBGTIME.C
**
*/
#include "httpd.h"

int
dbgtime(const char *sep)
{
    CLIBGRT         *grt    = __grtget();
    HTTPD           *httpd  = grt->grtapp1;
    int             rc      = 0;
    struct tm       *tm;
    U64             tod;
    struct timeval  tv;
    char            buf[40];

    if (!httpd->dbg) goto quit;
    if (!sep) sep = ": ";

    /* get STCK value */
    __asm__("STCK\t0(%0)" : : "r" (&tod));

    /* make Jan 1 1900 (STCK) relative to Jan 1 1970 (unix epoch) */
    tod -=  0x7D91048BCA000000ULL;  /* STCK value for Jan 1 1970 */

    /* convert to microseconds (bits 0-51==number of microseconds) */
    tod >>= 12; /* convert to microseconds (1 us = .000001 sec) */

    /* calc seconds and microseconds (divide by 1000000) */
    __asm__(
        "LM\t0,1,0(%0)       load TOD microseconds\n\t"
        "D\t0,=F'1000000'  divide by 1000000\n\t"
        "ST\t1,0(0,%1)       store seconds (quotient)\n\t"
        "ST\t0,4(0,%1)       store microseconds (remainder)"
        : : "r" (&tod), "r" (&tv));

    tm = localtime((time_t*)&tv.tv_sec);
    if (!tm) goto quit;

    strftime(buf, sizeof(buf), "%Y%m%d.%H%M%S", tm);
    fprintf(httpd->dbg, "%s.%06u%s", buf, tv.tv_usec, sep);

quit:
    return rc;
}
