# HTTPD / mvsMF Authentication — Analysis & Redesign

**Status:** design / planning (2026-07-04). Not yet implemented.
**Scope:** the auth story across **httpd** (core + `credentials/`) and **mvsMF**
(the z/OSMF-clone CGI + its `static/` SPA). Goal: **one credential model,
several credential sources, per-route policy** — and retire mvsMF's parallel
auth track.

This document is the basis for a joint httpd ↔ mvsMF review. See also the
`docs/refactoring-backlog.md` *Security architecture* section and the
`credential-package-unification` memory.

---

## 1. Current state (Ist) — grounded in the code

### 1.1 httpd core + `credentials/`

- **Credential store** (`credentials/`, AS-wide WSA `cred_array()`): a `CRED`
  (80 B) = `{ last, token, id, acee*, flags }`. `CREDID = (addr, userid,
  password)` with userid/password **Blowfish-encrypted** in memory.
  `CREDTOK = SHA-256(CREDID)` (32 B) — **deterministic** (same login → same
  token). RACF `ACEE` per login.
- **The only auth surface is the form flow:**
  - `GET /login` → HTML form (`httpcred.c` `print_login`/`print_body`).
  - `POST /login` → `cred_login(addr,user,pass)` = `racf_login` + `cred_new` +
    `array_add`; delivers `Set-Cookie: Sec-Token=<base64(CREDTOK)>` (303).
  - Per request: `httppc.c:71` reads the `Sec-Token` cookie →
    `cred_find_by_token()` → `httpc->cred`.
  - `/logout` → `credtok_logout()`.
- **M2 (done, PR #95):** idle CRED+ACEE reaper (`SESSION_TIMEOUT`), `cred->last`
  refreshed on each `cred_find_by_token` hit.
- **No** `Authorization` header handling, **no** Basic Auth, **no** token API,
  **no** `WWW-Authenticate` challenge (401 exists as a status only).
- **Login policy is coarse:** a global `LOGIN` bitmask (`HTTPD_LOGIN_ALL/CGI/
  GET/HEAD/POST`). `httpcgi` has a per-CGI `login` flag, **but** `httpprm.c`
  `parse_mod()` sets it uniformly from the global flag
  (`int login = httpd->login & HTTPD_LOGIN_CGI;` → every CGI gets the same).
  So **httprexx and mvsMF cannot differ**, and it is binary (login-or-not) with
  no method and no RACF resource.
- **CGI exposure:** `httpcgi.h` declares `CRED` **opaque**; `HTTPC.cred` is
  passed to the CGI, but there is **no** `cred_*`/`racf_auth` in the HTTPX
  vector — a CGI cannot use the resolved session.

### 1.2 mvsMF — its own parallel auth (`src/authmw.c`)

`authentication_middleware(session)`:
- reads `HTTP_Authorization`, requires `Basic `, base64-decodes, splits
  `user:pass`;
- `validate_user()` → **`racf_login(user,pass)` on every request**, then
  `racf_set_acee(acee)` + stores the ACEE in the session;
- stores the credentials in env vars **`HTTP_CURRENT_USER` /
  `HTTP_CURRENT_PASSWORD`** (the password, in the clear, in an env var);
- on any failure: `HTTP_STATUS_UNAUTHORIZED` (401) **without** a
  `WWW-Authenticate` header.

It **bypasses `credentials/` entirely** — no `CRED`, no token, no caching → a
RACF login SVC per API call, and ACEE churn.

### 1.3 mvsMF SPA (`static/js/login.js`, `systems.js`)

- "Login" = `checkSystem(sys, {user,pass})` → `GET /zosmf/info` with
  `Authorization: Basic btoa(user:pass)` + `X-CSRF-ZOSMF-HEADER: true`. A `200`
  means "logged in", `401` "auth failed".
- On success it stores **`Session.user` and `Session.pass` (the raw
  password)** in JS.
- The API layer (`Session`, `systems.js:157-160`) attaches
  `Authorization: Basic btoa(user:pass)` to **every** request.
- Logout = `location.reload()` (drop JS state only).

### 1.4 Problems

- **Two auth tracks** (httpd form/cookie vs mvsMF Basic) that share nothing.
- **Password lifetime:** kept in the browser for the whole session and in an
  httpd env var per request.
- **Cost:** `racf_login` SVC on **every** mvsMF request (no session/token).
- **No token issuance / expiry / real logout** for the SPA.
- **Coarse policy:** can't say "httprexx: none, mvsMF: required".
- **Presentation baked in:** the HTML login form is hard-wired in the httpd
  core; a REST/SPA deployment neither needs nor wants it.

---

## 2. Target model (Soll)

### 2.1 One store, three credential *sources*

Keep the single `credentials/` store (CRED+ACEE, deterministic token, M2
reaper). Feed it from **three sources**, all resolving to the same
`cred_find_by_token` / `cred_login`:

| Source | For | Flow |
|--------|-----|------|
| **Form** | browser (optional) | HTML form → `POST /login` → `Set-Cookie: Sec-Token` |
| **Basic** | REST / curl / Zowe CLI | `Authorization: Basic` → resolve-or-create by token |
| **Token** | SPA / z/OSMF clients | login endpoint (Basic, **owned by mvsMF**) hands back our token as `LtpaToken2`; later requests present it (cookie or `Bearer`) |

> **Division of labour (corrected 2026-07-04):** the **login/logout endpoints
> live in mvsMF** (`POST`/`DELETE /zosmf/services/authenticate`), **not** in
> httpd. httpd owns only the *mechanism*: it resolves the Basic credentials on
> the login call (source **Basic**, already done — #96), **exposes the resulting
> token** to the CGI (HTTPX `http_get_token`, §2.4), and **accepts that token
> back** on later requests (resolver, §2.5). The "LTPA token" is just our
> internal `CREDTOK` wrapped in a `LtpaToken2` cookie — opaque to clients.

The **deterministic token** makes Basic and re-auth cheap: build
`CREDTOK = SHA-256(addr,user,pass)`, `cred_find_by_token()` — a hit reuses the
cached CRED+ACEE (no RACF call); only a miss does `cred_login()`.

**httpd request resolver** (factor the inline `httppc.c:59-73` logic into one
helper): try **Sec-Token cookie → `Authorization: Basic` → `Authorization:
Bearer <token>`**, each → `httpc->cred`.

### 2.2 Per-route auth **policy** — a 2-stage gate in the pipeline

Each **route** (a path prefix) declares its own policy; the global `LOGIN` stays
as the default. Two kinds of route carry the *same* policy:

- **`MOD=`** — a route **with a program** (CGI), as today.
- **`LOC=`** — a route **without a program**: a path prefix served statically
  (falls through to `httpget`) that still carries auth policy. This is the piece
  the SPA / static HTML deployments need.

```
MOD HTTPREXX /*.rxp                                        # CGI, no auth
MOD MVSMF    /zosmf/*  AUTH=BASIC,TOKEN RES=FACILITY:MVSMF.ACCESS   # CGI + RACF/RAKF resource
MOD HTTPDSRV /.dsrv    AUTH=FORM                           # CGI, browser form
LOC /admin/*           AUTH=BASIC       RES=FACILITY:HTTPD.ADMIN    # STATIC subtree behind a profile
LOC /                  AUTH=NONE                           # public (the SPA login page must load)
```

Axes per route: **whether** (overrides the global default), **method / challenge**
(`FORM` | `BASIC` | `TOKEN` | `ANY`), and an **optional RACF/RAKF resource**
(`RES=class:resource`).

**The check runs in httpd's request pipeline — not in the CGI** — so it applies
**uniformly to static and CGI routes**. Enforced as a **2-stage gate** before
httpd serves a file *or* dispatches a CGI:

1. **Authenticate** → resolve `httpc->cred` (cookie / Basic / token). Missing →
   **401** (+ challenge per method).
2. **Authorize** → if the route has `RES=`, call
   `racf_auth(httpc->cred->acee, class, resource, attr)`. Denied → **403**.

A CGI (mvsMF) can still do *finer* checks via `http_check_auth` (§2.4, e.g.
per-dataset RACHECK), but the coarse route-level gate is httpd's — which is why a
**static** route (`LOC=`) can be protected by a RACF/RAKF profile exactly like a
CGI. `needs_login()`/the route lookup drive both stages.

*Notes:* RAKF implements SAF, so `racf_auth` (RACHECK/FRACHECK, `safp.h`) works
for defined resources — it only needs the ACEE, which comes from `httpc->cred`.
For the SPA, the assets are usually **public** (`LOC / AUTH=NONE` — you must load
the login page to log in) and the **API** routes are protected; `LOC=`+`RES=` is
for gating a *whole* static deployment / admin area behind a profile.

### 2.3 Presentation decoupled (mechanism ≠ UI)

- **Core = mechanism:** resolve credential, `cred_login`, `credtok_logout`, and
  a *neutral* "auth required" outcome.
- **Presentation = policy:** `AUTH=BASIC`/`TOKEN` → `401 + WWW-Authenticate:
  Basic realm="…"`; `AUTH=FORM` → the form. The **form itself** becomes
  optional/replaceable (a resource/CGI or template), not hard-wired — so the
  **SPA keeps its own login UI** and a pure-REST deployment has none.

### 2.4 HTTPX auth export (so mvsMF drops `authmw.c`)

Append to the HTTPX vector (append-only) a small auth contract, e.g.:

```c
UCHAR   *http_get_userid(HTTPC *);               /* from httpc->cred        */
ACEE    *http_get_acee(HTTPC *);                 /* the RACF ACEE           */
CREDTOK *http_get_token(HTTPC *);                /* the session token       */
int      http_logout(HTTPC *);                   /* credtok_logout()        */
int      http_check_auth(HTTPC *, const char *class,
                         const char *resource, int attr);  /* racf_auth     */
UCHAR   *http_get_password(HTTPC *, UCHAR *out, unsigned outlen);
                                                 /* decrypted pw (INTRDR)   */
```

Then mvsMF **deletes `authmw.c`**: httpd has already resolved `httpc->cred`
(from Basic/token — the resolver runs on every request), so mvsMF **reads** the
userid/ACEE, gets the **token** to hand back as `LtpaToken2`, calls `http_logout`
for its `DELETE` handler, and (where needed) `http_check_auth` — **no
per-request `racf_login`**, no password in env vars, and no bespoke
credential-store logic. mvsMF does **not** call `cred_login` itself; it just
reads what httpd resolved.

### 2.5 The token source — mvsMF owns the endpoint, httpd owns the token

The **login/logout endpoints live in mvsMF** (`POST`/`DELETE
/zosmf/services/authenticate`); httpd only supplies and accepts the token.

- **Login** (`POST`, mvsMF): the client sends `Authorization: Basic`. httpd's
  resolver (#96) resolves it → `httpc->cred`. mvsMF's handler reads the token
  via `http_get_token()` and returns `Set-Cookie: LtpaToken2=<base64(CREDTOK)>`
  (+ JSON per the z/OSMF spec). If `httpc->cred` is NULL (bad creds), mvsMF
  returns its own z/OSMF-shaped 401. httpd must **not** gate the authenticate
  route (it's the default; #98 makes it explicit).
- **Later requests** (httpd): the resolver **accepts the token back** — a
  `LtpaToken2` cookie (and/or `Authorization: Bearer <token>`) → decode →
  `cred_find_by_token`.
- **Logout** (`DELETE`, mvsMF): calls `http_logout()` (→ `credtok_logout`),
  expires the cookie.

The `LtpaToken2` is our internal `CREDTOK` (base64), **opaque** to clients —
Zowe/SPA store and replay it as-is. Real WebSphere-LTPA encoding is **not**
implemented; if we ever need a structured/validated token, go straight to
**JWT** rather than emulating LTPA.

**Authoritative wire contract (IBM z/OSMF "Basic authentication" docs — pins the
exact flow):**
1. First request carries `Authorization: Basic <base64(userid:password)>`.
2. On success the response is **HTTP 200** with an **`LtpaToken2`** value (IBM's
   real one "supports strong encryption"; ours is the opaque `CREDTOK`).
3. Subsequent requests supply the token via the **`Cookie`** header —
   `Cookie: LtpaToken2=<tokenvalue>` — **instead of** the Basic header.

So the standard transport is the **`LtpaToken2` cookie** — that is what httpd's
resolver (§2.5 / #97) and Zowe/the SPA rely on; `Authorization: Bearer` is an
optional convenience, not part of the z/OSMF contract. Note our `CREDTOK` is a
**one-way `SHA-256` session id** (it does not carry a recoverable password), so
opaque replay is a reasonable session token even without LTPA-style content
encryption; its only weakness is that it is *deterministic / not rotated* — see
the token-model note in §4.

---

## 3. Migration path

### 3.1 httpd (the foundation)
1. ✅ **(done — #96)** **Basic Auth source** + `WWW-Authenticate` challenge +
   factor the credential resolver (cookie → Basic → Bearer). *(no `credentials/`
   change)*
2. ✅ **(done — #97)** **Accept the session token back** on later requests:
   extend the resolver to read a `LtpaToken2` cookie (and/or `Authorization:
   Bearer <token>`) → `cred_find_by_token`. *(The `/zosmf/services/authenticate`
   endpoint itself is mvsMF's — httpd only accepts the token and exposes it via
   4.)*
3. ✅ **(done — #98)** **Per-route policy**: `MOD=` (CGI) **and** `LOC=` (static
   prefix) carry the same policy; a **2-stage gate in the pipeline** —
   authenticate (→401) then, if `RES=` is set, `racf_auth` (→403) — applied
   *before* serving a file or dispatching a CGI, so static/SPA routes get
   RACF/RAKF protection too. Decouple the challenge (form vs 401) from the core.
   *(`AUTH=` is `NONE`/`FORM`/`BASIC`; a route without `AUTH=` inherits the
   global `LOGIN` default.)*
4. ✅ **(done — #99)** **HTTPX auth export** (`get_userid`/`get_acee`/
   `get_token`/`logout`/`check_auth`).

### 3.2 mvsMF
5. Implement `POST`/`DELETE /zosmf/services/authenticate` (mvslovers/mvsmf#162):
   the login handler reads the httpd-resolved token via `http_get_token` and
   returns it as `LtpaToken2`; the logout handler calls `http_logout`. **Delete
   `authmw.c`**; read userid/ACEE via the export (`http_check_auth` for
   fine-grained routes). Configure `MOD MVSMF /zosmf/* AUTH=BASIC,TOKEN`.

### 3.3 SPA (`static/`)
6. Login: `POST` creds **once** to the authenticate endpoint → store the
   **token** (not the password); drop `Session.pass`.
7. API layer: send the **token** (cookie or `Bearer`), not
   `Basic btoa(user:pass)` per call.
8. Logout: call the authenticate `DELETE` (real server-side logout via
   `credtok_logout`).

---

## 4. Open decisions
- **Token transport:** cookie (`Sec-Token` / z/OSMF `LtpaToken2`) vs
  `Authorization: Bearer` vs both (CSRF vs cross-origin trade-offs; the SPA
  already sends `X-CSRF-ZOSMF-HEADER`).
- **Challenge heuristic** when auth is missing: `Authorization` header present →
  401; else form (simplest) — vs `Accept`/path/per-route.
- **Token model:** keep deterministic `SHA-256(addr,user,pass)` (cheap re-auth,
  IP-bound) or add a random session id / rotation / hard max-age (token
  *expiry* independent of idle).
- **Crypto:** Blowfish (64-bit) + weak salt (`cred_init` uses the HTTPD struct /
  object code) — revisit for a unified model.
- **Realm** source (fixed vs Parmlib).
- **~~LTPA fidelity~~ decided (2026-07-04):** `LtpaToken2` carries our **opaque**
  `CREDTOK` (Zowe/SPA replay it as-is). We do **not** emulate real WebSphere
  LTPA; if a structured/validated token is ever needed, go straight to **JWT**.

## 5. Suggested sequencing
An **epic** with sub-issues, foundation-first:
1. httpd: Basic Auth + resolver + `WWW-Authenticate` (unblocks REST/curl now).
2. httpd: token API (`/zosmf/services/authenticate`) + `Bearer`.
3. httpd: per-route policy in `MOD=` + challenge decoupling.
4. httpd: HTTPX auth export.
5. mvsMF: drop `authmw.c`; adopt the export.
6. SPA: token login; drop the stored password.

Steps 1–2 already let the SPA and Zowe authenticate properly and remove the
per-request `racf_login`; 3–6 complete the unification and the fine-grained
authorization.
