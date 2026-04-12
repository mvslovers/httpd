# HTTPD — Open Code Review Findings

Date: 2026-04-12
Consolidated from: `code-review-2026-03-10.md` and `code-review-2026-04-11.md`

## Overview

Both reviews have been merged and resolved findings removed. **13 open findings** remain:

- 6 × P1 (Remote exploitable — fix immediately)
- 4 × P2 (Security relevant — fix soon)
- 3 × P3 (Hardening / defense-in-depth)

### Quick Wins (Top 5)

These five findings are XS/S effort and should be fixed first:

| # | Finding | File | Fix |
|---|---------|------|-----|
| F-01 | Stack overflow: directory URI + `index.html` into `buf[300]` | httpget.c | Bounds check before `memcpy`/`strcpy` |
| F-07 | Percent decoder reads past buffer on trailing `%` | httpdeco.c | Check `str[1]`/`str[2]` exist before access |
| F-08 | Unbounded `vsprintf` in SSI output path | httpfile.c | Replace with `vsnprintf` |
| F-02 | Stack overflow + XSS via cookie value in `sprintf buf[512]` | httpcred.c | `snprintf` + HTML-escape `uri` |
| F-03 | Header injection via base64-decoded redirect URI | httpcred.c | NUL-terminate + reject CR/LF + allow only relative paths |

## Previously Resolved (for reference)

| Finding | Source | Resolved by |
|---------|--------|-------------|
| `sprintf` → `snprintf` in header parsing (httpshen, httpsqen) | Old #1 | GH#2 / TSK-108 |
| `vsprintf` in httpprtv.c | Old #2 | GH#3 |
| FTP path traversal + sprintf overflows | Old #5, #12 | TSK-2 (FTPD removed) |
| Integer overflow in httpnenv.c | Old #6 | GH#7 |
| Missing return in parse_cookies() | Old #7 | GH#8 |
| Race condition in client array_add | Old #8 | GH#9 |
| `strtok` in 5 files (httpdbug, httpjes2, httpshen, httpfile) | Old #10 | GH#11 |
| Version string sprintf overflow | Old #13 | GH#11 |
| Lua config removed | Old #11 | TSK-1/5 |

---

## P1 — Remote Exploitable (fix immediately)

### F-01: Stack overflow in httpget.c — directory URI + index.html

**File:** `src/httpget.c:46-56`

`REQUEST_PATH` is client-controlled and can approach `CBUFSIZE` in length. `httpget()` copies the path into `buf[300]` and appends `index.html` or `default.html` without bounds checking. A long directory URI overwrites the stack before the file is even opened.

```c
len = strlen(path);
if (path[len-1]=='/') {
    memcpy(buf, path, len);
    strcpy(&buf[len], "index.html");   /* no bounds check */
}
```

**Impact:** Remote stack overflow

**Fix:** Check path length before copy: `if (len + 13 > sizeof(buf)) { http_resp(httpc, 414); return; }`. Or use `snprintf()`.

---

### F-02: Stack overflow + XSS in httpcred.c — cookie formatted into buf[512]

**File:** `src/httpcred.c:253-255`

`Sec-Uri` originates from a request cookie. `print_body()` formats the value via `sprintf()` into `buf[512]`. A long cookie overwrites the stack. Additionally, the value is written unescaped into an HTML attribute, enabling reflected markup/XSS injection.

```c
for(i=0; body3[i]; i++) {
    sprintf(buf, body3[i], uri);       /* no bounds check */
    if (rc=http_printf(httpc, " %s\n", buf)) goto quit;
}
```

**Impact:** Remote stack overflow + HTML/attribute injection

**Fix:**
- Replace `sprintf` with `snprintf`
- HTML-escape `uri` before output (replace `< > " & '`)
- Enforce server-side cookie length limit

---

### F-03: Header injection in httpcred.c — base64 decode → Location redirect

**File:** `src/httpcred.c:392-435`

`base64_decode()` can return arbitrary byte sequences. `strncpy()` into `uribuf[256]` does not guarantee NUL termination. The value is then used unfiltered in the `Location:` header, enabling CR/LF-based header injection and response splitting.

```c
buf = base64_decode(uri, strlen(uri), &len);
if (buf) {
    strncpy(uribuf, buf, sizeof(uribuf));  /* no NUL termination */
    free(buf);
    uri = uribuf;
}
/* ... */
http_printf(httpc, "Location: %s\r\n", uri);  /* CRLF injection possible */
```

**Impact:** Header injection, response splitting, open redirect

**Fix:**
- Always terminate after `strncpy`: `uribuf[sizeof(uribuf)-1] = 0;`
- Reject `\r` and `\n` in decoded value
- Allow only relative paths (`/...`) as redirect targets

---

### F-04: Non-blocking send busy-loop in httpsend.c

**File:** `src/httpsend.c:14-70`

Accepted sockets are set to non-blocking. `send_raw()` breaks on `EWOULDBLOCK` and returns the number of bytes sent so far, which can be 0. Callers like `httpprtv()` increment `pos` by this return value in a loop. When the peer is blocked, the worker spins indefinitely. In chunked mode, partial writes can corrupt the chunk framing (header, body, and trailer treated as fully sent when they are not).

```c
for(pos=0; pos < len; pos+=rc) {
    rc = send(httpc->socket, &buf[pos], len-pos, 0);
    if (rc>0) { httpc->sent += rc; }
    else {
        if (errno == EWOULDBLOCK) break;  /* pos+=0 → infinite loop */
    }
}
```

**Impact:** Worker hangs in busy-loop, chunked responses corrupted under load

**Fix:**
- On `EWOULDBLOCK`: retry with `select()` wait or bounded retry count
- Or: define clean semantics — `rc=0` on EWOULDBLOCK, caller decides
- Handle chunk header/body/trailer atomically via state machine

---

### F-05: SSI path traversal — no `..` validation

**File:** `src/httpfile.c` (ssi_include)

The SSI include handler opens paths without verifying they are within the document root. Anyone who can place an HTML file with SSI directives on the server can read arbitrary files.

```html
<!--#include virtual="../../../../etc/passwd" -->
```

**Impact:** Arbitrary file read via SSI

**Fix:**
- After path resolution, verify the path is within DOCROOT
- Reject paths containing `..` sequences

---

### F-06: Stack overflow in httplua.c — strcpy into dataset[256]

**File:** `src/httplua.c:460-464` (now in `mvslovers/httplua` repo)

`REQUEST_PATH` is copied via `strcpy()` into `dataset[256]`. The request path can be significantly longer.

```c
script = http_get_env(httpc, "REQUEST_PATH");
if (script) {
    strcpy(dataset, script);           /* no bounds check */
}
```

**Impact:** Remote stack overflow

**Fix:** `snprintf(dataset, sizeof(dataset), "%s", script)` or length check before copy.

**Note:** This finding affects the `mvslovers/httplua` repo, not HTTPD core.

---

## P2 — Security Relevant (fix soon)

### F-07: Percent decoder out-of-bounds read in httpdeco.c

**File:** `src/httpdeco.c:15-21`

`http_decode()` reads `str[1]` and `str[2]` before checking whether these bytes exist. A URI ending with `%` or `%x` causes an out-of-bounds read. Invalid escape sequences are silently consumed rather than rejected.

```c
case '%':
    temp[0] = str[1];             /* str[1] may not exist */
    temp[1] = str[2];             /* str[2] may not exist */
    temp[2] = 0;
    out[0] = asc2ebc[strtoul(temp, NULL, 16)];
    if (str[1] && str[2]) str+=2;
    break;
```

**Impact:** Out-of-bounds read

**Fix:** Check before access: `if (str[1] && str[2] && isxdigit(str[1]) && isxdigit(str[2]))`. Reject invalid escapes with 400.

---

### F-08: Unbounded vsprintf in SSI output path (httpfile.c)

**File:** `src/httpfile.c:591-605`

The SSI path formats output via `vsprintf()` into a 4096-byte stack buffer. SSI pages can output request data and environment variables, making this a classic overflow vector.

```c
char buf[4096];
len = vsprintf(buf, fmt, args);       /* unbounded */
```

**Impact:** Stack overflow in SSI output path

**Fix:** Replace with `vsnprintf(buf, sizeof(buf), fmt, args)`. Handle truncation or use chunked buffering.

---

### F-09: Missing POST Content-Length validation

**File:** `src/httppars.c:144-156`

POST data is read without checking the Content-Length header. A large payload fills the buffer, and excess data remains on the socket, corrupting subsequent keep-alive requests.

**Impact:** Keep-alive request poisoning, potential buffer overflow

**Fix:** Parse Content-Length, reject payloads exceeding CBUFSIZE with HTTP 413. Reject missing Content-Length as well (RFC 7230 §3.3.3).

---

### F-10: Latent strcat overflow risk in httppars.c

**File:** `src/httppars.c:50,169`

A bounds check `strlen(buf) < CBUFSIZE - 2` was added, but `strcat(buf, "&")` remains. The check protects against overflow, but the pattern is fragile — the guard can be accidentally removed during refactoring.

```c
if (strlen(buf) < CBUFSIZE - 2)
    strcat(buf, "&");
```

**Impact:** Latent overflow risk

**Fix:** Use `strncat()` or position-based write logic. Or rewrite the query parser to use explicit position tracking.

---

## P3 — Hardening / Defense-in-Depth

### F-11: strtok in httpin.c (3 locations)

**File:** `src/httpin.c:45,49,53`

The request line is parsed with `strtok()`. While each worker thread has its own buffer (so cross-thread interference is unlikely on MVS 3.8j), `strtok` is non-reentrant and the pattern is inconsistent with the rest of the codebase where it has been replaced.

```c
p = strtok(buf, " ");    /* METHOD */
p = strtok(NULL, " ");   /* URI */
p = strtok(NULL, " ");   /* VERSION */
```

**Fix:** Manual parsing with `strchr()` or pointer-based tokenizer (same pattern as GH#11 fixes).

---

### F-12: No limit on query/POST parameter count

**File:** `src/httppars.c:52-66`

Each query parameter is stored via `array_add()` to the environment list. An attacker can send thousands of parameters (`?a=1&b=2&c=3...`), exhausting memory.

**Impact:** DoS via memory exhaustion

**Fix:** Define a maximum (e.g. 256 parameters), reject requests exceeding it with 400.

---

### F-13: SSI echo — missing NULL check for var

**File:** `src/httpfile.c:384-390`

If `var` is NULL due to a parsing error, both `http_get_env()` and `getenv()` receive NULL, causing undefined behavior.

```c
p = http_get_env(httpc, var);
if (!p) p = getenv(var);
```

**Fix:** Add `if (!var || !*var) goto quit;` before the lookup.

---

## Recommended Fix Order

| Priority | Finding | Effort | Files |
|----------|---------|--------|-------|
| 1 | F-01: httpget.c stack overflow | XS | httpget.c |
| 2 | F-02: httpcred.c stack overflow + XSS | S | httpcred.c |
| 3 | F-03: httpcred.c header injection | S | httpcred.c |
| 4 | F-07: httpdeco.c percent decoder OOB | XS | httpdeco.c |
| 5 | F-08: httpfile.c vsprintf SSI | XS | httpfile.c |
| 6 | F-05: httpfile.c SSI path traversal | S | httpfile.c |
| 7 | F-04: httpsend.c non-blocking busy-loop | M | httpsend.c |
| 8 | F-09: httppars.c POST Content-Length | S | httppars.c |
| 9 | F-10: httppars.c strcat risk | XS | httppars.c |
| 10 | F-11: httpin.c strtok | XS | httpin.c |
| 11 | F-12: httppars.c parameter limit | XS | httppars.c |
| 12 | F-13: httpfile.c SSI NULL check | XS | httpfile.c |
| 13 | F-06: httplua.c strcpy (httplua repo) | XS | httplua/src/httplua.c |

## Methodology

Consolidation of findings from two static code reviews (2026-03-10 and 2026-04-11). Cross-referenced against the current codebase via `grep` and manual inspection. No runtime testing performed.
