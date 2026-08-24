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
MOD=MVSMF   /zosmf/*   AUTH=TOKEN
MOD=HTTPLUA *.lua      AUTH=BASIC
```

Every route carries its own `AUTH=`, and one that does not is **public**: the
3.x `LOGIN` policy is retired, so there is nothing left to fall back on. The
modes are `NONE`, `FORM`, `BASIC` and `TOKEN` — see
[configuration.md](configuration.md).

Copy `samplib/httpprm0` from the distribution to `SYS2.PARMLIB(HTTPPRM0)` as a starting point. The JCL procedure references it via the `HTTPPRM` DD card.

### JCL Changes

The `PARM` field on the `EXEC` statement is no longer used for configuration. Remove any `PARM='CONFIG=...'` references. The Parmlib member is read from the `HTTPPRM` DD card instead.

If you leave one in, the server says so rather than pretending it took effect:

```
HTTPD024W CONFIG=MY.CONFIG(HTTPD) IS IGNORED, CONFIGURATION COMES FROM THE HTTPPRM DD
HTTPD024I USE S HTTPD,M=MEMBER TO SELECT A DIFFERENT MEMBER
```

The `HTTPPRM` DD already exists in the sample JCL procedure (`samplib/httpd`).

Which member is actually in effect is reported on demand, along with everything
else the server parsed:

```
F HTTPD,D CONFIG

HTTPD133I CONFIG FROM SYS2.PARMLIB(HTTPPRM0)
```

## Removed Features

### Embedded FTP Daemon

The built-in FTP server has been removed from HTTPD. It has been replaced by a standalone project: [mvslovers/ftpd](https://github.com/mvslovers/ftpd).

If you were using HTTPD's FTP functionality, install the standalone FTPD as a separate STC.

### MQTT Telemetry

The MQTT telemetry publisher has been removed. There is no direct replacement in 4.0.0. Server metrics are now available through SMF records.

### Lua Scripting Engine

The embedded Lua interpreter has been removed from the HTTPD core. The Lua module (HTTPLUA) has been moved to its own project: [mvslovers/httplua](https://github.com/mvslovers/httplua). Lua and REXX scripting modules are being reworked and will be available as optional add-ons in a future release.

### Demo Modules

The demo modules `hello.c` and `test.c` have been removed. They were test programs from the original codebase.

`abend0c1.c` stayed and **is shipped** in the load library. It is not a demo any more: it allocates (`?kb=n`, default 128) and then abends S0C1 without freeing, which is how the per-request storage reclaim is exercised on a live system. Like every module since 4.0.0 it is inert unless a `MOD=` line names it, and the shipped `HTTPPRM0` leaves that line commented out.

### DD-Based Document Root

Static file serving from MVS datasets via DD cards is no longer supported. HTTPD 4.0.0 serves files exclusively from UFS via [UFSD](https://github.com/mvslovers/ufsd). Set `DOCROOT=/www` (or your preferred UFS path) in the Parmlib.

### In-Memory Statistics System

The 4-tier time-series statistics (`httpstat.c`, `httprepo.c`) with dataset persistence and Apache Combined Log format have been removed. They are replaced by SMF Type 243 records. See [docs/smf-records.md](smf-records.md) for the record format.

### JES2 Browser and Dataset List

The built-in web interfaces for JES2 job browsing (`HTTPJES2`) and dataset listing (`HTTPDSL`) are no longer built or shipped. [mvsMF](https://github.com/mvslovers/mvsmf)'s dataset and jobs REST APIs replace them.

Two things to check when migrating a 3.3.x configuration: drop any `CGI=`/`MOD=` line naming `HTTPDSL` or `HTTPJES2` — the route is accepted but the program load fails on the first matching request — and drop the `HASPCKPT` and `HASPACE1` DDs from the STC procedure, which existed only so `HTTPJES2` could read the JES2 spool.

The sources are kept under `tbd/` for reference and are outside the build.

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

In addition to URL prefix matching (e.g. `MOD=MVSMF /zosmf/*`), a module can be registered without an explicit pattern. HTTPD then derives the pattern from the **program name**, lower-cased: `MOD=LUA` handles `*.lua` from `DOCROOT`.

That derivation is the whole rule, and it is why the shipped scripting modules do **not** use it. They are called `HTTPLUA` and `HTTPREXX` (separate products: [mvslovers/httplua](https://github.com/mvslovers/httplua), [mvslovers/httprexx](https://github.com/mvslovers/httprexx)), so a pattern-less line would register `*.httplua`. Name the extension instead:

```
MOD=HTTPLUA  *.lua       AUTH=BASIC     Lua scripts from DOCROOT
MOD=HTTPREXX *.rexx      AUTH=BASIC     REXX scripts from DOCROOT
MOD=MVSMF    /zosmf/*    AUTH=TOKEN     all requests under /zosmf/
```

This allows script files to live alongside static content in the normal document root, similar to how Apache handles PHP.

**Write the `AUTH=`.** A route that carries none is public — there is no global `LOGIN` policy left to fall back on — and a script handler reached by anyone who can find the port is a shell on your system.

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
