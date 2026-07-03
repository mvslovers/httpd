# HTTPD Core — Refactoring Backlog (Performance · Security · Memory)

**Last consolidated:** 2026-06-30
**Supersedes:** `code-review-2026-03-10.md`, `code-review-2026-04-11.md`,
`code-review-open-findings.md`, `memory-audit-2026-06-30.md` (all merged here;
see *Provenance* at the end).

This is the single working backlog for the next performance, security and
memory refactorings of the **HTTPD core** (the HTTPD load module and the core
pipeline it links — input, parse, dispatch, file serving, response/send, env
vars, credentials, SMF, operator console, translation). The built-in **CGI
modules are out of scope** (HTTPDSRV/HTTPJES2/HTTPDSL/HTTPDM/HTTPDMTT, mvsMF).

## How to read this

Every finding below was **re-verified against the current `main` on
2026-06-30** — the older reviews predate the mbt v2 migration, the FTP/MQTT/Lua
removals, and the `CGI=`→`MOD=` and 204/304-chunked fixes, so a large share of
their findings are already fixed (see *Resolved / moved*). Findings carry a
**Status**:

- **Open** — present and unmitigated in current code (verified).
- **Partial** — partially mitigated; residual risk remains.
- **Latent** — the unsafe code exists but is not reachable by any current caller.
- **Resolved / Moved** — fixed, or relocated out of the HTTPD core.

**Severity:** Critical / High / Medium / Low. **Effort:** XS (<1h) … L (days).
Each finding cross-references its original ID (`F-xx` from the open-findings
list, `H/M/L` from the memory audit, `CRIT/HIGH#n` from the 2026-03 review).

Constraints that bound every recommendation (do not propose violations):
24-bit, EBCDIC (CP037), gnu99, no VLAs, no POSIX; **HTTPC is exactly 4096 bytes**
(fixed ABI); **HTTPX is append-only**; **per-byte `recv()` in
`http_getc`/`http_gets` is an intentional TCP/IP ring-buffer workaround — never
propose bulk `recv()`**.

---

## Executive summary

The per-request hot path is overwhelmingly stack-based and leak-free on the
happy path; the prior reviews' worst items (the `sprintf`/`vsprintf` family, the
integer overflow, the array-add race, FTP/Lua/MQTT attack surface) are **fixed
or removed**. What remains clusters into a few high-value refactors:

**Top priority — two remotely-reachable defects that compound into a DoS:**

1. **`S1` — remote stack overflow** in the static-file directory-index fallback
   (`httpget.c`), reachable by an unauthenticated GET in the default config.
   **✔ Resolved (PR #73 / issue #72)** — the copy is now bounded and `path`
   NULL/empty is guarded (S9b).
2. **`M1` — per-abend resource leak**: any abend in HTTPD *core* code skipped
   `http_close()`, leaking the 4 KB HTTPC, its socket, env and UFS handles, and
   leaving a stale pointer in `httpd->busy`.
   **✔ Resolved (PR #77 / issue #76)** — request processing now runs under
   `try()`; cleanup always runs and a caught abend logs `HTTPD062E`.

`S1` forced a core abend → `M1` leaked an HTTPC + socket on every hit → file
descriptors and worker slots exhaust → with `M5` the listener then dies. **The
whole chain is now closed:** `S1` (remote trigger, PR #73), `M1` (the leak +
every *other* core fault, PR #77), and `M5` (the listener no longer bails on a
transient error, PR #85).

**Also high-value:**
- ~~`S2`/`S3` — `httpcred` login/redirect path~~ **✔ Resolved (PR #79 / issue
  #78)** — cookie `sprintf`/XSS and the unterminated, unfiltered redirect are
  fixed via `http_html_escape()` + `http_safe_redirect()`.
- `M2` — the credential array has **no reaper** (unbounded CRED+ACEE growth);
  this is also the missing "session timeout" from the security backlog.
- `P1` — environment-variable lookup is **O(n) linear** on every access — a
  clear win for static-request throughput (for CGI paths the LINK SVC, `P7`,
  dominates; profile before investing).

**Counts (open/partial):** Security 4 · Memory & Stability 6 · Performance 7.
*(2026-07-02: S1 #73; M1 #77; S2+S3 #79; S5 #81; S6 #83; M3+M4+M5 #85; M8+M9+M10 #87; S4 already mitigated — corrected. 2026-07-03: S7 #89; S8 #91.)*

---

## A. Security

### S1 — Remote stack overflow: directory-index fallback `memcpy` into `buf[300]`
*Critical · Resolved (2026-07-02, PR #73 / issue #72) · Effort XS · `httpget.c:23,47-50` · was F-01 / audit H1*

> **Resolved:** the directory-index fallback now rejects candidates that would
> not fit the appended index filename (`if (len + sizeof("default.html") >
> sizeof(buf)) → 404`) *before* the `memcpy`, and `httpget()` guards
> `REQUEST_PATH` NULL/empty at the top (folding in **S9b**) before `http_cmp`
> and the `path[len-1]` index. No ABI change. Fix in PR #73 (issue #72);
> pending review/merge to `main`. Once merged the DoS chain's remote entry
> point is closed; **M1** remains open as the defence-in-depth containment
> layer for any *other* core fault.

```c
23   UCHAR  buf[300];
47   len = strlen(path);                 /* path = REQUEST_PATH, bounded by CBUFSIZE (4000) */
48   if (path[len-1]=='/') {
49       memcpy(buf, path, len);         /* up to ~4000 bytes into buf[300] */
50       strcpy(&buf[len], "index.html");
```
`REQUEST_PATH` is client-controlled and bounded only by `CBUFSIZE` (4000), not
300. **Reachability verified:** the only URI cap is the 414 gate in
`httpin.c:24-31`, which fires only when the whole request line exceeds
`CBUFSIZE-1` (~3999) — so a path of ~301–3980 chars ending in `/` passes 414,
survives parsing, and reaches this `memcpy`. The preceding `http_open` clamps
into its own `buf[256]`, fails, and falls through here with the full-length
path. **Unauthenticated** in the default config (`LOGIN NONE`).
**Fix:** reject over-long paths (or clamp) before the copy, e.g.
`if (len + sizeof("default.html") > sizeof(buf)) { http_resp_not_found(...); ... }`;
build the candidate into the size-checked buffer. Do **not** just enlarge `buf` —
the input is unbounded; bound it. (Also guard `path` NULL/empty — see S9b.)

### S2 — Stack overflow + reflected XSS: cookie value `sprintf` into `buf[512]`
*Critical · Resolved (2026-07-02, PR #79 / issue #78) · Effort S · `httpcred.c:237,253-255` · was F-02 / CRIT(2026-04)*

> **Resolved:** the fixed `buf[512]` + `sprintf` is gone. `print_body()` now
> HTML-escapes the `Sec-Uri` cookie with the new `http_html_escape()`
> (`src/httpesc.c`, escapes `& < > " '`, bounded + always terminated) and
> prints the hidden field directly — no unbounded copy into a stack buffer, no
> unescaped reflection. Verified natively (`TSTSAFE`, incl. a `"><script>`
> breakout).

```c
237  char buf[512];
253  for(i=0; body3[i]; i++) {
254      sprintf(buf, body3[i], uri);            /* uri = Sec-Uri cookie, unbounded */
255      if (rc=http_printf(httpc, " %s\n", buf)) goto quit;
```
`print_body()` formats the `Sec-Uri` cookie value into a fixed 512-byte stack
buffer with `sprintf` (overflow), and writes it **unescaped** into the login
form HTML (reflected markup/XSS).
**Fix:** `snprintf`; HTML-escape `uri` (`< > " & '`); enforce a server-side
cookie-length limit.

### S3 — Header injection / over-read: `Sec-Uri` redirect not terminated or filtered
*High · Resolved (2026-07-02, PR #79 / issue #78) · Effort S · `httpcred.c:393-401` (+ `Location:` use) · was F-03 / audit L10*

> **Resolved:** the `strncpy` (a *source* over-read — `base64_decode` returns
> binary and does not NUL-terminate, so it scanned past the decoded allocation)
> is replaced by a length-bounded `memcpy` of the reported decoded length +
> explicit terminate. Before it reaches `Location:`, the target is validated by
> the new `http_safe_redirect()` (`src/httpsrdr.c`): reject unless it is a
> single-leading-`/` site-local path with no `//`/`/\` and no CR/LF, else fall
> back to `/` — closing the header-injection and open-redirect. Edge-case table
> verified natively (`TSTSAFE`).

```c
393  buf = base64_decode(uri, strlen(uri), &len);
395  if (buf) { strncpy(uribuf, buf, sizeof(uribuf));   /* uribuf[256]: not NUL-terminated */
396             free(buf); ... uri = uribuf; }
...  http_printf(httpc, "Location: %s\r\n", uri);       /* CR/LF not rejected */
```
`strncpy` does not terminate a ≥256-byte decoded value (over-read when
`%s`-printed), and the decoded value reaches the `Location:` header without
CR/LF filtering (response splitting / open redirect).
**Fix:** `uribuf[sizeof(uribuf)-1] = 0;` after the copy; reject `\r`/`\n`; allow
only relative (`/…`) redirect targets. (Aligns with TSK-108.)

### S4 — SSI path traversal: `ssi_include` opens directive path unchecked
*High · Resolved — already mitigated (finding was inaccurate; no code change) · `httpfile.c:533` → `httpopen.c:17,50` · was F-05 / CRIT#4(2026-03)*

> **Not a live defect — the finding traced the caller but missed the callee's
> guard.** `ssi_include()`'s **only** file open is line 533 → `http_open()`, and
> `http_open` already: (1) rejects **any** `..` in the path — `httpopen.c:17`
> `if (strstr(path, "..")) goto quit;` (commit `0c171fe`, 2026-03-01) — and (2)
> prepends `DOCROOT` before opening — `httpopen.c:50`
> `snprintf(ufspath, "%s%s", dr, buf)` (commit `1c2ad3c`, 2026-03-16). Both
> predate the 2026-06-30 consolidation, so the "no `..` rejection / no docroot
> confinement" claim was wrong about the then-current code. Attack walk-through:
> `virtual="../../etc/passwd"` → `strstr` hits `..` → `goto quit` → NULL → SSI
> prints "Oops: we didn't find". There is **no** open path in `ssi_include` that
> bypasses `http_open`, so **no `ssi_include` code change is warranted** (a
> redundant reject would be defensive noise). *Separate, not S4:* with `DOCROOT`
> **unset** (`dr[0]==0`) there is no docroot confinement — still bounded to the
> UFS namespace and still `..`-free, so not a traversal; tracked as a config-
> hardening note under *Security architecture* below.

### S5 — Percent-decoder out-of-bounds read on trailing `%`
*Medium · Resolved (2026-07-02, PR #81 / issue #80) · Effort XS · `httpdeco.c:15-21` · was F-07 / audit L7*

> **Resolved:** the `str[1]`/`str[2]` reads moved **inside** the `if (str[1] &&
> str[2])` guard, so `&&` short-circuits and a trailing bare `%` never reads
> `str[2]` (one past the NUL). An incomplete escape at end-of-string (`%`, `%4`)
> is now passed through literally instead of decoding a partial value.
> `TSTDECO`'s trailing-`%` assertion was updated to the new contract (+ an
> incomplete-escape case).

```c
case '%':
    temp[0] = str[1];           /* str[1] may be the NUL */
    temp[1] = str[2];           /* reads one byte past the NUL */
```
A URI ending in `%` reads one byte past the string (worst case one past
`httpc->buf`); the value is discarded (benign read) but invalid escapes are also
silently accepted. **Fix:** decode only inside the existing `if (str[1] &&
str[2])` guard; optionally reject invalid escapes with 400.

### S6 — SSI output uses unbounded `vsprintf` into `buf[4096]`
*Medium · Resolved (2026-07-02, PR #83 / issue #82) · Effort XS · `httpfile.c:595,598` · was F-08 / audit M3*

> **Resolved:** `ssi_printv()` now uses `vsnprintf(buf, sizeof(buf), …)` and
> clamps the would-be length to `sizeof(buf)-1` before `ssi_buffer()` copies it
> (so an over-long SSI line — e.g. an attacker-controlled `HTTP_*` header echoed
> by `ssi_printenv`/`ssi_echo` — truncates instead of overflowing the stack).
> Mirrors the `http_printv()` pattern (`httpprtv.c:14-16`).

```c
595  char buf[4096];
598  len = vsprintf(buf, fmt, args);     /* not vsnprintf */
```
`ssi_printv` (every `ssi_printf`) is unbounded. `ssi_printenv` emits a row per
env var — including attacker-controlled `HTTP_*` headers (~4000 bytes) — and
`ssi_echo` echoes an env value; either can exceed 4096 → stack overflow.
**Fix:** `vsnprintf(buf, sizeof(buf), fmt, args)` + handle truncation.

### S7 — POST/PUT body: no `Content-Length` enforcement (413)
*Medium · Resolved (2026-07-03, PR #89 / issue #88) · Effort S · `httppars.c:140-215` · was F-09 / MED#9(2026-03)*

The keep-alive **poisoning** half was already mitigated (`keepalive=0` on an
unread body). The residual was: a blanket `recv(buf, CBUFSIZE-1)` with no
`Content-Length` parse, no `413`, and mishandling of a missing CL.

> **Resolved:** a new pure classifier `httpbody()` (`src/httpbody.c`, RFC 7230
> §3.3.3) maps `(Content-Length, Transfer-Encoding?) → {NONE, READ(n),
> TOO_LARGE, BAD}`. `httppars` now: **413** on an oversize CL (added the missing
> `413 Payload Too Large` to `http_resp`, ref TSK-110), **400** on a malformed
> CL, treats a missing CL / no TE as a **zero-length body** (RFC rule 6 — fixes
> the old blanket-`recv` → `EWOULDBLOCK` → RESET on a bodyless POST), and caps
> the single non-blocking `recv` at the declared length. Classifier dual-tested
> (`TSTBODY`, 16 assertions, run natively). **Deliberately out of scope** (kept
> tight): no "read exactly CL" loop (on a non-blocking socket that is itself a
> slowloris — needs `select`+`CLIENT_TIMEOUT`, see `P2`), no chunked
> request-body decode, no POST keep-alive enablement. **PUT** bodies are left
> for the CGI to read (`httppars.c:113-118`) — unchanged.

### S8 — No limit on query/POST parameter count (memory-exhaustion DoS)
*Medium · Resolved (2026-07-03, PR #91 / issue #90) · Effort XS · `httppars.c` query/POST loops · was F-12 / MED#11*

Each parameter was `array_add`'d to the env list with no cap; `?a=1&b=2&…`
(thousands) exhausts memory.

> **Resolved:** a single `nparam` counter caps the **combined** query + POST
> parameter count at `HTTP_MAX_PARAMS` (256); exceeding it → **400** via a shared
> `badrequest:` handler (onto which S7's malformed-`Content-Length` case was also
> folded, deduping the 400 emission).

### S9 — Operator `D M` / `D TI` with no argument abends the console thread
*Low · Open · Effort XS · `httpcons.c:326,377` · audit L4*

`display()` passes `rest = strtok(NULL,"")` (NULL when the verb has no argument)
into `strtoul(NULL,…)` / `strtol(NULL,…)` on the console thread (no `try()`).
**Fix:** `if (!buf) { wtof(usage); return 0; }` (as `s_login`/`s_stats` already
do).
**S9b (related to S1) — Resolved (PR #73):** `httpget.c:31,36,47` assumed `path`
was non-NULL/non-empty; `http_get_env` can return NULL → `http_cmp(NULL,…)`/
`strlen(NULL)`/`path[-1]`. Now guarded at the top of `httpget()` as part of the
S1 fix (respond 404 on NULL/empty `REQUEST_PATH`).

### S10 — Latent `strcat("&")` in query parsing
*Low · Open · Effort XS · `httppars.c:49,168` · was F-10*

Guarded today (`if (strlen(buf) < CBUFSIZE-2) strcat(buf,"&")`) but fragile.
**Fix:** position-based write or `strncat`.

### S11 — `strtok` in the request-line parser
*Low · Open · Effort XS · `httpin.c:45,49,51` · was F-11*

Non-reentrant and inconsistent with the rest of the codebase (other `strtok`
sites were replaced under GH#11). Safe today (per-worker buffer). **Fix:**
manual `strchr`-based tokenisation.

### S12 — SSI echo NULL/empty var guard *(mostly resolved)*
*Low · Partial · `httpfile.c:391` · was F-13*

An empty-variable guard is now present (`if (!*var) goto quit;`), and `var` is a
non-NULL pointer into the parse buffer, so the original NULL-deref is not
reachable. Optionally harden to `if (!var || !*var)` for symmetry.

### Security architecture backlog (design-level, cross-cutting)
*From the 2026-03 review §Security Architecture — still valid:*
- **No TLS** — login credentials are sent in plaintext POST.
- **No `HttpOnly`/`Secure`/`SameSite`** on the `Sec-Token` cookie.
- **No session timeout / token expiry** — see `M2` (no reaper); fixing `M2`
  delivers this.
- **No login rate-limiting** — brute force possible.
- **Blowfish** (64-bit block) for in-memory CREDID — adequate for transient
  storage, dated as a standard.
- **Binary authorization only** (login-or-not). For per-endpoint/RACF-resource
  authorization, see *Architecture* (HTTPX auth helper).
- **`DOCROOT` unset ⇒ no docroot confinement** (`httpopen.c:48-54`). With no
  `DOCROOT` configured, `http_open` opens the raw (still `..`-free) path in the
  UFS root — bounded to the UFS namespace but not to a subtree. Not the SSI
  traversal (S4), but a config-hardening gap: consider requiring `DOCROOT` (or
  defaulting to a safe subtree) rather than serving the UFS root.

---

## B. Memory & Stability

### M1 — Worker abend leaks HTTPC + socket + env + UFS, and corrupts `busy`
*High · Resolved (2026-07-02, PR #77 / issue #76) · Effort M · `httpd.c:591,606-612` · audit H2*

> **Resolved:** the per-request state loop is now factored into `serve_client()`
> and run under `try()` (ESTAE) in `worker_thread`. A core abend returns for
> cleanup instead of percolating past `http_close()`, so the HTTPC, socket, env
> and UFS handles are always freed; a caught abend logs `HTTPD062E` (no dump —
> we don't want an SVC dump per malicious `/abend`). **Ordering correction to
> the fix below:** `http_reset_busy(httpc)` must run **before** `http_close(httpc)`
> — `http_close` frees `httpc`, and `httprbz` reads `httpc->httpd` and matches
> pointer values in `busy`, so resetting *after* close is a use-after-free that
> can even evict a *different* client (whose new HTTPC reused the freed address)
> from `busy`. The reset is gated on the abend branch (the normal path already
> cleared `busy` at `httppc.c:149`). `abendrpt` stays as the worker-level net.
> The **S1 → M1 → M5** DoS leak-chain is now broken at the leak half.

```c
591  abendrpt(ESTAE_CREATE, DUMP_DEFAULT);      /* worker ESTAE: dumps + percolates */
606  while(httpc->state != CSTATE_CLOSE) { http_process_client(httpc); ... }  /* abend here */
612  http_close(httpc);                          /* SKIPPED on abend */
```
The ESTAE installed by `abendrpt` does not retry — `libc370 @@abrpt.c:361,366`
does `SETRP(sdwa,0,0,0); return 0` (percolate). Any abend in **core** code (not
a CGI under `__linkds`, whose abends *are* caught and reach `CSTATE_CLOSE`)
terminates the worker before `http_close`. Leaked per abend: the 4 KB HTTPC
(`httpd.c:516`), its env array, its `ufs` session, any open `ufp`, **and the
socket** (FD leak); plus `httpd->busy` keeps a **stale pointer** (the worker
added it at `httppc.c:28` and abended before `http_reset_busy` at `httppc.c:149`;
`http_close`/`httpclos` never touch `busy`). Reproducible trigger: `httpget.c:36`
`/abend`; remote trigger: **S1**.
**Fix:** wrap the per-request processing in `try()`/`tryrc()` (`clibtry.h`,
already used at `httpd.c:903` and in mvsMF's `router.c`); on non-zero rc run
`http_close(httpc)` + `http_reset_busy(httpc)` before looping. Keep `abendrpt`
for the dump. This also contains S9b and the other core derefs.

### M2 — Credential array has no reaper → unbounded CRED + ACEE growth
*Medium · Open · Effort M · `credentials/src/crednew.c:10` · audit M1 (= security session-timeout)*

`cred->last = time64(NULL)` is written once and **never read** (verified: no
reader in `src/` or `credentials/src/`); there is no idle-expiry, reaper, or
reconfig. A `CRED` (≈80 B) + a RACF ACEE is added per distinct
`(addr,user,pass)` login and removed only by an explicit `/logout` with the
exact `Sec-Token`, or at shutdown. A client that never logs out, or re-logs-in,
leaves the prior CRED+ACEE resident forever.
**Fix:** refresh `cred->last` on each `cred_find_by_token` hit; add a periodic
sweep that `array_del`+`cred_free`s entries idle beyond a configurable TTL (this
*is* the missing session timeout); or cap the array.

### M3 — `cred_free` releases storage before releasing its lock
*Medium · Resolved (2026-07-02, PR #85 / issue #84) · Effort XS · `credentials/src/credfree.c:42-45` · audit M2*

> **Resolved:** reordered to `memset → unlock → free → *cred = NULL`, so the
> ENQ is released before the block returns to the free-list.

```c
42   free(c);                  /* storage returned to heap... */
45   unlock(c, LOCK_EXC);      /* ...ENQ on &c released AFTER */
```
The address-keyed ENQ is held while the block is back on the free-list; a
concurrent worker that re-`malloc`s the address and `trylock`s it sees a stale
"busy". **Fix:** unlock before free (`memset → unlock → free → *cred = NULL`).

### M4 — Queue-add failure leaks the accepted HTTPC + socket
*Medium · Resolved (2026-07-02, PR #85 / issue #84) · Effort XS · `httpd.c:551` · audit M4*

`cthread_queue_add(mgr, httpc)`'s return was ignored; on its internal `calloc`
failure (OOM) the client is neither queued nor freed → HTTPC + socket leak.

> **Resolved:** `if (cthread_queue_add(mgr, httpc)) http_close(httpc);`.
> Verified against the libc370 source: `cthread_queue_add` returns `-1` **only**
> on the `calloc` OOM (not queued), and `0` on success (`ecb_post` always
> returns 0) — so the close never double-frees a queued client. *Residual:* the
> QUIESCE/STOPPED race also returns `0` (the API can't distinguish it from
> success), but it is covered by the state check just above; the narrow TOCTOU
> window is a benign shutdown-time leak (reclaimed at AS end), not fixable
> without a libc370 API change.

### M5 — Listener thread dies permanently on a transient accept/calloc error
*Medium · Resolved (2026-07-02, PR #85 / issue #84) · Effort XS · `httpd.c:498-502,516-520` · audit M5*

A single transient `accept()` error or HTTPC `calloc` failure did `goto quit`,
exiting the accept loop for good — the server stopped accepting all new
connections. (This was where FD exhaustion from a fault became a total outage —
the last piece of the S1→M1→M5 chain.)

> **Resolved:** both now `continue` (a real SHUTDOWN/QUIESCE still exits via the
> checks at the top of the loop, mirrored in the accept handler). On the
> `calloc` failure the already-accepted socket is `closesocket()`d first so the
> fd is not leaked.

### M6 — `crt->crtufs` dangling after `ufsfree`
*Low · Open · Effort XS · `httpgufs.c:17` + `httpclos.c:36` · audit L2*

`http_get_ufs` caches the session in the per-worker `crt->crtufs`; `httpclos`
frees+NULLs `httpc->ufs` but not the cached alias. Safe today (next
`http_get_ufs` overwrites it before any UFS op). **Fix:** clear `crt->crtufs` in
`httpclos` when it equals the freed session.

### M7 — Keep-alive reset does not close `fp`/`ufp`
*Low · Latent · Effort XS · `httprese.c` · audit L3*

`httprese` frees `env` and retains `ufs` (correct), but never closes `fp`/`ufp`.
Safe only because every current RESET is preceded by `httpdone` (which closes
them) or reaches RESET with no file open. A future handler that opens a UFS file
then transitions straight to RESET would leak a handle per persistent
connection. **Fix:** mirror `httpclos.c:34-35`'s defensive close at the top of
`httprese`.

### M8 — `array_add` return ignored in `http_set_env`
*Low · Resolved (2026-07-02, PR #87 / issue #86) · Effort XS · `httpsenv.c:25` · audit L5*

On OOM the new HTTPV was neither stored nor freed (orphan the teardown can't
reach), and `http_del_env` already removed the old one — the variable silently
vanished.

> **Resolved:** `if (array_add(&httpc->env, v)) { free(v); rc = -1; }`
> (`array_add`/`@@aradd.c` returns 0 on success, -1 on failure — verified).

### M9 — Env-var block over-allocation (~4 bytes/var)
*Low · Resolved (2026-07-02, PR #87 / issue #86) · Effort XS · `httpnenv.c:11` · audit L6*

`total = sizeof(HTTPV)(16) + namelen + vallen + 2` where the layout needs
`offsetof(HTTPV,name)(12) + namelen+1 + vallen+1`. ~4 wasted bytes **per env
var, per request** — the only pure per-request memory waste.

> **Resolved:** `total = offsetof(HTTPV, name) + namelen + 1 + vallen + 1;`.
> Guarded by the new `TSTNENV` (name/value round-trip + layout assertions).

### M10 — Unchecked `strdup` in CGI registration
*Low · Resolved (2026-07-02, PR #87 / issue #86) · Effort XS · `httpacgi.c:23-24` · was F (mem 2026-03) / audit L9*

`cgi->path`/`cgi->pgm` weren't NULL-checked and `array_add` was unconditional; an
OOM at config time registered a half-built entry → later NULL-deref at
match/link.

> **Resolved:** on a `strdup` NULL or an `array_add` failure the partial entry
> is freed and not registered. (The strdup storage of a *successfully*
> registered entry stays AS-lifetime by design.)

### M11 — `cgictx` array not freed at `terminate()`
*Low · Open · Effort XS · `httpd.c:167-168,284-304` · audit L19*

Benign one-shot (reclaimed at AS end). **Fix:** `free(httpd->cgictx)` in
`terminate()` for completeness. (The pointed-to `__getm` context blocks are
AS-lifetime **by design** — not a leak.)

### M12 — `http_gets` NULL-buf 1-byte overflow
*Low · Latent · Effort XS · `httpgets.c:76-77` · audit L8*

In the `buf==NULL` branch `max==CBUFSIZE` (4000), so an exact-length line + LF
writes `buf[4000]` (one past `httpc->buf`). **Unreachable today** — both callers
pass `max=CBUFSIZE-1`. **Fix:** set `max=CBUFSIZE-1` in the NULL branch too.

### M13 — Shutdown teardown race: worker force-DETACHed → S33E + nested reporter abend
*Medium · Open (root cause in libc370 cthread) · Effort M · `httpd.c:274` (`terminate` → `cthread_manager_term`) + libc370 `@@cmterm.c`/`@@cminit.c`/`@@cmwshu.c` · observed in a 2026-07-01 STC log*

On a live `P HTTPD`, 5 of 6 workers logged `HTTPD060I SHUTDOWN` cleanly; the
**first-created** worker (`0CB0C8`, `TCB=009CD500`) did **not**, and abended
instead — after which `terminate()` still reached `HTTPD002I SHUTDOWN`:

```
ABEND S33E detected ... TCB=009CD500   PSW 00000000 00000000  KEY(0) MODE(SUP)
ABEND S0C4 detected ... epname get_addr offset 00000064  TCB=009CD500  KEY(8) MODE(PROB)
```

**Mechanism (confirmed by reading the teardown):** `terminate()` →
`cthread_manager_term` (`httpd.c:274`) tears workers down **in reverse creation
order** using **fixed `STIMER` waits, not a join**, and force-detaches subtasks
that have not set `termecb`:
- `cthread_worker_shutdown` posts `POST_SHUTDOWN`, waits ≤5 s, then
  `cthread_detach`s the still-active task; `dispatch_thread_term` does
  shutdown + `cthread_worker_del` back-to-back; `cthread_delete` →
  `cthread_detach` → `try(detach,…)` issues an MVS **DETACH of an incomplete
  subtask**, which abnormally terminates it → the **S33E** (KEY 0/SUP = the
  DETACH/termination path; PSW 0 = no clean failing PSW).
- The worker's ESTAE (`abendrpt`) then runs `get_addr` (libc370
  `@@abrpt.c:50`), which dereferences a register value (`*np`, ≈offset 0x64)
  pointing into the just-freed save area → the **nested S0C4**, caught by the
  reporter's own `failed()` recovery (it even ships a commented "force a bad
  address to test failed()" hook). So the S0C4 is *noise on top of* the S33E,
  not the defect.

The **first-created** worker is the consistent victim because the reverse walk
handles it **last** — it is the one most exposed to the window where
`cthread_manager_term` stops waiting and the force-detach / `free(mgr)` opens.
(*The exact victim selection is a well-supported hypothesis; the underlying race
is confirmed from the code.*)

**Impact:** shutdown is **unclean** — two spurious abends per `P HTTPD`.
Functional impact today is low (the address space still terminates), but under
different timing (a slow-to-exit worker) `cthread_manager_term` can
`cthread_delete(&mgr->task)` + `free(mgr)` while the dispatch thread is still in
`dispatch_thread_term` iterating `mgr->worker` → a genuine use-after-free.

**Root cause & fix — in `libc370` (shared by all cthread users: ufsd, ftpd, …,
so coordinate there):** `cthread_manager_term` should **join** the dispatch
thread (wait on `mgr->task->termecb`) before `cthread_delete(&mgr->task)` /
`free(mgr)`, instead of fixed 0.25 s `STIMER` waits; and worker teardown should
confirm `termecb` before `cthread_worker_del` (as `dispatch_thread_check`
already does on the graceful path) rather than force-detach an active subtask.
HTTPD only calls the shared API — no local fix. Tracked in
**mvslovers/libc370#6**. Same abend-fragility family as **M1** (different
trigger: shutdown teardown vs request processing).

---

## C. Performance

### P1 — Environment-variable lookup is O(n) linear, on every access
*Medium · Open · Effort S-M · `httpfenv.c` · from 2026-03 §Memory & Optimization*

```c
for(n=0; n<count; n++)
    if (http_cmp(v->name, name)==0) { indx = n+1; break; }   /* caseless, full scan */
```
`http_get_env`/`http_find_env` scan the whole env array (50+ vars/request:
headers + query + cookies) on every lookup, and the request path looks up many
vars — a clear O(n) inefficiency worth removing. Comparison is **caseless**
(`http_cmp`), so any index must normalise case. *Caveat (unprofiled):* this is
single-digit microseconds per lookup — meaningful for **static/non-CGI**
requests, but for CGI dispatch the LINK SVC (`P7`, ~50 ms) dominates by orders
of magnitude. Confirm the bottleneck by profiling before investing here.
**Options (pick one):** a small hash (FNV/DJB2 over the upcased name); keep the
array sorted + binary search (no extra memory); or a one-slot last-hit cache
(smallest change). Hash gives the best worst-case for header-heavy requests.

### P2 — Worker busy-spins on `EWOULDBLOCK` mid-send
*Medium · Open · Effort M · `httpd.c:606-609` + `httpsend.c:24-25` · audit M6 / was F-04*

Sockets are non-blocking (`ioctlsocket(FIONBIO)`, `httpd.c:509`). On
`EWOULDBLOCK`, `send_raw` returns the partial count without advancing state, and
the worker loop (`while(state!=CSTATE_CLOSE) http_process_client(httpc);`) has no
`select`/yield between iterations — a slow/stalled client makes the worker spin
at 100% CPU while holding `httpc->ufp` open, degrading the whole pool.
**Fix:** gate re-entry on writability (the existing `select` in `socket_thread`)
and/or enforce `CLIENT_TIMEOUT` during transfer. (Keep the per-byte `recv` read
path — intentional.)

### P3 — Chunked short-send desyncs the chunk frame
*Low · Open · Effort S · `httpsend.c:54-68` · audit L17 / part of F-04*

The chunked branch only checks `rc < 0`; `send_raw` returns a positive short
count on `EWOULDBLOCK`, so a partial header/data send is treated as complete and
corrupts framing. **Fix:** treat `rc < hdrlen`/`rc < len` as incomplete (ties to
P2; do the send-state machine once).

### P4 — Triple 4 KB `memset` of `httpc->buf` per request
*Low · Open · Effort XS · `httppars.c:151,189,213` · from 2026-03*

Three `memset(httpc->buf, 0, CBUFSIZE)` (4000 B each) per request body path.
**Fix:** zero only the used portion (`memset(buf, 0, len+1)`), or rely on
`http_gets`'s own clearing where it already zeroes.

### P5 — Hot-path env allocation churn (arena opportunity)
*Low · Open · Effort M · `httpnenv.c` + `httprese.c` · from 2026-03 (ties M9)*

Each request `calloc`s 10–20 small HTTPV blocks and frees them in `httprese`.
For higher load, a per-HTTPC env arena/pool (reset, not freed, between requests)
would cut allocator traffic and fragmentation. Acceptable at current load (3–9
clients); revisit if concurrency grows. Combine with M9's sizing fix.

### P6 — Large transient stack frames vs the 64 KB worker stack
*Info · `httpprtv.c:12` (`buf[5120]`), `httpfile.c:595` (`buf[4096]`), SSI recursion ≤10 · audit L18/L15*

Not leaks (stack, auto-freed), but the deep chain
`worker → httppc → __linkds → CGI → http_printf(buf[5120])` plus SSI recursion
to depth 10 (per-frame `uri[256]`+`tmp[256]`+`save[256]` + `buf[4096]`) is the
stack budget to watch. **Action:** keep an eye on worst-case depth; shrink
per-frame buffers or lower `SSI_LEVEL_MAX` only if stack-overflow evidence
appears.

### P7 — LINK SVC per CGI dispatch (~50 ms)
*Architecture · `httppc.c`/`httplink.c` · from 2026-03 §Router comparison*

Every CGI dispatch is a `__linkds` LINK SVC. A built-in method/`{param}` router
(see *Architecture*) for hot, in-process handlers would avoid the re-LINK cost
for frequently-called endpoints and enable mvsMF integration.

---

## Verified clean / intentional — do **not** re-flag

- **Per-byte `recv()`** (`http_getc`/`http_gets`) — TCP/IP ring-buffer
  workaround; must not become bulk `recv()`.
- **`__getm` CGI-context blocks** (`httpgctx.c`) and the **`strdup`s in
  `httpacgi.c`** — allocated once, AS-lifetime, never freed by design.
- **`httpc->cred` is a borrowed pointer** (the CRED lives in the process-wide
  array); `httprese` correctly NULLs it, never frees it.
- **Env-var ownership:** the `httpc->env` array owns each HTTPV; `http_set_env`
  replaces via delete-old-then-add; `array_free(&httpc->env)` frees each element
  and NULLs the pointer — no double-free/UAF across keep-alive then close.
- **CGI abends** are caught by `__linkds`'s ESTAE (→ `CSTATE_CLOSE` →
  `http_close`); only *core* abends leak (M1).
- **`httpclos`/`httpdone`** are a correct teardown net on the normal/keep-alive
  paths. **SMF records** and **LINK parameter lists** are stack structs.

---

## Resolved / moved since the earlier reviews (do not re-open)

| Earlier finding | Status | Evidence |
|-----------------|--------|----------|
| `sprintf` in header/query/POST name build (CRIT#1) | **Resolved** | `snprintf` — `httpshen.c:14,65`, `httpsqen.c:11`, `httppars.c:231` |
| Unbounded `vsprintf` in `http_printv` (CRIT#2) | **Resolved** | `vsnprintf` — `httpprtv.c:14` |
| Integer overflow in `httpnenv` (HIGH#6) | **Resolved** | `size_t` + `if (namelen+vallen>8192) return NULL` — `httpnenv.c:9-15` |
| Missing return in `parse_cookies` (HIGH#7) | **Resolved** | `httpshen.c` returns on all paths |
| Race on client `array_add` (HIGH#8) | **Resolved** | `lock(httpd,0)` around add — `httpd.c:556-558` |
| Version-string `sprintf` (LOW#13) | **Resolved** | `snprintf` — `httppars.c:92` |
| FTP path traversal + `sprintf` overflows (CRIT#5, MED#12) | **Removed** | FTP daemon removed (TSK-2) |
| MQTT telemetry NULL-checks + partial leak (`httppubf.c`) | **Removed** | MQTT telemetry removed |
| Lua config NULL-checks (`httpconf.c`) | **Removed** | Lua config replaced by Parmlib (`httpprm.c`) |
| `httplua.c` `strcpy` into `dataset[256]` (F-06) | **Moved** | now `mvslovers/httplua` |
| Keep-alive body poisoning (part of F-09) | **Mitigated** | `keepalive=0` on unread body — `httppars.c:206-210` |
| SSI echo empty-var (F-13) | **Mostly resolved** | `if (!*var)` — `httpfile.c:391` (see S12) |
| Directory-index stack overflow + NULL path (S1 / S9b) | **Resolved (2026-07-02)** | bounded copy + NULL/empty guard — `httpget.c`, PR #73 / issue #72 |
| Worker abend leaks HTTPC/socket + corrupts `busy` (M1) | **Resolved (2026-07-02)** | `try()` net + gated `http_reset_busy` before `http_close` — `httpd.c`, PR #77 / issue #76 |
| Cookie `sprintf` overflow + reflected XSS (S2) | **Resolved (2026-07-02)** | `http_html_escape()` + direct print — `httpcred.c`/`httpesc.c`, PR #79 / issue #78 |
| `Sec-Uri` redirect over-read + header injection / open redirect (S3) | **Resolved (2026-07-02)** | length-bounded `memcpy` + `http_safe_redirect()` — `httpcred.c`/`httpsrdr.c`, PR #79 / issue #78 |
| Percent-decoder OOB read on trailing `%` (S5) | **Resolved (2026-07-02)** | reads guarded by `str[1] && str[2]`; incomplete escape passed through — `httpdeco.c`, PR #81 / issue #80 |
| Unbounded `vsprintf` in SSI `ssi_printv` (S6) | **Resolved (2026-07-02)** | `vsnprintf` + clamp — `httpfile.c`, PR #83 / issue #82 |
| `cred_free` frees storage before unlock (M3) | **Resolved (2026-07-02)** | reorder `memset → unlock → free` — `credfree.c`, PR #85 / issue #84 |
| Queue-add failure leaks HTTPC + socket (M4) | **Resolved (2026-07-02)** | check `cthread_queue_add` rc → `http_close` — `httpd.c`, PR #85 / issue #84 |
| Listener dies on transient accept/calloc error (M5) | **Resolved (2026-07-02)** | `continue` (+ `closesocket` on OOM) instead of `goto quit` — `httpd.c`, PR #85 / issue #84 |
| `http_set_env` drops the var on `array_add` OOM (M8) | **Resolved (2026-07-02)** | check rc → free orphan — `httpsenv.c`, PR #87 / issue #86 |
| Env-block ~4 B/var over-allocation (M9) | **Resolved (2026-07-02)** | `offsetof`-based sizing + `TSTNENV` — `httpnenv.c`, PR #87 / issue #86 |
| Unchecked `strdup`/`array_add` in CGI registration (M10) | **Resolved (2026-07-02)** | free partial entry on failure — `httpacgi.c`, PR #87 / issue #86 |
| SSI path traversal (S4) | **Already mitigated** — finding corrected (no code) | `http_open` rejects `..` (`httpopen.c:17`, `0c171fe`) + prepends DOCROOT (`:50`, `1c2ad3c`) |
| POST body `Content-Length` enforcement (S7) | **Resolved (2026-07-03)** | `httpbody()` classifier: 413/400/zero-body + recv capped at CL + `TSTBODY` — `httppars.c`/`httpbody.c`, PR #89 / issue #88 |
| Unbounded query/POST parameter count (S8) | **Resolved (2026-07-03)** | combined 256-param cap → 400 — `httppars.c`, PR #91 / issue #90 |

---

## Testing backlog

HTTPD had one test (`TSTGCTX`); a small suite is now being grown alongside these
refactorings (the working policy: **when a fix lands and a unit test is
sensible, add it in the same PR**). Because `httpd.h` pulls the non-host-portable
crent370/libc370 runtime stack, tests that reach through it are **MVS-target**
(`make test-mvs`) and carry `host = false` so `make test-host` skips them
cleanly (needs mbt ≥ `d57d447`). **Where a fix extracts a pure helper, keep that
helper free of `httpd.h`** (only `<stddef.h>` + char literals): the test then
runs **DUAL** (host *and* MVS), so the logic is actually *executed* on the host
— real signal, not just a cc370 compile. Assertions on decoded/translated bytes
must use the `asc2ebc`/`ebc2asc` tables (or char literals), never bare hex —
EBCDIC.

**Done**
- `TSTDECO` — `http_decode()`/`httpdeco()` percent/plus decoder, incl. the **S5**
  trailing-`%` / incomplete-escape boundary (asserts the post-fix contract:
  incomplete escapes preserved literally). MVS-target. *(PR #75 / issue #74;
  S5 assertion updated in PR #81.)*
- `TSTSAFE` — `http_html_escape()` (**S2**) + `http_safe_redirect()` (**S3**),
  the extracted string-safety helpers. **Dual** (runs on host + MVS); 26
  assertions incl. an XSS breakout and the open-redirect / CRLF edge-case table.
  *(PR #79 / issue #78.)*
- `TSTNENV` — `http_new_env()`/`httpnenv()` env-block allocation + layout,
  guarding the **M9** `offsetof` sizing (name/value round-trip). MVS-target.
  *(PR #87 / issue #86.)*
- `TSTBODY` — `httpbody()` request-body length classifier (**S7**, RFC 7230
  §3.3.3). **Dual** (host + MVS); 16 assertions over the decision table
  (absent/zero/valid/oversize/negative/junk/OWS/`Transfer-Encoding`).
  *(PR #89 / issue #88.)*

**Open — pure helpers, cleanly unit-testable (good next targets)**
- **`httpcmp`/`httpcmpn`** — EBCDIC caseless compare (collation correctness; XS).
- **`http_mime`** — extension→type mapping (table lookup; XS).
- *(base64 decode itself is a **libc370** function — a test belongs there, not
  here; the httpd-side S3 sanitiser is covered by `TSTSAFE`.)*

**Hard to unit-test (integration / MVS-only) — verify behaviourally, not with mbtcheck**
- **M1** abend recovery — drive via `GET /abend` (`httpget.c:36`); confirm the
  worker survives (`HTTPD062E`) and FD / worker / `busy` counts stay flat.
- Socket/keep-alive framing, CGI dispatch, SSI, auth — covered by the mvsMF
  `curl-*.sh` scripts against a live server.

---

## Architecture & roadmap (planning horizon, lower urgency)

- **File consolidation.** Post FTP/MQTT/Lua removal the tree is much smaller, but
  several single-function clusters remain (env CRUD across `httpfenv/nenv/denv/`
  `senv/shen/sqen`, `dbg*` ×7, busy `is/sbz/rbz`). Consolidating is a
  build-system change (source list) with **no ABI impact** — improves
  navigability for the refactors above.
- **Built-in router + middleware** (method dispatch, `{param}` extraction,
  before/after middleware) exported via HTTPX (append-only). Reduces LINK
  overhead (P7), enables path-parameter handlers, and is the basis for mvsMF to
  register routes instead of carrying its own `router.c`.
- **RACF resource-auth helper in HTTPX** (`http_check_auth(class,resource,attr)`)
  so CGIs can do fine-grained authorization and reuse the httpd session instead
  of re-authenticating (mvsMF `authmw.c` currently re-auths every request).
- **Unified error model.** Define `enum httpd_rc` in `errors.h` and adopt
  goto-cleanup consistently (three error conventions coexist today).

### Suggested refactoring order

1. **Stability/security criticals:** ~~**S1** (bound the copy)~~ **✔ done (PR #73)**
   + ~~**M1** (`try()` net)~~ **✔ done (PR #77)** — the S1→M1→M5 DoS chain is now
   fully closed (M5 in step 2).
2. **Quick security wins:** ~~**S2**, **S3**~~ **✔ (PR #79)**; ~~**S5**~~ **✔
   (PR #81)**; ~~**S6**~~ **✔ (PR #83)**; ~~**M3** (unlock/free swap), **M4**,
   **M5** (one-line robustness)~~ **✔ (PR #85)**. **Step 2 complete.**
3. **Memory stability:** ~~**M8**/**M9**/**M10** (env/CGI alloc hygiene)~~ **✔
   (PR #87)**; remaining: **M2** (credential reaper — also closes the
   session-timeout security gap).
4. **Performance:** **P1** (env-lookup index — biggest CPU win), then **P2**/**P3**
   (one send-state machine), **P4**.
5. **Hardening:** ~~**S4**~~ (already mitigated — finding corrected), ~~**S7**~~
   **✔ (PR #89)**, ~~**S8**~~ **✔ (PR #91)**, **S9–S12**,
   **M6**/**M7**/**M11**/**M12**.
   **M13** (shutdown teardown race) is mostly a **libc370** fix — tracked in
   mvslovers/libc370#6; it affects every cthread user, not just HTTPD.
6. **Architecture:** router/middleware, HTTPX auth helper, error enum, file
   consolidation, and the security-architecture backlog (cookie flags, rate
   limiting, TLS).

---

## Provenance & methodology

Consolidated from four prior documents and **re-verified against `main` on
2026-06-30** (static inspection of `src/` and `credentials/src/`, plus the
`libc370` abend-recovery path; no runtime fuzzing):

- `code-review-2026-03-10.md` — broad review (security, architecture, memory,
  router comparison; pre-FTP/MQTT/Lua-removal).
- `code-review-2026-04-11.md` — focused memory-safety/security pass (7 findings).
- `code-review-open-findings.md` — the 2026-04-12 consolidation (F-01…F-13).
- `memory-audit-2026-06-30.md` — six-subsystem memory & stability audit
  (H1/H2 + M/L series), every concrete finding hand-verified.

Each finding above carries its original ID for traceability. Where the older
reviews and the current code disagreed, the **current code wins** and the status
reflects today's `main`.

**2026-07-01:** added **M13** (shutdown teardown race) from a live `P HTTPD` STC
log, verified against `httpd.c`'s `terminate()` and the libc370 cthread teardown
(`@@cmterm.c`/`@@cminit.c`/`@@cmwshu.c`/`@@ctdel.c`/`@@ctdet.c`) and the abend
reporter (`@@abrpt.c`).

**2026-07-02:** **S1** (remote directory-index stack overflow) and **S9b**
(NULL/empty `REQUEST_PATH` guard) fixed in `httpget.c` — bounded the index-append
copy and guard `path` before `http_cmp`/`path[len-1]`. PR #73, issue #72. Status
tables, counts (Security 12→11) and the refactoring order updated accordingly.

**2026-07-02:** **M1** (worker-abend resource leak) fixed in `httpd.c` — the
per-request loop is factored into `serve_client()` and run under `try()`;
cleanup always runs, a caught abend logs `HTTPD062E`, and `http_reset_busy` was
placed **before** `http_close` (resetting after close is a use-after-free — see
the M1 note). PR #77, issue #76. Counts M&S 13→12. Added a **Testing backlog**
section and began an MVS-target unit suite (`TSTDECO`, PR #75) with the
`host = false` skip convention.

**2026-07-02:** **S2** (cookie `sprintf` overflow + reflected XSS) and **S3**
(`Sec-Uri` redirect over-read + `Location:` header injection / open redirect)
fixed in `httpcred.c` — extracted `http_html_escape()` (`src/httpesc.c`) and
`http_safe_redirect()` (`src/httpsrdr.c`), replaced `strncpy` with a
length-bounded `memcpy` (the real bug: `base64_decode` output is not
NUL-terminated). Both helpers are host-portable → **dual** `TSTSAFE` test, run
natively (26/26). PR #79, issue #78. Counts Security 11→9.

**2026-07-02:** **S5** (percent-decoder OOB read on trailing `%`) fixed in
`httpdeco.c` — the `str[1]`/`str[2]` reads moved inside the `if (str[1] &&
str[2])` guard (`&&` short-circuits, so `str[2]` is never read past the NUL);
incomplete escapes are passed through literally. `TSTDECO`'s trailing-`%`
assertion updated to the new contract. PR #81, issue #80. Counts Security 9→8.

**2026-07-02:** **S6** (unbounded `vsprintf` in SSI `ssi_printv`) fixed in
`httpfile.c` — `vsnprintf(buf, sizeof(buf), …)` + clamp the would-be length to
`sizeof(buf)-1` before `ssi_buffer()`, mirroring `http_printv()`. PR #83,
issue #82. Counts Security 8→7.

**2026-07-02:** **M3** (`cred_free` unlock/free order), **M4** (`cthread_queue_add`
return unchecked → HTTPC+socket leak), **M5** (listener bails on transient
`accept`/`calloc` error) fixed in `credfree.c`/`httpd.c` — reorder to `unlock`
before `free`; check the queue-add rc (verified against libc370 that it returns
`-1` only on OOM, `0` on success); `continue` instead of `goto quit` (+
`closesocket` on OOM). M5 closes the last piece of the S1→M1→M5 chain. PR #85,
issue #84. Counts M&S 12→9.

**2026-07-02:** **M8** (`http_set_env` drops the var on `array_add` OOM), **M9**
(env-block ~4 B/var over-allocation), **M10** (unchecked `strdup`/`array_add` in
CGI registration) fixed in `httpsenv.c`/`httpnenv.c`/`httpacgi.c` — check
`array_add` rc (verified `@@aradd.c`: 0 success / -1 failure) and free orphans;
size the env block with `offsetof(HTTPV,name)` not `sizeof(HTTPV)`; free a
half-built CGI entry on OOM. Added `TSTNENV` (guards the M9 layout). PR #87,
issue #86. Counts M&S 9→6. **Backlog steps 2–3 (quick wins + alloc hygiene)
complete; remaining memory item is M2 (credential reaper / session timeout).**

**2026-07-02:** **S4** (SSI path traversal) reviewed and found **already
mitigated** — `ssi_include`'s only open (`httpfile.c:533`) goes through
`http_open`, which already rejects `..` (`httpopen.c:17`, commit `0c171fe`,
2026-03-01) and prepends `DOCROOT` (`:50`, `1c2ad3c`, 2026-03-16), both
predating the consolidation. The finding traced the caller but missed the
callee's guard. No code change (a redundant `ssi_include` reject would be
defensive noise). Backlog corrected; Security count 7→6. Filed the separate
`DOCROOT`-unset confinement gap under *Security architecture*.

**2026-07-03:** **S7** (POST body `Content-Length` enforcement) fixed in
`httppars.c` — new pure `httpbody()` classifier (`src/httpbody.c`, RFC 7230
§3.3.3): **413** on oversize CL (added the missing `413` to `http_resp`, ref
TSK-110), **400** on malformed CL, **zero-length body** on missing CL/no TE
(fixes the old blanket-`recv`→RESET on a bodyless POST), and the `recv` is
capped at the declared length. Scope kept tight (no exact-CL loop = slowloris
risk, no chunked decode, no POST keep-alive). Dual `TSTBODY` (16, run
natively). PR #89, issue #88. Counts Security 6→5.

**2026-07-03:** **S8** (unbounded query/POST parameter count) fixed in
`httppars.c` — a combined `nparam` counter caps query + POST parameters at
`HTTP_MAX_PARAMS` (256); over the cap → **400** via a shared `badrequest:`
handler (S7's malformed-CL case folded onto it too). PR #91, issue #90.
Counts Security 5→4. With **S7** this closes the request-body DoS pair.
