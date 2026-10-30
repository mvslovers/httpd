/* CREDUNAM.C
** Does the user an ACEE names match the userid that was asked for?
**
** The security system answers a login with an ACEE.  Some configurations
** answer every request with the same ACEE, whatever the userid was; a caller
** that takes the answer for granted then acts as somebody it never checked.
** This compares what was asked with what came back.
**
** Kept free of project headers so it unit-tests on the host as well as on MVS
** (see test/tstunam.c).  The C name is <= 8 chars, so cc370 maps it to CSECT
** CREDUNAM.
*/

#include <ctype.h>

#define UNAM_LEN    8       /* userid length: racf_login() cuts at 8 */

/* number of characters of s[0..max) before trailing blanks and NULs */
static unsigned
unam_len(const unsigned char *s, unsigned max)
{
    while (max > 0 && (s[max - 1] == ' ' || s[max - 1] == 0)) max--;
    return max;
}

/* want:    the userid as requested (NUL-terminated, any case, cut at 8)
** name:    the ACEE's user name, aceeusri
** namelen: the ACEE's name length, aceeusrl (only the first 8 count)
** returns 1 if both name the same user, else 0 (also for an empty name)
*/
int
credunam(const char *want, const unsigned char *name, unsigned namelen)
{
    unsigned char w[UNAM_LEN];
    unsigned wl = 0;
    unsigned nl;
    unsigned i;

    if (!want || !name) return 0;

    while (wl < UNAM_LEN && want[wl]) {
        w[wl] = (unsigned char) want[wl];
        wl++;
    }
    wl = unam_len(w, wl);
    if (namelen > UNAM_LEN) namelen = UNAM_LEN;
    nl = unam_len(name, namelen);

    if (wl == 0 || nl == 0 || wl != nl) return 0;

    for (i = 0; i < wl; i++) {
        if (toupper(w[i]) != toupper(name[i])) return 0;
    }
    return 1;
}
