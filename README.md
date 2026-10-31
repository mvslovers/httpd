# HTTPD

**HTTPD** is a multi-threaded HTTP/1.1 server for IBM MVS 3.8j, running on Hercules-emulated mainframe systems. It runs as a started task, hosts server modules that answer requests for a URL prefix or a file extension, and serves static files from a UFS filesystem provided by [UFSD](https://github.com/mvslovers/ufsd).

The current release is **4.5.1** ([release page](https://github.com/mvslovers/httpd/releases/tag/v4.5.1)).

Originally created by Michael Dean Rayborn (versions 1.x–3.3.x), the project has been taken over and substantially reworked by Mike Großmann starting with version 4.0.0. The original author is no longer involved in development or support. A maintenance branch (`v3.3.x`) preserves the legacy codebase for reference; all active development happens on `main`.

## Documentation

The manuals are the place to look things up:

- _HTTPD User's Guide_ (ML07-0001): installing, configuring, operating and upgrading HTTPD, routes and authentication, serving files, writing a server module, troubleshooting.
- _HTTPD Reference_ (ML07-0002): every Parmlib keyword, the route and authentication policy, the console commands, the started task, the server module interface, the display modules, the SMF records and the messages.

Read them on Read the Docs: <https://mvslovers.readthedocs.io/projects/httpd/>. The PDFs are attached to each release from 4.5.0 on as `ml07-0001-<edition>.pdf` and `ml07-0002-<edition>.pdf`; their sources are in [`docs/books`](docs/books).

Installation, upgrade and removal step by step are in [docs/installation.md](docs/installation.md), the README of the installation package. Release notes are on the [releases page](https://github.com/mvslovers/httpd/releases).

## Installation

HTTPD is installed with SMP from the release archive; [docs/installation.md](docs/installation.md) walks through it. What you need on the system:

- MVS 3.8j on Hercules with SMP 4 usable (TK4-, TK5, MVS/CE).
- RAKF, or an APF list entry for the load library. HTTPD does not start without one of the two.
- [UFSD](https://github.com/mvslovers/ufsd), only if you want to serve static files.
- A server module, only if you want the REST API that the shipped configuration routes to (see [Server Modules](#server-modules)).

The archive holds the load modules, a sample library with the started task procedure (`HTTPD`) and the configuration pattern (`HTTPPRM0`), the install jobs and a webroot image.

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
| `SMF` | NONE | SMF recording level (NONE / ERROR / AUTH / ALL) |
| `MOD` | — | Server module route, with its own `AUTH=` |
| `LOC` | — | Static path prefix without a program, with its own `AUTH=` |

Authentication is set per route with `AUTH=NONE|FORM|BASIC|TOKEN`. **A route with neither `AUTH=` nor `RES=` is public.** A route with `RES=` and no `AUTH=` is challenged with `BASIC`. The `LOGIN` keyword of earlier 4.x releases is retired.

For every keyword, see the _HTTPD Reference_, "The HTTPPRM Keywords" ([docs/configuration.md](docs/configuration.md) says where each topic is).

## Server Modules

HTTPD uses a server module system for extending the server with custom functionality. Unlike traditional CGI (which forks a new process per request and communicates via stdin/stdout), HTTPD modules are load modules that run inside the server's address space and are called directly through a function vector — similar to Apache's `mod_php` or `mod_rewrite`.

The primary module is [mvsMF](https://github.com/mvslovers/mvsmf), which provides a z/OSMF-compatible REST API for datasets, jobs, and USS files.

```
MOD=MVSMF /zosmf/*      AUTH=TOKEN
```

Module routing supports URL prefix matching and extension matching:

```
MOD=MVSMF /zosmf/*      URL prefix — all requests under /zosmf/
MOD=REXX                Extension — *.rexx files served from DOCROOT
```

A route without a pattern gets one from the module name: `MOD=REXX` becomes `*.rexx`.

Three display modules (HTTPDSRV, HTTPDM, HTTPDMTT) and a deliberate-abend diagnostic (ABEND0C1) ship with the load library but are not routed by default. They are development and troubleshooting tools; give each route an `AUTH=` of its own. See [docs/server-modules.md](docs/server-modules.md) for where to read about writing and using modules.

## Ecosystem

HTTPD is part of the [mvslovers](https://github.com/mvslovers) open-source ecosystem for MVS 3.8j. For an overview of all projects and how they fit together, see the [mvsMF Ecosystem](https://mvslovers.github.io/mvsmf/) documentation.

| Project | Description |
|---------|-------------|
| [libc370](https://github.com/mvslovers/libc370) | C library for MVS 3.8j |
| [ufsd](https://github.com/mvslovers/ufsd) | Cross-address-space UFS filesystem daemon |
| [crypto370](https://github.com/mvslovers/crypto370) | SHA-256, Blowfish and base64 |
| [mvsmf](https://github.com/mvslovers/mvsmf) | z/OSMF-compatible REST API |
| [httplua](https://github.com/mvslovers/httplua) | Lua CGI handler (server module) |
| [httprexx](https://github.com/mvslovers/httprexx) | REXX Server Pages (server module) |
| [ftpd](https://github.com/mvslovers/ftpd) | Standalone FTP daemon |

## History: 4.0.0

Version 4.0.0 was a major release with two goals: HTTP/1.1 (persistent connections, chunked transfer encoding, strict request parsing) and a leaner footprint.

**Added:** HTTP/1.1 with Keep-Alive and chunked transfer encoding; Parmlib-based configuration; SMF recording with configurable levels; the server module system with URL prefix and file extension routing; automatic chunked fallback for modules that do not set `Content-Length`; request parsing hardened per RFC 7230.

**Changed:** static file serving is UFS-only, provided by UFSD; the old in-memory statistics were replaced by SMF type 243 records.

**Removed:** the embedded FTP daemon (replaced by the standalone [FTPD](https://github.com/mvslovers/ftpd)); MQTT telemetry; the Lua configuration engine (replaced by Parmlib); the DD-based document root.

The notes of every release since 4.0.0 are on the [releases page](https://github.com/mvslovers/httpd/releases).

## For Developers

Build instructions, the architecture overview and the ASCII/EBCDIC translation system are documented in [internals/development.md](internals/development.md); how to write your own server modules is in the _HTTPD User's Guide_ and the _HTTPD Reference_ (see [docs/server-modules.md](docs/server-modules.md)).

## Credits

- **Michael Dean Rayborn** — original author (HTTPD 1.x through 3.3.x)
- **Mike Großmann** — maintainer and developer of version 4.0.0+

## License

See [LICENSE](LICENSE) for details.
