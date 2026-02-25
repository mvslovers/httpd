/* HTTPMIME.C
** HTTP Multipurpose Internet Mail Extensions
*/
#include "httpd.h"

static HTTPM mimes[] = {
/*  Extension  Document Type                    Binary */
   {"ai",      "application/postscript",        1},
   {"aif",     "audio/x-aiff",                  1},
   {"aifc",    "audio/x-aiff",                  1},
   {"aiff",    "audio/x-aiff",                  1},
   {"au",      "audio/basic",                   1},
   {"avi",     "video/x-msvideo",               1},
   {"bcpio",   "application/x-bcpio",           1},
   {"bin",     "application/octet-stream",      1},
   {"c",       "text/plain",                    0},
   {"cdf",     "application/x-netcdf",          1},
   {"cpio",    "application/x-cpio",            1},
   {"csh",     "application/x-csh",             1},
   {"css",     "text/css",                      0},
   {"dvi",     "application/x-dvi",             1},
   {"eps",     "application/postscript",        0},
   {"etx",     "text/x-setext",                 0},
   {"gif",     "image/gif",                     1},
   {"gtar",    "application/x-gtar",            1},
   {"h",       "text/plain",                    0},
   {"hdf",     "application/x-hdf",             1},
   {"htm",     "text/html",                     0},
   {"html",    "text/html",                     0},
   {"ico",     "image/x-icon",                  1},
   {"icon",    "image/x-icon",                  1},
   {"ief",     "image/ief",                     1},
   {"jpe",     "image/jpeg",                    1},
   {"jpeg",    "image/jpeg",                    1},
   {"jpg",     "image/jpeg",                    1},
   {"js",      "application/x-javascript",      0},
   {"latex",   "application/x-latex",           1},
   {"lod",     "application/x-load",            1},
   {"lua",     "text/plain",                    0},
   {"man",     "application/x-troff-man",       0},
   {"me",      "application/x-troff-me",        0},
   {"mif",     "application/x-mif",             1},
   {"mov",     "video/quicktime",               1},
   {"movie",   "video/x-sgi-movie",             1},
   {"mpe",     "video/mpeg",                    1},
   {"mpeg",    "video/mpeg",                    1},
   {"mpg",     "video/mpeg",                    1},
   {"ms",      "application/x-troff-ms",        1},
   {"nc",      "application/x-netcdf",          1},
   {"oda",     "application/oda",               1},
   {"pbm",     "image/x-portable-bitmap",       1},
   {"pdf",     "application/pdf",               1},
   {"pgm",     "image/x-portable-graymap",      1},
   {"png",     "image/png",                     1},
   {"pnm",     "image/x-portable-anymap",       1},
   {"ppm",     "image/x-portable-pixmap",       1},
   {"ps",      "application/postscript",        0},
   {"qt",      "video/quicktime",               1},
   {"ras",     "image/x-cmu-raster",            1},
   {"rex",     "text/plain",                    0},
   {"rexx",    "text/plain",                    0},
   {"rgb",     "image/x-rgb",                   1},
   {"rng",     "text/xml",                      0},
   {"roff",    "application/x-troff",           1},
   {"rtf",     "application/rtf",               1},
   {"rtx",     "text/richtext",                 1},
   {"sh",      "application/x-sh",              1},
   {"shar",    "application/x-shar",            1},
   {"shtm",    "text/x-server-parsed-html",     0},
   {"shtml",   "text/x-server-parsed-html",     0},
   {"snd",     "audio/basic",                   1},
   {"src",     "application/x-wais-source",     0},
   {"svg",     "image/svg+xml",                 0},
   {"sv4cpio", "application/x-sv4cpio",         1},
   {"sv4src",  "application/x-sv4crc",          1},
   {"t",       "application/x-troff",           1},
   {"tar",     "application/x-tar",             1},
   {"tcl",     "application/x-tcl",             1},
   {"tex",     "application/x-tex",             1},
   {"texi",    "application/x-texinfo",         1},
   {"texinfo", "application/x-texinfo",         1},
   {"tif",     "image/tiff",                    1},
   {"tiff",    "image/tiff",                    1},
   {"tr",      "application/x-troff",           1},
   {"tsv",     "text/tab-separated-values",     0},
   {"txt",     "text/plain",                    0},
   {"ustar",   "application/x-ustar",           1},
   {"wav",     "audio/x-wav",                   1},
   {"xbm",     "image/x-xbitmap",               1},
   {"xhtml",   "application/xhtml+xml",         0},
   {"xml",     "text/xml",                      0},
   {"xpm",     "image/x-xpixmap",               1},
   {"xsl",     "text/xsl",                      0},
   {"xslt",    "text/xsl",                      0},
   {"xwd",     "image/x-xwindowdump",           1},
   {"zip",     "application/zip",               1},
};
#define MAX_MIME (sizeof(mimes) / sizeof(mimes[0]))

static HTTPM default_mime = { "txt", "text/plain", 0};

static int
mime_cmp(const void *pv1, const void *pv2)
{
    const HTTPM *p1 = pv1;
    const UCHAR *p2 = pv2;
    int   rc;

    rc = http_cmp(p1->ext, p2);

    return rc;
}

const HTTPM *
httpmime(const UCHAR *path)
{
    UCHAR   *ext    = NULL;
    HTTPM   *mime   = NULL;
    UCHAR   *sep;
    int     len;
    UCHAR   *p;
    UCHAR   buf[80];

    http_enter("httpmime()\n" );

    if (path) {

        sep = strrchr(path, '/');

        /* look in last path fragment for the file extension */
        if (sep) ext = strrchr(sep, '.');

        /* look in full path for extension "filename.ext" */
        if (!ext) ext = strrchr(path, '.');

        /* look in full path for colon "dd:name(member)" */
        if (!ext) ext = strchr(path, ':');
    }

    if ( ext ) {
        ext++;    /* skip past '.' or ':' character */

        len = strlen(ext);
        if (len >= sizeof(buf)) len = sizeof(buf)-1;
        memcpy(buf,ext,len);
        buf[len] = 0;

        /* strip member from dd name */
        p = strchr(buf,'(');
        if (p) *p = 0;

        /* Look for the mime for this file extension */
        mime = bsearch(buf, mimes, MAX_MIME, sizeof(mimes[0]), mime_cmp);
    }

    if (!mime) {
        /* return default mime for this path */
        mime = &default_mime;
    }

    http_exit("httpmime() \"%s\",\"%s\",%s\n",
        mime->ext, mime->type, mime->binary?"binary":"character" );
    return mime;
}
