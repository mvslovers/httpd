# HTTPD Server — Security Code Review

Date: 2026-02-28

## Summary

| Severity | Count |
|----------|-------|
| CRITICAL | 5 |
| HIGH     | 3 |
| MEDIUM   | 4 |
| LOW      | 2 |

---

## CRITICAL

### 1. Buffer overflow in HTTP header/query environment variable names

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

### 2. Unbounded vsprintf in HTTP response output

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

### 3. Unprotected strcat in query/POST string parsing

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

### 4. Path traversal in SSI include handler

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

### 5. FTP path traversal

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

## HIGH

### 6. Integer overflow in allocation size

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

### 7. Missing return value in parse_cookies

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

### 8. Race condition in client array management

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

## MEDIUM

### 9. No Content-Length validation for POST

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

### 10. strtok() in SSI processing (not thread-safe)

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

### 11. No limit on query/POST parameter count

**File:** httppars.c:52-66

Each query parameter is stored via `array_add()` to the environment.
An attacker can send thousands of parameters (`?a=1&b=2&c=3...`),
exhausting memory.

**Fix:** Define a maximum parameter count (e.g., 256) and reject
requests exceeding it.

---

### 12. FTP sprintf overflow in path construction

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

## LOW

### 13. Version string overflow (latent)

**File:** httppars.c:91

```c
UCHAR tmp[80];
sprintf(tmp, "HTTPD/%s", httpc->httpd->version);
```

Currently safe (version = "3.3.1-dev"), but if the version string
grows beyond ~74 characters, the buffer overflows.

**Fix:** `snprintf(tmp, sizeof(tmp), ...)`.

---

### 14. Missing NULL check in SSI echo

**File:** httpfile.c:384-390

```c
p = http_get_env(httpc, var);
if (!p) p = getenv(var);
```

If `var` is NULL (from a parsing error), both `http_get_env()` and
`getenv()` receive NULL, leading to undefined behavior.

**Fix:** Add `if (!var || !*var) goto quit;` before the lookup.

---

## Remediation Priority

1. **Immediate:** All `sprintf` -> `snprintf`, `vsprintf` -> `vsnprintf`
   (findings 1, 2, 3, 5, 12, 13). This is the largest attack surface.
2. **Immediate:** Integer overflow check in httpnenv.c (finding 6).
3. **High:** Path validation in SSI include and FTP (findings 4, 5).
4. **High:** Locking around client array management (finding 8).
5. **High:** Content-Length validation for POST (finding 9).
6. **Medium:** Parameter count limits (finding 11).
7. **Medium:** Fix missing return in parse_cookies (finding 7).
8. **Low:** SSI strtok and NULL checks (findings 10, 14).

## Methodology

Review covered all 152 source files in `src/` and `credentials/src/`.
Focus areas: buffer handling, user-controlled input paths, memory
allocation, concurrency, and encoding boundaries.
