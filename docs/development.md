# HTTPD — Development Guide

This document covers building HTTPD from source, the server architecture, the ASCII/EBCDIC translation system, and how to write custom CGI modules.

## Building from Source

### Prerequisites

- `mbt` build system ([mvslovers/mbt](https://github.com/mvslovers/mbt))
- `c2asm370` cross-compiler
- Access to an MVS 3.8j system (Hercules) for linking and testing

### Build Commands

```bash
make clean build link        # Full build
make build                   # Compile only
make link                    # Link only (after compile)
make install                 # Install load modules to MVS
```

The build system uses `project.toml` for project metadata and dependency management. Dependencies (crent370, ufsd) are resolved automatically by `mbt`.

### Project Structure

```
httpd/
  include/           C headers (httpd.h, httpcgi.h, httpxlat.h, ...)
  src/               Server source files (~86 .c files)
  credentials/       Authentication subsystem (Blowfish, RACF, sessions)
    include/         Credential headers
    src/             Credential source files
  samplib/           Sample JCL procedure and Parmlib member
  docs/              Documentation
  project.toml       Build configuration and dependencies
```

### C Standard

HTTPD uses `-std=gnu99`. This means `//` line comments, mixed declarations and statements, and C99 features are permitted. See `CLAUDE.md` for detailed coding guidelines.

## Architecture

### Thread Model

```
┌──────────────────────────────────────────────┐
│                    httpd.c                    │
│               HTTPD struct (main)            │
├──────────────┬───────────────────────────────┤
│              │                               │
│  socket_thread                               │
│  (select loop)     Worker Pool (CTHDMGR)     │
│  accepts TCP       3-9 threads               │
│  connections       process requests          │
│              │                               │
│  ┌──────────┴──────────────────────────────┐ │
│  │        Request Pipeline (per HTTPC)     │ │
│  │  IN → PARSE → GET/POST/PUT → DONE →    │ │
│  │  REPORT → RESET (or CLOSE)             │ │
│  └─────────────────────────────────────────┘ │
└──────────────────────────────────────────────┘
```

**Socket thread** (32 KB stack): Runs a `select()`/`selectex()` loop accepting new TCP connections. Dispatches accepted clients to the worker pool.

**Worker pool** (64 KB stack per worker): Managed by crent370's CTHDMGR. Each worker processes one client through the complete request state machine. Default: 3 minimum, 9 maximum workers.

### Request State Machine

Each HTTP request flows through these states:

| State | File | Description |
|-------|------|-------------|
| `CSTATE_IN` | httpin.c | Receive and buffer the request line and headers |
| `CSTATE_PARSE` | httppars.c | Parse method, URI, headers, query string, POST body |
| `CSTATE_GET` | httpget.c | Serve static file from UFS or dispatch to CGI |
| `CSTATE_POST` | httppars.c | Parse POST body |
| `CSTATE_DONE` | httpdone.c | Send final chunk terminator, finalize response |
| `CSTATE_REPORT` | httpsmf.c | Write SMF record, increment counters |
| `CSTATE_RESET` | httprese.c | Clean up for next request (Keep-Alive) or close |

### Key Data Structures

**HTTPD** — Server-wide state. Single instance. Holds the listener socket, client array, worker pool, CGI registrations, configuration, and SMF settings.

**HTTPC** — Per-client session. Allocated on `accept()`, freed on connection close. Contains the socket, receive buffer (4 KB), environment variables, request state, response code, UFS handle, and credential pointer.

**HTTPX** — Function vector table (~270 bytes). Contains ~68 function pointers that CGI modules use to call server functions without linking directly to server code. CGI modules receive this via `httpd->httpx`.

**HTTPCGI** — CGI registration. Maps a URL pattern or file extension to a load module name.

**HTTPV** — Environment variable. Header followed by `name\0value\0` storage. Allocated per variable, freed on request reset.

### Response Pipeline

```
CGI Module / httpget.c
  → http_resp(httpc, 200)         Set status line
  → http_printf(httpc, header)    Send headers
  → http_printf(httpc, "\r\n")    End headers (triggers chunked fallback)
  → http_printf(httpc, body)      Send body (chunked if needed)
  → httpdone()                    Send chunk terminator "0\r\n\r\n"
```

The chunked fallback in `httpprtv.c` automatically detects when headers end (`\r\n`) without `Content-Length` or `Transfer-Encoding` being set, and injects `Transfer-Encoding: chunked` for HTTP/1.1 clients.

## ASCII/EBCDIC Translation

MVS stores data in EBCDIC. HTTP is an ASCII protocol. HTTPD translates at the network I/O boundary.

### Codepages

| Codepage | Use Case |
|----------|----------|
| CP037 | Default. HTTP-safe variant where EBCDIC NEL (0x15) maps to ASCII LF (0x0A). Used for HTTP headers and general text. |
| IBM-1047 | USS/UFS paths and file content. Standard z/OS codepage. |
| Legacy | Preserved 3.3.x behavior. Retained for backward compatibility. |

### Translation Functions

| Function | Direction | Description |
|----------|-----------|-------------|
| `http_etoa(buf, len)` | EBCDIC → ASCII | Translate buffer using the server's default codepage |
| `http_atoe(buf, len)` | ASCII → EBCDIC | Translate buffer using the server's default codepage |
| `http_xlate(buf, len, table)` | Explicit | Translate using a specific 256-byte translation table |

CGI modules access translation through the HTTPX function vector. The codepage pair pointers (`xlate_cp037`, `xlate_1047`, `xlate_legacy`) provide direct access to specific tables when needed.

### Key Design Decision

CP037 is the default because the C compiler generates EBCDIC NEL (0x15) for `'\n'`. The CP037 etoa table maps 0x15 to ASCII 0x0A (LF), which is what HTTP expects. This avoids the asymmetric roundtrip problem that IBM-1047 has with the LF/NEL mapping.

For details on the codepage tables, roundtrip verification, and integration with mvsMF, see the source in `src/httpxlat.c` and `include/httpxlat.h`.

## Writing CGI Modules

### Overview

CGI modules are external load modules that HTTPD loads at startup. They are registered via the Parmlib:

```
CGI=MYMODULE /api/*        URL prefix routing
CGI=MYMODULE *.ext         Extension-based routing
```

HTTPD loads the module via `__load()` and calls its entry point for matching requests.

### Module Structure

A CGI module links against `httpcgi.h` (not `httpd.h`). This lightweight header provides the HTTPX function vector macros without pulling in the full server internals.

```c
#include "httpcgi.h"

int cgimain(HTTPD *httpd, HTTPC *httpc)
{
    /* Send response */
    http_resp(httpc, 200);
    http_printf(httpc, "Content-Type: text/plain\r\n");
    http_printf(httpc, "\r\n");
    http_printf(httpc, "Hello from my CGI module!\n");
    return 0;
}
```

### Available Functions (via HTTPX)

CGI modules call server functions through the HTTPX function vector. Key functions:

**Response:**
- `http_resp(httpc, code)` — set HTTP status code
- `http_printf(httpc, fmt, ...)` — send formatted output (headers or body)

**Environment:**
- `http_get_env(httpc, name)` — get request environment variable (e.g. `REQUEST_METHOD`, `REQUEST_PATH`, `HTTP_Host`)
- `http_set_env(httpc, name, value)` — set environment variable

**Translation:**
- `http_etoa(buf, len)` — EBCDIC to ASCII
- `http_atoe(buf, len)` — ASCII to EBCDIC

**UFS:**
- `http_open(httpc, path, mime)` — open a UFS file for serving

### Building a CGI Module

Create a `project.toml` with dependencies on `crent370` and `httpd`:

```toml
[project]
name = "mymodule"
version = "1.0.0"
type = "application"

[build.sources]
c_dirs = ["src/"]

[[link.module]]
name = "MYMODULE"
entry = "@@CRT0"
options = ["LIST", "MAP", "XREF", "RENT"]
include = ["@@CRT1", "MYMODULE"]

[link.module.dep_includes]
"mvslovers/crent370" = "*"
```

### Examples

The following modules in the HTTPD source tree serve as examples:

- `src/httpdsrv.c` — simple server status display
- `src/httpdmtt.c` — master trace table dump
- [mvsMF](https://github.com/mvslovers/mvsmf) — full REST API implementation with router, middleware, and path parameter extraction

### Debug CGI Modules

These modules are built into the HTTPD binary and can be enabled via Parmlib for development:

| Module | Pattern | What it Shows |
|--------|---------|---------------|
| HTTPDSRV | `/.dsrv` | Server configuration, worker status, connected clients |
| HTTPDM | `/.dm` | Memory dump of server structures |
| HTTPDMTT | `/.dmtt` | Master trace table (last N WTO messages) |

Enable them during development:

```
CGI=HTTPDSRV /.dsrv
CGI=HTTPDMTT /.dmtt
```

Do not enable in production — they expose server internals.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Follow the coding style in `CLAUDE.md`
4. Test on MVS (Hercules) with `curl` and browser
5. Run h1spec tests for HTTP compliance changes
6. Submit a pull request

### Code Review

Open security findings are tracked in `docs/code-review-open-findings.md`. Contributions that address these findings are especially welcome.

### Testing

**HTTP compliance:** Use [h1spec](https://github.com/uNetworking/h1spec) (Deno-based test runner, serial execution, 5s timeout). Current score: 33/33.

**Manual testing:**

```bash
# Basic request
curl -v http://mvsdev.lan:1080/

# Keep-Alive verification
curl -v http://mvsdev.lan:1080/ http://mvsdev.lan:1080/

# CGI module
curl -v http://mvsdev.lan:1080/.dmtt

# HTTP/1.0 fallback
curl -v --http1.0 http://mvsdev.lan:1080/

# Authentication
curl -v -u ibmuser:sys1 http://mvsdev.lan:1080/zosmf/info
```
