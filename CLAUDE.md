# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.
It extends the ecosystem root `CLAUDE.md`, which covers the platform constraints,
mbt basics, SMP/FMID rules, the deploy libraries and the git rules.

## Project Overview

HTTPD is an HTTP web server for MVS 3.8j, originally by Mike Rayborn (v3.3.x). Version 4.0.0 is a major evolution by Mike Großmann — targeting HTTP/1.1 support, Parmlib-based configuration, and a leaner default footprint.

The server's primary role is hosting CGI modules (especially mvsMF) that provide REST APIs for MVS. It also serves static files from the UFS filesystem via UFSD.

**Current version:** 4.5.0-dev (on `main`; `[project] version` in `mbt.toml`)
**Maintenance branch:** `v3.3.x` (from commit dd07c1b, for fixes to Mike Rayborn's version)

## Read when relevant

| Before you … | read |
|---|---|
| change response framing, keep-alive, or look for where something lives | `internals/architecture.md` (4.0.0 decisions, HTTP/1.1 design, source file map) |
| inspect a live server with `/.dsrv`, `/.dm`, `/.dmtt` | `internals/display-modules.md`, and `~/repos/mvs/.workspace/context/httpd-debug.md` |
| write or change a server module | `docs/server-modules.md`, `internals/development.md` |
| cut a release or touch `[smp]` | `~/repos/mvs/.workspace/context/smp.md` (the httpd FMID row and burned ids) |

## Open Items

Not tracked here. The four Notion tasks this section used to list (TSK-112,
TSK-110, TSK-108, TSK-10) are all **Done**, and the list sat stale for months
because nothing updates it when work lands. Read the live trackers instead:

```
gh issue list --repo mvslovers/httpd
```

plus the *Issues & Tasks* database in Notion. Do not restore a task list here —
a copy of a tracker is wrong the first time someone closes something.

`TODO.md` in the repo root is the one permitted companion, and it is not that
copy: it holds the **ranking** of the open issues, the reasoning behind the
order, and which ones wait on a decision rather than on code — none of which
GitHub stores. It names issues but never owns their state. Reconcile it after
every merge, and if it ever disagrees with `gh issue list`, the tracker wins.

## Dependencies (from mbt.toml)

```toml
[dependencies]
"mvslovers/ufsd" = ">=1.5.1"
"mvslovers/crypto370" = ">=1.0.1"

[tools]
ufsd-utils = { repo = "mvslovers/ufsd-utils", version = "1.0.1" }

[plugins]
"mvslovers/mbt-ufs" = "^0.1"
"mvslovers/mbt-stc" = "^0.1"
```

That is the whole list: the two libraries the load modules link, the host tool
that writes the webroot image, the plugin that drives it, and the plugin that
restarts the server and compresses its development library. `libc370` is the
cc370 sysroot (`-lc`), not a declared dependency. crent370 was superseded by libc370; ufs370 by ufsd; the lua370 and
mqtt370 entries went with the Lua engine and the MQTT telemetry. Resolved
versions are pinned in `mbt.lock` (committed).

## Development Workflow

When a fix or feature is done, merge via Pull Request and close the issue with a short comment.

`/fix-issue <number>` is the workspace command (`~/repos/mvs/.workspace/commands/fix-issue.md`).
Here its verify step is `mbt compiledb` plus clangd diagnostics, and `mbt build --all`
(see below: `mbt test` cannot catch a `-Werror` regression).

## Build System (mbt)

HTTPD is built with [mbt](https://github.com/mvslovers/mbt) 3, a program
installed on the machine (`[toolchain] mbt = "3.0"` in `mbt.toml` pins the
line; a newer installed mbt switches to it). The project file is `mbt.toml`;
there is no submodule, no Makefile, no `project.toml` and no `VERSION` any more
-- the version lives in `[project] version` alone. `mbt/` is the project's own
Lua: `mbt/init.lua` builds the webroot image through the mbt-ufs plugin.

### Build Commands

The whole build runs on the host with the cc370 toolchain, and only
`mbt deploy` and `mbt test --mvs` touch MVS. `mbt` alone lists every command;
the ones used most here:

```bash
mbt deps              # stage ufsd, crypto370, ufsd-utils and mbt-ufs (pinned in mbt.lock)
mbt build             # build the load modules
mbt build --all       # load modules + the CGI library (build/httpd.a)
mbt build --tests     # cross-compile the test modules
mbt test              # build + run the dual tests natively — the fast inner loop
mbt test --mvs        # deploy to TESTLIB + run the suite on MVS
mbt package           # dist/: SMP package, load XMIT, lib archive, webroot image
mbt run webroot       # build only the webroot image
mbt run restart -- HTTPD   # compress HTTPD.DEV.LINKLIB + restart HTTPD (mbt-stc)
mbt doctor --offline  # verify the toolchain without contacting MVS
mbt compiledb         # generate compile_commands.json for clangd
mbt clean             # remove build/ and dist/ (keeps staged deps)
mbt distclean         # clean + remove all of .mbt/ (incl. deps)
```

`mbt deps` never moves a pin on its own: a prerelease that was published again
(ufsd's `-dev` levels are) is refused until `mbt deps --update` re-pins it.

**`mbt test` cannot catch a `-Werror` regression.** `[build.host] cflags =
["-Wno-error"]` in mbt.toml, deliberately — the host compiler's `-Wall` is a
different and much larger set. The target build is the gate, so run
`mbt build --all` before calling a change clean. CI builds the modules and the
test modules and runs the host tests; it never runs an MVS assertion.

### Configuration

MVS targets come from `~/.mbt/targets.toml` (see root). `mbt target import .env --name <name>`
turns an old `.env` into a target; `mbt target list` shows them.

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

**HTTPROUTE (32 bytes):** One route. URL pattern → load module name (`MOD=`) or NULL for a program-less static prefix (`LOC=`), plus the per-route auth policy (`auth`, `resattr`, `resclass`, `resname`). Called `HTTPCGI` until #105; a `LOC=` route has no program, so the old name described only half of what the struct models. Defined once, in `httpd.h` — `httpcgi.h` publishes it opaque.

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

CGI modules are loaded via MVS LINK SVC (`__linkds`). The HTTPD/HTTPC pointers are passed through the GRT (Global Register Table) and discovered by the CGI's custom `__start` (cgistart.c).

CGI modules call server functions through the HTTPX vector table:
```c
HTTPD *httpd = grt->grtapp1;
HTTPC *httpc = grt->grtapp2;
HTTPX *httpx = httpd->httpx;

httpx->http_resp(httpc, 200);
httpx->http_printf(httpc, "Content-Type: text/html\r\n\r\n");
```

In HTTPD 4.0.0, CGI registration moves from Lua defaults to Parmlib-only. No CGIs are active unless explicitly configured.

**A module runs in HTTPD's task, so `samplib/httpd` is part of the modules'
contract, not just the server's.** The LINK SVC dispatches into this task; a
module has no allocations of its own and opens any ddname against the STC's.
`HASPCKPT` / `HASPACE1` are there for exactly that reason — HTTPD never touches
them, mvsMF's jobs API opens both through libc370's `jesopen()`. They were
dropped in 4.0.0 on the reasoning that "nothing else in the server opens them",
which was measured against `httpd/src/` alone and missed the three repos whose
modules run in this address space; issue #256. **Before removing anything from the PROC, grep `../mvsmf`,
`../httplua` and `../httprexx` too** — a `grep` that stops at this repo's edge
cannot answer the question.

### Parmlib Configuration

Read from `DD:HTTPPRM` (FB-80 dataset). Lines starting with `#` or `*` are
comments. The format is **`KEYWORD=VALUE`** — `parse_line()` (httpprm.c) looks
for an `=` and rejects any line without one as `HTTPD026W UNRECOGNIZED
CONFIGURATION LINE`. Keywords are folded to upper case; values are not.

```
# HTTPD 4.0.0 Configuration
PORT=8080
MINTASK=3
MAXTASK=9
DOCROOT=/www
CLIENT_TIMEOUT=10
KEEPALIVE_TIMEOUT=5
KEEPALIVE_MAX=100
REALM=MVSDEV
MOD=MVSMF /zosmf/*                       AUTH=TOKEN
MOD=HTTPDSRV /.dsrv                      AUTH=FORM
MOD=HTTPDM /.dm                          AUTH=BASIC
LOC=/pub/*                               AUTH=NONE
```

A route is `MOD=` (a program) or `LOC=` (a program-less static prefix), each
with its own `AUTH=`; there is no `CGI` keyword. **Write the `AUTH=`** — since
#105 a route without one is public, with no global policy to fall back on.

Missing `DD:HTTPPRM` → server starts with defaults (port 8080, no routes).

### Conventions

- Headers use `asm("SYMBOL")` annotations for MVS external symbol naming.
- ASM entry points use `__asm__("\n&FUNC SETC '...'")` for 8-char MVS CSECT names.
- Manual memory management — watch for malloc/free pairing on ALL error paths.
- C is gnu99 (see root); variable declarations should still prefer top-of-block for readability.
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
  attribute — measured, see libc370's `docs/consumer-notes.md`. A `__super()`
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

The suite is a set of mbt test modules in `test/` -- every `test/**/*.c` is a
test named after its file, with a `[test.NAME]` entry in mbt.toml only where it
needs more sources or `host = false` -- written against `<mbtcheck.h>` (`CHECK`
/ `CHECK_EQ` / `mbt_test_summary`, RC 0 = all passed). `mbt test` runs the DUAL
ones natively in seconds; `mbt test --mvs` runs every one on MVS as a batch **and** a
TSO step and prints a per-test matrix.

A test is DUAL only if the code under test is free of httpd.h. That is worth
arranging: pull the pure decision out into its own small source with its own
header — `httpbody.c`, `httpracf.c`, `httpesc.c`, `httpstat.c` all exist for
this reason — and list that source in the `[test.NAME]` entry alongside the test
for the host link. Everything reaching a control block, an SVC or a socket stays
`host = false` and runs only under `mbt test --mvs`.

Test names are MVS member names: **8 characters, uppercase**. A 9-character name
builds fine and then kills the entire runner job with a JCL error (#180).

End-to-end checks against a live HTTPD + mvsMF are `curl` scripts in the mvsMF
repo (`tests/test.sh` and friends) — integration, not a substitute for the above.

## SMP FMID

`[smp] prefix = "THTP"` in mbt.toml: mbt 3 derives the FMID and the predecessor
it deletes from the version (4.5.0-dev → `THTP450`, deleting `THTP440`). Rules,
the burned list and the per-release install record are in the root's `smp.md`.
Each release adds its id to the removal job in `docs/installation.md` section 12,
which names every id ever spent. A test package needs a throwaway id **and**
throwaway module names.
