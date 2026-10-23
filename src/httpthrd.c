/* HTTPTHRD.C
** Start-up hand-shake helpers for the server threads (issue #296); the why is
** in include/httpthrd.h.
**
** Free of httpd.h so they unit-test dual (host + MVS).
*/
#include <stddef.h>
#include "httpthrd.h"

void *
httpself(void *(*probe)(void), unsigned tries, void (*pause)(void))
{
    void        *answer = probe();
    unsigned    n;

    for (n = 0; answer == NULL && n < tries; n++) {
        if (pause) pause();
        answer = probe();
    }

    return answer;
}

int
httpthup(const volatile unsigned char *state, unsigned tries,
         void (*pause)(void))
{
    unsigned    n;

    for (n = 0; ; n++) {
        if (*state == HTTP_THRD_RUNNING) return 1;
        if (*state != HTTP_THRD_STARTING) return 0;
        if (n >= tries) return 0;
        if (pause) pause();
    }
}
