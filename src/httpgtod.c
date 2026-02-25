#include "httpd.h"

void
httpgtod(struct timeval *tv)
{
    U64             tod;

    /* get STCK value */
    __asm__("STCK\t0(%0)" : : "r" (&tod));

    /* convert STCK to timeval */
    /* make relative to 1970 (unix epoch) */
    tod  -= 0x7D91048BCA000000ULL;
    tod  >>= 12;            /* convert to microseconds */
    __asm__(
        "LM\t14,15,0(%0)    load microseconds\n\t"
        "D\t14,=F'1000000'  calc seconds and microseconds\n\t"
        "ST\t15,0(0,%1)     return seconds\n\t"
        "ST\t14,4(0,%1)     and microseconds"
        : : "r" (&tod), "r" (tv));
}
