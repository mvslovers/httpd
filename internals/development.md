# HTTPD — Development Guide

This document covers building HTTPD from source, the server architecture, and the ASCII/EBCDIC translation system. Writing your own server modules is covered in [docs/server-modules.md](../docs/server-modules.md).

## Building from Source

### Prerequisites

- `mbt` build system ([mvslovers/mbt](https://github.com/mvslovers/mbt))
- `c2asm370` cross-compiler
- [crent370](https://github.com/mvslovers/crent370) — C runtime library (build dependency)
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
  docs/              Documentation for users (configuration, installation, ...)
  internals/         Documentation for maintainers (design notes, backlog)
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
| `CSTATE_GET` | httpget.c | Serve static file from UFS or dispatch to server module |
| `CSTATE_POST` | httppars.c | Parse POST body |
| `CSTATE_DONE` | httpdone.c | Send final chunk terminator, finalize response |
| `CSTATE_REPORT` | httpsmf.c | Write SMF record, increment counters |
| `CSTATE_RESET` | httprese.c | Clean up for next request (Keep-Alive) or close |

### Key Data Structures

**HTTPD** — Server-wide state. Single instance. Holds the listener socket, client array, worker pool, module registrations, configuration, and SMF settings.

**HTTPC** — Per-client session. Allocated on `accept()`, freed on connection close. Contains the socket, receive buffer (4 KB), environment variables, request state, response code, UFS handle, and credential pointer.

**HTTPX** — Function vector table (~270 bytes). Contains ~68 function pointers that server modules use to call server functions without linking directly to server code. server modules receive this via `httpd->httpx`.

**HTTPROUTE** — One route. Maps a URL pattern or file extension to a load module name (`MOD=`), or carries no program at all (`LOC=`), and holds the route's auth policy. Renamed from `HTTPCGI` in #105.

**HTTPV** — Environment variable. Header followed by `name\0value\0` storage. Allocated per variable, freed on request reset.

### Dynamic arrays: no holes below the count

Most of the server's collections are libc370 dynamic arrays — a `void **` with
an `ARRAY` header (count, size, eyecatcher) immediately in front of it, reached
through `array_add()` / `array_del()` / `array_count()` / `array_get()`.

**None of those operations can leave an empty slot below the count.** Measured
in libc370's sources, not assumed:

| source | behaviour |
|--------|-----------|
| `@@aradd.c:65` | `(*carray)[array->count++] = vitem;` — appends at the end, growing by `ARRAY_DEFAULT` when full |
| `@@ardel.c:22-26` | shifts every later element left, decrements `count`, and NULLs only the slot *past* the new count |
| `@@arcou.c:18` | `array_count()` returns `array->count` |

So for `n < array_count(&a)`, `a[n]` is non-NULL — **provided** nothing puts a
NULL there in the first place. There are exactly two ways that could happen,
and both are the caller's doing, not the array's:

1. **A producer adds NULL.** `array_add()` stores whatever it is handed; it
   does not reject a NULL item. The server's own producers all check first —
   `httpacgi.c` (`httpd->route`), `httpsenv.c` (`httpc->env`), `httpsbz.c`
   (`httpd->busy`) and the accept path in `httpd.c` (`httpd->httpc`) each add
   an item they have just verified is non-NULL.
2. **Code stores NULL through the raw pointer.** There is no `array_set()`, but
   the backing store is a plain `void **` and nothing stops an assignment.
   As of #229 no server code does this.

**Do not add a NULL check when indexing `a[n]` below `array_count()`.** It
cannot fire, and a check that cannot fire teaches the next reader a contract
that does not exist. `httpfenv()` — the hottest lookup in the server —
dereferences `httpc->env[n]` directly, and that is the house style.

That rule is about direct indexing only. **`array_get()` callers still check**:
it returns NULL for an index outside `1..count`, so the NULL means "bad index",
not "hole" — which is why `d_login()` in `httpcons.c` tests its result.

Two situations genuinely do need a check on a direct index, and both are
visible at the call site:

- **Arrays the server does not produce.** `grt->grtsock` and `grt->grtenv`
  belong to libc370, `mgr->worker` and the task array to CTHDMGR, and the
  `MTENTRY` array in `httpdmtt.c` is built by the Master Trace Table reader.
  Their producers have not been audited here, so their guards stay.
- **An array pointer that came from outside.** `display_route()` in
  `httpdsrv.c` walks whatever address the request passed in `?m=`. The
  `ARRAY` eyecatcher check inside `array_count()` makes a wrong address return
  0 rather than a wild count, but nothing proves the array it did find is the
  route table, so that walk keeps its NULL check.

Concurrency is a separate question and a NULL check answers none of it. A
concurrent `array_del()` *shifts*, so an unsynchronised walker can see an entry
move past an index it has already passed — it skips an element, it never finds
a hole.

**`httpd->httpc` is the array where that looked like it mattered**, and #235
settled it: the array has exactly one writer, and it is the socket thread, so
`build_fd_set()` may read it without the lock.

Entries reach it only through the accept path's `httpd->mgr == NULL` branch,
and that branch is the no-worker fallback alone — there is no startup window
into it. `initialize()` holds `lock(httpd,0)` from before `cthread_create_ex()`
until after `cthread_manager_init()` (`httpd.c:303`…`:473`), and every pass of
the socket loop begins in `process_clients()` → `http_process_clients()`, which
ENQs that same resource. `lock()` is ENQ `RET=HAVE` (libc370 `@@enqdeq.c`), so
a holder on another TCB makes it **wait** rather than fail — the socket thread
cannot reach `accept()` until `mgr` is set or has failed. With a worker pool the
array stays empty for the life of the server; `?target=HTTPD` prints its count.

In the fallback there are no worker threads at all, so the accept, both walks
and the `array_del()` in `httpclos()` all run on the socket thread. The one
exception is a shutdown that reports `HTTPD041I` and detaches an unresponsive
socket thread: `terminate()` then walks and frees the array from the main thread
while that thread may still be running. The three NULL checks those walks used
to carry are gone with #235.

### Response Pipeline

```
Server Module / httpget.c
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

server modules access translation through the HTTPX function vector. The codepage pair pointers (`xlate_cp037`, `xlate_1047`, `xlate_legacy`) provide direct access to specific tables when needed.

### Key Design Decision

CP037 is the default because the C compiler generates EBCDIC NEL (0x15) for `'\n'`. The CP037 etoa table maps 0x15 to ASCII 0x0A (LF), which is what HTTP expects. This avoids the asymmetric roundtrip problem that IBM-1047 has with the LF/NEL mapping.

For details on the codepage tables, roundtrip verification, and integration with mvsMF, see the source in `src/httpxlat.c` and `include/httpxlat.h`.

## Contributing

1. Fork the repository
2. Create a feature branch
3. Follow the coding style in `CLAUDE.md`
4. Test on MVS (Hercules) with `curl` and browser
5. Run h1spec tests for HTTP compliance changes
6. Submit a pull request

### Code Review

Open security findings are tracked in `internals/refactoring-backlog.md`. Contributions that address these findings are especially welcome.

### Testing

**HTTP compliance:** Use [h1spec](https://github.com/uNetworking/h1spec) (Deno-based test runner, serial execution, 5s timeout). Current score: 33/33.

**Manual testing:**

```bash
# Basic request
curl -v http://mvsdev.lan:1080/

# Keep-Alive verification
curl -v http://mvsdev.lan:1080/ http://mvsdev.lan:1080/

# server module
curl -v http://mvsdev.lan:1080/.dmtt

# HTTP/1.0 fallback
curl -v --http1.0 http://mvsdev.lan:1080/

# Authentication
curl -v -u ibmuser:sys1 http://mvsdev.lan:1080/zosmf/info
```
