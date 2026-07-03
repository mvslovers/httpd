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
| **Token API** | SPA / z/OSMF clients | `POST` creds once → token back → present token (cookie or `Bearer`) |

The **deterministic token** makes Basic and re-auth cheap: build
`CREDTOK = SHA-256(addr,user,pass)`, `cred_find_by_token()` — a hit reuses the
cached CRED+ACEE (no RACF call); only a miss does `cred_login()`.

**httpd request resolver** (factor the inline `httppc.c:59-73` logic into one
helper): try **Sec-Token cookie → `Authorization: Basic` → `Authorization:
Bearer <token>`**, each → `httpc->cred`.

### 2.2 Per-route auth **policy** (replace the global bitmask)

Each route declares its own policy in the `MOD=` line; the global `LOGIN` stays
as the default:

```
MOD HTTPREXX /*.rxp                              # no auth
MOD MVSMF    /zosmf/*   AUTH=BASIC,TOKEN         # required; accept Basic + token
MOD HTTPDSRV /.dsrv     AUTH=FORM                # required; browser form
MOD ADMIN    /admin/*   AUTH=BASIC RES=FACILITY:HTTPD.ADMIN   # + RACF resource
```

Three axes per route: **whether** (overrides the global default), **method /
challenge** (`FORM` | `BASIC` | `TOKEN` | `ANY`), and an **optional RACF
resource** for `racf_auth(class,resource,attr)` → fine-grained authorization
instead of binary "logged in". `needs_login()` reads the route policy.

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
UCHAR *http_get_userid(HTTPC *);                 /* from httpc->cred */
ACEE  *http_get_acee(HTTPC *);                   /* the RACF ACEE    */
int    http_check_auth(HTTPC *, const char *class,
                       const char *resource, int attr);  /* racf_auth */
```

Then mvsMF **deletes `authmw.c`**: httpd has already resolved `httpc->cred`
(from Basic/token), so mvsMF reads the userid/ACEE and, where needed, calls
`http_check_auth` — **no per-request `racf_login`**, no password in env vars.

### 2.5 z/OSMF compatibility (the token source)

Implement the z/OSMF authenticate contract so the SPA and z/OSMF-compatible
clients work the standard way:
- `POST /zosmf/services/authenticate` → `cred_login`, return the token (body +
  `Set-Cookie`); the token *is* the `Sec-Token` (CREDTOK), just wrapped
  z/OSMF-style (endpoint path, cookie name, JSON).
- Accept that token on later requests (cookie or `Bearer`) → the resolver.
- `DELETE /zosmf/services/authenticate` → `credtok_logout`.

---

## 3. Migration path

### 3.1 httpd (the foundation)
1. **Basic Auth source** + `WWW-Authenticate` challenge + factor the credential
   resolver (cookie → Basic → Bearer). *(no `credentials/` change)*
2. **Token API** (`/zosmf/services/authenticate`) returning the token; accept
   `Bearer`.
3. **Per-route policy** in `MOD=` + `needs_login()` reads it; decouple the
   challenge (form vs 401) from the core.
4. **HTTPX auth export** (`get_userid`/`get_acee`/`check_auth`).

### 3.2 mvsMF
5. **Delete `authmw.c`**; use `http_get_userid`/`http_get_acee` (+
   `http_check_auth` for fine-grained routes). Configure `MOD MVSMF /zosmf/*
   AUTH=BASIC,TOKEN`.

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
- **z/OSMF fidelity:** how close to the real `/zosmf/services/authenticate`
  response/cookies do we need to be for Zowe-CLI compatibility?

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
