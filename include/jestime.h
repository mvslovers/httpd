/* JESTIME.H
** Render a JES2 job timestamp for a JSON response.
**
** Shared by jesst.c (/jes/status, /jes/ddlist) and httpjes2.c, which emitted
** the same field two ways and both wrong -- see src/jestime.c for why.
*/
#ifndef JESTIME_H
#define JESTIME_H

#include <time64.h>

/* Longest output is "2026-08-07T17:25:18.000Z" plus NUL. */
#define JESTIME_LEN 32

/* Format *t as an ISO 8601 instant in UTC ("2026-08-07T17:25:18.000Z") into
** out -- the shape real z/OSMF uses for exec-submitted / exec-started /
** exec-ended, so this API and mvsMF's stay directly comparable.
** A zero timestamp (a job that has not started or ended) yields "...", which
** is what the JSON carried before and what clients already tolerate.
** out must hold at least JESTIME_LEN bytes. */
void jestime(const time64_t *t, char *out, unsigned outlen);

#endif /* JESTIME_H */
