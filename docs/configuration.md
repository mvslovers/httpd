# HTTPD Configuration Reference

HTTPD is configured through a Parmlib member, referenced by the `HTTPPRM` DD card in the STC JCL procedure.

## Format

```
# Lines starting with # or * are comments.
# Format: KEYWORD=VALUE (keys are case-insensitive)
PORT=8080
DOCROOT=/www
```

## Network

| Keyword | Default | Description |
|---------|---------|-------------|
| `PORT` | 8080 | TCP port the server listens on. |
| `BIND_TRIES` | 10 | Number of times to retry `bind()` if the port is in use. |
| `BIND_SLEEP` | 10 | Seconds to wait between bind retries. |
| `LISTEN_QUEUE` | 5 | TCP listen backlog (maximum pending connections). |

## Worker Threads

| Keyword | Default | Description |
|---------|---------|-------------|
| `MINTASK` | 3 | Minimum number of worker threads. Threads are pre-started at server initialization. |
| `MAXTASK` | 9 | Maximum number of worker threads. Additional threads are started on demand. |
| `CLIENT_TIMEOUT` | 10 | Seconds before an idle client in the request-reading phase is disconnected. |

## Keep-Alive

| Keyword | Default | Description |
|---------|---------|-------------|
| `KEEPALIVE_TIMEOUT` | 5 | Seconds to keep an idle connection open waiting for the next request. |
| `KEEPALIVE_MAX` | 100 | Maximum number of requests allowed on a single persistent connection before the server closes it. |

HTTP/1.1 clients get persistent connections by default. HTTP/1.0 clients always get `Connection: close`.

## Document Root

| Keyword | Default | Description |
|---------|---------|-------------|
| `DOCROOT` | `/www` | UFS path for static file serving. UFSD must be running and the path must exist. |

All static content (HTML, CSS, JS, images) is served from this directory. Server-Side Includes (SSI) are processed for `.html` files.

## Security

| Keyword | Default | Description |
|---------|---------|-------------|
| `LOGIN` | NONE | Authentication mode. `NONE` = no authentication required. `RACF` = cookie-based RACF authentication with the built-in login page. |

When `LOGIN=RACF`, unauthenticated requests to protected resources receive a redirect to the login page. After successful RACF authentication, a `Sec-Token` cookie is issued. Sessions are bound to the client's IP address.

## Timezone

| Keyword | Default | Description |
|---------|---------|-------------|
| `TZOFFSET` | +00:00 | UTC offset for the `Date:` response header and SMF record timestamps. Format: `+HH:MM` or `-HH:MM`. |

## Debug

| Keyword | Default | Description |
|---------|---------|-------------|
| `DEBUG` | 0 | Debug level. `0` = off, `1` = basic debug output to `HTTPDBG` DD. |

## CGI Modules

```
CGI=PROGRAM /url/pattern        URL prefix match
CGI=PROGRAM *.ext               File extension match
```

CGI modules are external load modules that HTTPD loads at startup via `__load()`. Each CGI entry maps a URL pattern or file extension to a program name.

| Routing Type | Pattern | Behavior |
|-------------|---------|----------|
| URL prefix | `/zosmf/*` | All requests where the path starts with `/zosmf/` are routed to the module. |
| Extension | `*.lua` | Requests for files with the `.lua` extension are routed to the module. The file path is resolved relative to `DOCROOT`. |

URL prefix matching is checked first. If no prefix matches, extension matching is attempted.

### Available CGI Modules

| Module | Pattern | Description |
|--------|---------|-------------|
| MVSMF | `/zosmf/*` | z/OSMF-compatible REST API (datasets, jobs, USS files). Separate project: [mvsmf](https://github.com/mvslovers/mvsmf). |
| HTTPDSRV | `/.dsrv` | Display server status. Debug tool, not for production. |
| HTTPDM | `/.dm` | Display memory. Debug tool, not for production. |
| HTTPDMTT | `/.dmtt` | Display master trace table. Debug tool, not for production. |
| HTTPDSL | `/dsl/*` | Dataset list browser. **Deprecated** — will be replaced by mvsMF. |
| HTTPJES2 | `/jes/*` | JES2 job browser. **Deprecated** — will be replaced by mvsMF. |

### Example

```
# Production: only mvsMF
CGI=MVSMF /zosmf/*

# Development: mvsMF + debug tools
CGI=MVSMF /zosmf/*
CGI=HTTPDSRV /.dsrv
CGI=HTTPDMTT /.dmtt
```

## SMF Recording

```
SMF=LEVEL [TYPE=nnn]
```

| Keyword | Default | Description |
|---------|---------|-------------|
| `SMF` | NONE | Recording level. See below. |
| `TYPE` | 243 | SMF record type number (range 128–255). Change if type 243 is already in use on your system. |

### Recording Levels

| Level | Request Records (sub=1) | Session Records (sub=2) | Auth Events |
|-------|------------------------|------------------------|-------------|
| `NONE` | — | — | — |
| `ERROR` | Only responses with status >= 400 | — | — |
| `AUTH` | Only responses with status >= 400 | — | Login, logout, auth failures |
| `ALL` | Every completed request | At connection close | Login, logout, auth failures |

### Examples

```
SMF=NONE                 No SMF recording (default)
SMF=ERROR                Record errors only
SMF=AUTH                 Record auth events and errors
SMF=ALL                  Record everything
SMF=ALL TYPE=200         Record everything, use SMF type 200
```

For the SMF record format and field descriptions, see [smf-records.md](smf-records.md).

## Complete Example

```
# HTTPD 4.0.0 — Production Configuration
PORT=1080
DOCROOT=/www
MINTASK=3
MAXTASK=9
KEEPALIVE_TIMEOUT=5
KEEPALIVE_MAX=100
CLIENT_TIMEOUT=10
LOGIN=RACF
TZOFFSET=+01:00
CGI=MVSMF /zosmf/*
SMF=AUTH TYPE=243
```
