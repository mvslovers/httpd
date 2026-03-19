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

/**
 * Select the server-default codepage.
 * Called once at startup from httpconf.c.
 *
 * @param codepage  "CP037", "IBM1047", or "LEGACY" (case insensitive)
 * @return 0 on success, -1 on unknown codepage (falls back to CP037)
 */
int http_xlate_init(const char *codepage);

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
