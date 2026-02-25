/* HTTPCMPN.C
** Caseless compare of two strings
*/
#include "httpd.h"

int
httpcmpn(const UCHAR *s1, const UCHAR *s2, int n)
{
    UCHAR c1, c2;

    if (s1 == s2) return 0;

    do {
        c1 = *s1;
        if (isupper(c1)) {
            c1 = tolower(c1);
        }

        c2 = *s2;
        if (isupper(c2)) {
            c2 = tolower(c2);
        }
        
        if ((--n == 0) ??!??! (c1 == '\0')) break;

        ++s1;
        ++s2;
    } while (c1 == c2);

    return (c1 - c2);
}
