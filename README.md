
# HTTPD

**HTTPD** is a multi-threaded HTTP/FTP server for IBM MVS 3.8j, originally created by Michael Dean Rayborn. It runs on Hercules-emulated MVS systems and provides static file serving, Lua CGI, BREXX CGI, JES2 job management, Server-Side Includes, MQTT telemetry, and a full FTP daemon.

This project is maintained as part of the [mvslovers](https://github.com/mvslovers) community.

## Features

- **HTTP Server** — Multi-threaded with configurable worker pool (3-9 threads)
- **FTP Daemon** — Full FTP server on a separate port (default 8021)
- **Lua CGI** — Run Lua scripts via `/lua/scriptname`
- **BREXX CGI** — Run REXX scripts via `/rexx/scriptname`
- **JES2 Integration** — Job status and output management via web interface
- **Server-Side Includes** — Dynamic HTML processing
- **MQTT Telemetry** — Optional server metrics publishing
- **Credentials/RACF** — Authentication and session management with token support
- **Lua Configuration** — All server settings via Lua scripts (`PARM='CONFIG=dataset(member)'`)

## Architecture

The server uses a socket thread for accepting connections and a pool of worker threads for request processing. Each HTTP request flows through a state machine:

`CSTATE_IN` → `CSTATE_PARSE` → `CSTATE_GET/POST/...` → `CSTATE_DONE` → `CSTATE_REPORT` → `CSTATE_RESET`

All internal processing uses EBCDIC; ASCII conversion happens at network I/O boundaries.

## Building

### Prerequisites

- [c2asm370](https://github.com/mvslovers/c2asm370) — GCC 3.2.3 cross-compiler (C to S/370 assembly)
- A running MVS 3.8j system with the [mvsMF](https://github.com/mvslovers/mvsmf) REST API (for remote assembly and link)
- NCAL libraries on MVS: CRENT370, UFS370, LUA370, MQTT370

### Clone

```bash
git clone --recursive https://github.com/mvslovers/httpd.git
cd httpd
```

### Configure

Copy `.env.example` to `.env` and fill in your MVS connection and dataset settings:

```bash
cp .env.example .env
vi .env
```

### Build

```bash
make                          # Compile all C sources, assemble on MVS
make clean                    # Remove generated .s and .o files
```

`make` compiles all source files (including the `credentials/` subsystem),
transfers the assembly to MVS via the mvsMF API, and produces NCAL modules
in the configured HTTPD.NCALIB dataset.

### Link

```bash
make link                     # Link all 13 load modules on MVS
make link MODULES=HTTPJES2    # Link a single module
make link MODULES="HTTPD HTTPLUA"  # Link specific modules
```

`make link` invokes `scripts/mvslink` which submits link-edit JCL for each
load module. Each module has its own SYSLIB/NCALIB DD layout matching the
required dependencies (see `scripts/mvslink` for details).

The 13 load modules are: ABEND0C1, HELLO, HTTPD, HTTPDM, HTTPDMTT,
HTTPDSL, HTTPDSRV, HTTPJES2, HTTPLUA, HTTPLUAX, HTTPREXX, HTTPSAY, HTTPTEST.

### Build Pipeline

```
C source (.c)
  --> c2asm370 (cross-compile to S/370 assembly)
  --> mvsasm (assemble + NCAL link on MVS via mvsMF API)
  --> mvslink (link load modules on MVS via mvsMF API)
```

### IDE Support

```bash
make compiledb                # Generate compile_commands.json for clangd
```

## Configuration

The server is configured via Lua scripts specified on the EXEC card:

```
PARM='CONFIG=your.lua.config.dataset'
```

If the server defaults are acceptable, the `PARM` can be omitted entirely.

## Console Commands

The server responds to MVS modify commands:

| Command | Description |
|---------|-------------|
| `/F HTTPD,D V` | Display version |
| `/F HTTPD,D M` | Display memory usage |
| `/F HTTPD,D TIME` | Display server uptime |
| `/F HTTPD,D LOGIN` | Display active sessions |
| `/F HTTPD,D STATS` | Display server statistics |
| `/F HTTPD,SET MIN=n` | Set minimum worker threads |
| `/F HTTPD,SET MAX=n` | Set maximum worker threads |

## Acknowledgments

This project was created by **Michael Dean Rayborn**, whose extensive work on MVS 3.8j tooling — including CRENT370, UFS, Lua370, MQTT370, and BREXX370 — forms the foundation of the entire MVS open-source ecosystem.

## License

This project is licensed under the [MIT License](LICENSE).
