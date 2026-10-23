/* HTTPGIVE.C
** Flood guard for the "client abandoned" operator message (issue #347).
**
** Kept free of httpd.h so the decision unit-tests dual (host + MVS).
*/
#include "httpgive.h"

int
httpgive(unsigned *last, unsigned *held, unsigned now,
         unsigned interval, unsigned *suppressed)
{
    /* last == 0 is "no message yet".  now < last is a clock that was set
    ** back; waiting for it to catch up could silence the message for hours,
    ** so it counts as an elapsed interval. */
    if (*last != 0 && now >= *last && now - *last < interval) {
        (*held)++;
        return 0;
    }

    *suppressed = *held;
    *held = 0;
    *last = now;
    return 1;
}
