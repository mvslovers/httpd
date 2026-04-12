# HTTPD Server — TODO

## Distribution & CGI SDK

Authoritative plan for producing a full application distribution and
publishing CGI SDK headers as a build dependency for CGI module developers.

### Background: 3.3.0 Release Structure

The [3.3.0 release](https://github.com/mvslovers/httpd/releases/tag/3.3.0)
ships as `HTTPD330.zip` containing:

```
HTTPD330/
├── HTTPD330.XMIT        <- nested XMIT (PDS whose members are XMIT files)
│   members:
│     LINKLIB            <- load modules (HTTPD, HTTPLUA, HTTPLUAX, ...)
│     HTML               <- web HTML PDS
│     ICON               <- icon PDS
│     LUA                <- Lua scripts PDS
│     BREXX              <- BREXX programs PDS
│     UFSDISK0           <- UFS filesystem image
├── INSTALL.JCL          <- installation JCL (upload -> nested RECEIVE)
├── httpd.jcl            <- JCL to start the HTTPD server
├── httpd.proc           <- PROC for the HTTPD server
├── readme.txt
├── changelog
├── css/                 <- Bootstrap CSS (bundled for docs/UI)
├── js/                  <- jQuery, Bootstrap, DataTables JS
├── docs/                <- HTML documentation
└── lua/                 <- example Lua CGI scripts
```

Nested XMIT mechanics: `HTTPD330.XMIT` is a TRANSMIT of a PDS where each
member is itself the TRANSMIT of one MVS dataset. Upload outer XMIT, RECEIVE
it, then RECEIVE each member individually.

### Phase 0 — mbt Type Taxonomy Cleanup

**Status: Done.**

- `runtime` type removed from mbt (migration error points to `library`)
- `module` type: skips `package.toml` (never a build dependency)
- `application` type: `package.toml` lists headers artifact only, no MVS
  datasets, no dependencies (consumers get headers, not transitive deps)
- `[artifacts] header_files` filter added: selectively export headers
- httpd `project.toml`: `headers = true`, `header_files = ["httpcgi.h", "dbg.h", "errors.h"]`

### Phase 1 — httpd Header Refactor

**Status: Done** (PRs #18 + #19).

`httpcgi.h` extracted as minimal CGI interface. No dependency on ufs, mqtt,
lua, or crent370 internals. `httpd.h` unchanged for server-internal use.

CGI SDK header set: `httpcgi.h`, `dbg.h`, `errors.h`.

### Phase 1b — First Consumer: mvsmf

**Status: Done.** Branch `feature/httpcgi-migration`, build + link RC=0.

Migration: `httpd.h` -> `httpcgi.h`, explicit crent370 includes for
previously-transitive symbols (racf.h, clibary.h, MIN macro).

### Phase 2 — CGI SDK Headers Artifact

**Status: Done.**

- httpd `project.toml` configured with `[artifacts] headers = true` and
  `header_files = ["httpcgi.h", "dbg.h", "errors.h"]`
- `make package` produces `httpd-<version>-headers.tar.gz`
- `package.toml` lists headers artifact only (no MVS, no deps)
- CGI module developers declare `"mvslovers/httpd" = ">=3.3.1"` and
  `make bootstrap` unpacks headers to `contrib/httpd-<version>/include/`

### Phase 3 — Static File Bundling (`make dist`, basic zip)

**Status:** Not started.

A `make dist` target that produces a zip containing:
- The existing LOAD XMIT (from `make package` output)
- Static web files from the repo (`static/css/`, `static/js/`, `static/html/`)
- Documentation (`doc/`)
- Runtime JCL (`scripts/httpd.jcl`, `scripts/httpd.proc`)
- Generated `INSTALL.JCL` (simple single-XMIT version for now)
- `README.md` and changelog

mbt changes needed:
- New `make dist` target in `mbt/mk/targets.mk`
- New `[distribution]` section in `project.toml`
- New `mvsdist.py` script: assembles zip from XMIT + static dirs + JCL
- `mvsrelease.py`: upload the distribution zip to GitHub releases

### Phase 4 — Content Datasets + Nested XMIT

**Status:** Not started. Requires Phase 3.

Content datasets to produce:

| Suffix   | Source in repo   | Target MVS dataset |
|----------|------------------|--------------------|
| LINKLIB  | build output     | HTTPD.LINKLIB      |
| HTML     | `static/html/`   | HTTPD.HTML         |
| ICON     | `static/icon/`   | HTTPD.ICON         |
| LUA      | `static/lua/`    | HTTPD.LUA          |
| BREXX    | `static/brexx/`  | HTTPD.BREXX        |
| UFSDISK0 | built UFS image  | HTTPD.UFSDISK0     |

mbt changes: `[[mvs.content.dataset]]` in project.toml, content upload
script, nested XMIT generation, full INSTALL.JCL.

Depends on mbt issues #21 (unit/volume) and #18 (absolute dataset names).

### Phase 5 — Distribution Bundling of External Modules

**Status:** Not started. Requires Phase 4.

Bundle load modules from other `module`-type projects (e.g. ufs370-app)
into the httpd distribution zip at `make dist` time.

### Phase 6 — UFS Image Build

**Status:** Out of scope. Separate investigation needed.

---

## Security

- [ ] Replace all `sprintf()` with `snprintf()` across the codebase
      (httpshen.c, httpsqen.c, httppars.c, ftpfqn.c, httppars.c:91)
- [ ] Replace `vsprintf()` with `vsnprintf()` in httpprtv.c
- [ ] Add bounds check on `strcat()` in httppars.c query/POST parsing
- [ ] Validate SSI include paths against document root (httpfile.c)
- [ ] Validate FTP paths against FTP root directory (ftpfqn.c)
- [ ] Fix integer overflow in httpnenv.c allocation
- [ ] Add missing `return` in httpshen.c parse_cookies()
- [ ] Add locking around client array_add() in httpd.c
- [ ] Validate Content-Length for POST requests (httppars.c)
- [ ] Limit query/POST parameter count (httppars.c)

## Missing Dependencies

- [ ] **zlib370**: HTTPDSL needs ZLIB370.NCALIB for gzip compression.
      Rayborn's LKEDDSL.JCL includes `MDR.ZLIB370.NCALIB` in SYSLIB
      and `INCLUDE SYSLIB(ZUTIL)`. SSI works without it, but compressed
      output is not available. Needs its own project or vendored build.

## Build System

- [ ] Create `.env.example` with all required variables documented
- [ ] Investigate HTTPLUA link warning (CC 0004) — may be harmless
      unresolved weak external, but should be confirmed

## Lua CGI

- [ ] Lua scripts not found when served from UFS — verify UFS disk
      configuration and script paths in Lua config

## Documentation

- [ ] Document Lua configuration file format (tables, keys, defaults)
- [ ] Document CGI module development (httpcgi.h API, cgistart linkage)
- [ ] Document FTP daemon configuration and capabilities
- [ ] Document console commands (`/F HTTPD,...`) with examples

## UFS / UFSD Integration

- [ ] **Replace `httpd->ufssys` with `int httpd->ufs_enabled`**
      The UFSSYS handle from `ufs_sys_new()` is a stub — a `calloc` +
      eyecatcher used purely as a boolean "UFS is available" flag.
      Replace with a plain `int ufs_enabled` in the HTTPD struct and
      change `ftpd->sys` to `int ftpd->ufs_enabled` accordingly.
      All checks like `if (httpd->ufssys)` become `if (httpd->ufs_enabled)`.
      Priority: none — purely cosmetic cleanup.

- [ ] **Memory leak in `ufs_sys_term()`**
      `ufs_sys_new()` allocates a UFSSYS via `calloc()`, but
      `ufs_sys_term()` is a no-op and never frees it. Either
      `ufs_sys_term()` should accept a `UFSSYS**` and free it, or
      the caller (`terminate()` in httpd.c) should `free()` the handle
      after `ufs_sys_term()`. Not critical at STC shutdown (address space
      goes away), but should be fixed for correctness.

- [ ] **Per-connection UFSD sessions instead of global session**
      Currently HTTPD opens one server-level UFS session (`httpd->ufs`)
      at startup, and each HTTP client gets its own session via `ufsnew()`
      in `socket_thread()`. Evaluate whether per-connection sessions are
      worth the overhead vs. sharing the server session with per-request
      `ufs_setuser()` calls for identity switching. Consider implications
      for concurrent access, permission checks, and session state (cwd).

## Future Work

- [ ] Hot-reload for CGI modules (currently requires STC restart)
- [ ] HTTP/1.1 support (chunked transfer, persistent connections)
- [ ] HTTPS/TLS support (requires TLS library port)
