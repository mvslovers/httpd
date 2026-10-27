/* HTTPTASK.C
** Check and clamp the operand of SET MAXTASK / SET MINTASK.
**
** Kept free of httpd.h (char literals only) so the decision unit-tests dual
** (host + MVS).
*/
#include <stdlib.h>
#include "httptask.h"

int
httptask(const char *s, unsigned *value)
{
    unsigned long   n;

    if (!s) return HTTP_TASK_USAGE;

    while (*s == ' ') s++;
    if (*s == '\0' || *s == '-') return HTTP_TASK_USAGE;

    n = strtoul(s, NULL, 10);
    if (n < 1) n = 1;
    if (n > HTTP_TASK_MAX) n = HTTP_TASK_MAX;

    *value = (unsigned) n;
    return HTTP_TASK_OK;
}
