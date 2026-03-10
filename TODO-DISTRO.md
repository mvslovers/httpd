# TODO: Full Distribution & CGI SDK

This document captures the analysis and planned work for two related features:
publishing CGI SDK headers and producing a full application distribution
(matching the 3.3.0 release format).

It also documents required changes to the mbt type taxonomy that are a
prerequisite for clean implementation.

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
├── css/                 ← Bootstrap CSS (bundled for docs)
├── js/                  ← jQuery, Bootstrap, DataTables JS
├── docs/                ← HTML documentation
│     config.html, cgi.html, mqtt.html, ufs.html, …
└── lua/                 ← example Lua CGI scripts
```

### Nested XMIT mechanics

`HTTPD330.XMIT` is a single TRANSMIT of a PDS. Each member of that PDS is
itself the result of a `TRANSMIT` of one MVS dataset. Installation:

1. Upload `HTTPD330.XMIT` to an MVS sequential dataset
2. `RECEIVE` the outer XMIT → creates `INSTALL.HTTPD330` (PDS)
3. `RECEIVE INDATASET('INSTALL.HTTPD330(LINKLIB)')` → `HTTPD.LINKLIB`
4. `RECEIVE INDATASET('INSTALL.HTTPD330(HTML)')` → `HTTPD.HTML`
5. … and so on for each member
6. Delete `INSTALL.HTTPD330`

---

## Prerequisite: mbt Type Taxonomy Cleanup

Before implementing distribution support, the mbt project type taxonomy needs
to be cleaned up and given clear, distinct semantics.

### Current state (verified in mbt source)

| Type | LINK_TYPES | Default artifact datasets | package.toml | Special logic |
|------|-----------|--------------------------|--------------|---------------|
| `library` | No | `ncalib`, `maclib` | Yes | — |
| `runtime` | No | `ncalib`, `maclib` | Yes | **none — identical to library** |
| `module` | Yes | `syslmod` | Yes | **none — identical to application** |
| `application` | Yes | `syslmod` | Yes | — |

`runtime` is a dead alias for `library`. `module` is a dead alias for
`application`. No code path distinguishes them.

### Target taxonomy

| Type | Purpose | Release artifact | package.toml |
|------|---------|-----------------|--------------|
| `library` | C library; provides NCALIB + headers for build-time linking | `headers.tar.gz` + `mvs.tar.gz` (NCALIB) | Always — lists both artifacts + provided datasets |
| `module` | Standalone program(s); end-user installable | `mvs.tar.gz` (LOAD/syslmod) | Never |
| `application` | Full application distribution; optionally exposes SDK headers | Distribution zip + optional `headers.tar.gz` | Only if `[artifacts] headers = true` — lists headers artifact only, no MVS artifact |

**`runtime` is removed** — use `library`.

### Rationale

- `module` is never a build-time dependency for anything. No `package.toml` needed.
  Examples: lua370-app, mqtt370-broker, mqtt370-cli, mvsmf, ufs370-app.
- `application` (httpd) can be a headers-only dependency for CGI developers.
  Its loadlib is an end-user installation artifact, not a build dependency — so
  the `package.toml` must never reference the MVS artifact.
- `[artifacts] mvs = true` becomes implicit (no flag needed): `library` always
  ships NCALIB, `module` always ships LOAD, `application` ships what the
  distribution step produces. The flag is removed.

### Required mbt changes

- Remove `runtime` from `VALID_TYPES`; reject it with a clear error pointing
  to `library`
- Give `module` its own behavior: skip `package.toml` generation entirely
- Give `application` conditional `package.toml`: only when
  `[artifacts] headers = true`; content = headers artifact only
- Remove the `[artifacts] mvs = true` flag; make it implicit per type
- Projects to update: lua370-app, mvsmf, mqtt370-broker, mqtt370-cli →
  change `type = "application"` to `type = "module"`

---

## Problem 1: CGI SDK Headers

### Current situation

- httpd headers live in `include/`: `httpd.h`, `httppub.h`, `httpluax.h`,
  `cib.h`, `dbg.h`, `errors.h`, `ftpd.h`, `httpds.h`, `safp.h`
- Previously published separately as the
  [httpd_cgi_sdk](https://github.com/mvslovers/httpd_cgi_sdk) repo
- mbt does not publish headers for `type = "application"`

### What we want

A `httpd-<version>-headers.tar.gz` release artifact so CGI developers can
declare:

```toml
[dependencies]
"mvslovers/httpd" = ">=3.3.1"
```

and receive the CGI SDK headers via `make bootstrap`. Transitive headers
(lua.h, ufs.h, mqtt headers) are already provided by those packages.

### Required mbt changes

- When `type = "application"` and `[artifacts] headers = true`: pack
  `include/` into `headers.tar.gz` and upload it to the GitHub release
- Generate `package.toml` listing only the headers artifact (no MVS entry)
- `make bootstrap` in dependent projects must handle `application`-type deps:
  download + unpack headers, skip MVS step (no NCALIB to RECEIVE)

### httpd project.toml change

```toml
[artifacts]
headers = true   # publish include/ as CGI SDK headers
                 # (mvs = true is implicit for application type)
```

---

## Problem 2: Full Application Distribution

### Current mbt behavior

For any non-library type mbt currently produces:
- `<project>-<version>-mvs.tar.gz` — XMIT of the LOAD (syslmod) dataset only

### What we want

A full distribution zip mirroring the 3.3.0 structure:

```
HTTPD<VER>.zip
├── HTTPD<VER>.XMIT        ← nested XMIT with LINKLIB + content datasets
├── INSTALL.JCL            ← generated installation JCL
├── httpd.jcl              ← from scripts/
├── httpd.proc             ← from scripts/
├── readme.txt / changelog
├── css/, js/              ← from static/css/, static/js/
├── docs/                  ← from doc/
└── lua/                   ← example Lua scripts
```

### Content datasets

| Dataset suffix | Source in repo | Target MVS dataset |
|---------------|----------------|--------------------|
| LINKLIB | build output (syslmod) | HTTPD.LINKLIB |
| HTML | `static/html/` | HTTPD.HTML |
| ICON | `static/icon/` | HTTPD.ICON |
| LUA | `static/lua/` | HTTPD.LUA |
| BREXX | `static/brexx/` | HTTPD.BREXX |
| UFSDISK0 | built UFS image | HTTPD.UFSDISK0 |

### Note: distribution bundling of external tools

The distribution may need to include binaries from other `module`-type
projects (e.g. ufs370-app tools alongside the httpd load modules). This is
distinct from build-time dependencies — it is a distribution-time composition.
A new `[[distribution.include]]` concept in project.toml would handle this,
resolved at `make dist` time, not `make bootstrap` time.

### Required mbt changes (high-level)

1. **Content dataset declarations** in `project.toml`
   - New `[[mvs.content.dataset]]` section: source dir, target dataset name,
     DCB attributes
   - `make upload-content` step: upload PDS members from repo directories

2. **Nested XMIT assembly** (extended `make package`)
   - TRANSMIT LINKLIB + each content dataset individually
   - RECEIVE each into a member of a staging PDS on MVS
   - TRANSMIT the staging PDS as the outer XMIT

3. **INSTALL.JCL generation**
   - Template-driven, based on content dataset list in project.toml
   - Target dataset names from `[mvs.install]` section

4. **Distribution zip assembly** (`make dist`)
   - Collects: outer XMIT, INSTALL.JCL, static files from repo, docs, JCL
   - Produces: `HTTPD<VER>.zip`

5. **UFS image** — separate concern, out of mbt scope for now

### Open mbt issues this depends on

- Issue #21 (unit/volume support) — for content dataset allocation
- Issue #18 (absolute dataset names in install) — for HTTPD.LINKLIB, HTTPD.HTML etc.

---

## Phasing

| Phase | Scope | Where | Effort |
|-------|-------|-------|--------|
| 0 | **Type taxonomy cleanup** — remove `runtime`, define `module` vs `application` semantics, remove `[artifacts] mvs` flag | mbt | Small |
| 1 | **CGI SDK headers** — `[artifacts] headers = true` for `application` type; generate headers-only `package.toml` | mbt + httpd | Small |
| 2 | **Static file bundling** — `make dist` packs LOAD XMIT + static files + docs into zip (no content datasets yet) | mbt + httpd | Small–Medium |
| 3 | **Content datasets** — `[[mvs.content.dataset]]`, upload + TRANSMIT HTML/LUA/ICON/BREXX | mbt + httpd | Medium |
| 4 | **Nested XMIT + INSTALL.JCL** — full nested XMIT with generated install JCL | mbt + httpd | Large |
| 5 | **Distribution bundling** — pull in external module binaries at dist time | mbt | Medium |
| 6 | **UFS image build** | separate | — |

Phase 0 is a prerequisite and a good cleanup independent of httpd.
Phase 1 directly unblocks CGI SDK consumers.
Phases 2–4 are sequential and deliver the full distribution incrementally.
