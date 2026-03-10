# TODO: Full Distribution & CGI SDK

This document captures the analysis and planned work for two related features:
publishing CGI SDK headers and producing a full application distribution
(matching the 3.3.0 release format).

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

## Problem 1: CGI SDK Headers

### Current situation

- httpd headers live in `include/`: `httpd.h`, `httppub.h`, `httpluax.h`,
  `cib.h`, `dbg.h`, `errors.h`, `ftpd.h`, `httpds.h`, `httppub.h`, `safp.h`
- Previously published separately as the
  [httpd_cgi_sdk](https://github.com/mvslovers/httpd_cgi_sdk) repo
  (also included re-exports of lua.h, ufs.h, mqtt headers, cred.h)
- mbt currently publishes `headers.tar.gz` only for `type = "library"`;
  httpd is `type = "application"` so headers are not exported

### What we want

A `httpd-<version>-headers.tar.gz` release artifact containing the headers
that CGI developers need, so they can declare:

```toml
[dependencies]
"mvslovers/httpd" = ">=3.3.1"
```

and get the CGI SDK headers via `make bootstrap` (same as any library dep).

Transitive headers (lua.h, ufs.h, mqtt headers) are already shipped by those
packages. The SDK only needs httpd's own public headers.

### Required mbt changes

- Allow `[artifacts] headers = true` in `project.toml` for `application` type
  (currently blocked/ignored)
- Optionally: `[sdk] include_dirs = ["include/"]` to control which directories
  are packed — fall back to `include/` by default
- Release workflow (`mvsrelease.py` / `release.yml`) must pack and upload
  `headers.tar.gz` for applications when the flag is set
- `make bootstrap` in dependent projects must be able to consume headers
  from an application dependency (currently only library/module types are
  expected to provide headers)

### httpd project.toml additions needed

```toml
[artifacts]
mvs     = true
headers = true   # publish include/ as CGI SDK headers
```

---

## Problem 2: Full Application Distribution

### Current mbt behavior (application type)

mbt currently produces:
- `<project>-<version>-mvs.tar.gz` — XMIT of the LOAD (syslmod) dataset only

### What we want

A full distribution zip mirroring the 3.3.0 structure:

```
HTTPD<ver>.zip
├── HTTPD<ver>.XMIT        ← nested XMIT with LINKLIB + content datasets
├── INSTALL.JCL            ← generated installation JCL
├── httpd.jcl              ← from scripts/ or templates/
├── httpd.proc             ← from scripts/ or templates/
├── readme.txt / changelog
├── css/, js/              ← from static/css/, static/js/
├── docs/                  ← from doc/
└── lua/                   ← example Lua scripts
```

### Content datasets

Beyond the load library, httpd ships several content datasets that need to be
uploaded from the repo and transmitted:

| Dataset suffix | Source in repo | MVS dataset |
|---------------|----------------|-------------|
| LINKLIB | build output (syslmod) | HTTPD.LINKLIB |
| HTML | `static/html/` | HTTPD.HTML |
| ICON | `static/icon/` | HTTPD.ICON |
| LUA | `static/lua/` (or `lua/`) | HTTPD.LUA |
| BREXX | `static/brexx/` | HTTPD.BREXX |
| UFSDISK0 | built UFS image | HTTPD.UFSDISK0 |

### Required mbt changes (high-level)

1. **Content dataset support** in `project.toml`
   - A new section (e.g. `[[mvs.content.dataset]]`) to declare non-build
     datasets that get uploaded from repo directories and included in the
     distribution XMIT
   - Each content dataset has: source dir in repo, MVS dataset name, DCB attrs

2. **Nested XMIT build step** (`make package`)
   - TRANSMIT each dataset (LINKLIB + content datasets) individually
   - RECEIVE each into a member of a staging PDS
   - TRANSMIT the staging PDS as the final outer XMIT

3. **INSTALL.JCL generation**
   - Template-based, driven by the content dataset list in project.toml
   - Target dataset names configurable (install naming section)

4. **Distribution zip assembly** (`make dist` or extended `make package`)
   - Collects: outer XMIT, INSTALL.JCL, static files from repo, docs, JCL templates
   - Produces: `HTTPD<VER>.zip` matching the 3.3.0 structure

5. **UFS image** — separate concern, likely a dedicated build step outside mbt scope

### Dependency on open mbt issues

- Issue #21 (unit/volume support in datasets) — needed to place content
  datasets on the correct volume during the nested-XMIT build
- Issue #18 (absolute dataset names in install) — needed for HTTPD.LINKLIB,
  HTTPD.HTML etc. (fixed names, not HLQ-qualified)

---

## Phasing

| Phase | Scope | Effort |
|-------|-------|--------|
| 1 | **CGI SDK headers** — `[artifacts] headers = true` for application type in mbt | Small (1–2 days) |
| 2 | **Static file bundling** — `make dist` packs XMIT + static files + docs into zip, without content datasets | Small–Medium |
| 3 | **Content datasets** — upload + TRANSMIT HTML/LUA/ICON/BREXX datasets | Medium |
| 4 | **Nested XMIT + INSTALL.JCL** — full distribution XMIT with generated install JCL | Large |
| 5 | **UFS image build** | Separate project / out of scope for mbt |

Phase 1 is independent and unblocks CGI SDK consumers immediately.
Phases 2–4 are sequential and build on each other.
