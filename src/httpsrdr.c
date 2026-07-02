/* HTTPSRDR.C
** Validate a redirect target is a safe, site-local path.
**
** Returns 1 only for a path that is safe to place in a Location: header
** without enabling header injection or an open redirect:
**   - must be non-empty and start with a single '/'   (site-relative)
**   - the 2nd char must not be '/' or '\'              (//host, /\host are
**                                                       protocol-relative;
**                                                       browsers normalise
**                                                       '\' to '/')
**   - must contain no CR or LF                         (Location: splitting)
** Anything else returns 0 and the caller should fall back to "/".
**
** Kept free of httpd.h (only char literals) so the pure logic unit-tests on
** the host as well as on MVS; correct under both EBCDIC and ASCII.
*/

typedef unsigned char UCHAR;

/* http_safe_redirect() */
int
httpsrdr(const UCHAR *uri)
{
    const UCHAR *p;

    if (!uri || uri[0] != '/')            return 0;  /* must be site-relative */
    if (uri[1] == '/' || uri[1] == '\\')  return 0;  /* //host or /\host      */

    for (p = uri; *p; p++) {
        if (*p == '\r' || *p == '\n')     return 0;  /* header injection      */
    }

    return 1;
}
