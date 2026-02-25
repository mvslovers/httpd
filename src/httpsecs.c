#include "httpd.h"

#if 1

double
httpsecs(double *psecs)
{
	utime64_t	unow = utime64(NULL);
	__64		sec;
	__64		usec;
	double		result;
	
	/* convert unow value to seconds and micro seconds */ 
	__64_divmod_u32(&unow, 1000000, &sec, &usec);

	/* note: we're only using the low 32 bits which is good enough */
	/* create a seconds.useconds result */
    result = (((double)sec.u32[1]) + (((double)usec.u32[1]) / 1000000.0));

	if (psecs) *psecs = result;
	
	return result;
}

#else

double *
httpsecs(double *psecs)
{
    U64             tod;
    struct timeval  tv;
    double          tmp;

    if (!secs) secs = &tmp;

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

    /* Scale the microseconds and add to our result (secs.usecs) */
    *secs = ((double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0));
quit:
    return secs;
}
#endif
