# HTTPD Server — Code Review

Date: 2026-03-10 (updated)
Previous: 2026-02-28

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Security Vulnerabilities](#security-vulnerabilities)
3. [Security Architecture](#security-architecture)
4. [Architecture & Maintainability](#architecture--maintainability)
5. [Memory & Optimization](#memory--optimization)
6. [Router Comparison: httpd vs mvsmf](#router-comparison-httpd-vs-mvsmf)
7. [Remediation Roadmap](#remediation-roadmap)

---

## Executive Summary

| Category | Findings |
|----------|----------|
| Security Vulnerabilities | 5 CRITICAL, 3 HIGH, 4 MEDIUM, 2 LOW |
| Memory Issues | 0 critical leaks, 6 missing NULL checks, 1 partial leak |
| Architecture | Solid foundations, extreme file fragmentation (143 files), tight FTP coupling |
| Security Concept | RACF-based auth, Blowfish-encrypted tokens, IP-bound sessions — no fine-grained authorization |
| Router | httpd uses flat CGI dispatch; mvsmf has method+pattern router with middleware — integration possible |

---

## Security Vulnerabilities

### CRITICAL

#### 1. Buffer overflow in HTTP header/query environment variable names

**Files:** httpshen.c:14,54 / httpsqen.c:11 / httppars.c:218

User-controlled header and query parameter names are written into a
fixed 256-byte buffer via unbounded `sprintf()`:

```c
/* httpshen.c:14 */
char newname[256];
sprintf(newname, "HTTP_%s", name);

/* httpshen.c:54 */
sprintf(newname, "HTTP_Cookie-%s", name);

/* httpsqen.c:11 */
sprintf(newname, "QUERY_%s", name);

/* httppars.c:218 */
sprintf(newname, "POST_%s", name);
```

**Attack:** Send a header with a name exceeding 250 bytes. Overflows
the stack buffer, corrupting the return address or adjacent data.

**Fix:** Replace with `snprintf(newname, sizeof(newname), ...)`.

---

#### 2. Unbounded vsprintf in HTTP response output

**File:** httpprtv.c:14

```c
UCHAR buf[5120];
len = vsprintf(buf, fmt, args);
```

This is the core output function (`http_printv`) used by `http_printf`
throughout the entire server and all CGI modules. If the formatted
output exceeds 5120 bytes, the stack buffer overflows.

**Attack:** Any code path where user-controlled data (URL paths, query
parameters, header values) is included in an `http_printf()` call with
a large expansion.

**Fix:** Replace with `vsnprintf(buf, sizeof(buf), fmt, args)`.

---

#### 3. Unprotected strcat in query/POST string parsing

**File:** httppars.c:49,162

```c
strcpyp(buf, CBUFSIZE, p, 0);
strcat(buf, "&");    /* line 49: append delimiter to query string */

/* ... */
strcat(buf, "&");    /* line 162: append delimiter to POST data */
```

The buffer is CBUFSIZE (4008 bytes). If `strcpyp()` fills it near
capacity, the `strcat("&")` writes one byte past the end.

**Attack:** Send a request with a query string close to 4008 bytes.

**Fix:** Check `strlen(buf) < CBUFSIZE - 2` before `strcat()`.

---

#### 4. Path traversal in SSI include handler

**File:** httpfile.c:501-624 (ssi_include)

```c
strncpy(uri, path, sizeof(uri));
/* ... */
httpc->fp = http_open(httpc, path, mime);
```

The SSI include handler opens arbitrary paths without validating
they are within the document root. An attacker who can place an HTML
file with SSI directives on the server can include files outside the
intended directory.

**Attack:**
```html
<!--#include virtual="../../../../sensitive/file" -->
```

**Fix:** Validate the resolved path is within the document root before
opening. Reject paths containing `..` sequences.

---

#### 5. FTP path traversal

**File:** ftpfqn.c:176-254 (resolve_path)

```c
while (p = strstr(path, "/..")) {
    /* attempts to normalize but does not enforce root boundary */
}
```

The path resolution function handles `..` but does not verify the
resulting path stays within the FTP root directory. Combined with
multiple `sprintf()` calls without bounds checking (lines 108, 112,
120, 150), this is both a traversal and overflow risk.

**Attack:**
```
CWD /app
CWD ../../etc
RETR passwd
```

**Fix:** After resolution, verify the canonical path starts with the
configured FTP root. Replace all `sprintf()` with `snprintf()`.

---

### HIGH

#### 6. Integer overflow in allocation size

**File:** httpnenv.c:9-11

```c
int namelen = strlen(name);
int vallen  = value ? strlen(value) : 0;
HTTPV *v    = calloc(1, sizeof(HTTPV) + namelen + vallen);
```

If `namelen + vallen + sizeof(HTTPV)` overflows an `int`, `calloc()`
allocates a smaller buffer than expected. The subsequent `strcpy()`
calls at lines 14-17 write beyond the allocation.

**Fix:** Use `size_t` for lengths. Add an overflow check before the
addition. Add `+2` for the two null terminators.

---

#### 7. Missing return value in parse_cookies

**File:** httpshen.c:46

```c
static int parse_cookies(HTTPC *httpc, const UCHAR *in) {
    /* ... processing ... */
    free(buf);
    /* no return statement */
}
```

Function declared as returning `int` but has no return statement.
Undefined behavior per C89 — the caller receives whatever value
happens to be in the return register.

**Fix:** Add `return 0;` (or appropriate error code).

---

#### 8. Race condition in client array management

**File:** httpd.c:670-704

```c
httpc = calloc(1, sizeof(HTTPC));
/* ... initialize fields ... */
array_add(&httpd->httpc, httpc);  /* no lock! */
```

The client is added to the shared array without holding a lock.
Worker threads iterating the array could see a partially initialized
HTTPC structure.

**Fix:** Wrap the `array_add()` in `lock(httpd, 0)` / `unlock(httpd, 0)`.

---

### MEDIUM

#### 9. No Content-Length validation for POST

**File:** httppars.c:144-156

```c
memset(httpc->buf, 0, CBUFSIZE);
buf = httpc->buf;
len = CBUFSIZE - 1;
rc = recv(httpc->socket, buf, len, 0);
```

POST data is read without checking the Content-Length header. A large
payload fills the buffer but excess data remains on the socket,
corrupting subsequent keep-alive requests.

**Fix:** Parse Content-Length, reject payloads exceeding CBUFSIZE with
HTTP 413 Payload Too Large.

---

#### 10. strtok() in SSI processing (not thread-safe)

**File:** httpfile.c:281-285

```c
p = strtok(p, " ");
next = strtok(NULL, "");
```

`strtok()` uses internal static state. While HTTPD worker threads each
process their own client, recursive SSI includes (up to 10 levels)
could interfere with each other's `strtok()` state within the same
thread.

**Fix:** Use manual parsing or implement a reentrant tokenizer.
(Note: `strtok_r()` may not be available in crent370.)

---

#### 11. No limit on query/POST parameter count

**File:** httppars.c:52-66

Each query parameter is stored via `array_add()` to the environment.
An attacker can send thousands of parameters (`?a=1&b=2&c=3...`),
exhausting memory.

**Fix:** Define a maximum parameter count (e.g., 256) and reject
requests exceeding it.

---

#### 12. FTP sprintf overflow in path construction

**File:** ftpfqn.c (multiple locations: lines 35, 52, 62, 73, 89, 108,
112, 120, 124, 140, 150, 153)

Multiple `sprintf()` calls combine working directory and user input
into fixed-size buffers without bounds checking.

```c
sprintf(buf, "%s%s", ftpc->cwd, in);
sprintf(buf, "%s.%s", ftpc->cwd, in);
sprintf(out, "%s/%s", ftpc->ufs->cwd->path, buf);
```

**Fix:** Replace all with `snprintf()` using the destination buffer size.

---

### LOW

#### 13. Version string overflow (latent)

**File:** httppars.c:91

```c
UCHAR tmp[80];
sprintf(tmp, "HTTPD/%s", httpc->httpd->version);
```

Currently safe (version = "3.3.1-dev"), but if the version string
grows beyond ~74 characters, the buffer overflows.

**Fix:** `snprintf(tmp, sizeof(tmp), ...)`.

---

#### 14. Missing NULL check in SSI echo

**File:** httpfile.c:384-390

```c
p = http_get_env(httpc, var);
if (!p) p = getenv(var);
```

If `var` is NULL (from a parsing error), both `http_get_env()` and
`getenv()` receive NULL, leading to undefined behavior.

**Fix:** Add `if (!var || !*var) goto quit;` before the lookup.

---

## Security Architecture

### Authentication Flow

```
Browser                    HTTPD                      RACF
  |                          |                          |
  |-- GET /protected ------->|                          |
  |<-- 302 /login -----------|                          |
  |-- GET /login ----------->|                          |
  |<-- HTML login form ------|                          |
  |-- POST /login ---------->|                          |
  |  (userid, password)      |-- racf_login() -------->|
  |                          |<-- ACEE (success) ------|
  |                          |  encrypt(CREDID)         |
  |                          |  token = SHA256(CREDID)  |
  |                          |  store CRED in WSA array |
  |<-- 303 + Set-Cookie -----|                          |
  |  Sec-Token=base64(token) |                          |
  |-- GET /protected ------->|                          |
  |  Cookie: Sec-Token=...   |  lookup token in array   |
  |                          |  verify IP binding       |
  |<-- 200 response ---------|                          |
```

### Core Components

| Component | Purpose | File |
|-----------|---------|------|
| CREDID | userid + password + client IP (24 bytes) | credentials/include/cred.h:38-46 |
| CREDTOK | SHA256 hash of encrypted CREDID (32 bytes) | credentials/include/cred.h:48-51 |
| CRED | Complete credential: CREDID + CREDTOK + ACEE + flags (80 bytes) | credentials/include/cred.h:54-66 |
| cred_login() | RACF validation, encrypt, token gen, store | credentials/src/credlin.c |
| cred_find_by_token() | Session lookup by token hash | credentials/src/credfbtk.c |
| credtok_logout() | Remove session from array | credentials/src/credtokl.c |
| credid_enc/dec() | Blowfish encryption of CREDID | credentials/src/crediden.c, credidde.c |

### Security Properties

**Strengths:**
- RACF integration for OS-level authentication (not a custom password store)
- Tokens are SHA256 hashes, not plaintext credentials
- In-memory encryption of stored credentials (Blowfish)
- IP address binding: `cred->id.addr == httpc->addr` (httppc.c:167)
- Sensitive buffers wiped after use (memset to 0, credlin.c:91)
- APF authorization check before RACF calls (httpauth.c:16)

**Weaknesses:**
- **No TLS/HTTPS** — passwords sent in plaintext during login POST
- **No HttpOnly/Secure/SameSite cookie flags** on Sec-Token
- **No session timeout** — tokens live until logout or job ends
- **No login rate limiting** — brute force possible
- **No token refresh** mechanism
- **Blowfish** is dated (64-bit block) — adequate for temporary memory storage but not modern standard

### Authorization Model

Current authorization is **binary**: a request either requires login or it does not.

**Configuration levels** (httpd.h:105-110, httpconf.c:828-879):

```lua
httpd.login = "ALL"         -- require login for all requests
httpd.login = "CGI"         -- login for CGI modules only
httpd.login = "CGI,POST"    -- login for CGI and POST
httpd.login = "NONE"        -- no login required
```

**Per-CGI flag** (httpd.h:269): Each CGI registration carries a `login` flag.

**Enforcement** (httppc.c:151-222):
1. Check per-CGI `login` flag
2. Check per-method flags (GET, HEAD, POST)
3. Exempt `/login`, `/logout`, `/favicon.*`
4. Validate session: token lookup + IP match

**Limitations for mvsmf:**
- No per-endpoint authorization (all authenticated users get same access)
- No role-based access control
- No RACF resource class checking at the httpd level
- CGI modules must implement their own authorization (mvsmf does this via `racf_login()` in authmw.c)

### Extending for mvsmf: What Would Be Needed

For mvsmf to **inherit** httpd's security and add fine-grained permissions:

1. **Expose authenticated identity to CGI** — currently `httpc->cred` is available but opaque. CGI can access `cred->acee` (RACF ACEE) for resource checks.

2. **Add RACF resource authorization helper** to httpx function vector:
   ```c
   /* proposed addition to HTTPX */
   int (*http_check_auth)(HTTPC *, const char *class, const char *resource, int attr);
   ```
   This would let CGI modules check RACF resource access without reimplementing auth.

3. **Per-route authorization in httpd** (if router is added):
   ```c
   add_route(router, GET, "/zosmf/restfiles/ds/*", dsHandler);
   set_route_auth(route, "FACILITY", "ZOSMF.RESTFILES.READ");
   ```

4. **Token inheritance**: mvsmf currently does its own `racf_login()` in authmw.c using Basic Auth. If httpd's session were passed through, mvsmf could skip re-authentication for cookie-based requests.

**Current mvsmf approach** (authmw.c:12-72):
- Requires HTTP Basic Auth on every request
- Calls `racf_login()` independently of httpd sessions
- Creates its own ACEE per request
- No session reuse — every API call re-authenticates

**Better approach**: Check `httpc->cred` first. If httpd already authenticated the user (via cookie), use that ACEE. Fall back to Basic Auth only if no session exists.

---

## Architecture & Maintainability

### Module Structure: Extreme Fragmentation

The codebase contains **143 .c files** (80 HTTP, 21 FTP, 12 credentials, 30 utility/debug).
Many files contain a single function of 15-30 lines:

| Files | Lines each | Purpose |
|-------|-----------|---------|
| httpcmp.c, httpcmpn.c | ~32 | String comparison (2 variants) |
| httpfenv.c, httpnenv.c, httpdenv.c, httpsenv.c | 15-25 | Env var CRUD (4 files for 1 concept) |
| httpisbuy.c, httpsbz.c, httprbz.c | ~20 | Busy state is/set/reset |
| httpdsl.c, httpdsld.c, httpdsle.c, httpdsli.c, httpdslp.c, httpdslx.c | varies | SSI handlers (6 files) |
| dbgenter.c, dbgexit.c, dbgdump.c, dbgf.c, dbgs.c, dbgw.c, dbgtime.c | ~20 | Debug output (7 files) |

**Impact:** Hard to navigate, hard to refactor. Related logic is scattered across many tiny files.

**Recommended consolidation** (reduces ~40 files):

| Current (files) | Proposed | Saves |
|-----------------|----------|-------|
| httpcmp.c + httpcmpn.c | httpstr.c | 1 file |
| httpfenv.c + httpnenv.c + httpdenv.c + httpsenv.c + httpshen.c + httpsqen.c | httpenv.c | 5 files |
| httpisbuy.c + httpsbz.c + httprbz.c | httpbusy.c | 2 files |
| httpdsl*.c (6 files) | httpssi.c | 5 files |
| dbg*.c (7 files) | dbg.c | 6 files |
| ftpdnoop.c, ftpdquit.c, ftpdsyst.c, ftpdtype.c, ftpdprot.c, ftpdauth.c | ftpdmisc.c | 5 files |

**Note:** This is a build system change (Makefile/project.toml source list), not an ABI change. CGI modules are unaffected.

### Core Data Structures

**HTTPD** (160 bytes, httpd.h:84-153) — Server state:

Good: compact, well-commented with byte offsets.
Issue: Mixed concerns — HTTP config, FTP daemon, UFS, Lua, MQTT telemetry all in one struct. FTP and MQTT should be optional (conditional initialization, NULL when disabled).

**HTTPC** (4096 bytes, httpd.h:216-255) — Per-client session:

Good: Fixed size enables simple allocation, buffer embedded in struct avoids extra malloc.
Issue: 88 bytes of header + 4008-byte buffer. Several `unused` fields (waste). UFS pointers (`ufs`, `ufp`) present even when UFS is not used.

**HTTPX** (272 bytes, httpd.h:293-427) — Function vector for CGI:

Good: Excellent design for CGI decoupling without dynamic linking.
Issue: 68 function pointers — all CGI modules pay for the full vector even if they use 5 functions. Consider a minimal "core CGI" vector (20 functions) with optional extension vectors.

### Error Handling: Inconsistent

Three patterns coexist, sometimes in the same file:

1. **goto-cleanup** (httpin.c, httppars.c) — good for resource cleanup
2. **Log and continue** (httpconf.c:95) — silent failures in config
3. **Direct state transition** (httppc.c) — sets client state on error

**Issue:** No unified error code enum. Functions return int but meaning varies (0=ok, <0=socket error, 1=parse error). Callers cannot distinguish error types.

**Recommendation:** Define `enum httpd_rc` in errors.h. Adopt goto-cleanup consistently.

### Threading Model

Clean design:
- **Socket thread**: `select()` loop accepting HTTP/FTP connections
- **Worker pool** (CTHDMGR): 3-9 threads, configurable
- **Telemetry thread** (optional): MQTT publishing
- **ABEND isolation**: Worker crash kills only that thread, not the server

Issues:
- No per-thread error recovery or health metrics
- HTTP and FTP share the worker pool — FTP transfer can starve HTTP
- No queue depth monitoring

### Configuration (Lua-based, httpconf.c)

Strengths: Lua allows expressive, dynamic configuration.
Issues:
- No validation of config values after Lua returns (port range, thread limits, etc.)
- Error in Lua script logged but processing continues (silent misconfiguration)
- Dataset-vs-member heuristic fragile (if member name contains `.`)

### Coupling

```
HTTPD (core)
  +-- FTPD (FTP daemon) .......... tightly coupled, shares worker pool
  +-- UFSSYS (filesystem) ........ required for file serving, embedded in HTTPC
  +-- LUAX (Lua engine) .......... required for config + CGI, owns lua_State
  +-- HTTPT (MQTT telemetry) ..... optional but statically linked
  +-- CRED (credentials) ......... well-isolated, separate subsystem
```

FTP coupling is the biggest concern: FTP bugs can crash the server; deploying HTTP-only requires building the full binary anyway.

---

## Memory & Optimization

### Memory Footprint

| Component | Size | Notes |
|-----------|------|-------|
| HTTPD (server) | 160 bytes | Single instance |
| HTTPX (function vector) | 272 bytes | Single instance |
| HTTPC (per client) | 4,096 bytes | Includes 4,008-byte I/O buffer |
| Env vars (per request) | ~3,200 bytes | ~50 vars x avg 64 bytes, heap-allocated |
| Thread manager | ~4 KB | CTHDMGR overhead |
| CGI array | ~400 bytes | ~20 entries |
| **Total baseline** | **~5 KB** | Without clients |
| **Per concurrent client** | **~7.5 KB** | HTTPC + env vars + UFS |
| **3 clients on 16 MB** | **~28 KB** | 0.17% of total memory |

Assessment: Memory footprint is reasonable for the target platform.

### Missing NULL Checks After Allocation

Six locations call `strdup()` or `calloc()` without checking the return value.
On a 24-bit system with limited memory, these will silently produce NULL
pointers that crash later.

| File | Line | Call | Fix |
|------|------|------|-----|
| httpconf.c | 181 | `httpt->broker_host = strdup(p)` | Add NULL check |
| httpconf.c | 186 | `httpt->broker_port = strdup(p)` | Add NULL check |
| httpconf.c | 975 | `httpd->st_dataset = strdup(p)` | Add NULL check |
| httpconf.c | 982 | `httpd->cgilua_dataset = strdup(p)` | Add NULL check |
| httpconf.c | 989 | `httpd->cgilua_path = strdup(p)` | Add NULL check |
| httpconf.c | 996 | `httpd->cgilua_cpath = strdup(p)` | Add NULL check |
| httpacgi.c | 23-24 | `cgi->path = strdup(path)` / `cgi->pgm = strdup(pgm)` | Add NULL check |
| httppubf.c | 292-294 | Two consecutive `strdup()` — if second fails, first leaks | Add error handling between calls |

### Partial Memory Leak: Telemetry Cache

**File:** httppubf.c:292-294

```c
httptc->topic = strdup(topic);   /* line 292 */
httptc->data_len = strlen(data);
httptc->data = strdup(data);     /* line 294 */
```

If the second `strdup()` fails, `httptc->topic` is leaked. No error
handling between the two allocations.

### Cleanup Patterns: Good

The codebase handles cleanup well in the main request path:

- **httprese.c:16-36**: Frees all env vars, clears buffer per request
- **httpclos.c:25-42**: Closes file handles, UFS, socket, frees HTTPC
- **httpd.c:298-410**: Server shutdown frees all clients, CGI array, telemetry

No critical leaks found in the hot path (request processing loop).

### Environment Variable Lookup: O(n) Linear Search

`http_get_env()` (httpfenv.c) does a linear scan of the env array on every
lookup. With 50+ variables per request (headers + query + cookies), this
is the main per-request CPU cost.

**Optimization options:**
- Hash table (DJB2 or FNV hash) — fastest, ~10 lines of code
- Sorted array + binary search — no extra memory
- LRU cache for last-accessed variable — simplest change

### Buffer Memset Overhead

httppars.c calls `memset(httpc->buf, 0, CBUFSIZE)` (4008 bytes) three times
during parsing. Consider zeroing only the used portion (`memset(buf, 0, len+1)`).

### Allocation in Hot Path

Each HTTP request allocates 10-20 small heap chunks for environment variables
(one `calloc` per variable via `httpnenv()`). These are freed in `httprese()`.

For high-traffic scenarios, a pre-allocated env var pool (arena allocator)
would reduce heap fragmentation and allocation overhead. For current load
levels (3-9 concurrent clients), this is acceptable.

---

## Router Comparison: httpd vs mvsmf

### httpd: Flat CGI Dispatch

**Request dispatch** (httppc.c:42-122):

```
Request → parse method → http_find_cgi(path) → linear scan of CGI array
                                                  → __patmat() glob matching
                                                  → LINK SVC to load module
```

**CGI registration** (httpconf.c:285-320, via Lua config):
```lua
cgi = {
    MVSMF    = "/zosmf/*",
    HTTPLUA  = "/lua/(.*)",
    HTTPREXX = "/rexx/(.*)",
}
```

Each entry becomes an HTTPCGI struct: `{ path, program, login, wildcard }`.
Matching uses `__patmat()` (glob-style: `*`, `?`). No method dispatch, no
path parameter extraction.

### mvsmf: Method+Pattern Router with Middleware

**Router** (router.h, router.c):

```c
struct router {
    Route routes[MAX_ROUTES];           /* 100 max */
    Middleware middlewares[MAX_ROUTES];  /* 10 max */
};

struct route {
    HttpMethod method;      /* GET, POST, PUT, DELETE, ... */
    const char *pattern;    /* "/zosmf/restfiles/ds/{dataset-name}" */
    RouteHandler handler;   /* function pointer */
};
```

**Route registration** (mvsmf.c:63-88):
```c
add_route(&router, GET,    "/zosmf/restfiles/ds/{dataset-name}", datasetGetHandler);
add_route(&router, PUT,    "/zosmf/restfiles/ds/{dataset-name}", datasetPutHandler);
add_route(&router, DELETE, "/zosmf/restfiles/ds/{dataset-name}", datasetDeleteHandler);
```

**Middleware chain** (mvsmf.c:56-60):
```c
init_router(&router);   /* adds RouteMatching + PathVars middlewares */
add_middleware(&router, "Authentication", authentication_middleware);
```

Execution: all middleware runs in order, then matched route handler.

**Path parameter extraction** (router.c:250-302):
Pattern `{name}` segments are extracted and set as `HTTP_name` env vars.
E.g., `/zosmf/restfiles/ds/SYS1.PARMLIB` with pattern `{dataset-name}`
sets `HTTP_dataset-name=SYS1.PARMLIB`.

### Comparison

| Aspect | httpd | mvsmf Router |
|--------|-------|-------------|
| Method dispatch | State machine (httppc.c) | Per-route method enum |
| Pattern matching | `__patmat()` glob | Custom `{param}` patterns |
| Path parameters | Not supported | Extracted to env vars |
| Middleware | None | Chain (auth, logging, ...) |
| Handler type | External module (LINK SVC) | Function pointer (in-process) |
| Registration | Lua config at startup | Hardcoded `add_route()` calls |
| Max routes | Unlimited (dynamic array) | 100 (static array) |
| Performance | ~50ms LINK SVC per dispatch | Direct function call (~10us) |

### Could httpd Benefit from a Built-In Router?

**Yes, but as an optional layer — not replacing CGI dispatch.**

A router in httpd would:
1. Enable **method-based routing** (same path, different handler per method)
2. Enable **path parameter extraction** (no more manual parsing in CGI)
3. Enable **middleware** (auth, logging, CORS as shared infrastructure)
4. Allow CGI modules to register routes **programmatically** instead of only via Lua config
5. Reduce LINK SVC overhead for frequently-called endpoints

**Proposed design:**
```c
/* New: route registration API (exported via HTTPX) */
typedef int (*HTTPROUTE_HANDLER)(HTTPC *);

int http_add_route(HTTPD *, int method, const char *pattern,
                   HTTPROUTE_HANDLER handler, int flags);

int http_add_middleware(HTTPD *, const char *name,
                       int (*handler)(HTTPC *), int phase);

/* phases: MW_BEFORE_DISPATCH, MW_AFTER_DISPATCH */
```

CGI modules registering routes at startup:
```c
/* In mvsmf cgxstart or main: */
http_add_route(httpd, HTTP_GET, "/zosmf/restfiles/ds/{name}",
               datasetGetHandler, ROUTE_LOGIN);
http_add_route(httpd, HTTP_PUT, "/zosmf/restfiles/ds/{name}",
               datasetPutHandler, ROUTE_LOGIN);
```

**This would NOT eliminate mvsmf's router immediately** — mvsmf has 26 routes
with complex path patterns including parenthesized groups like
`/ds/{dataset-name}({member-name})`. The httpd router would need to support
these patterns first.

**Migration path:**
1. Add basic router to httpd (method + `{param}` patterns)
2. Extend with mvsmf's pattern syntax (parenthesized groups)
3. Add middleware support (auth, logging)
4. mvsmf registers routes via httpd router instead of internal router
5. Remove mvsmf's router.c

### Issues in mvsmf's Current Router

Several issues worth noting for any future integration:

1. **`//` comments** throughout router.c — violates C89 (must use `/* */`)
2. **`__attribute__((aligned(...)))`** on structs — GCC extension, may not work with c2asm370
3. **Designated initializers** in mvsmf.c:32-33 (`.routes = 0`) — C99, not C89
4. **`const char *` in pattern** — route patterns reference string literals, but EBCDIC conversion not handled at registration time
5. **`sprintf` in extract_path_vars** (router.c:288) — same buffer overflow risk as httpd
6. **Route matching middleware stubs** (router.c:178-196) — registered but do nothing (dead code)
7. **MAX_ROUTES = 100 as static array** — wastes ~3.2 KB even with 26 routes

---

## Remediation Roadmap

### Phase 1: Critical Security Fixes (immediate)

| # | Task | Files | Effort |
|---|------|-------|--------|
| 1 | Replace all `sprintf` with `snprintf` | httpshen.c, httpsqen.c, httppars.c, httpprtv.c, ftpfqn.c, httppars.c:91 | 1 day |
| 2 | Integer overflow check in httpnenv.c | httpnenv.c | 1 hour |
| 3 | Path validation in SSI include + FTP | httpfile.c, ftpfqn.c | 1 day |
| 4 | Locking around client array | httpd.c | 2 hours |
| 5 | Content-Length validation for POST | httppars.c | 4 hours |
| 6 | Add NULL checks after all strdup/calloc | httpconf.c, httpacgi.c, httppubf.c | 4 hours |
| 7 | Fix missing return in parse_cookies | httpshen.c | 5 minutes |
| 8 | Fix telemetry cache partial leak | httppubf.c | 30 minutes |

### Phase 2: Code Quality (1-2 weeks)

| # | Task | Impact |
|---|------|--------|
| 9 | Consolidate small utility files (~40 files) | Maintainability |
| 10 | Define error code enum in errors.h | Consistency |
| 11 | Add config validation after Lua parsing | Reliability |
| 12 | Add parameter count limit (256 max) | DoS prevention |
| 13 | Replace strtok() with manual parsing in SSI | Thread safety |

### Phase 3: Security Enhancements (2-4 weeks)

| # | Task | Impact |
|---|------|--------|
| 14 | Add HttpOnly/Secure/SameSite to Sec-Token cookie | Session security |
| 15 | Add session timeout/expiry | Token lifecycle |
| 16 | Add login rate limiting | Brute force prevention |
| 17 | Add RACF resource auth helper to HTTPX | CGI authorization |
| 18 | Allow mvsmf to reuse httpd sessions (skip re-auth) | Performance, UX |

### Phase 4: Architecture (4-8 weeks)

| # | Task | Impact |
|---|------|--------|
| 19 | Add basic router to httpd (method + {param}) | Extensibility |
| 20 | Add middleware support | Shared auth/logging |
| 21 | Make FTP optional (conditional init) | Deployment flexibility |
| 22 | Optimize env var lookup (hash table) | Performance |
| 23 | Add basic test infrastructure | Quality assurance |

---

## Methodology

Review covered all 152 source files in `src/` and `credentials/src/`,
plus the mvsmf router implementation (`router.h`, `router.c`, `authmw.c`,
`logmw.c`, `mvsmf.c`) for comparative analysis.

Focus areas: buffer handling, user-controlled input paths, memory
allocation/deallocation, concurrency, authentication/authorization,
module organization, data structure design, and cross-project patterns.
