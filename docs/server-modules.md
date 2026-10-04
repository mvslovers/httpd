# HTTPD — Writing Server Modules

## Overview

HTTPD uses a **server module** system for extensibility — not traditional CGI.

The difference matters: [Traditional CGI](https://publib.boulder.ibm.com/httpserv/manual24/howto/cgi.html) (as defined by [RFC 3875](https://datatracker.ietf.org/doc/html/rfc3875)) forks a new process for each request and communicates via stdin/stdout and environment variables. HTTPD modules are fundamentally different:

| | Traditional CGI | HTTPD Server Modules |
|---|----------------|---------------------|
| Execution | Forked process per request | LINK SVC per request, inside the server address space |
| Communication | stdin/stdout, env vars | HTTPX function vector (direct function calls) |
| Request data | `$REQUEST_METHOD`, `$QUERY_STRING` env vars | `http_get_env(httpc, "REQUEST_METHOD")` via HTTPX |
| Response | Write to stdout | `http_resp()`, `http_printf()` via HTTPX |
| Performance | Process fork overhead per request | No fork; one LINK SVC per dispatch |
| Analogy | Perl/Python CGI scripts | Apache `mod_php`, `mod_rewrite` |

This means you cannot write a standard CGI script (e.g. a REXX program that reads environment variables and writes to stdout) and expect it to work with HTTPD. Server modules must be compiled as MVS load modules and use the HTTPX API.

**How a module is actually reached.** `httppcgi()` calls `http_link()` for every
dispatch (`src/httppcgi.c`), and `http_link()` is the MVS LINK SVC — `__linkds()`
in `src/httplink.c`, with a parameter list carrying the `HTTPD` and `HTTPC`
pointers. So the `MOD=` name is resolved and the module entered *on each
request*, on the worker thread's TCB. There is no startup load and no cached
entry point, and the module does not stay resident by virtue of having run once.

The direct calls are the other direction only: the module reaching back into the
server through HTTPX, an indirect branch through a vector table. The LINK is the
part with a cost worth talking about — `internals/refactoring-backlog.md` P7 puts it
at ~50 ms per dispatch and proposes an in-process router for hot endpoints.
Treat that figure as an estimate: it has not been measured, and no measured
number for either side of this exists in the repo.

> Whether the copy LINK brings in survives in the Job Pack Area to the next
> request, or is fetched from the library again each time, is **not settled** —
> see [#250](https://github.com/mvslovers/httpd/issues/250). Do not write either
> answer down anywhere until it has been measured.

> **Note on naming:** The route type was renamed `HTTPCGI` → `HTTPROUTE` in #105, and `http_find_cgi` / `http_add_cgi` / `http_process_cgi` became `http_find_route` / `http_add_route` / `http_process_route`. What did **not** change, and must not: the httpx vector offsets `0x100`/`0x104`/`0x108`, the external symbols `HTTPFCGI`/`HTTPACGI`/`HTTPPCGI`, and the field layout. `httpcgi.h` keeps the old macro spellings as aliases for out-of-tree modules. The header filename, the `cgimain` entry point and `http_cgi_subpool()` still say CGI — those really are about CGI programs. The Parmlib keyword changed from `CGI=` to `MOD=` earlier.

Modules are registered via the Parmlib:

```
MOD=MYMODULE /api/*        URL prefix routing
MOD=LUA                    Extension routing (derives *.lua from name)
```

When no pattern is specified, HTTPD derives the file extension from the module name (lowercase). The module then handles all requests for files with that extension in the DOCROOT.

For a matching request, HTTPD enters the module through the LINK SVC, as described under [Overview](#overview) — not through `__load()`, and not once at startup.

## Module Structure

A server module links against `httpcgi.h` (not `httpd.h`). This lightweight header provides the HTTPX function vector macros without pulling in the full server internals.

```c
#include "httpcgi.h"

int cgimain(HTTPD *httpd, HTTPC *httpc)
{
    /* Send response */
    http_resp(httpc, 200);
    http_printf(httpc, "Content-Type: text/plain\r\n");
    http_printf(httpc, "\r\n");
    http_printf(httpc, "Hello from my server module!\n");
    return 0;
}
```

## Available Functions (via HTTPX)

Server modules call server functions through the HTTPX function vector. Key functions:

**Response:**
- `http_resp(httpc, code)` — set HTTP status code
- `http_printf(httpc, fmt, ...)` — send formatted output (headers or body)

`http_resp()` can only send a code it has a reason phrase for (`src/httpstat.c`):

```
200 201 202 204 206
301 302 303 304
400 401 403 404 405 409 410 412 413 414 429
500 501 502 503 505 507
```

Anything else goes out as **500 Internal Server Error** — in front of whatever
body the module then writes — and logs `HTTPD909E` to the console. A module
needing a code that is not on the list should have it added to `httpstat()`
rather than work around it; the table is covered by `TSTSTAT`.

**Environment:**
- `http_get_env(httpc, name)` — get request environment variable (e.g. `REQUEST_METHOD`, `REQUEST_PATH`, `HTTP_Host`)
- `http_set_env(httpc, name, value)` — set environment variable

**Translation:**
- `http_etoa(buf, len)` — EBCDIC to ASCII
- `http_atoe(buf, len)` — ASCII to EBCDIC

**UFS:**
- `http_open(httpc, path, mime)` — open a UFS file for serving

**Cross-request context:**
- `http_cgictx_get(httpd, eye, size)` — find or create one context block per 8-byte eyecatcher. A module that needs a single, address-space-lifetime block (shared across requests and workers) registers it here. The block begins with the 8-byte eyecatcher (`char eye[8]`, stamped on create); `size` is used only on create and ignored on a hit, so always pass the same size for the same eyecatcher. The block is zeroed on create, lives until the address space ends, and is never freed individually. Returns the block, or `NULL` if the context array is full or storage is exhausted. Example: `http_cgictx_get(httpd, "MVSMFCTX", sizeof(MVSMF_CTX))`.

## Direct Accessors (not HTTPX entries)

`HTTPD` is opaque to a module — you hold the pointer and never dereference it.
Two macros in `httpcgi.h` are the documented exception. Both read a fixed offset
into the block and are ABI commitments, asserted at compile time in httpd's
`src/httpx.c`, so you may rely on them:

- `http_get_httpx(httpd)` — the HTTPX function vector. Needed once per function
  before any of the macros above will compile: `HTTPX *httpx = http_get_httpx(httpd);`
- `http_get_flag(httpd)` — the server's processing flags.

## Long-Running Handlers Must Drain on Shutdown

A handler that blocks — a long poll, a retry loop, anything waiting on an
external event — has to notice that the server is going down and return
promptly. It is not interrupted for you, and the window is bounded.

On `P HTTPD` the thread manager first posts a quiesce and waits for the workers
to drain on their own. That graceful window is sized from `maxtask`
(`20 + 10 × maxtask` units of 0.10 s in libc370's `cthread_manager_term()`, so
roughly 11 seconds at the default `maxtask 9`). If a worker is still parked when
it expires, the manager escalates to an immediate shutdown and force-DETACHes
the task. A worker that never returns can wedge the teardown entirely — libc370
then deliberately leaks the manager storage rather than risk a use-after-free.

Poll the flag and abandon the work:

```c
if (http_get_flag(httpd) & (HTTPD_FLAG_QUIESCE | HTTPD_FLAG_SHUTDOWN)) {
    http_resp(httpc, 503);
    return 0;
}
```

`HTTPD_FLAG_QUIESCE` means the server has stopped accepting new requests;
`HTTPD_FLAG_SHUTDOWN` means it is going down now. A draining handler should
treat both the same way. Check on every iteration of the wait loop, not once on
entry — the byte is `volatile` precisely because the operator-command thread
sets it while your loop is running.

Return a response rather than simply returning: the client has an open
connection, and 503 tells it the work was abandoned rather than completed. Make
clear it must not be blindly retried if the request had a side effect (issuing a
console command, for instance).

This is not hypothetical — a poll loop that ignored the flag is what produced
the `SA03` shutdown crash in issue #122.

The same rule applies inside the server: the send-stall retry in `httpprtv.c` /
`httpsend.c` checks both flags before each pause (issue #205), which is what
lets `P HTTPD` complete in ~3 s even while workers are parked on clients that
have stopped reading. Note that a stalled client is also the trigger for a
Hercules platform bug that can kill the whole emulator — see
[hercules-x75-send-stall-repro.md](../internals/hercules-x75-send-stall-repro.md) before
running that kind of load test.

## Building a Server Module

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

## Examples

The following modules in the HTTPD source tree serve as examples:

- `src/httpdsrv.c` — simple server status display
- `src/httpdmtt.c` — master trace table dump
- [mvsMF](https://github.com/mvslovers/mvsmf) — full REST API implementation with router, middleware, and path parameter extraction

## Debug Server Modules

These modules are built into the HTTPD binary and can be enabled via Parmlib for development:

| Module | Pattern | What it Shows |
|--------|---------|---------------|
| HTTPDSRV | `/.dsrv` | Server configuration, worker status, connected clients |
| HTTPDM | `/.dm` | Memory dump of server structures |
| HTTPDMTT | `/.dmtt` | Master trace table (last N WTO messages) |

Enable them during development:

```
MOD=HTTPDSRV /.dsrv
MOD=HTTPDMTT /.dmtt
```

Do not enable in production — they expose server internals.

