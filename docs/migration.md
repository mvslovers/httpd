# Migration Guide: HTTPD 3.x → 4.0.0

This guide covers everything you need to know when upgrading from HTTPD 3.3.x to 4.0.0.

## Configuration

### Lua Configuration → Parmlib

HTTPD 3.x used Lua scripts for configuration (`PARM='CONFIG=dataset(member)'`). Version 4.0.0 replaces this with a plain-text Parmlib member.

**Before (3.x Lua config):**
```lua
server = {
    port = 8080,
    mintask = 3,
    maxtask = 9,
    docroot = "/www",
}
cgi = {
    MVSMF = "/zosmf/*",
    HTTPLUA = "/lua/*",
}
```

**After (4.0.0 Parmlib):**
```
PORT=8080
MINTASK=3
MAXTASK=9
DOCROOT=/www
MOD=MVSMF /zosmf/*
MOD=LUA
```

Copy `samplib/httpprm0` from the distribution to `SYS2.PARMLIB(HTTPPRM0)` as a starting point. The JCL procedure references it via the `HTTPPRM` DD card.

### JCL Changes

The `PARM` field on the `EXEC` statement is no longer used for configuration. Remove any `PARM='CONFIG=...'` references. The Parmlib member is read from the `HTTPPRM` DD card instead.

The `HTTPPRM` DD already exists in the sample JCL procedure (`samplib/httpd`).

## Removed Features

### Embedded FTP Daemon

The built-in FTP server has been removed from HTTPD. It has been replaced by a standalone project: [mvslovers/ftpd](https://github.com/mvslovers/ftpd).

If you were using HTTPD's FTP functionality, install the standalone FTPD as a separate STC.

### MQTT Telemetry

The MQTT telemetry publisher has been removed. There is no direct replacement in 4.0.0. Server metrics are now available through SMF records.

### Lua Scripting Engine

The embedded Lua interpreter has been removed from the HTTPD core. The Lua module (HTTPLUA) has been moved to its own project: [mvslovers/httplua](https://github.com/mvslovers/httplua). Lua and REXX scripting modules are being reworked and will be available as optional add-ons in a future release.

### Demo Modules

The demo modules `hello.c`, `abend0c1.c`, and `test.c` have been removed. They were test programs from the original codebase.

### DD-Based Document Root

Static file serving from MVS datasets via DD cards is no longer supported. HTTPD 4.0.0 serves files exclusively from UFS via [UFSD](https://github.com/mvslovers/ufsd). Set `DOCROOT=/www` (or your preferred UFS path) in the Parmlib.

### In-Memory Statistics System

The 4-tier time-series statistics (`httpstat.c`, `httprepo.c`) with dataset persistence and Apache Combined Log format have been removed. They are replaced by SMF Type 243 records. See [docs/smf-records.md](smf-records.md) for the record format.

### JES2 Browser and Dataset List

The built-in web interfaces for JES2 job browsing (`HTTPJES2`) and dataset listing (`HTTPDSL`) are still present in the code but marked as deprecated. They will be replaced by [mvsMF](https://github.com/mvslovers/mvsmf) REST API endpoints in a future release.

## New Features

### HTTP/1.1

HTTPD 4.0.0 is a full HTTP/1.1 server:

- **Keep-Alive** — persistent connections reduce TCP overhead. Configurable via `KEEPALIVE_TIMEOUT` and `KEEPALIVE_MAX`.
- **Chunked Transfer Encoding** — responses without a known Content-Length are automatically sent using chunked encoding. This includes server module output.
- **Strict request parsing** — invalid headers, malformed Content-Length, duplicate Host headers, and other protocol violations are properly rejected. The server passes all 33 h1spec HTTP/1.1 compliance tests.

### SMF Recording

HTTPD can write SMF records for auditing and performance analysis. Configure via Parmlib:

```
SMF=NONE              No recording (default)
SMF=ERROR             Record HTTP errors (status >= 400)
SMF=AUTH              Record auth events + errors
SMF=ALL               Record every request + session summaries
SMF=ALL TYPE=200      Same, using SMF record type 200 instead of default 243
```

See [docs/smf-records.md](smf-records.md) for the record format and field descriptions.

### Extension-Based Module Routing

In addition to URL prefix matching (e.g. `MOD=MVSMF /zosmf/*`), scripting modules can be registered without an explicit pattern. HTTPD automatically derives the file extension from the module name:

```
MOD=LUA                 Handles *.lua files from DOCROOT
MOD=REXX                Handles *.rexx files from DOCROOT
MOD=MVSMF /zosmf/*      All requests under /zosmf/ (explicit prefix)
```

This allows script files to live alongside static content in the normal document root, similar to how Apache handles PHP.

### Automatic Chunked Fallback for Modules

Server modules that send HTTP response bodies without setting a `Content-Length` header automatically get chunked transfer encoding. This prevents browsers from hanging on Keep-Alive connections waiting for the response to end.

## Checklist

When upgrading from 3.x to 4.0.0:

1. Create a Parmlib member from `samplib/httpprm0`
2. Update the JCL procedure from `samplib/httpd` (or add `HTTPPRM` DD to your existing JCL)
3. Remove any `PARM='CONFIG=...'` from the EXEC statement
4. Ensure UFSD is running and your document root exists on UFS
5. Install standalone FTPD if you need FTP functionality
6. Review SMF settings — default is `SMF=NONE` (no recording)
7. Test with `curl -v http://your-host:port/` to verify HTTP/1.1 responses
