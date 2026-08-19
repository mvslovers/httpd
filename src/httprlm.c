/* HTTPRLM.C
** Basic-auth realm from the SMF ID (#191).
**
** The realm used to be the constant "MVS", which identifies nothing: anyone
** running more than one system got the same credential dialog from all of them
** with no way to tell which machine was asking, and every httpd on the network
** advertised one shared protection space -- (origin, realm) is the key a
** browser caches Basic credentials under, so a common realm invites it to reuse
** them across servers.
**
** The SMF ID is the name the system already carries for itself, and using it
** needs no configuration at all.
**
** Kept free of project headers so the trimming and the fallback are host
** testable (test/tstrealm.c).
*/
#include "httprlm.h"

char *
httprlm(const char *smfid, char *out, unsigned outlen)
{
    unsigned n = 0;
    unsigned i;

    if (!out || outlen == 0) return out;

    /* Copy up to 4 characters, stopping at the first blank or NUL.  __smfid()
       hands over a fixed 4-byte field that is blank padded and not terminated,
       so neither bound may be dropped. */
    if (smfid) {
        while (n < 4 && n < outlen - 1 && smfid[n] && smfid[n] != ' ') {
            out[n] = smfid[n];
            n++;
        }
    }

    if (n == 0) {
        /* No id, or one that is empty or all blanks -> never an empty realm. */
        for (i = 0; HTTP_REALM_FALLBACK[i] && i < outlen - 1; i++) {
            out[i] = HTTP_REALM_FALLBACK[i];
        }
        n = i;
    }

    out[n] = '\0';

    return out;
}
