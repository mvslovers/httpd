# HTTPD Translation Subsystem — Integration Guide

## New Files

- `src/httpxlat.c` — All tables, codepage management, http_xlate(), httpetoa(), httpatoe()
- `include/httpxlat.h` — HTTPCP typedef, function declarations

## Files to DELETE

- `src/asc2ebc.c` — Replaced by tables in httpxlat.c
- `src/ebc2asc.c` — Replaced by tables in httpxlat.c
- `src/httpetoa.c` — httpetoa() now in httpxlat.c (no switch hacks)
- `src/httpatoe.c` — httpatoe() now in httpxlat.c

## Files to MODIFY

Both `httpd.h` and `httpcgi.h` define their own copy of `struct httpx` and
the macro layer.  **Both must be updated in sync.**

- `httpd.h` — used by the server itself (full crent370/libufs includes,
  `extern` declarations with `asm()` labels, `#ifndef HTTP_PRIVATE` guard)
- `httpcgi.h` — used by external CGI modules (lightweight, no crent370
  dependency, macros only)

### include/httpd.h

1. Add `#include "httpxlat.h"` near top (after existing includes)

2. Add `HTTPCP` typedef alongside the other typedefs (after `HTTPCGI`):

```c
typedef struct httpcp   HTTPCP;     /* Codepage pair            */
```

3. In the HTTPX struct, **append at end** (after mqtc_pub at offset 0x10C):

```c
    unsigned char *(*http_xlate)(unsigned char *, int, const unsigned char *);
                                    /* 110 translate with explicit table */
    HTTPCP      *xlate_cp037;       /* 114 CP037 codepage pair          */
    HTTPCP      *xlate_1047;        /* 118 IBM-1047 codepage pair       */
    HTTPCP      *xlate_legacy;      /* 11C legacy hybrid codepage pair  */
```

4. Add `extern` with `asm()` label (near the other extern declarations):

```c
extern unsigned char *http_xlate(unsigned char *, int, const unsigned char *) asm("HTTPXLAT");
```

5. In the `#ifndef HTTP_PRIVATE` macro section, add CGI-accessible macro:

```c
#define http_xlate(buf,len,tbl) \
    ((httpx->http_xlate)((buf),(len),(tbl)))
```

6. Keep the `extern` declarations for old globals (lines 81-82) — they
   still exist (defined in httpxlat.c for backward compat) but should not
   be used in new code:

```c
extern UCHAR *ebc2asc;   /* backward compat — points to active default */
extern UCHAR *asc2ebc;   /* backward compat — points to active default */
```

### include/httpcgi.h

1. Add `HTTPCP` typedef alongside the other forward declarations:

```c
typedef struct httpcp   HTTPCP;     /* Codepage pair        — opaque    */
```

2. In the HTTPX struct, **append at end** (after mqtc_pub at offset 0x10C):

```c
    unsigned char *(*http_xlate)(unsigned char *, int, const unsigned char *);
                                    /* 110 translate with explicit table */
    HTTPCP      *xlate_cp037;       /* 114 CP037 codepage pair          */
    HTTPCP      *xlate_1047;        /* 118 IBM-1047 codepage pair       */
    HTTPCP      *xlate_legacy;      /* 11C legacy hybrid codepage pair  */
```

3. Add `http_xlate` macro (after the `mqtc_pub` macro):

```c
#define http_xlate(buf,len,tbl) \
    ((httpx->http_xlate)((buf),(len),(tbl)))
```

### src/httpx.c

Add the four new entries at the end of the static vector:

```c
    mqtc_pub,                   /* 10C mqtc_pub()                   */
    http_xlate,                 /* 110 http_xlate()                 */
    &http_cp037,                /* 114 CP037 codepage pair          */
    &http_cp1047,               /* 118 IBM-1047 codepage pair       */
    &http_legacy,               /* 11C legacy hybrid codepage pair  */
};
```

### src/httpconf.c (Lua version, until Parmlib replaces it)

Add `http_xlate_init()` call during startup:

```c
/* After process_httpd() or at the end of http_config(): */
http_xlate_init("CP037");  /* or read from Lua/config */
```

When Parmlib replaces Lua, the parser reads `CODEPAGE` keyword and calls
`http_xlate_init(value)`.

### Parmlib addition

```
CODEPAGE    CP037       # default if omitted
# CODEPAGE  IBM1047
# CODEPAGE  LEGACY
```

## How mvsMF Uses This

### Before (private tables in xlate.c)

```c
#include "xlate.h"  /* mvsMF private IBM-1047 tables */
mvsmf_atoe(buf, len);
mvsmf_etoa(buf, len);
```

### After (via HTTPX vector)

```c
/* Use server default (what CODEPAGE says in Parmlib) */
http_etoa(buf, len);
http_atoe(buf, len);

/* Or use explicit codepage */
HTTPX *httpx = http_get_httpx(session->httpd);
http_xlate(buf, len, httpx->xlate_1047->etoa);
http_xlate(buf, len, httpx->xlate_cp037->atoe);
```

mvsMF can then delete `src/xlate.c` and `include/xlate.h`.

## Key Design Decisions

1. **CP037 as default** — HTTP-safe variant: NEL (0x15) maps to LF (0x0A) in
   etoa direction because the C compiler generates 0x15 for `'\n'`.
2. **Legacy tables preserved exactly** — Including the httpetoa.c switch
   behavior baked into legacy_etoa[0x15]=0x0A. No existing behavior lost.
3. **HTTPX backward compatible** — http_etoa/http_atoe stay at offsets
   0x74/0x78 with identical signature. New entries appended at end.
4. **512 bytes per codepage** (256 atoe + 256 etoa). Total: 1,536 bytes
   for all three. Negligible.
5. **Global pointers asc2ebc/ebc2asc retained** — Old httpd code that
   references them directly (httpgets.c, httpdeco.c, etc.) continues
   to work. They point to the active default tables.

## Roundtrip Verification (Critical Characters)

### CP037 (default, HTTP-safe variant)
```
ASCII LF  (0x0A) -> atoe -> 0x25 -> etoa -> 0x0A  OK
EBCDIC NEL(0x15)                  -> etoa -> 0x0A  HTTP override (pure CP037 = 0x85)
ASCII |   (0x7C) -> atoe -> 0x4F -> etoa -> 0x7C  OK
ASCII [   (0x5B) -> atoe -> 0xAD -> etoa -> 0x5B  OK
ASCII ]   (0x5D) -> atoe -> 0xBD -> etoa -> 0x5D  OK
ASCII ^   (0x5E) -> atoe -> 0xB0 -> etoa -> 0x5E  OK
ASCII \   (0x5C) -> atoe -> 0xE0 -> etoa -> 0x5C  OK
```

### IBM-1047
```
ASCII LF  (0x0A) -> atoe -> 0x25 -> etoa -> 0x85  ASYMMETRIC (known)
EBCDIC NEL(0x15)                  -> etoa -> 0x0A  (NEL -> LF, 1047 convention)
ASCII |   (0x7C) -> atoe -> 0x4F -> etoa -> 0x7C  OK
ASCII [   (0x5B) -> atoe -> 0xAD -> etoa -> 0x5B  OK
ASCII ]   (0x5D) -> atoe -> 0xBD -> etoa -> 0x5D  OK
```

### LEGACY (preserved 3.3.x behavior)
```
ASCII |   (0x7C) -> atoe -> 0x6A -> etoa -> 0x7C  OK (self-consistent but non-standard)
ASCII [   (0x5B) -> atoe -> 0xAD -> etoa -> 0x5B  OK
ASCII ]   (0x5D) -> atoe -> 0xBD -> etoa -> 0x5D  OK
EBCDIC 0x4A                       -> etoa -> 0x5B  [  (maps to [ like 0xAD — double mapping)
EBCDIC 0x4F                       -> etoa -> 0x5D  ]  (maps to ] — legacy bug preserved)
```
