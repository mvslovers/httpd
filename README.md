# HTTPD

**HTTPD** is a multi-threaded HTTP/1.1 server for IBM MVS 3.8j, running on Hercules-emulated mainframe systems.

Originally created by Michael Dean Rayborn (versions 1.x–3.3.x), the project has been taken over and substantially reworked by Mike Großmann starting with version 4.0.0. The original author is no longer involved in development or support. A maintenance branch (`v3.3.x`) preserves the legacy codebase for reference; all active development happens on `main`.

Version 4.0.0 is a major release with two primary goals:

- **HTTP/1.1 compliance** — persistent connections (Keep-Alive), chunked transfer encoding, strict request parsing (h1spec 33/33 tests passing)
- **Leaner footprint** — removal of the embedded FTP daemon, MQTT telemetry, Lua scripting engine, demo CGIs, and the 4-tier in-memory statistics system

## What's New in 4.0.0

**Added:**

- Full HTTP/1.1 support with Keep-Alive and Chunked Transfer Encoding
- Parmlib-based configuration (replaces Lua scripting)
- SMF recording with configurable levels (NONE / ERROR / AUTH / ALL)
- Extension-based CGI routing (`CGI=HTTPLUA *.lua` alongside URL prefix routing)
- Automatic chunked fallback for CGI modules that don't set Content-Length
- Hardened HTTP request parsing — invalid headers, methods, URIs rejected per RFC 7230

**Changed:**

- Static file serving is now UFS-only, provided by [UFSD](https://github.com/mvslovers/ufsd)
- The old in-memory statistics system has been replaced by SMF Type 243 records

**Removed:**

- Embedded FTP daemon — replaced by the standalone [FTPD](https://github.com/mvslovers/ftpd) project
- MQTT telemetry
- Lua configuration engine (replaced by Parmlib)
- Demo CGI modules (hello, abend0c1, test)
- DD-based document root (UFS-only now)

**Outlook:**

- JES2 browser and dataset browser will be re-implemented as part of [mvsMF](https://github.com/mvslovers/mvsmf) in a future release
- Lua and REXX CGI support is being reworked and may return in a later version

## Installation

> **Note:** Installation instructions will be finalized once the packaging system is complete. The following is a preliminary guide.

### Prerequisites

- Hercules-emulated MVS 3.8j system (TK4- or TK5)
- [UFSD](https://github.com/mvslovers/ufsd) — UFS filesystem daemon (required for static file serving)
- [crent370](https://github.com/mvslovers/crent370) — C runtime library

### JCL Procedure

Copy the STC procedure from `samplib/httpd` to `SYS2.PROCLIB(HTTPD)`:

```
//HTTPD    EXEC PGM=HTTPD,REGION=8M,TIME=1440
//STEPLIB  DD  DISP=SHR,DSN=HTTPD.LINKLIB
//HTTPPRM  DD  DSN=SYS2.PARMLIB(HTTPPRM0),DISP=SHR,FREE=CLOSE
```

### Parmlib

Copy `samplib/httpprm0` to `SYS2.PARMLIB(HTTPPRM0)` and adjust as needed. A minimal configuration:

```
PORT=8080
DOCROOT=/www
CGI=MVSMF /zosmf/*
```

### Starting and Stopping

```
/S HTTPD                         Start with default config
/S HTTPD,M=HTTPPRM1              Start with alternate config member
/F HTTPD,DISPLAY CONFIG          Show current configuration
/F HTTPD,DISPLAY STATS           Show request counters
/F HTTPD,HELP                    List available commands
/P HTTPD                         Stop the server
```

For upgrading from HTTPD 3.x, see [docs/migration.md](docs/migration.md).

## Configuration

The server is configured through a Parmlib member referenced by the `HTTPPRM` DD card. Key settings:

| Keyword | Default | Description |
|---------|---------|-------------|
| `PORT` | 8080 | Listening port |
| `DOCROOT` | `/www` | UFS document root |
| `MINTASK` | 3 | Minimum worker threads |
| `MAXTASK` | 9 | Maximum worker threads |
| `KEEPALIVE_TIMEOUT` | 5 | Keep-alive idle timeout (seconds) |
| `KEEPALIVE_MAX` | 100 | Max requests per connection |
| `LOGIN` | NONE | Authentication mode (NONE / RACF) |
| `SMF` | NONE | SMF recording level |
| `CGI` | — | CGI module registration |

For the complete reference, see [docs/configuration.md](docs/configuration.md).

## CGI Modules

HTTPD supports external CGI modules loaded at startup. In version 4.0.0, the primary CGI module is [mvsMF](https://github.com/mvslovers/mvsmf), which provides a z/OSMF-compatible REST API for datasets, jobs, and USS files.

```
CGI=MVSMF /zosmf/*
```

CGI routing supports both URL prefix matching and file extension matching:

```
CGI=MVSMF /zosmf/*       URL prefix — all requests under /zosmf/
CGI=HTTPLUA *.lua         Extension  — .lua files served from DOCROOT
```

Debug CGI modules (HTTPDSRV, HTTPDMTT) are included in the server binary but not enabled by default. They are intended for development and troubleshooting only. See [docs/development.md](docs/development.md) for details.

## Ecosystem

HTTPD is part of the [mvslovers](https://github.com/mvslovers) open-source ecosystem for MVS 3.8j:

| Project | Description |
|---------|-------------|
| [crent370](https://github.com/mvslovers/crent370) | C runtime library for MVS 3.8j |
| [ufsd](https://github.com/mvslovers/ufsd) | Cross-address-space UFS filesystem daemon |
| [mvsmf](https://github.com/mvslovers/mvsmf) | z/OSMF-compatible REST API |
| [ftpd](https://github.com/mvslovers/ftpd) | Standalone FTP daemon (replaces embedded FTPD from 3.x) |

## For Developers

Build instructions, architecture overview, the ASCII/EBCDIC translation system, and how to write your own CGI modules are documented in [docs/development.md](docs/development.md).

## Credits

- **Michael Dean Rayborn** — original author (HTTPD 1.x through 3.3.x)
- **Mike Großmann** — maintainer and developer of version 4.0.0+

## License

See [LICENSE](LICENSE) for details.
