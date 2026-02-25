#include "httpd.h"

int
stck2tv(U64 *stck, struct timeval *tv)
{
    U64     tod;
    int     rc;

    /* if less than Jan 1 1970 we can't do it here */
    if (*stck < 0x7D91048BCA000000ULL) {
        memset(tv, 0, sizeof(struct timeval));
        rc = -1;
        goto quit;
    }

    /* convert STCK to timeval */
    /* make relative to 1970 (unix epoch) */
    tod  = *stck - 0x7D91048BCA000000ULL;
    tod  >>= 12;            /* convert to microseconds */
    __asm__(
        "LM\t14,15,0(%0)    load microseconds\n\t"
        "D\t14,=F'1000000'  calc seconds and microseconds\n\t"
        "ST\t15,0(0,%1)     return seconds\n\t"
        "ST\t14,4(0,%1)     and microseconds"
        : : "r" (&tod), "r" (tv));
    rc = 0;

quit:
    return rc;
}
