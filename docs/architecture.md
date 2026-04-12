# HTTPD Server — Architecture

Version 3.3.1-dev

## Overview

```
+---------------------------------------------------------------------+
|                     httpd.c (main / initialize)                     |
|                      HTTPD struct (160 bytes)                       |
+----------+--------------+---------------+--------------------------+
|          |              |               |                          |
v          v              v               v                          |
+--------+ +------------+ +-------------+ +-------------+           |
| socket | |  CTHDMGR   | | telemetry   | | httpconf.c  |           |
| thread | | Worker Pool| |   thread    | |  (Lua cfg)  |           |
+---+----+ +-----+------+ +------+------+ +-------------+           |
    |            |               |                                   |
    | accept()   | worker_thread | httppub*.c                        |
    |            |               | httpdmtt.c                        |
    v            v               v                                   |
+--------------------------------------------+  +------------------+|
|       Request Processing Pipeline          |  |   FTP Daemon     ||
|  HTTPC (4096 bytes per client)             |  |   FTPD / FTPC    ||
|                                            |  |                  ||
|  +---------+ +----------+ +-----------+    |  |  ftpd*.c (25+)  ||
|  | httpin.c| |httppars.c| | httppc.c  |    |  |  PORT/PASV/RETR ||
|  | RECV    | | Headers  | | Dispatch  |    |  |  STOR/LIST/...  ||
|  +---------+ | Query    | +-----+-----+    |  +------------------+|
|              | POST     |       |          |                      |
|              +----------+       v          |                      |
|                        +----------------+  |                      |
|                        | Method Handler |  |                      |
|                        | GET/POST/PUT   |  |                      |
|                        | HEAD/DELETE    |  |                      |
|                        +-------+--------+  |                      |
|                                |           |                      |
|              +-----------------+--------+  |                      |
|              v                 v        v  |                      |
| +------------------+ +--------+ +------------+                   |
| |  Static Files    | |  CGI   | |    SSI     |                   |
| |  httpfile.c      | |        | | httpdsl*.c |                   |
| |  httpsndb/t.c    | |httplua | | echo/incl  |                   |
| |  httpmime.c      | |httprexx| | printenv   |                   |
| |  httpopen.c      | |httpjes2| +------------+                   |
| +------------------+ +--------+                                  |
|                                                                  |
| +------------------+ +--------------+ +--------------------+     |
| |  httpresp.c      | |  httprepo.c  | |  httpcred.c        |     |
| |  Response Headers| |  SMF Type243 | |  httpauth.c        |     |
| |  httpsend.c      | |  + Counters  | |  Credentials/RACF  |     |
| +------------------+ +--------------+ +--------------------+     |
+------------------------------------------------------------------+
        |                    |                      |
        v                    v                      v
+-----------------+  +--------------+  +-----------------------+
|  crent370       |  |  ufs370      |  |  mqtt370              |
|  C Runtime      |  |  Virtual FS  |  |  Telemetry            |
|  Thread Manager |  |              |  |                       |
+-----------------+  +--------------+  +-----------------------+
```

## Data Structures

### HTTPD (160 bytes)

Server-wide state. Single instance, allocated on the heap.

| Offset | Field | Description |
|--------|-------|-------------|
| 0x00 | `eye[8]` | Eye catcher `*HTTPD*` |
| 0x08 | `httpx` | HTTPX function vector pointer |
| 0x0C | `httpc` | Client array (HTTPC pointers) |
| 0x10 | `addr` | Listener IP address |
| 0x14 | `port` | Listener port |
| 0x18 | `listen` | Listener socket |
| 0x1C | `stats` | Statistics file handle |
| 0x20 | `dbg` | Debug/trace output file |
| 0x24 | `tzoffset` | Time zone offset (seconds) |
| 0x28 | `busy` | Busy clients array |
| 0x2C | `flag` | Processing flags (INIT/LISTENER/READY/QUIESCE/SHUTDOWN) |
| 0x2D | `login` | Login rules (ALL/CGI/GET/HEAD/POST) |
| 0x2E | `client` | Client options (INMSG/INDUMP/STATS) |
| 0x30 | `socket_thread` | Socket listener thread (CTHDTASK) |
| 0x34 | `mgr` | Worker thread manager (CTHDMGR) |
| 0x44 | `httpcgi` | CGI path/program mappings |
| 0x48 | `uptime` | Server startup time (time64_t) |
| 0x50 | `ftpd` | FTP daemon handle |
| 0x54 | `ufssys` | UFS filesystem system |
| 0x58 | `luax` | Lua function vector (LUAX) |
| 0x5C | `version` | Version string pointer |
| 0x60 | `config` | Lua configuration state |
| 0x64 | `cfg_maxtask` | Max worker threads (default 9) |
| 0x65 | `cfg_mintask` | Min worker threads (default 3) |
| 0x66 | `cfg_client_timeout` | Client timeout (seconds) |
| 0x70 | `st_month` | Monthly statistics array |
| 0x74 | `st_day` | Daily statistics array |
| 0x78 | `st_hour` | Hourly statistics array |
| 0x7C | `st_min` | Per-minute statistics array |
| 0x84 | `cgilua_dataset` | CGI Lua dataset name |
| 0x90 | `ufs` | UFS handle |
| 0x94 | `httpt` | MQTT telemetry handle |
| 0x98 | `self` | HTTPD main thread |
| 0x9C | `cgictx` | CGI context pointer array |

### HTTPC (4096 bytes)

Per-client HTTP session. Allocated on each accept(), freed on close.

| Offset | Field | Description |
|--------|-------|-------------|
| 0x00 | `eye[8]` | Eye catcher `*HTTPC*` |
| 0x08 | `httpd` | Back-pointer to HTTPD |
| 0x0C | `env` | Environment variables array (HTTPV pointers) |
| 0x10 | `state` | Current state (CSTATE_*) |
| 0x14 | `socket` | Client socket |
| 0x18 | `addr` | Client IP address |
| 0x1C | `port` | Client port |
| 0x20 | `fp` | File handle (for serving) |
| 0x24 | `len` | Data length in buffer |
| 0x28 | `pos` | Position in buffer |
| 0x2C | `sent` | Bytes sent |
| 0x30 | `start` | Request start time (double, seconds) |
| 0x38 | `end` | Request end time |
| 0x42 | `resp` | HTTP response code |
| 0x44 | `cred` | Client credential (CRED pointer) |
| 0x48 | `ufs` | UFS handle |
| 0x4C | `ufp` | UFS file pointer |
| 0x50 | `ssi` | SSI enabled flag |
| 0x51 | `ssilevel` | SSI recursion depth |
| 0x58 | `buf[4008]` | Data buffer (CBUFSIZE) |

### HTTPX (~270 bytes)

Function vector table with 68+ function pointers for runtime dispatch.
CGI modules receive this via `httpd->httpx` and call server functions
through it — they never link directly to server code.

Key function pointers:
- `http_in`, `http_parse`, `http_get`, `http_head`, `http_put`, `http_post`
- `http_done`, `http_report`, `http_reset`, `http_close`
- `http_send`, `http_printf`, `http_printv`
- `http_get_env`, `http_set_env`, `http_find_env`, `http_del_env`
- `http_resp`, `http_open`, `http_mime`
- `http_etoa`, `http_atoe`, `http_cmp`, `http_cmpn`
- `array_new`, `array_add`, `array_get`, `array_count`, `array_free`
- `http_dbgf`, `http_dump`

### FTPD (48 bytes) / FTPC (4088 bytes)

FTP daemon and per-client session structures.

FTPD holds the FTP listener socket and client array.
FTPC holds per-session state: control/data sockets, working directory (252 bytes),
command buffer (252 bytes), data buffer (3496 bytes), transfer type/mode/structure,
and RACF security context (ACEE pointer).

### HTTPT (64 bytes)

MQTT telemetry handle. Holds broker connection (host/port), topic prefix,
telemetry cache array, and the telemetry thread reference.

### HTTPCGI (20 bytes)

CGI path-to-program mapping. Each entry maps a URL path pattern
to a load module name. Supports wildcard matching.

### HTTPSTAT (64 bytes)

Statistics record. Tracks first/last timestamp, lowest/highest/total
response time, tally count, and response code.

### HTTPV (variable size)

Environment variable. Fixed header (eye catcher + value pointer) followed
by flexible name/value storage as `name\0value\0`.

## Thread Model

### Socket Thread (32 KB stack)

Single thread running a `select()`/`selectex()` loop with 1-second timeout.
Accepts new HTTP and FTP connections. Dispatches accepted clients to the
worker pool via `cthread_queue_add(mgr, httpc)`.

### Worker Pool — CTHDMGR (64 KB stack per worker)

Thread pool managed by crent370's CTHDMGR:
- **Default**: 3 minimum, 9 maximum workers
- **Configurable**: via Lua config or console (`/F HTTPD,SET MINTASK`,
  `/F HTTPD,SET MAXTASK`)
- Each worker processes one HTTPC through the full state machine
  (CSTATE_IN through CSTATE_CLOSE)

### Telemetry Thread (32 KB stack, optional)

Single thread for MQTT publishing. Waits on an ECB, woken by worker
threads via `cthread_post()`. Publishes thread status, client counts,
and server metrics.

### ABEND Isolation

Worker threads are wrapped with `abendrpt(ESTAE_CREATE, ...)` /
`abendrpt(ESTAE_DELETE, ...)`. A crash (S0C4, S80A, etc.) in a CGI
module or request handler kills only the affected worker thread.
The server continues accepting new requests on surviving workers,
and the dispatcher creates replacement workers up to the configured maximum.

## Request Pipeline (State Machine)

```
CSTATE_IN -----> CSTATE_PARSE -----> CSTATE_GET/HEAD/PUT/POST/DELETE
                                              |
                                              v
                                         CSTATE_DONE
                                              |
                                              v
                                        CSTATE_REPORT
                                              |
                                              v
                                        CSTATE_RESET
                                           |     |
                                           v     v
                                   CSTATE_IN   CSTATE_CLOSE
                                  (keep-alive)
```

| Phase | File | Function | Description |
|-------|------|----------|-------------|
| 1. Input | httpin.c | `http_in()` | Read request line + headers from socket |
| 2. Parse | httppars.c | `http_parse()` | Parse method, URI, query string, POST body |
| 3. Dispatch | httppc.c | `http_process_client()` | CGI matching, auth check, method dispatch |
| 4. Method | httpget.c etc. | `http_get()` etc. | Open file, determine MIME, serve response |
| 5. Response | httpresp.c | `http_resp()` | Generate HTTP status line + headers |
| 6. Send | httpsend.c | `http_send_*()` | Binary or text data with encoding conversion |
| 7. Report | httprepo.c | `http_report()` | Write SMF record, update counters |
| 8. Reset | httprese.c | `http_reset()` | Free env vars, clear buffer, prepare for next |

### Dispatch Logic (httppc.c)

1. Check for CGI path match via `http_find_cgi()` against registered patterns
2. If CGI: `http_process_cgi()` -> `http_link()` -> `__linkds(pgm, dcb, plist, &prc)`
3. If `/login` or `/logout`: `httpcred()` credentials handler
4. Default: static file serving via `http_open()` + `http_send_file()`

### Environment Variables

Set per HTTPC during parsing:
- `REQUEST_METHOD`, `REQUEST_URI`, `REQUEST_PATH`, `REQUEST_VERSION`
- `QUERY_STRING` — everything after `?`
- `HTTP_<name>` — request headers (e.g., `HTTP_Content-Type`)
- `QUERY_<name>` — individual query parameters
- `SERVER_ADDR`, `SERVER_PORT`, `SERVER_SOFTWARE`

## CGI Subsystem

### Module Loading (httplink.c)

CGI modules are loaded via MVS LINK SVC:

```c
plist[0] = (unsigned)&parms       /* request path */
plist[1] = (unsigned)httpd        /* HTTPD pointer -> grtapp1 */
plist[2] = (unsigned)httpc | VL   /* HTTPC pointer -> grtapp2 */

rc = __linkds(pgm, dcb, plist, &prc);
```

The GRT (Global Register Table) makes httpd/httpc available to the
linked module via `grt->grtapp1` and `grt->grtapp2`.

### CGI Startup (cgistart.c)

Provides a custom `__start` (symbol @@START) that replaces crent370's
default startup. Scans the parameter list for HTTPD/HTTPC eye catchers
(`*HTTPD*`, `*HTTPC*`) and stores them in `grt->grtapp1` / `grt->grtapp2`.
This is how CGI modules discover the server context.

Also provides a custom `printf()` that routes output through the HTTPX
vector table when available, sending directly to the HTTP client socket
instead of stdout.

### HTTPX Vector Table (httpx.c)

CGI modules never link directly to server functions. Instead, they call
through function pointers in the HTTPX struct:

```c
/* In a CGI module: */
HTTPD *httpd = (HTTPD *)grt->grtapp1;
HTTPC *httpc = (HTTPC *)grt->grtapp2;
HTTPX *httpx = httpd->httpx;

httpx->http_resp(httpc, 200);
httpx->http_printf(httpc, "Content-Type: text/html\r\n\r\n");
httpx->http_printf(httpc, "<h1>Hello</h1>");
```

### LUAX Vector Table (httpluax.c / httpluax.h)

Separate function vector with ~190 Lua 5.4 C API pointers. Loaded once
via `__load(0, "HTTPLUAX", 0, 0)` and stored in `httpd->luax`.
Lua CGI modules call the Lua API through this vector.

### CGI Path Registration

Configured in Lua configuration:

```lua
httpd.cgi["/lua/(.*)"]  = "HTTPLUA"
httpd.cgi["/rexx/(.*)"] = "HTTPREXX"
httpd.cgi["/jes/(.*)"]  = "HTTPJES2"
```

### Module Caching

Loaded CGI modules are cached in memory by MVS. After installing a new
version into the LOADLIB, the HTTPD STC must be restarted.

## Subsystems

### Static File Serving

- `httpopen.c` — Resolve paths to UFS files or MVS datasets (`DD:NAME(MBR)`)
- `httpfile.c` — File delivery orchestration
- `httpmime.c` — MIME type detection from file extension
- `httpsndb.c` — Send binary data (images, downloads)
- `httpsndt.c` — Send text data with EBCDIC-to-ASCII conversion

### Lua CGI (httplua.c)

Entry via CGISTART. Creates temporary datasets for STDIN/STDOUT/STDERR.
Loads Lua script from UFS or MVS dataset. Sets environment variables as
Lua globals. Executes via `lua_pcall()` through the LUAX vector table.

### BREXX CGI (httprexx.c)

Similar to Lua CGI but for REXX scripts. Serialized access — only one
REXX instance at a time.

### JES2 (httpjes2.c, jesst.c, jespr.c)

Job management via JES2 API (crent370's clibjes2.h). Supports:
- Job status query (`QUERY_JOBNAME`, `QUERY_JOBID`)
- Spool output retrieval
- Job cancel and purge
- DD list enumeration

### Server-Side Includes (httpdsl.c + 6 helper files)

Processes `<!--#...-->` directives embedded in HTML:
- `#echo var=...` — output environment variable (httpdsle.c)
- `#include file=...` — include another file (httpdsli.c)
- `#printenv` — list all variables (httpdslp.c)

Maximum recursion depth: 10 levels (SSI_LEVEL_MAX).

### FTP Daemon (28 files)

Full FTP server on a separate port (default 8021). Per-client FTPC
sessions with control and data channels. Supports:
- Active (PORT) and passive (PASV) mode
- ASCII, EBCDIC, and binary (IMAGE) transfer types
- RETR, STOR, LIST, CWD, MKD, DELE, RMD, and more
- UFS and MVS dataset access

### Credentials (httpcred.c, httpauth.c, credentials/)

Token-based authentication with RACF password verification.
SHA-256 hashed tokens stored as HTTP cookies. The credentials
subsystem (18 source files) handles token generation, lookup,
and lifecycle management.

### MQTT Telemetry (httppubf.c, httpdmtt.c)

Optional MQTT 3.1.1 client for publishing server metrics.
Topics include thread status, client counts, and request statistics.
Telemetry cache (HTTPTC array) stores latest values per topic.

### Configuration (httpconf.c)

Lua-based configuration loaded from `PARM='CONFIG=dataset(member)'`.
Global Lua tables: `httpd`, `ftpd`, `cgi`, `mqtc`.

### Statistics (httprepo.c)

SMF Type 243 records written per HTTP request via `smf_write()`.
Simple in-memory counters (total_requests, total_errors,
total_bytes_sent, active_connections) displayed via `/F HTTPD,D S`.

### Console Commands (httpcons.c, httpcmd.c)

Operator interface via `/F HTTPD,command`:
- `D V` — display version
- `D T` — display threads
- `D S` — display statistics counters
- `S MAXTASK n` — set max worker threads
- `S MINTASK n` — set min worker threads
- `S LOGIN [all|cgi|none]` — set login requirements
- `S STATS ON|OFF [RESET]` — control SMF recording, reset counters

## Network I/O

### Character Encoding

MVS operates in EBCDIC. Network I/O (HTTP) uses ASCII. Conversion
happens at the I/O boundary:

- **Receive side**: Raw bytes from socket are ASCII. Converted to EBCDIC
  via `http_atoe()` for internal processing.
- **Send side**: Text data is EBCDIC internally. Converted to ASCII via
  `http_etoa()` before sending. Binary data sent as-is.
- **Variable storage**: All environment variables stored as EBCDIC.

Conversion tables: `asc2ebc[]` (256-byte lookup, asc2ebc.c) and
`ebc2asc[]` (ebc2asc.c).

Trigraph macros for EBCDIC limitations (the `|` character is not
available on all EBCDIC terminals):

```c
#define OR  ??!??!    /* logical OR  */
#define BOR ??!       /* bitwise OR  */
```

### TCP/IP Ring Buffer Bug

The MVS 3.8j TCP/IP stack has a ring buffer bug: multi-byte `recv()` calls
that span the internal buffer wrap-around point return corrupted (replayed)
data. The workaround in `http_getc()` reads exactly one byte at a time:

```c
i = recv(httpc->socket, buf, 1, 0);  /* always 1 byte */
```

This is intentional. Do not optimize to larger recv sizes.

## Memory Management

- HTTPD: single heap allocation, locked via `lock(httpd, 0)` for concurrent access
- HTTPC: 4096 bytes per client, `calloc()` on accept, `free()` on close
- HTTPV: variable-size env vars, allocated per request, freed in CSTATE_RESET
- Arrays: dynamic via crent370's `array_*()` functions
- All memory must fit in 24-bit address space (16 MB maximum)
- No memory leaks allowed — explicit free on every allocation path

## External Dependencies

| Project | Purpose | SDK Location |
|---------|---------|-------------|
| crent370 | C runtime, threading (CTHDMGR), crypto (SHA-256), base64 | `contrib/crent370_sdk/inc/` |
| ufs370 | Unix-like virtual filesystem for MVS datasets | `contrib/ufs370_sdk/inc/` |
| lua370 | Lua 5.4 scripting engine (C89 compatible) | `contrib/lua370_sdk/inc/` |
| mqtt370 | MQTT 3.1.1 client for telemetry | `contrib/mqtt370_sdk/inc/` |
| brexx370 | REXX interpreter (runtime only, no SDK) | — |

## Build Pipeline

```
C source (.c)
  --> c2asm370 (GCC 3.2.3 cross-compiler, -fverbose-asm -S -O1)
        --> S/370 assembly (.s), truncated to column 71
  --> mvsasm (submits JCL via mvsMF REST API)
        --> ASMBLR (IFOX00) + IEWL (NCAL,LET,XREF,RENT)
        --> NCAL modules in HTTPD.NCALIB
  --> mvslink (submits link JCL via mvsMF REST API)
        --> IEWL (LIST,MAP,XREF,RENT)
        --> Load modules in HTTPD.LOADLIB
```

Build targets:
- `make` — compile all C sources and assemble on MVS
- `make clean` — remove generated .s and .o files
- `make link` — link all 13 load modules on MVS
- `make link MODULES=HTTPJES2` — link a single module
- `make compiledb` — generate compile_commands.json for clangd

## Load Modules

13 load modules are produced by mvslink:

| Module | Type | Entry | Description |
|--------|------|-------|-------------|
| HTTPD | Server | @@CRT0 | Main server (includes Lua members, SETCODE AC(1)) |
| HTTPLUAX | Extension | HTTPLUAX | Lua function vector (no CRT, includes Lua members) |
| HTTPLUA | CGI | @@CRT0 | Lua CGI handler |
| HTTPREXX | CGI | @@CRT0 | BREXX CGI handler |
| HTTPJES2 | CGI | @@CRT0 | JES2 job management |
| HTTPDSL | CGI | @@CRT0 | Server-Side Includes |
| HTTPDSRV | CGI | @@CRT0 | Server status display |
| HTTPDM | CGI | @@CRT0 | Diagnostics/monitoring |
| HTTPDMTT | CGI | @@CRT0 | MQTT telemetry |
| HTTPTEST | CGI | @@CRT0 | Test module |
| HELLO | CGI | @@CRT0 | Sample CGI module |
| HTTPSAY | CGI | @@CRT0 | BREXX helper (uses @@CRTM, no CGISTART) |
| ABEND0C1 | CGI | @@CRT0 | Deliberate ABEND for testing ESTAE recovery |

## Source File Organization (152 files)

| Category | Count | Key Files |
|----------|-------|-----------|
| Core server | 10 | httpd.c, httpstrt.c, httpx.c, httpcons.c, httpconf.c |
| Request pipeline | 8 | httpin.c, httppars.c, httppc.c, httpget/post/put/del/head.c |
| Response | 6 | httpresp.c, httpsend.c, httpsndb.c, httpsndt.c, httprepo.c, httprese.c |
| Static files | 5 | httpfile.c, httpopen.c, httpmime.c, httprnf.c, httprnim.c |
| CGI framework | 7 | httplink.c, httpacgi.c, httpfcgi.c, httppcgi.c, cgistart.c, cgihttpd.c, cgihttpc.c |
| Env variables | 7 | httpfenv.c, httpgenv.c, httpnenv.c, httpsenv.c, httpshen.c, httpsqen.c, httpdenv.c |
| Lua scripting | 5 | httplua.c, httpluax.c, lauxlib.c, liolib.c, loadlib.c |
| BREXX scripting | 2 | httprexx.c, httpsay.c |
| JES2 | 3 | httpjes2.c, jesst.c, jespr.c |
| SSI | 7 | httpdsl.c, httpdsld.c, httpdsle.c, httpdsli.c, httpdsll.c, httpdslp.c, httpdslx.c |
| FTP daemon | 28 | ftpd*.c (commands), ftpc*.c (client) |
| Encoding | 5 | asc2ebc.c, ebc2asc.c, httpatoe.c, httpetoa.c, httpdeco.c |
| Network I/O | 3 | httpgetc.c, httpgets.c, httpisb.c |
| Credentials | 20 | httpcred.c, httpauth.c, credentials/src/*.c (18 files) |
| MQTT telemetry | 2 | httppubf.c, httpdmtt.c |
| Debug | 7 | dbgdump.c, dbgenter.c, dbgexit.c, dbgf.c, dbgs.c, dbgtime.c, dbgw.c |
| Statistics/SMF | 1 | httprepo.c |
| Utilities | 16 | hello.c, abend0c1.c, httptest.c, httpclos.c, http1123.c, httpd048.c, httpntoa.c, httpgsna.c, httpgtod.c, httpsecs.c, httprise.c, httpsbz.c, httprbz.c, httpsubt.c, httpcmp.c, httpcmpn.c |
| String/compare | 4 | httpcmp.c, httpcmpn.c, stck2tv.c, httpdbug.c |
