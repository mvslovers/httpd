# HTTPD Core — Memory & Stability Audit (2026-06-30)

Audit of the **HTTPD load module** and the core server sources it pulls in.
Goal: make the server **stable and fast** by finding memory leaks, unnecessary
allocations, handle leaks, and the failure paths where resources escape. The
built-in **CGI modules are out of scope** (HTTPDSRV/HTTPJES2/HTTPDSL/HTTPDM/…).

## Scope

The HTTPD module links `httpstrt.c` + `httpd.c` + `httpprm.c` and autocalls the
rest of the core pipeline from the internal archive (input, parse, dispatch,
file serving, response/send, env vars, credentials, SMF, operator console,
translation). All of those were audited.

## Method

Six parallel subsystem audits, then **every concrete finding was verified by
hand against the actual source** (and against the `libc370` sibling repo for the
abend-recovery path). The discriminating test applied to each allocation/handle:

> *Is there a reachable path — an error/early return, a keep-alive **reset**
> (the HTTPC is reused across requests on a persistent connection), or a
> **worker-abend** recovery — where this allocation is not freed before the
> HTTPC is reset or released?*

A passing happy path was never treated as proof of correctness.

---

## Executive summary

The per-request hot path is overwhelmingly **stack-based and leak-free** on the
happy path. The real problems live in two places, and they **compound**:

1. **A remotely reachable stack-buffer overflow** in the static-file
   directory-index fallback (`httpget.c`), reachable by an unauthenticated GET
   in the default configuration.
2. **A per-abend resource leak**: when *any* request abends in HTTPD **core**
   code, the worker's recovery routine percolates the abend, so the worker dies
   **before** `http_close()` runs — leaking the 4 KB `HTTPC`, its socket
   (FD), its env array and UFS session, and leaving a **stale pointer in
   `httpd->busy`**.

Because the overflow (#1) causes a core abend, a single crafted URL triggers
both: it can crash a worker *and* leak an HTTPC + socket on every hit — a remote
resource-exhaustion DoS that eventually exhausts file descriptors
(`FD_SETSIZE`) and worker slots. These two are the priority.

A third, slower problem is **unbounded growth of the credential array** (no
idle reaper), which leaks ~80 bytes + a RACF ACEE per distinct login for the
life of the address space.

Everything else is low severity (NULL-path derefs, OOM-only orphans, a few
bytes of per-env-var over-allocation, operator-command hardening).

### Findings at a glance

| ID | Severity | Conf. | Type | Location | One-liner |
|----|----------|-------|------|----------|-----------|
| **H1** | **High** | confirmed | Bug | `httpget.c:23,47-50` | Stack overflow: `memcpy(buf[300], path, strlen(path))`, path ≤ 4000, dir-index fallback, unauth |
| **H2** | **High** | confirmed | Bug | `httpd.c:591,606-612` | Core abend ⇒ `http_close` skipped ⇒ HTTPC + socket + env + ufs leak + stale `busy` |
| **M1** | Med | confirmed | Design gap | `credentials/src/crednew.c:10` | Credential array has no reaper ⇒ unbounded CRED+ACEE growth |
| **M2** | Med | confirmed | Bug | `credentials/src/credfree.c:42-45` | `free(c)` *before* `unlock(c)` — storage released while its ENQ is held |
| **M3** | Med | confirmed | Bug | `httpfile.c:595,598` | SSI `ssi_printv` uses **`vsprintf`** (unbounded) into `buf[4096]` |
| **M4** | Med | confirmed | Bug | `httpd.c:551` | `cthread_queue_add()` return ignored ⇒ HTTPC + socket leak on OOM / quiesce race |
| **M5** | Med | confirmed | Bug | `httpd.c:498-502,516-520` | Transient `accept()`/`calloc` error ⇒ `goto quit` kills the accept loop forever |
| **M6** | Med | suspected | Bug | `httpd.c:606-609` + `httpsend.c:24-25` | Non-blocking `EWOULDBLOCK` mid-send ⇒ worker busy-spins (100% CPU) and pins the handle |
| L1 | Low | confirmed | Bug | `httpget.c:31,36,47` | NULL/empty `REQUEST_PATH` ⇒ `strlen(NULL)` / `path[-1]` deref |
| L2 | Low | suspected | Latent UAF | `httpgufs.c:17` + `httpclos.c:36` | `crt->crtufs` left dangling after `ufsfree()` |
| L3 | Low | suspected | Latent leak | `httprese.c` | Reset does not close `fp`/`ufp` (safe today, fragile) |
| L4 | Low | confirmed | Bug | `httpcons.c:326,377` | `D M` / `D TI` with no arg ⇒ `strtoul(NULL)`/`strtol(NULL)` abend on console thread |
| L5 | Low | confirmed | Bug | `httpsenv.c:25` | `array_add()` return ignored ⇒ HTTPV orphaned + variable lost on OOM |
| L6 | Low | confirmed | Optimization | `httpnenv.c:11` | `sizeof(HTTPV)` over-allocates ~4 bytes per env var, every request |
| L7 | Low | confirmed | Bug | `httpdeco.c:18` | Trailing `%` reads one byte past the NUL (benign OOB read) |
| L8 | Low | suspected | Latent | `httpgets.c:76-77` | NULL-buf path can write 1 byte past `httpc->buf` (unreachable by current callers) |
| L9 | Low | confirmed | Bug | `httpacgi.c:23-24` | Unchecked `strdup` ⇒ half-built CGI entry registered (later NULL-deref) |
| L10 | Low | confirmed | Bug | `httpcred.c:395` | `strncpy` into `uribuf[256]` not NUL-terminated ⇒ over-read of `Sec-Uri` cookie |
| L11 | Low | confirmed | Bug | `httpcred.c:415-445` | `base64_encode` fail after successful login shows "invalid credentials" page |
| L12 | Low | confirmed | Cleanup | `httppc.c:38` | Dead store `cred = httpc->cred` (overwritten before use) |
| L13 | Low | confirmed | Footgun | `httpopen.c` | `http_open()` always returns NULL; handle only via side-effect `httpc->ufp` |
| L14 | Low | suspected | Bug (UB) | `httpfile.c:229-231` | Overlapping `strcpy(tmp, end)` — use `memmove` |
| L15 | Low | suspected | Optimization | `httpfile.c` SSI | SSI recursion to depth 10 × per-frame buffers — verify 64 KB stack budget |
| L16 | Low | confirmed | Optimization | `httpcons.c:43` | Per-MODIFY `calloc` of a tiny buffer — could be a stack buffer |
| L17 | Low | confirmed | Bug | `httpsend.c:54-68` | Chunked path checks only `rc < 0`; a short send desyncs the chunk frame |
| L18 | Low | confirmed | Optimization | `httpprtv.c:12` | `buf[5120]` is the largest transient stack frame (safe; sizing note) |
| L19 | Low | confirmed | Optimization | `httpd.c:167-168,284-304` | `httpd->cgictx` array not freed in `terminate()` (benign one-shot) |

Non-memory notes (no fix proposed): `httppost` is dispatched through `http_put`
(`httppc.c:124`); `httpsubt.c` is vestigial dead code; `httprese` leaves
`resp`/`ssi`/`ssilevel` stale between keep-alive requests (overwritten before
use).

---

## Critical / High

### H1 — Remote stack-buffer overflow in the directory-index fallback

- **Location:** `src/httpget.c:23, 47-50` · **Severity:** High · **Confidence:** confirmed · **Type:** Bug (security + stability)

```c
23      UCHAR       buf[300];
...
31      path = http_get_env(httpc, "REQUEST_PATH");
...
43      fp = http_open(httpc, path, mime);
44      if (fp || httpc->ufp) goto okay;
47      len = strlen(path);
48      if (path[len-1]=='/') {
49          memcpy(buf, path, len);                 /* <-- len up to ~4000 into buf[300] */
50          strcpy(&buf[len], "index.html");
```

- **Problem path:** `httpget()`, first-time (`subtype == CTYPE_UNKNOWN`) branch.
  `REQUEST_PATH` is produced in `httppars.c:39` with
  `strcpyp(buf, CBUFSIZE, p, 0)` — bounded by **CBUFSIZE = 4000**, *not* by 300.
  `len = strlen(path)` is unclamped. When the requested path is longer than ~289
  bytes and ends in `/`, `memcpy(buf, path, len)` writes far past the 300-byte
  stack buffer, then `strcpy` appends `"index.html"`. The preceding
  `http_open()` does not prevent this — `http_open` clamps the name into its own
  `buf[256]`, the truncated open fails (404 internally), and control falls into
  this branch with the **full-length** `path`. Reachable **unauthenticated** in
  the default config (`LOGIN NONE`, no CGI match) by any GET. **Reachability
  verified:** the only URI cap is the 414 "URI Too Long" gate in `httpin.c:24-31`,
  which fires only when the entire request line exceeds `CBUFSIZE-1` (~3999
  bytes) — far above 300 — so a path of ~301–3980 chars ending in `/` passes
  414, survives parsing, and reaches this `memcpy` intact.
- **Impact:** Stack smash → overwrites saved registers / adjacent frames →
  worker **abend** (which then triggers H2's leak) or memory corruption. Remote,
  unauthenticated, trivially reproducible (`GET /aaaa…aaaa/`).
- **Recommendation:** Reject over-long paths or clamp before copying — e.g.
  `if (len + sizeof("default.html") > sizeof(buf)) { rc = http_resp_not_found(...); ... }`
  and build the candidate into the size-checked buffer (use the longer of
  `"index.html"`/`"default.html"` in the budget). Do **not** simply enlarge
  `buf` — the input is unbounded; bound it.
- **Related (L1):** lines 31/36/47 also assume `path` is non-NULL/non-empty;
  `http_get_env` can return NULL (then `http_cmp(NULL,…)` at 36 and
  `strlen(NULL)` at 47 deref NULL; an empty path reads `path[-1]`).

### H2 — Worker abend leaks the HTTPC, its socket, env, UFS, and a `busy` slot

- **Location:** `src/httpd.c:591, 606-612` (recovery: `libc370 @@abrpt.c:360-366`) · **Severity:** High · **Confidence:** confirmed · **Type:** Bug (stability)

```c
591  abendrpt(ESTAE_CREATE, DUMP_DEFAULT);     /* worker installs ESTAE once */
...
605  if (httpc && strcmp(httpc->eye, HTTPC_EYE)==0) {
606      while(httpc->state != CSTATE_CLOSE) {
607          http_process_client(httpc);        /* <-- abend here */
608          if (work->state == CTHDWORK_STATE_SHUTDOWN) break;
609      }
612      http_close(httpc);                     /* <-- SKIPPED on abend */
613  }
```

The ESTAE installed by `abendrpt` does **not** retry — it captures a dump and
percolates (verified in `libc370/src/clib/@@abrpt.c`):

```c
360  /* set RC=0, RETRY=NULL, RETREGS=NO */
361  SETRP(sdwa,0,0,0);
366  return 0;   /* 0 = continue with abend (NOT retry) */
```

- **Problem path:** Any abend in HTTPD **core** code — `httpin` / `httppars` /
  `httpget` static serving / `httpcred` / `httpsend` / `httpdone` / `httppc`
  dispatch — i.e. anything **not** reached through `httplink → __linkds` (CGI
  abends *are* caught by `__linkds`'s ESTAE, which returns `rc < 0` so the loop
  reaches `CSTATE_CLOSE` and `http_close` runs). On a core abend the worker
  subtask terminates and `http_close()` at line 612 never runs. The thread
  manager spawns a replacement worker that has **no reference** to the orphaned
  `httpc`. There is a reproducible in-tree trigger: `httpget.c:36` `/abend` does
  `__asm__("DC H'0'")` (S0C1) in core, and **H1** is a remote trigger.
- **Leaked per abend:** the 4096-byte `HTTPC` (`calloc`, `httpd.c:516`); its
  entire `env` HTTPV array; its lazily-created `ufs` session (`ufsnew`,
  `httpgufs.c:14`); any open `ufp`; **the client socket is never closed**
  (FD leak, bounded by `FD_SETSIZE`). Additionally, the worker added `httpc` to
  `httpd->busy` (`httppc.c:28`) and abended before `http_reset_busy`
  (`httppc.c:149`), and `http_close`/`httpclos` never touch `busy` — so
  `httpd->busy` permanently retains a **stale pointer** into freed/orphaned
  storage (later traversals of `busy`, e.g. status display, read a dead block).
- **Impact:** Each core abend leaks an HTTPC + a socket FD and corrupts the busy
  set. Under H1 (or any other core fault) this is a remote resource-exhaustion
  DoS: FDs run out → `accept()` fails → with **M5** the listener then dies.
- **Recommendation:** Wrap the per-request processing in the project's
  ESTAE-protected `try()`/`tryrc()` (`clibtry.h`, already used at `httpd.c:903`
  and in mvsMF's `router.c`). On non-zero `try()` rc, run the existing cleanup
  (`http_close(httpc)` — which already releases env/ufs/ufp/socket and removes
  the client from `httpd->httpc`) **and** `http_reset_busy(httpc)` before the
  worker loops for the next item. This realizes the documented contract ("an
  abend kills only the affected worker") *without* leaking the request's
  resources. Keep `abendrpt` for the diagnostic dump.

---

## Medium

### M1 — Credential array has no reaper → unbounded growth

- **Location:** `credentials/src/crednew.c:10` (+ no reader anywhere) · **Severity:** Med · **Confidence:** confirmed · **Type:** Design gap (memory stability)

`cred->last = time64(NULL)` is written once at creation and **never read** —
verified: no reader of `->last` exists in `src/` or `credentials/src/`, and
there is no idle-expiry / reaper / `MODIFY REFRESH` path. A `CRED` (≈80 bytes)
plus a RACF **ACEE** is added to the process-wide credential array on every
distinct successful `(addr, userid, password)` login and is removed **only** by
an explicit `/logout` carrying that session's exact `Sec-Token` cookie, or at
shutdown (`cred_free_array`, which `src/` never calls).

- **Impact:** A client that never logs out, or re-logs-in after a password
  change, leaves the prior CRED + ACEE resident forever. On a long-running,
  16 MB-constrained server this is monotonic growth of storage and ACEE control
  blocks — a slow leak by accumulation.
- **Recommendation:** Use the already-present `cred->last`: refresh it on each
  `cred_find_by_token` hit, and add a periodic sweep (the server already has a
  timer/worker model) that `array_del` + `cred_free`s entries idle beyond a
  configurable TTL; or cap the array size. This is the intended cache design —
  it just needs eviction.

### M2 — `cred_free` releases storage before releasing its lock

- **Location:** `credentials/src/credfree.c:39-48` · **Severity:** Med · **Confidence:** confirmed · **Type:** Bug (concurrency)

```c
39   memset(c, 0xFE, sizeof(CRED));   /* wipe */
42   free(c);                         /* storage returned to heap... */
45   unlock(c, LOCK_EXC);             /* ...but the ENQ on &c is released AFTER */
48   *cred = NULL;
```

- **Problem path:** `cred_free()` (reached from `/logout`, from the
  `array_add`-failure cleanup in `credlin.c`, and from the shutdown sweep). The
  lock is an MVS ENQ keyed on the **pointer value** (`LOCK.%08X`). Between
  `free(c)` and `unlock(c)` the block is back on the heap free-list while its
  ENQ is still held. If another worker `malloc`s the same address and
  `trylock`s it, it gets rc=4 ("busy") off the stale ENQ — in `cred_free` that
  makes the caller leak; in the shutdown sweep it trips the false "still locked"
  re-insert branch. (`unlock` only formats the pointer value into the ENQ name;
  it does not dereference freed storage, so this is a lock-protocol defect, not
  a wild read.)
- **Recommendation:** Reorder to **unlock before free**:
  `memset → unlock(c, LOCK_EXC) → free(c) → *cred = NULL`. One-line swap.

### M3 — SSI `ssi_printv` uses unbounded `vsprintf`

- **Location:** `src/httpfile.c:595, 598` · **Severity:** Med · **Confidence:** confirmed · **Type:** Bug (overflow)

```c
595      char	buf[4096];
598      len = vsprintf(buf, fmt, args);   /* not vsnprintf */
```

- **Problem path:** `ssi_printv()` (every `ssi_printf`). `vsprintf` has no bound.
  Most callers use small fixed formats, but `ssi_printenv()` emits one row per
  env var — `ssi_printf(httpc, " <tr><td>%s</td><td>%s</td>...", v->name, v->value)`
  over **all** `httpc->env` entries, which include `HTTP_*` request headers
  (attacker-controlled, bounded only by ~4000). A long request header + a served
  `.shtml`/`text/x-server-parsed-html` file containing `<!--#printenv-->` (or
  `<!--#echo-->` of a long value) can drive the formatted row past 4096 → stack
  overflow. Requires SSI to be enabled and an SSI document, so narrower than H1.
- **Recommendation:** `vsnprintf(buf, sizeof(buf), fmt, args)` and handle
  truncation (the surrounding code already inspects `len`).

### M4 — Queue-add failure leaks the accepted HTTPC + socket

- **Location:** `src/httpd.c:551` · **Severity:** Med · **Confidence:** confirmed · **Type:** Bug (alloc-failure leak)

```c
551  cthread_queue_add(mgr, httpc);   /* return value ignored */
```

`cthread_queue_add` returns `-1` if its `CTHDQUE` wrapper `calloc` fails, and
returns 0 **without enqueuing** if `mgr->state` flipped to QUIESCE/STOPPED after
the check at line 544 (TOCTOU). In either case `httpc` is neither queued nor
freed → the 4 KB HTTPC and its accepted socket leak — case (a) precisely when
memory is already scarce.

- **Recommendation:** `if (cthread_queue_add(mgr, httpc)) http_close(httpc);`
  (`http_close` on an un-enqueued HTTPC just frees it + closes the socket; env
  and ufs are still NULL here, so it is safe).

### M5 — Listener thread dies permanently on a transient accept/calloc error

- **Location:** `src/httpd.c:498-502, 516-520` · **Severity:** Med · **Confidence:** confirmed · **Type:** Bug (availability)

```c
498  if (sock<0) { ... goto quit; }            /* exits the whole accept loop */
...
516  httpc = calloc(1, sizeof(HTTPC));
517  if (!httpc) { wtof("HTTPD999E Out of memory!"); goto quit; }
```

A single transient `accept()` error, or one transient HTTPC `calloc` failure,
does `goto quit`, which exits the `socket_thread` accept loop **for good** — the
server then accepts no further connections. No memory leaks here (this is an
availability defect), but it is the failure that an FD exhaustion from H2 leads
into.

- **Recommendation:** `continue` (retry the select/accept loop) for transient
  errors; reserve loop exit for the SHUTDOWN/QUIESCE flags. Pair with a small
  backoff if `accept` errors are storming.

### M6 — Non-blocking `EWOULDBLOCK` mid-send busy-spins the worker

- **Location:** `src/httpd.c:606-609` + `src/httpsend.c:24-25` · **Severity:** Med · **Confidence:** suspected · **Type:** Bug (CPU/throughput, soft-DoS)

Sockets are non-blocking (`ioctlsocket(sock, FIONBIO, …)`, `httpd.c:509`). On
`EWOULDBLOCK`, `send_raw` returns the partial count without advancing state, so
`http_send_file` makes no progress and `httpc->state` stays `CSTATE_GET`. The
worker loop (`while(state != CSTATE_CLOSE) http_process_client(httpc);`) has no
`select`/yield between iterations, so a slow or stalled client makes the worker
**spin at 100% CPU** while holding `httpc->ufp` open — one slow client can pin a
worker and degrade the whole pool.

- **Recommendation:** Don't re-drive a blocked send in a tight loop — gate
  re-entry on writability (the existing `select` in `socket_thread`) and/or
  enforce `CLIENT_TIMEOUT` during transfer so a wedged send eventually
  transitions to `CSTATE_DONE`/`CLOSE`. (Do **not** change the per-byte `recv`
  read path — that is the intentional TCP/IP ring-buffer workaround.)

---

## Low (correctness / hardening / efficiency)

- **L1 — NULL/empty `REQUEST_PATH` deref** · `httpget.c:31,36,47` · Bug.
  `http_get_env` may return NULL; `http_cmp(NULL,"/abend")` (36), `strlen(NULL)`
  (47), `path[len-1]` on `""` all fault. Guard `path` once after line 31.

- **L2 — `crt->crtufs` dangling after `ufsfree`** · `httpgufs.c:17` + `httpclos.c:36` · Latent UAF.
  `http_get_ufs` caches the session in the per-worker `crt->crtufs`; `httpclos`
  frees and NULLs `httpc->ufs` but not the cached alias. Safe today (the next
  `http_get_ufs` overwrites it before any UFS op), fragile if anything in the
  `libc370` UFS layer reads `crt->crtufs` in the idle window. Fix: clear
  `crt->crtufs` in `httpclos` when it equals the freed session.

- **L3 — Reset does not close `fp`/`ufp`** · `httprese.c` · Latent leak.
  Keep-alive RESET frees `env`, retains `ufs` (correct), but never closes
  `fp`/`ufp`. Safe only because every current RESET is preceded by `httpdone`
  (which closes them) or reaches RESET with no file open. A future handler that
  opens a UFS file then RESETs would leak a handle **per request** on every
  persistent connection. Fix: mirror `httpclos.c:34-35`'s defensive close at the
  top of `httprese`.

- **L4 — Operator `D M` / `D TI` NULL-deref** · `httpcons.c:326,377` · Bug.
  `display()` passes `rest = strtok(NULL,"")` (NULL when the verb has no arg)
  into `strtoul(NULL,…)` / `strtol(NULL,…)`, on the console thread (no `try()`
  wrapper). Guard `if (!buf) { wtof(usage); return 0; }` like `s_login`/`s_stats`.

- **L5 — `array_add` return ignored in `http_set_env`** · `httpsenv.c:25` · Bug.
  On OOM, the new HTTPV is neither stored nor freed (orphan the teardown can't
  reach), and `http_del_env` already removed the old one, so the variable
  silently vanishes. Fix: `if (array_add(&httpc->env, v)) { free(v); rc=-1; }`.

- **L6 — Env-var over-allocation** · `httpnenv.c:11` · Optimization.
  `total = sizeof(HTTPV) + namelen + vallen + 2` uses `sizeof(HTTPV)` (16) where
  the layout needs `offsetof(HTTPV,name)` (12) — ~4 wasted bytes **per env var,
  per request**. Fix: `total = offsetof(HTTPV,name) + namelen + 1 + vallen + 1`.
  Small, but it is the one piece of pure per-request memory waste.

- **L7 — `httpdeco` trailing-`%` over-read** · `httpdeco.c:18` · Bug (benign).
  `temp[1] = str[2]` reads one byte past the NUL when `%` is the last char
  (worst case one byte past `httpc->buf`). Value is discarded; still an OOB read
  of an attacker-influenced position. Decode only inside the existing
  `if (str[1] && str[2])` guard.

- **L8 — `http_gets` NULL-buf 1-byte overflow** · `httpgets.c:76-77` · Latent.
  In the `buf==NULL` branch `max==CBUFSIZE` (4000), so an exact-length line + LF
  writes `buf[4000]` (one past `httpc->buf`). **Unreachable today** — both
  callers pass `max=CBUFSIZE-1`. Fix defensively: set `max=CBUFSIZE-1` in the
  NULL branch too.

- **L9 — Unchecked `strdup` in CGI registration** · `httpacgi.c:23-24` · Bug.
  `cgi->path`/`cgi->pgm` from `strdup` are not NULL-checked; `array_add` is
  unconditional, so an OOM at config time registers a half-built entry → later
  NULL-deref at match/link. Config-time only (the strdup'd storage itself is
  intentionally AS-lifetime). Fix: on failure, free the partial entry and don't
  register it.

- **L10 — `Sec-Uri` cookie over-read** · `httpcred.c:395` · Bug (hardening, TSK-108).
  `strncpy(uribuf, buf, sizeof(uribuf))` with `uribuf[256]` does not NUL-
  terminate a ≥256-byte decoded value; `uri=uribuf` is then `%s`-printed.
  Fix: `uribuf[sizeof(uribuf)-1] = 0;`.

- **L11 — Login shown as failure when `base64_encode` fails** · `httpcred.c:415-445` · Bug.
  After a *successful* `cred_login`, if the token encode returns NULL the code
  falls through to the "Invalid userid or password" page — user is authenticated
  (ACEE built, in the array) but told it failed and gets no cookie. Treat encode
  failure as a 500/retry.

- **L12 — Dead store** · `httppc.c:38` · Cleanup. `cred = httpc->cred` is
  overwritten (to NULL) before any use. Remove.

- **L13 — `http_open` always returns NULL** · `httpopen.c` · Footgun. The opened
  handle is communicated only via `httpc->ufp`; all current callers compensate,
  but a future caller trusting the return value would misbehave. Also `mode` is
  only set inside `if (mime)` (safe today only because `http_mime` never returns
  NULL). Return `httpc->ufp` or document the side-effect contract.

- **L14 — Overlapping `strcpy`** · `httpfile.c:229-231` · Bug (UB). `strcpy(tmp, end)`
  where `end` points within `tmp` — undefined behavior. Use `memmove`.

- **L15 — SSI recursion stack depth** · `httpfile.c` SSI · Optimization. `ssi_include`
  recurses to `SSI_LEVEL_MAX = 10`; per level ~`uri[256]`+`tmp[256]`+`save[256]`
  plus leaf `buf[4096]`/`buf[5120]`, against a 64 KB worker stack. Likely within
  budget but tight; verify worst-case depth, or shrink per-frame buffers / lower
  the max.

- **L16 — Per-MODIFY `calloc`** · `httpcons.c:43` · Optimization. Every operator
  MODIFY `calloc(1, cibdatln+2)`s a tiny buffer. Console text is small; a fixed
  stack buffer (`char buf[136]`) avoids the per-command heap churn. (Also: if
  the `calloc` fails the command is silently dropped — add a WTO.)

- **L17 — Chunked short-send desync** · `httpsend.c:54-68` · Bug (correctness).
  The chunked branch only checks `rc < 0`; `send_raw` returns a positive short
  count on `EWOULDBLOCK`, so a partial header/data send corrupts the chunk
  frame. Treat `rc < hdrlen` / `rc < len` as incomplete (ties into M6).

- **L18 — `httpprtv` `buf[5120]`** · `httpprtv.c:12` · Optimization/info. The
  largest transient stack frame in the response path (stack, bounded `vsnprintf`,
  auto-freed — **not** a leak). Live during the deep `worker→httppc→__linkds→CGI
  →http_printf` chain; non-recursive so peak nesting is one frame. Note for the
  stack budget; shrink only if stack-overflow evidence appears.

- **L19 — `cgictx` array not freed at terminate** · `httpd.c:167-168,284-304` · Optimization.
  `httpd->cgictx` is `calloc`'d once at startup and never freed in `terminate()`
  (which frees `httpcgi` and `ufssys`). One-shot, reclaimed at AS end — benign;
  add `free(httpd->cgictx)` for completeness. (The pointed-to context blocks are
  `__getm` AS-lifetime **by design** — not a leak.)

---

## Verified clean / intentional — do **not** re-flag

These were checked and are correct **by design**; future audits should not
re-raise them:

- **Per-byte `recv()`** in `http_getc`/`http_gets` — intentional workaround for
  the MVS 3.8j TCP/IP ring-buffer corruption bug. Must not become bulk `recv()`.
- **`__getm` CGI-context blocks** (`httpgctx.c`) and the **`strdup`s in
  `httpacgi.c`** — allocated once, AS-lifetime, never freed by design.
- **`httpc->cred` is a borrowed pointer**, not owned by the HTTPC. The CRED lives
  in the process-wide credential array; `httprese.c:28` correctly **NULLs** it
  (does not free) on reset; freeing it on reset/close/abend would be a bug.
- **Env-var ownership:** `http_new_env` allocates each HTTPV; the
  `httpc->env` array owns them; `http_set_env` replaces via delete-old-then-add
  (old freed first — no overwrite leak); `array_free(&httpc->env)` frees each
  element **and** NULLs the pointer (verified `@@arfre.c:32`), so the
  keep-alive-then-close sequence has no double-free/UAF.
- **`http_del_env` index math** — `http_find_env` is 1-based, `free(env[indx-1])`
  + `array_del(…, indx)` is correct; freed exactly once.
- **CGI abends** are caught by `__linkds`'s ESTAE (return `rc < 0`), so they
  reach `CSTATE_CLOSE` → `http_close` and do **not** leak the HTTPC (this is the
  contrast that makes H2 specific to *core* abends).
- **`httpclos` / `httpdone`** are a correct teardown net (close `fp`/`ufp`/`ufs`,
  free `env`, close socket, free HTTPC) on the normal and keep-alive-close paths.
- **SMF records** (`httprepo.c`) and **`http_link` parameter lists**
  (`httplink.c`) are stack structs — no per-request heap.

---

## Recommended fix order

1. **H1** — bound the directory-index path copy (removes a remote stack smash).
2. **H2** — wrap per-request processing in `try()` and `http_close` +
   `http_reset_busy` on abend (removes the per-abend HTTPC/socket/busy leak;
   also contains any *other* core fault, including the L-series derefs).
3. **M4, M5** — check `cthread_queue_add`; `continue` instead of `goto quit` on
   transient accept/calloc errors (close the remaining HTTPC/FD leak and the
   listener-death path).
4. **M2** — swap unlock/free in `cred_free` (one line).
5. **M1** — add credential idle-reaping (stops slow accumulation).
6. **M3, M6** — `vsnprintf` in SSI; don't busy-spin on `EWOULDBLOCK`.
7. **L-series** — fold the NULL-guards (L1/L4) and the OOM-orphan (L5) in with
   H2's `try()` net; the rest as cleanup. **L6** (env over-alloc) is the only
   pure memory-efficiency win.

---

*Audit covers the HTTPD core only; the built-in CGI modules
(HTTPDSRV/HTTPJES2/HTTPDSL/HTTPDM/HTTPDMTT) and mvsMF were intentionally
excluded. Every finding above was verified by hand against the cited source.*
