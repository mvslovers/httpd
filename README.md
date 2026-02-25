
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

- Cross-compilation environment for MVS 3.8j
- `c2asm370` compiler (GCC-based, targeting S/370)
- `mvsasm` assembler script
- CRENT370 libraries by Mike Rayborn

### Clone

```bash
git clone --recursive https://github.com/mvslovers/httpd.git
cd httpd
```

### Compile

```bash
# Build main HTTPD server
make

# Build credentials subsystem
cd credentials && make

# Clean generated files
make clean
```

The build pipeline is: C source → `c2asm370` → S/370 Assembly (.s) → `mvsasm` → Object decks (.o) → MVS datasets (`MDR.HTTPD.OBJECT`, `MDR.HTTPD.NCALIB`)

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
