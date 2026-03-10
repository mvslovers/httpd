# TODO: Distribution, CGI SDK & mbt Type Taxonomy

This document is the authoritative plan for:

1. Cleaning up the mbt project type taxonomy
2. Refactoring httpd headers for a clean CGI SDK
3. Publishing CGI SDK headers as a release artifact
4. Producing a full application distribution zip (matching the 3.3.0 format)

It is written to be self-contained — a new session should be able to pick up
any phase without prior context.

---

## Ecosystem Context

- All projects target **MVS 3.8j / Hercules TK4-**, compiled with GCC 3.2.3
  (c2asm370), strict C89, EBCDIC, 24-bit addressing.
- **mbt** is the build system (Python + Make), shared as a git submodule.
  It lives at `mbt/` in each project. Source: `mbt/scripts/mbt/`.
- Project metadata is in `project.toml`. Local settings in `.env` (gitignored).
- Releases are cut with `make release VERSION=x.y.z`. The release script
  (`mbt/scripts/mvsrelease.py`) packages artifacts and publishes them to
  GitHub Releases.
- Dependency resolution: `make bootstrap` reads `project.toml [dependencies]`,
  fetches `package.toml` from each dep's GitHub release, downloads headers
  and MVS XMIT archives, and installs them.

---

## Background: 3.3.0 Release Structure

The [3.3.0 release](https://github.com/mvslovers/httpd/releases/tag/3.3.0)
ships as `HTTPD330.zip` containing:

```
HTTPD330/
├── HTTPD330.XMIT        ← nested XMIT (PDS whose members are XMIT files)
│   members:
│     LINKLIB            ← load modules (HTTPD, HTTPLUA, HTTPLUAX, …)
│     HTML               ← web HTML PDS
│     ICON               ← icon PDS
│     LUA                ← Lua scripts PDS
│     BREXX              ← BREXX programs PDS
│     UFSDISK0           ← UFS filesystem image
├── INSTALL.JCL          ← installation JCL (upload → nested RECEIVE)
├── httpd.jcl            ← JCL to start the HTTPD server
├── httpd.proc           ← PROC for the HTTPD server
├── readme.txt
├── changelog
├── css/                 ← Bootstrap CSS (bundled for docs/UI)
├── js/                  ← jQuery, Bootstrap, DataTables JS
├── docs/                ← HTML documentation
└── lua/                 ← example Lua CGI scripts
```

### Nested XMIT mechanics

`HTTPD330.XMIT` is a TRANSMIT of a PDS where each member is itself the
TRANSMIT of one MVS dataset. Installation procedure:

1. Upload `HTTPD330.XMIT` to an MVS sequential dataset
2. `RECEIVE` outer XMIT → creates `INSTALL.HTTPD330` (PDS)
3. `RECEIVE INDATASET('INSTALL.HTTPD330(LINKLIB)')` → `HTTPD.LINKLIB`
4. `RECEIVE INDATASET('INSTALL.HTTPD330(HTML)')` → `HTTPD.HTML`
5. … repeat for ICON, LUA, BREXX, UFSDISK0
6. `DELETE 'INSTALL.HTTPD330'`

---

## Phase 0 — mbt Type Taxonomy Cleanup

**Status:** Not started. Prerequisite for all other phases.
**Repo:** `mbt` (submodule at `mbt/`)

### Problem

The mbt type taxonomy has four values but only two distinct behaviors:

| Type | LINK_TYPES | Default artifact datasets | package.toml | Extra logic |
|------|-----------|--------------------------|--------------|-------------|
| `library` | No | `ncalib`, `maclib` | Yes | — |
| `runtime` | No | `ncalib`, `maclib` | Yes | **none — dead alias for library** |
| `module` | Yes | `syslmod` | Yes | **none — dead alias for application** |
| `application` | Yes | `syslmod` | Yes | — |

Verified in: `mbt/scripts/mbt/project.py` (`VALID_TYPES`, `LINK_TYPES`),
`mbt/scripts/mvspackage.py` (`_DEFAULT_MVS_DATASETS`).

### Target taxonomy

| Type | Purpose | Release artifacts | package.toml |
|------|---------|------------------|--------------|
| `library` | C library; provides NCALIB + headers for build-time linking | `headers.tar.gz` + `mvs.tar.gz` (NCALIB) | Always |
| `module` | Standalone program(s); end-user installable, never a build dep | `mvs.tar.gz` (LOAD/syslmod) | **Never** |
| `application` | Full application distribution; optionally exposes SDK headers | Distribution zip + optional `headers.tar.gz` | Only when `[artifacts] headers = true` — lists headers artifact only, no MVS entry |

`runtime` is removed. Use `library`.

### Rationale

- `module` is never a build-time dependency. No `package.toml` needed.
  `make bootstrap` would never have a reason to reference a module.
- `application` (httpd) can be a headers-only build dependency for CGI
  developers. Its loadlib is an end-user install artifact, not a build dep —
  so `package.toml` must never list the MVS artifact.
- `[artifacts] mvs = true` flag is removed — implicit per type:
  `library` always ships NCALIB, `module` always ships LOAD,
  `application` produces a distribution zip.

### Projects to update after this change

| Project | Current type | New type |
|---------|-------------|---------|
| lua370-app | `application` | `module` |
| mvsmf | `application` (verify) | `module` |
| mqtt370-broker | `application` | `module` |
| mqtt370-cli | `application` | `module` |
| ufs370-app (future) | `application` | `module` |
| httpd | `application` | stays `application` |

### mbt changes required

- `mbt/scripts/mbt/project.py`: remove `runtime` from `VALID_TYPES`; add
  migration error pointing to `library`
- `mbt/scripts/mvspackage.py`: skip `package.toml` generation for `module`
  type; generate headers-only `package.toml` for `application` when
  `[artifacts] headers = true`; remove `[artifacts] mvs = true` flag logic
- `mbt/scripts/mvsrelease.py`: same artifact flag changes
- Tests: update `test_datasets.py` and any type-related tests

---

## Phase 1 — httpd Header Refactor (prerequisite for Phase 2)

**Status:** Not started. Required before publishing CGI SDK headers.
**Repo:** `httpd`
**Related:** Root CLAUDE.md roadmap item #15

### Problem

`include/httpd.h` is a kitchen-sink header that transitively pulls in:

- crent370 runtime internals (`clibppa.h`, `clibcrt.h`, `clibthrd.h`, …)
- `ufs.h` — UFS filesystem (dependency of httpd, not of CGI modules)
- `mqtc370.h` — MQTT client
- `httpluax.h` → `lua.h` + `lualib.h` + `lauxlib.h`
  → **forces CGI consumers to compile with `-DLUA_USE_C89`**

`httppub.h` has `#error #include "httpd.h" first` — so it forces httpd.h
transitively. A CGI module developer only needs the CGI callback interface
(`HTTPCGI`, `HTTPC`, `HTTPX` function vector, env var helpers) — not
MQTT, UFS, Lua, or crent370 internals.

### Target structure

```
include/
├── httpcgi.h     ← NEW: pure CGI interface
│                    contains: HTTPCGI, HTTPC, HTTPX (function vector),
│                    HTTPV (variables), CGI callback typedefs, env helpers
│                    depends only on: stddef.h, stdio.h (standard C89)
├── httpd.h       ← unchanged role but includes httpcgi.h instead of
│                    duplicating CGI types; still pulls in ufs/mqtt/lua
│                    (for HTTPD server internals only, not exported in SDK)
├── httpluax.h    ← Lua scripting interface (unchanged, not in CGI SDK)
├── dbg.h         ← exported in CGI SDK (debugging helpers, no extra deps)
├── errors.h      ← exported in CGI SDK (missing errno values)
└── (others)      ← internal, not exported
```

### CGI SDK header set (after refactor)

```
httpcgi.h    ← primary CGI interface
dbg.h        ← optional but useful
errors.h     ← optional but useful
```

Transitive deps (lua.h, ufs.h, mqtt headers, cred headers) come from their
own packages when a CGI module explicitly depends on those projects.

### Work

1. Extract `struct httpcgi`, `struct httpc`, `struct httpx`, `HTTPV`,
   CGI callback typedefs, and env-var helper prototypes from `httpd.h`
   into a new `include/httpcgi.h` with minimal includes (stddef.h, stdio.h)
2. Replace duplicated definitions in `httpd.h` with `#include "httpcgi.h"`
3. Fix all `httppub.h` consumers — change `#error` guard to accept
   `httpcgi.h` as sufficient alternative to `httpd.h`
4. Verify all `src/` files still compile with `-DLUA_USE_C89`
5. Verify a minimal CGI module compiles with only `httpcgi.h` and no
   `-DLUA_USE_C89` flag

---

## Phase 2 — CGI SDK Headers Artifact

**Status:** Not started. Requires Phase 0 (mbt) + Phase 1 (header refactor).
**Repos:** `mbt`, `httpd`

### What we want

A `httpd-<version>-headers.tar.gz` release artifact so CGI developers can
declare a dependency and receive just the CGI headers:

```toml
# In a CGI module's project.toml:
[dependencies]
"mvslovers/httpd" = ">=3.3.1"
```

`make bootstrap` downloads `headers.tar.gz`, unpacks to
`contrib/httpd-<version>/include/` — exactly as for library deps.
No MVS RECEIVE step (no NCALIB provided by an application).

### httpd project.toml change

```toml
[artifacts]
headers = true   # publish httpcgi.h + dbg.h + errors.h as CGI SDK
                 # mvs is implicit for application type (no flag needed)
```

### mbt changes (in addition to Phase 0)

- `mvsrelease.py`: when `type = "application"` and `artifact_headers = true`,
  pack configured `include_dirs` into `headers.tar.gz` and upload
- `package.toml` generation for application: list only `headers` artifact,
  omit `mvs` entry
- `mvsbootstrap.py` (or equivalent): when processing an `application` dep,
  download + unpack headers only; skip MVS RECEIVE step
- Optional: `[sdk] include_dirs = ["include/"]` in project.toml to select
  which dirs are packed (default: `include/`)

---

## Phase 3 — Static File Bundling (`make dist`, basic zip)

**Status:** Not started. Can start after Phase 0. Does not require Phases 1–2.
**Repos:** `mbt`, `httpd`

### What we want

A `make dist` target that produces a zip containing:
- The existing LOAD XMIT (from `make package` output)
- Static web files from the repo (`static/css/`, `static/js/`, `static/html/`)
- Documentation (`doc/`)
- Runtime JCL (`scripts/httpd.jcl`, `scripts/httpd.proc`)
- Generated `INSTALL.JCL` (simple single-XMIT version for now)
- `README.md` and changelog

This does **not** yet produce the nested XMIT with content datasets —
just the load library XMIT bundled with everything else.

### mbt changes

- New `make dist` target in `mbt/mk/targets.mk`
- New `[distribution]` section in `project.toml`:
  ```toml
  [distribution]
  zip_name  = "HTTPD{VER}.zip"   # {VER} = version without dots, e.g. 331
  static_dirs = ["static/", "doc/"]
  jcl_scripts = ["scripts/httpd.jcl", "scripts/httpd.proc"]
  ```
- New `mvsdist.py` script: assembles zip from XMIT + static dirs + JCL
- `mvsrelease.py`: upload the distribution zip to GitHub releases

---

## Phase 4 — Content Datasets + Nested XMIT

**Status:** Not started. Requires Phase 3.
**Repos:** `mbt`, `httpd`

### Content datasets

| Suffix | Source in repo | Target MVS dataset |
|--------|----------------|--------------------|
| LINKLIB | build output (syslmod) | HTTPD.LINKLIB |
| HTML | `static/html/` | HTTPD.HTML |
| ICON | `static/icon/` | HTTPD.ICON |
| LUA | `static/lua/` | HTTPD.LUA |
| BREXX | `static/brexx/` | HTTPD.BREXX |
| UFSDISK0 | built UFS image | HTTPD.UFSDISK0 |

### project.toml additions

```toml
[[mvs.content.dataset]]
key     = "html"
suffix  = "HTML"
dsorg   = "PO"
recfm   = "FB"
lrecl   = 80
blksize = 19040
space   = ["TRK", 50, 10, 10]
source  = "static/html/"

[[mvs.content.dataset]]
key     = "lua"
suffix  = "LUA"
dsorg   = "PO"
recfm   = "FB"
lrecl   = 80
blksize = 19040
space   = ["TRK", 10, 5, 5]
source  = "static/lua/"
# … etc.
```

### mbt changes

- `project.py`: parse `[[mvs.content.dataset]]` sections
- `datasets.py`: resolve content dataset names (use `[mvs.install]` naming)
- New `mvsuploadcontent.py`: upload PDS members from repo dirs via mvsMF API
- `mvsdist.py` (extended): TRANSMIT each dataset → RECEIVE into staging PDS
  → TRANSMIT staging PDS as nested XMIT; generate full `INSTALL.JCL`

### Open mbt issues this depends on

- Issue #21 (unit/volume support) — for content dataset allocation on correct volume
- Issue #18 (absolute dataset names in install) — HTTPD.LINKLIB, HTTPD.HTML
  etc. are fixed names, not HLQ-qualified

---

## Phase 5 — Distribution Bundling of External Modules

**Status:** Not started. Requires Phase 4.
**Repos:** `mbt`

Some distributions need to bundle load modules from other `module`-type
projects (e.g. ufs370-app tools in the httpd distribution).

This is a distribution-time composition, not a build-time dependency.
Proposed project.toml syntax:

```toml
[[distribution.include]]
project  = "mvslovers/ufs370-app"
version  = ">=1.0.0"
artifact = "mvs"     # download mvs.tar.gz, unpack LOAD members into dist XMIT
```

Resolved at `make dist` time: download the referenced release artifact,
extract load module members, include them in the nested XMIT.

---

## Phase 6 — UFS Image Build

**Status:** Out of scope for mbt. Separate investigation needed.

The `UFSDISK0` member is a UFS filesystem image. Building it likely requires
a dedicated step outside the standard mbt pipeline. Deferred.

---

## Summary Table

| Phase | Scope | Repos | Depends on | Effort |
|-------|-------|-------|-----------|--------|
| 0 | mbt type taxonomy: remove `runtime`, define `module` vs `application`, drop `[artifacts] mvs` flag | mbt + all projects | — | Small |
| 1 | httpd header refactor: extract `httpcgi.h` | httpd | — | Small–Medium |
| 2 | CGI SDK headers artifact: `[artifacts] headers = true` for application | mbt + httpd | 0 + 1 | Small |
| 3 | Static file bundling: `make dist` produces basic zip | mbt + httpd | 0 | Small–Medium |
| 4 | Content datasets + nested XMIT + INSTALL.JCL | mbt + httpd | 3 | Large |
| 5 | Distribution bundling of external modules | mbt | 4 | Medium |
| 6 | UFS image build | separate | — | Unknown |
