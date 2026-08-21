/* HTTPXLAT.H
** ASCII/EBCDIC codepage translation interface.
**
** Provides three codepage table pairs (CP037, IBM1047, LEGACY)
** and a generic translation function for use by HTTPD and CGI modules.
*/
#ifndef HTTPXLAT_H
#define HTTPXLAT_H

/* ------------------------------------------------------------------ */
/* Codepage pair — one table for each direction                       */
/* ------------------------------------------------------------------ */

typedef struct httpcp {
    const unsigned char *atoe;    /* ASCII-to-EBCDIC table (256 bytes) */
    const unsigned char *etoa;    /* EBCDIC-to-ASCII table (256 bytes) */
} HTTPCP;

/* ------------------------------------------------------------------ */
/* Pre-defined codepage pairs                                         */
/* ------------------------------------------------------------------ */

extern HTTPCP http_cp037;         /* IBM CP037 (CECP US/Canada)       */
extern HTTPCP http_cp1047;        /* IBM-1047  (Open Systems Latin-1) */
extern HTTPCP http_legacy;        /* HTTPD 3.3.x hybrid (compat only) */

/* ------------------------------------------------------------------ */
/* Functions                                                          */
/* ------------------------------------------------------------------ */

struct httpd;                     /* the server control block         */

/**
 * Select the server-default codepage.
 * Called once at startup from http_config().
 *
 * The selected pair is recorded in the HTTPD control block, NOT in a module
 * static: fetched from an APF-authorized or LNKLST library the load module
 * lands in key-0 storage and a key-8 store into it abends S0C4 (issue #197).
 *
 * @param httpd     server control block to record the selection in
 * @param codepage  "CP037", "IBM1047", or "LEGACY" (case insensitive)
 * @return 0 on success, -1 on unknown codepage (falls back to CP037)
 */
int http_xlate_init(struct httpd *httpd, const char *codepage);

/**
 * The codepage pair in effect.
 *
 * Resolves the HTTPD control block through the runtime anchors, so a CGI
 * module's autocalled copy reports the server's codepage rather than its own
 * uninitialised default.  Never returns NULL: without a server context (a unit
 * test, a module run by itself) it answers CP037, which is also the built-in
 * default.
 */
const HTTPCP *http_codepage(void) asm("HTTPCPGE");

/**
 * Translate a buffer in-place using an explicit table.
 *
 * @param buf  Buffer to translate
 * @param len  Length (0 = strlen)
 * @param tbl  256-byte translation table (use httpcp->atoe or ->etoa).
 *             NULL = server-default etoa table.
 * @return buf
 *
 * Usage from CGI via HTTPX:
 *   httpx->http_xlate(buf, len, httpx->xlate_1047->etoa);
 */
unsigned char *http_xlate(unsigned char *buf, int len,
                          const unsigned char *tbl) asm("HTTPXLAT");

#endif /* HTTPXLAT_H */
