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

`LOGIN` remains the global default. Individual routes can override it and add a
resource check with the per-route `AUTH=` / `RES=` options (see below).

### Per-route authentication policy (`AUTH=`, `RES=`)

Both `MOD=` (a CGI route) and `LOC=` (a program-less static route) can carry a
per-route auth policy as trailing options. The server enforces it in its request
pipeline as a **2-stage gate**, uniformly for CGI and static routes, *before* it
serves a file or dispatches a CGI:

1. **Authenticate** — the credential is resolved from the request (`Sec-Token` /
   `LtpaToken2` cookie, `Authorization: Bearer`, or `Authorization: Basic`). If
   the route requires an identity and none is present, the server answers **401**
   with the challenge selected by `AUTH=`.
2. **Authorize** — if the route sets `RES=class:resource`, the server issues a
   RACF/RAKF check (RACHECK/FRACHECK) under the client's ACEE. On denial it
   answers **403**.

> **A `RES=` resource with no profile defined permits the access.** That is
> standard SAF behaviour — an undefined resource is "not protected", not
> "denied" — so a typo in the class or resource name silently disables stage 2
> for that route rather than locking it. `DEBUG 1` traces every such check, so
> verify a new `RES=` route against the debug log before relying on it.

**A route that carries an auth policy is registered or the server does not
start.** Dropping it is not a safe fallback: the route does not disappear, its
requests are served under the global `LOGIN` default instead — and for a `LOC=`
prefix under `LOGIN NONE` that hands out the whole subtree it was protecting. So
whenever such a route cannot be built, the server issues `HTTPD418E`/`HTTPD419E`
naming it, then `HTTPD420E`, and terminates before the listener is bound. Three
things reach that path:

- the policy itself could not be built, or the route could not be added (both
  only when the region runs out of storage while the Parmlib is read)
- the line could not be tokenized at all — whether it carried a policy is then
  unknowable, so it is assumed to have
- **the positional token is missing and an option stands in its place** — the
  typo case, no allocation failure needed:

  ```
  LOC=AUTH=BASIC RES=FACILITY:HTTPD.ADMIN     path forgotten
  MOD=AUTH=BASIC /zosmf/*                     program name forgotten
  ```

  The first once published exactly the subtree it named; the second folded the
  option into an 8-character module name (`AUTH=BAS`) and registered a route
  that could never load.

A route that fails to register with no policy at all, or with `AUTH=NONE` only,
stays a warning and the server continues — the fallback can only be stricter
than what it asked for.

| Option | Values | Description |
|--------|--------|-------------|
| `AUTH=` | `NONE` \| `FORM` \| `BASIC` | Challenge for stage 1. `NONE` = public (no authentication). `FORM` = redirect to the HTML login form. `BASIC` = `401 WWW-Authenticate: Basic`. **Omitted** = inherit the global `LOGIN` default (backward compatible). |
| `RES=` | `class:resource` | Optional RACF/RAKF resource for stage 2, e.g. `RES=FACILITY:MVSMF.ACCESS`. Checked for `READ`. A resource always requires an identity, so it implies authentication even without `AUTH=`. |

The credential resolver always tries every source, so `AUTH=` only selects the
*challenge* shown when authentication is missing — it does not restrict which
source a client may use. `AUTH=NONE` combined with `RES=` is contradictory (a
public route has no ACEE to check) — the server warns and `NONE` wins.

`/login`, `/logout` and the login-page assets (`/login.*`, `/favicon.*`) are
always reachable so an `AUTH=FORM` challenge can render.

## Timezone

**There is no timezone keyword.** The server takes its offset from the system:
`tzset()` runs at startup and resolves it from the `TZ` environment variable if
one is set, otherwise from the system's `CVTTZ`.

To choose a different offset, set `TZ` in the STC's `SYSENV` or `ENVIRON` DD,
in `[-]HH[:MM[:SS]]` format:

```
TZ=+02:00
```

That reaches **every** task — the server, its worker threads and every module —
because both the server's and the modules' startup code call `tzset()` after
loading the environment. It is the only supported way to override it.

Timestamps in API responses are unaffected either way: they are returned as
ISO 8601 instants in UTC (`2026-08-07T17:25:18.000Z`), converted with `gmtime`,
so the caller localizes them. The `Date:` response header is likewise always
GMT, as HTTP requires.

> **Retired: `TZOFFSET`.** Up to 4.0.0-dev the Parmlib accepted a `TZOFFSET`
> keyword. It is now accepted and ignored, with a `HTTPD025W` warning naming the
> system offset in use, so an existing Parmlib still starts the server — but the
> statement has no effect and should be deleted.
>
> It was removed because it did not do what its name suggested. `TZOFFSET
> +02:00` reads like a display preference, but it asserted that the machine's
> TOD clock ran at UTC+2; set on a system at UTC−5 it silently shifted every
> JES2 timestamp by seven hours. Its documented purposes were never real either
> — the `Date:` header and the SMF timestamps never read it — and it reached
> only the one task that parsed the Parmlib, so workers and modules kept the
> system offset regardless. `TZ` does the job properly. See issue #145.

## Debug

| Keyword | Default | Description |
|---------|---------|-------------|
| `DEBUG` | 0 | Debug level. `0` = off, `1` = basic debug output to `HTTPDBG` DD. |

## Server Modules

```
MOD=PROGRAM /url/pattern        URL prefix match
MOD=PROGRAM                     Extension match (derives *.program from name)
MOD=PROGRAM /url/pattern AUTH=BASIC RES=class:resource   with auth policy
```

Server modules are load modules that HTTPD loads at startup via `__load()` and calls directly through the HTTPX function vector. They run inside the server's address space — unlike traditional CGI programs which fork a new process per request.

Any `MOD=` route may add the per-route `AUTH=` / `RES=` options described under
[Security](#security).

| Routing Type | Syntax | Behavior |
|-------------|--------|----------|
| URL prefix | `MOD=MVSMF /zosmf/*` | All requests where the path starts with `/zosmf/` are routed to the module. |
| Extension | `MOD=LUA` | HTTPD derives the extension `*.lua` from the module name. Requests for `.lua` files are routed to the module. The file path is resolved relative to `DOCROOT`. |

URL prefix matching is checked first. If no prefix matches, extension matching is attempted.

### Available Server Modules

| Module | Syntax | Description |
|--------|--------|-------------|
| MVSMF | `MOD=MVSMF /zosmf/*` | z/OSMF-compatible REST API (datasets, jobs, USS files). Separate project: [mvsmf](https://github.com/mvslovers/mvsmf). |
| LUA | `MOD=LUA` | Lua scripting module. Handles `*.lua` files. Separate project (not shipped with 4.0.0). |
| REXX | `MOD=REXX` | REXX scripting module. Handles `*.rexx` files. Separate project (not shipped with 4.0.0). |
| HTTPDSRV | `MOD=HTTPDSRV /.dsrv` | Display server status. Debug tool, not for production. |
| HTTPDM | `MOD=HTTPDM /.dm` | Display memory. Debug tool, not for production. |
| HTTPDMTT | `MOD=HTTPDMTT /.dmtt` | Display master trace table. Debug tool, not for production. |

`HTTPDSL` (`/dsl/*`, dataset list) and `HTTPJES2` (`/jes/*`, JES2 job browser)
were removed in 4.0.0 — mvsMF's dataset and jobs APIs replace them. They are no
longer built, so a `MOD=` line naming either one parses fine but fails to load
the program when a request first matches the route.

### Example

```
# Production: only mvsMF
MOD=MVSMF /zosmf/*

# Development: mvsMF + debug tools
MOD=MVSMF /zosmf/*
MOD=HTTPDSRV /.dsrv
MOD=HTTPDMTT /.dmtt
```

## Static Locations (`LOC=`)

```
LOC=/url/prefix                 static path prefix (falls through to DOCROOT)
LOC=/url/prefix AUTH=mode RES=class:resource   with auth policy
```

A `LOC=` route matches a path **without** a program: the request falls through
to the normal static file handler (`DOCROOT`), but the route still carries the
same per-route `AUTH=` / `RES=` policy as `MOD=`. This lets a whole static
subtree (an SPA, an admin area) sit behind a login or a RACF/RAKF profile without
a CGI.

Path matching is the same as for `MOD=`: a path is matched **exactly** unless it
contains a wildcard, so a subtree needs a trailing `*` (`LOC=/admin/*`, not
`LOC=/admin`). Routes are tested in the order they appear, so list the more
specific prefixes before a catch-all — a catch-all `LOC=/*` must come *after*
`MOD=MVSMF /zosmf/*`, or it shadows it.

```
# SPA: the whole document tree is public (the login page must load), the API
# is protected -- list the API route first so the catch-all doesn't shadow it
MOD=MVSMF /zosmf/*     AUTH=BASIC RES=FACILITY:MVSMF.ACCESS
LOC=/*                 AUTH=NONE

# Static admin area behind a RACF profile
LOC=/admin/*          AUTH=BASIC RES=FACILITY:HTTPD.ADMIN
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
MOD=MVSMF /zosmf/*
SMF=AUTH TYPE=243
```
