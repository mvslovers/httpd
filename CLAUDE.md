# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HTTPD is an HTTP web server for MVS 3.8j, originally by Mike Rayborn (v3.3.x). Version 4.0.0 is a major evolution by Mike Großmann — targeting HTTP/1.1 support, Parmlib-based configuration, and a leaner default footprint.

The server's primary role is hosting CGI modules (especially mvsMF) that provide REST APIs for MVS. It also serves static files from the UFS filesystem via UFSD.

**Current version:** 4.0.0-dev (on `main`)
**Maintenance branch:** `v3.3.x` (from commit dd07c1b, for fixes to Mike Rayborn's version)

## C Standard Override

**This project uses `-std=gnu99`**, overriding the root CLAUDE.md's strict C89 rule.

Implications:
- `//` line comments are allowed
- Mixed declarations and statements are allowed
- `snprintf`, `stdbool.h`, designated initializers are available
- VLAs are still forbidden (stack constraints)
- All variable declarations should still prefer top-of-block for readability

Cross-compiled for MVS/370 using the `c2asm370` compiler (GCC 3.2.3 fork). All other platform constraints from the root CLAUDE.md still apply (24-bit addressing, EBCDIC, no POSIX, memory efficiency, etc.).

## HTTPD 4.0.0 — Confirmed Changes

These decisions are final:

- **HTTP/1.1 support** — Chunked Transfer Encoding, Content-Length, Persistent Connections (Keep-Alive)
- **Parmlib configuration** — Replace Lua config engine with line-based Parmlib parser (DD:HTTPDPRM, FB-80)
- **UFSD-only docroot** — Remove DD-based file serving (`/DD:HTML(...)` paths), static files only from UFS
- **Remove embedded FTPD** — FTP functionality available separately
- **Remove MQTT telemetry** — HTTPT struct, telemetry_thread, mqtt370 dependency
- **Extract HTTPLUA/HTTPREXX** — Lua and REXX CGI handlers moved to separate projects (mvslovers/httplua, mvslovers/httprexx), lua370 dependency removed
- **CGIs disabled by default** — No CGIs are registered unless explicitly configured in Parmlib.

## HTTPD 4.0.0 — Open Items

1. **TSK-112 CGI Chunked Bug** (M, High) — CGI responses with chunked transfer encoding
2. **TSK-110 Fehlende Status-Codes** (XS, High) — Missing HTTP status codes
3. **TSK-108 HTTP Parsing härten** (L, High) — Harden HTTP request parsing
4. **TSK-10 Doku + Version Bump** (M, Medium) — Documentation and version bump to 4.0.0

## Dependencies (from project.toml)

```toml
[dependencies]
"mvslovers/crent370" = ">=1.0.6"
"mvslovers/ufs370" = ">=1.0.0"
```

**Note:** mqtt370 dependency will be removed (MQTT telemetry confirmed for removal).

## Development Workflow

1. Every bug fix or feature requires a **GitHub Issue**. If none exists, create one first.
2. **Plan and analyze first** using the Opus 4.6 model. Implementation follows using Sonnet 4.6.
3. Develop each fix/feature on a **feature branch**.
4. When done, merge via **Pull Request** and close the issue with a short comment.
5. **Never reference AI or Claude** in commit messages, comments, PR descriptions, or anywhere else in the project.

## Custom Commands

### /fix-issue \<number\>

Autonomous workflow for resolving a GitHub issue end-to-end:

1. **Read the issue** — `gh issue view <number> --repo mvslovers/httpd`
2. **Create a feature branch** — `git checkout -b issue-<number>-<short-description>`
3. **Analyze** — Identify affected files, understand the existing patterns in nearby code
4. **Implement** — Write code following the conventions in this CLAUDE.md
5. **Verify syntax** — Run `make compiledb` and check clangd diagnostics (no errors)
6. **Commit** — Descriptive message, no AI references. Reference the issue: `Fixes #<number>`
7. **Push and create PR** — `gh pr create --title "..." --body "Fixes #<number>"`
8. **Summary** — Report what was done, what to verify on the live MVS system

If any step fails, stop and report the issue rather than guessing.

## Build System (mbt)

HTTPD uses [mbt](https://github.com/mvslovers/mbt) as its build tool (Git submodule in `mbt/`). Clone with `--recursive` or run `git submodule update --init`.

### Build Commands

```bash
make doctor        # verify environment (MVS connectivity, tools)
make bootstrap     # resolve dependencies, allocate MVS datasets
make build         # cross-compile C → ASM, assemble on MVS, NCAL link
make link          # final linkedit into load module
make install       # copy load module to install dataset
make compiledb     # generate compile_commands.json for clangd
make clean         # remove .s, .o files, build stamps
make distclean     # deep clean (also removes contrib/ and .mbt/)
```

### Configuration

Local settings go in `.env` (gitignored). See `.env.example` for the template. Key variables: `MBT_MVS_HOST`, `MBT_MVS_PORT`, `MBT_MVS_USER`, `MBT_MVS_PASS`, `MBT_MVS_HLQ`.

## Architecture

### Thread Model

```
httpd.c (main / initialize)
  ├── socket_thread (32 KB stack)     — select() loop, accept(), dispatch to worker pool
  ├── worker_thread × N (64 KB stack) — full request lifecycle per client
  │     CSTATE_IN → CSTATE_PARSE → CSTATE_GET/POST/PUT/DELETE
  │       → CSTATE_DONE → CSTATE_REPORT → CSTATE_RESET
  │         → CSTATE_IN (keep-alive, planned) or CSTATE_CLOSE
  └── CTHDMGR worker pool (mintask..maxtask, default 3..9)
```

### Data Structures

**HTTPD (288 bytes, 0x120):** Server-wide singleton. Listener socket, worker pool manager, CGI table, config values, UFS handle, Lua state, FTPD handle, MQTT telemetry handle, stats arrays. (Will shrink as confirmed removals are implemented.)

**HTTPC (4,096 bytes):** Per-client session. Allocated on accept(), freed on close. Fixed layout with 4,008-byte inline buffer (CBUFSIZE). Contains state machine position, socket, environment variables, file handles, credential.

**HTTPX (~270 bytes):** Function vector table. CGI modules call all server functions through this vector — they never link directly to HTTPD code. **Never change existing offsets** — only append new function pointers at the end.

**HTTPCGI (20 bytes):** CGI path-to-program mapping. URL pattern → load module name.

### Request Processing Pipeline

```
httpin.c     → Read request line + headers from socket
httppars.c   → Parse method, URI, query string, POST body
httppc.c     → CGI matching, auth check, method dispatch
httpget.c    → Static file serving with MIME detection
httpresp.c   → HTTP response line + headers (currently HTTP/1.0)
httpsend.c   → Binary/text data delivery
httpdone.c   → Close files
httprepo.c   → Write SMF record, update counters
httprese.c   → Reset for next request or close (currently always closes)
httpclos.c   → Release HTTPC, close socket
```

### CGI Subsystem

CGI modules are loaded via MVS LINK SVC (`__linkds`). The HTTPD/HTTPC pointers are passed through the GRT (Global Register Table) and discovered by the CGI's custom `__start` (cgistart.c / cgxstart.c).

CGI modules call server functions through the HTTPX vector table:
```c
HTTPD *httpd = grt->grtapp1;
HTTPC *httpc = grt->grtapp2;
HTTPX *httpx = httpd->httpx;

httpx->http_resp(httpc, 200);
httpx->http_printf(httpc, "Content-Type: text/html\r\n\r\n");
```

In HTTPD 4.0.0, CGI registration moves from Lua defaults to Parmlib-only. No CGIs are active unless explicitly configured.

### HTTP/1.1 Design (to be implemented)

**Response body framing — decision logic:**
1. If handler/CGI set Content-Length (e.g. mvsMF's `sendDefaultHeaders()`) → use as-is
2. If static file with known size (UFS `filesize`) → send Content-Length
3. Otherwise → send `Transfer-Encoding: chunked`

**Chunked encoding — transparent in httpsend.c:**
When `httpc->chunked == 1`, every `http_send()` call is automatically wrapped in chunk framing. The terminating `0\r\n\r\n` is sent in `httpdone.c`.

**Keep-Alive:**
- HTTP/1.1: default keep-alive, `Connection: close` to opt out
- HTTP/1.0: default close, `Connection: keep-alive` to opt in
- Configurable idle timeout and max requests per connection

**Impact on CGI modules (mvsMF):** Zero changes needed. mvsMF already sets Content-Length via `sendDefaultHeaders()` for JSON responses. Streaming responses get chunked encoding transparently.

### Parmlib Configuration (to replace Lua)

Read from `DD:HTTPDPRM` (FB-80 dataset). Lines starting with `#` or `*` are comments. Format: `KEYWORD VALUE [VALUE...]`.

```
# HTTPD 4.0.0 Configuration
PORT              8080
MINTASK           3
MAXTASK           9
DOCROOT           /www
CLIENT_TIMEOUT    10
KEEPALIVE_TIMEOUT 5
KEEPALIVE_MAX     100
LOGIN             NONE
TZOFFSET          +01:00
DEBUG             0
CGI    MVSMF      /zosmf/*
CGI    HTTPDSRV   /.dsrv
```

Missing `DD:HTTPDPRM` → server starts with defaults (port 8080, no CGIs).

### Key Source Files

**Core HTTP engine:**
- **httpd.c**: Main entry, initialization, socket_thread, worker_thread, operator commands.
- **httpin.c**: Read HTTP request line and headers from socket.
- **httppars.c**: Parse method, URI, query parameters, POST body.
- **httppc.c**: Request dispatch — CGI match → http_link, or static file → http_get.
- **httpget.c**: Static file serving. MIME detection via httpmime.c.
- **httpresp.c**: HTTP response line + standard headers.
- **httpsend.c**: Low-level socket send.
- **httprese.c**: Reset between requests (currently always closes; keep-alive planned).
- **httpclos.c**: Release client handle, close socket, free UFS handles.
- **httpopen.c**: File open — UFS and DD paths (DD path to be removed).
- **httpmime.c**: MIME type detection from file extension.
- **httpauth.c / httpcred.c**: RACF authentication and credential management.
- **httplink.c**: CGI module loading via MVS LINK SVC.

**Configuration:**
- **httpconf.c**: Lua-based config loader (to be replaced with Parmlib parser).

**CGI infrastructure:**
- **cgistart.c / cgxstart.c**: Custom `__start` for CGI modules.
- **httpx.c**: HTTPX vector table initialization.

**Built-in CGI modules (all optional via config, status to be finalized):**
- **httpdsrv.c**: Server status display (2,675 LOC)
- **httpjes2.c**: JES2 spool browser (975 LOC)
- **httpdsl.c + helpers**: Dataset list/download (1,540 LOC total)
- **httplua.c**: Extracted → mvslovers/httplua
- **httprexx.c**: Extracted → mvslovers/httprexx
- **httpdm.c / httpdmtt.c**: Device manager display (440 LOC)
- **hello.c, abend0c1.c, test.c**: Demo/test CGIs

**Subsystems:**
- **httprepo.c**: SMF Type 243 recording + simple counters (~90 LOC)
- **Lua runtime**: Extracted → mvslovers/httplua
- **FTP daemon**: ftp*.c (3,290 LOC) — confirmed for removal
- **MQTT telemetry**: HTTPT struct, telemetry_thread — confirmed for removal

### Conventions

- Headers use `asm("SYMBOL")` annotations for MVS external symbol naming.
- ASM entry points use `__asm__("\n&FUNC SETC '...'")` for 8-char MVS CSECT names.
- Manual memory management — watch for malloc/free pairing on ALL error paths.
- Tab indentation, 4-space width, LF line endings.
- HTTPC is exactly 4,096 bytes. Do not grow it.
- HTTPX vector table: **append-only**. Never change existing offsets.
- WTO message IDs follow the pattern `HTTPDnnnX` where nnn is numeric and X is severity (I/W/E).

### Known Platform Bugs

The MVS 3.8j TCP/IP stack has a ring buffer bug that corrupts data when a multi-byte `recv()` call spans the internal buffer wrap-around point. Single-byte `recv()` calls are not affected. The `http_getc()` / `http_gets()` functions in the HTTPD core work around this. **Do not replace per-byte reads with bulk recv() calls** without understanding this constraint.

### ESTAE / Abend Recovery

Each worker thread is protected by `abendrpt(ESTAE_CREATE, ...)`. An abend in a CGI module kills only the affected worker. The server continues on surviving workers and creates replacements.

The mvsMF CGI module has its own additional ESTAE layer in `router.c` (via `try()`/`tryrc()`) that catches handler abends, cleans up tracked resources (FILE handles, UFS sessions), and returns a 500 error response instead of terminating the worker.

### Off-Limits

- **HTTPX vector offsets** — never change existing entries, only append
- **HTTPC size** — must remain exactly 4,096 bytes
- **http_getc / http_gets** — single-byte recv is intentional (TCP/IP bug workaround)
- **v3.3.x branch** — maintenance-only for Mike Rayborn's version, no 4.0.0 features

### Testing

Tests are shell scripts using `curl` against the live HTTPD + mvsMF:

```bash
# From the mvsMF repo:
tests/curl-datasets.sh   # Dataset API
tests/curl-jobs.sh        # Jobs API
tests/curl-uss.sh         # USS/UFS API
tests/test.sh             # Top-level runner
```
