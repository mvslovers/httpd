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

Cross-compiled for MVS/370 with the `cc370` toolchain (GCC 3.4.6 fork; c2asm370 is its unmaintained predecessor and is not used here). All other platform constraints from the root CLAUDE.md still apply (24-bit addressing, EBCDIC, no POSIX, memory efficiency, etc.).

## HTTPD 4.0.0 — Confirmed Changes

These decisions are final:

- **HTTP/1.1 support** — Chunked Transfer Encoding, Content-Length, Persistent Connections (Keep-Alive)
- **Parmlib configuration** — Replace Lua config engine with line-based Parmlib parser (DD:HTTPPRM, FB-80)
- **UFSD-only docroot** — Remove DD-based file serving (`/DD:HTML(...)` paths), static files only from UFS
- **Remove embedded FTPD** — FTP functionality available separately
- **Remove MQTT telemetry** — HTTPT struct, telemetry_thread, mqtt370 dependency
- **Extract HTTPLUA/HTTPREXX** — Lua and REXX CGI handlers moved to separate projects (mvslovers/httplua, mvslovers/httprexx), lua370 dependency removed
- **CGIs disabled by default** — No CGIs are registered unless explicitly configured in Parmlib.

## HTTPD 4.0.0 — Open Items

Not tracked here. The four Notion tasks this section used to list (TSK-112,
TSK-110, TSK-108, TSK-10) are all **Done**, and the list sat stale for months
because nothing updates it when work lands. Read the live trackers instead:

```
gh issue list --repo mvslovers/httpd
```

plus the *Issues & Tasks* database in Notion. Do not restore a task list here —
a copy of a tracker is wrong the first time someone closes something.

## Dependencies (from project.toml)

```toml
[dependencies]
"mvslovers/ufsd" = ">=1.1.0"
```

That is the whole list. `libc370` is the cc370 sysroot (`-lc`), not a declared
dependency. crent370 was superseded by libc370; ufs370 by ufsd; the lua370 and
mqtt370 entries went with the Lua engine and the MQTT telemetry. Resolved
versions are pinned in `mbt.lock` (committed).

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

mbt **v2**: the whole build runs on the host with the cc370 toolchain, and only
`make deploy` touches MVS. `make help` lists every target; the root CLAUDE.md
documents the full set. The ones used most here:

```bash
make               # build the load modules
make modules       # production modules only
make test          # cross-compile the [[test]] modules
make test-host     # build + run the dual tests natively — the fast inner loop
make test-mvs      # deploy to TESTLIB + run the suite on MVS
make doctor        # verify environment (MVS connectivity, tools)
make compiledb     # generate compile_commands.json for clangd
make clean         # remove build/ and dist/ (keeps staged deps)
make distclean     # clean + remove all of .mbt/ (incl. deps)
```

`make bootstrap` / `build` / `link` / `install` were mbt v1 and no longer exist.

**`make test-host` cannot catch a `-Werror` regression.** `[host] cflags =
["-Wno-error"]` in project.toml, deliberately — the host compiler's `-Wall` is a
different and much larger set. The target build is the gate, so run `make` or
`make modules` before calling a change clean. CI only *compiles* `make test`, it
never runs an assertion.

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
  │         → CSTATE_IN (keep-alive) or CSTATE_CLOSE
  └── CTHDMGR worker pool (mintask..maxtask, default 3..9)
```

### Data Structures

**HTTPD (392 bytes, 0x188):** Server-wide singleton. Listener socket, worker pool manager, route table, config values, UFS handle, docroot, codepage, keep-alive settings, credential key/array, stats counters. Slots freed by the 4.0.0 removals are kept as `unused_nn` placeholders so no offset moves — reuse one rather than appending when a new field fits. It also holds the state that must not live in module storage (issue #197): the codepage pair in effect (`xlate`, 0x80), the STC ACEE restored at shutdown (`stc_prev_acee`, 0x88), and the settled Basic realm (`cfg_realm_val`, 0x140). The block is a `main()` local, so it is key-8 automatic storage whatever key the load module is in.

**HTTPC (4,096 bytes):** Per-client session. Allocated on accept(), freed on close. Fixed layout with 4,008-byte inline buffer (CBUFSIZE). Contains state machine position, socket, environment variables, file handles, credential.

**HTTPX (~270 bytes):** Function vector table. CGI modules call all server functions through this vector — they never link directly to HTTPD code. **Never change existing offsets** — only append new function pointers at the end.

**HTTPCGI (32 bytes):** One route. URL pattern → load module name (`MOD=`) or NULL for a program-less static prefix (`LOC=`), plus the per-route auth policy (`auth`, `resattr`, `resclass`, `resname`).

### Request Processing Pipeline

```
httpin.c     → Read request line + headers from socket
httppars.c   → Parse method, URI, query string, POST body
httppc.c     → CGI matching, auth check, method dispatch
httpget.c    → Static file serving with MIME detection
httpresp.c   → HTTP response line + headers (version matches the request)
httpsend.c   → Binary/text data delivery
httpdone.c   → Close files
httprepo.c   → Write SMF record, update counters
httprese.c   → Reset for next request (keep-alive) or close
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

### HTTP/1.1 Design (implemented)

**Response body framing — decision logic:**
0. If the status is body-less (`1xx`, `204`, `304`) → never set Content-Length or
   Transfer-Encoding and never emit a body (RFC 7230 §3.3.1). The chunked fallback in
   `httpprtv.c` skips these; otherwise the `0\r\n\r\n` trailer from `httpdone.c` is an
   illegal body that strict parsers (llhttp/Node) reject.
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

### Parmlib Configuration

Read from `DD:HTTPPRM` (FB-80 dataset). Lines starting with `#` or `*` are comments. Format: `KEYWORD VALUE [VALUE...]`.

```
# HTTPD 4.0.0 Configuration
PORT              8080
MINTASK           3
MAXTASK           9
DOCROOT           /www
CLIENT_TIMEOUT    10
KEEPALIVE_TIMEOUT 5
KEEPALIVE_MAX     100
DEBUG             0
CGI    MVSMF      /zosmf/*
CGI    HTTPDSRV   /.dsrv
```

Missing `DD:HTTPPRM` → server starts with defaults (port 8080, no CGIs).

### Key Source Files

**Core HTTP engine:**
- **httpd.c**: Main entry, initialization, socket_thread, worker_thread, operator commands.
- **httpin.c**: Read HTTP request line and headers from socket.
- **httppars.c**: Parse method, URI, query parameters, POST body.
- **httppc.c**: Request dispatch — CGI match → http_link, or static file → http_get.
- **httpget.c**: Static file serving. MIME detection via httpmime.c.
- **httpresp.c**: HTTP response line + standard headers.
- **httpsend.c**: Low-level socket send.
- **httprese.c**: Reset for the next request on a kept-alive connection, or close.
- **httpclos.c**: Release client handle, close socket, free UFS handles.
- **httpopen.c**: File open — UFS only; the DD path is gone (4.0.0).
- **httpmime.c**: MIME type detection from file extension.
- **httpauth.c / httpcred.c**: RACF authentication and credential management.
- **httplink.c**: CGI module loading via MVS LINK SVC.

**Configuration:**
- **httpprm.c**: Parmlib parser — reads `DD:HTTPPRM` (`HTTPD_PARMLIB_DD` in httpd.h). Replaced the Lua-based httpconf.c, which is gone. Startup does not echo the settled configuration; `F HTTPD,D CONFIG` reports it on demand.
- **httpdmsg.h**: the operator message catalog. Every WTO literal, once. See *Conventions*.

**CGI infrastructure:**
- **cgistart.c**: Custom `__start` for CGI modules.
- **httpx.c**: HTTPX vector table initialization.

**Built-in CGI modules** — the four `[[module]]` entries besides HTTPD itself:
- **httpdsrv.c**: Server + control-block display (`/.dsrv`)
- **httpdm.c / httpdmtt.c**: Storage display (`/.dm`) and Master Trace Table (`/.dmtt`)
- **abend0c1.c**: Deliberate-abend test CGI

None is registered unless the Parmlib says so.

**Subsystems:**
- **httprepo.c**: SMF recording + simple counters (format in `docs/smf-records.md`)

**Gone, do not look for them:** httpconf.c and the Lua runtime (→
mvslovers/httplua), httprexx.c (→ mvslovers/httprexx), the FTP daemon (`ftp*.c`
→ mvslovers/ftpd), the MQTT telemetry (HTTPT, telemetry_thread), and the
hello.c / test.c demos. httpjes2.c and httpdsl.c live in `tbd/`, outside the
build — mvsMF's jobs and dataset APIs replaced them.

### Conventions

- Headers use `asm("SYMBOL")` annotations for MVS external symbol naming.
- ASM entry points use `__asm__("\n&FUNC SETC '...'")` for 8-char MVS CSECT names.
- Manual memory management — watch for malloc/free pairing on ALL error paths.
- Tab indentation, 4-space width. Line endings are **whatever the file already
  has** — most of `src/` is CRLF, newer files are LF. Do not convert a file as a
  side effect of editing it: a whole-file line-ending flip buries the real diff
  (it turned a 950-line change into 4,500 once already). Beware tools that
  rewrite files wholesale — Python's `read_text`/`write_text` silently
  normalizes CRLF to LF.
- HTTPC is exactly 4,096 bytes. Do not grow it.
- HTTPX vector table: **append-only**. Never change existing offsets.
- **Every WTO literal lives in `include/httpdmsg.h`**, once, upper case, behind
  an `MSG_*` macro — never inline at the call site. The header states the rules;
  `docs/messages.md` is its operator-facing form, and the two must stay in step.
  Values keep their original case (`http_upcase()` folds the ones that arrive
  lower case at runtime). Message IDs are `HTTPDnnnX`, severity I/W/E.
- A WTO is for **what an operator can act on**. Anything the client caused
  belongs in the HTTP response and nowhere else — every console line also lands
  in the Master Trace Table, which is the server's own diagnostic channel.

### Known Platform Bugs

The MVS 3.8j TCP/IP stack has a ring buffer bug that corrupts data when a multi-byte `recv()` call spans the internal buffer wrap-around point. Single-byte `recv()` calls are not affected. The `http_getc()` / `http_gets()` functions in the HTTPD core work around this. **Do not replace per-byte reads with bulk recv() calls** without understanding this constraint.

### ESTAE / Abend Recovery

Each worker thread is protected by `abendrpt(ESTAE_CREATE, ...)`. An abend in a CGI module kills only the affected worker. The server continues on surviving workers and creates replacements.

The mvsMF CGI module has its own additional ESTAE layer in `router.c` (via `try()`/`tryrc()`) that catches handler abends, cleans up tracked resources (FILE handles, UFS sessions), and returns a 500 error response instead of terminating the worker.

### Off-Limits

- **Mutable module storage** — no writable file-scope data, no function-local
  `static` that is ever stored into, and never translate or otherwise write a
  string literal in place. Fetched from an APF-authorized or LNKLST library the
  load module lands in key-0 storage while the code runs problem state key 8,
  so the first such store is an S0C4 (issue #197). It is the **library the
  module is fetched from** that decides this, not `AC(1)` and not the RENT
  attribute — measured, see libc370's `doc/consumer-notes.md`. A `__super()`
  window is not a way out: a module can be key-0 without holding the
  authorization to switch keys. Put the state in the HTTPD control block (a
  `main()` local, hence key 8), on the heap, or in `__wsaget()`. Read-only
  statics are fine — the tables in `httpxlat.c` stay where they are.
  Watch `UCHAR *x = "..."` versus `UCHAR x[] = "..."`: only the second is a
  stack copy, and `http_etoa()` on the first writes the literal.
- **HTTPX vector offsets** — never change existing entries, only append
- **HTTPC size** — must remain exactly 4,096 bytes
- **http_getc / http_gets** — single-byte recv is intentional (TCP/IP bug workaround)
- **v3.3.x branch** — maintenance-only for Mike Rayborn's version, no 4.0.0 features

### Testing

The suite is a set of mbt `[[test]]` modules in `test/`, declared in
project.toml and written against `<mbtcheck.h>` (`CHECK` / `CHECK_EQ` /
`mbt_test_summary`, RC 0 = all passed). `make test-host` runs the DUAL ones
natively in seconds; `make test-mvs` runs every one on MVS as a batch **and** a
TSO step and prints a per-test matrix.

A test is DUAL only if the code under test is free of httpd.h. That is worth
arranging: pull the pure decision out into its own small source with its own
header — `httpbody.c`, `httpracf.c`, `httpesc.c`, `httpstat.c` all exist for
this reason — and list that source in the `[[test]]` alongside the test for the
host link. Everything reaching a control block, an SVC or a socket stays
`host = false` and runs only under `make test-mvs`.

Test names are MVS member names: **8 characters, uppercase**. A 9-character name
builds fine and then kills the entire runner job with a JCL error (#180).

End-to-end checks against a live HTTPD + mvsMF are `curl` scripts in the mvsMF
repo (`tests/test.sh` and friends) — integration, not a substitute for the above.

### Display Modules (live storage inspection)

The root `CLAUDE.md` covers when to reach for these; this is the detail. All are
read-only, all need Basic auth, none is registered unless `DD:HTTPPRM` says so.

**`/.dsrv` — HTTPDSRV, server control blocks.** `?target=` selects the block and
each one is shown as hex plus a named field table:

| target | block | needs `&m=` |
|--------|-------|-------------|
| `HTTPD` | the server singleton (392 bytes, `0x188`) | no |
| `MGR` | `CTHDMGR`, the worker pool manager | no |
| `FS` | `UFSSYS` handle (8 bytes with the libufs stub) | no |
| `MOD` | the route array — every `MOD=` / `LOC=` entry | **yes** |
| `TASK` | a `CTHDTASK` | **yes** |
| `FILE` | a `FILE` handle | **yes** |

The addresses the `&m=` targets want come out of the `HTTPD` block:
`httpcgi` at `+44`, `mgr` at `+34`, `socket_thread` at `+30`, `ufssys` at `+54`.

**`/.dm` — HTTPDM, arbitrary storage.** `?m=<hex>&l=<bytes>&c=<chunk>&t=<title>`
— only `m` is required; `c` is capped at 64; `t` labels the dump header. Every
parameter has short and long aliases (`m`/`mem`/`memory`, `l`/`len`/`length`,
`c`/`s`/`chunk`/`size`, `t`/`title`).

Pointer chasing is the point: read a pointer, feed it back as `m`. `CVTPTR` is
at `0x10`, so `/.dm?m=10&l=16` yields the CVT, and CVT+`0x130` is `CVTTZ`.

**`/.dmtt` — HTTPDMTT, Master Trace Table.** No parameters needed (`?d=1`
toggles a raw-data variant). This is the console log, so it is where RAKF
messages, `HTTPDnnn` WTOs and abends actually show up — check it before
theorising about why a request failed.

**Gone in 4.0.0:** `/dsl/*` (HTTPDSL, dataset lister) and `/jes/*` (HTTPJES2,
JES spool) are no longer built — mvsMF's dataset and jobs APIs replace them.
Their sources sit in `tbd/`, outside the build; see `tbd/README.md`. So the
three display modules above are the whole set now, and `/jes/status` is not a
JES2 cross-check any more — use mvsMF's jobs API.

**Reading the route table:** `?target=MOD` decodes the whole 32-byte `HTTPCGI`,
so `auth` (`+14`) is named and spelled out — that is the field the request is
gated on, and since #105 the only one: the global `LOGIN` bitmask is retired,
`+09` is a reserved byte, and there is no `AUTH=DEFAULT` any more. A route
showing `AUTH=NONE` is public, whether the line said `AUTH=NONE` or said
nothing. The one line that reads as public but is not is `RES=` without
`AUTH=`: the parser resolves that to `BASIC` before the route is built, so what
`?target=MOD` shows is still what gates the request. And
`AUTH=` never selected a credential *source* — the resolver runs before the
route is matched, so every route accepts every source. It selects whether a
login is needed and how a missing one is challenged: `NONE`, `FORM`, `BASIC`,
and since #121 `TOKEN` (a bare 401, never a challenge, for API routes whose
clients handle the 401 themselves).
