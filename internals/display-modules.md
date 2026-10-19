# HTTPD -- display modules (live storage inspection)

Moved verbatim from `CLAUDE.md` on 2026-10-19. The ecosystem-wide part (when
to reach for them, their cost on older builds, `106-0F`) is in
`~/repos/mvs/.workspace/context/httpd-debug.md`; this is the httpd detail.

## Display Modules (live storage inspection)

`context/httpd-debug.md` covers when to reach for these; this is the detail. All are
read-only, all need Basic auth, none is registered unless `DD:HTTPPRM` says so.

**`/.dsrv` — HTTPDSRV, server control blocks.** `?target=` selects the block and
each one is shown as hex plus a named field table:

| target | block | needs `&m=` |
|--------|-------|-------------|
| `HTTPD` | the server singleton (392 bytes, `0x188`) | no |
| `MGR` | `CTHDMGR`, the worker pool manager | no |
| `FS` | `UFSSYS` handle (8 bytes with the libufs stub) | no |
| `MOD` | the route array — every `MOD=` / `LOC=` entry | **yes** |
| `TASK` | a `CTHDTASK` | **yes** |
| `FILE` | a `FILE` handle | **yes** |

The addresses the `&m=` targets want come out of the `HTTPD` block:
`httpcgi` at `+44`, `mgr` at `+34`, `socket_thread` at `+30`, `ufssys` at `+54`.

**`/.dm` — HTTPDM, arbitrary storage.** `?m=<hex>&l=<bytes>&c=<chunk>&t=<title>`
— only `m` is required; `c` is capped at 64; `t` labels the dump header. Every
parameter has short and long aliases (`m`/`mem`/`memory`, `l`/`len`/`length`,
`c`/`s`/`chunk`/`size`, `t`/`title`).

Pointer chasing is the point: read a pointer, feed it back as `m`. `CVTPTR` is
at `0x10`, so `/.dm?m=10&l=16` yields the CVT, and CVT+`0x130` is `CVTTZ`.

**`/.dmtt` — HTTPDMTT, Master Trace Table.** No parameters needed (`?d=1`
toggles a raw-data variant). This is the console log, so it is where RAKF
messages, `HTTPDnnn` WTOs and abends actually show up — check it before
theorising about why a request failed.

**Gone in 4.0.0:** `/dsl/*` (HTTPDSL, dataset lister) and `/jes/*` (HTTPJES2,
JES spool) are no longer built — mvsMF's dataset and jobs APIs replace them.
Their sources sit in `tbd/`, outside the build; see `tbd/README.md`. So the
three display modules above are the whole set now, and `/jes/status` is not a
JES2 cross-check any more — use mvsMF's jobs API.

**Reading the route table:** `?target=MOD` decodes the whole 32-byte `HTTPROUTE`,
so `auth` (`+14`) is named and spelled out — that is the field the request is
gated on, and since #105 the only one: the global `LOGIN` bitmask is retired,
`+09` is a reserved byte, and there is no `AUTH=DEFAULT` any more. A route
showing `AUTH=NONE` is public, whether the line said `AUTH=NONE` or said
nothing. The one line that reads as public but is not is `RES=` without
`AUTH=`: the parser resolves that to `BASIC` before the route is built, so what
`?target=MOD` shows is still what gates the request. And
`AUTH=` never selected a credential *source* — the resolver runs before the
route is matched, so every route accepts every source. It selects whether a
login is needed and how a missing one is challenged: `NONE`, `FORM`, `BASIC`,
and since #121 `TOKEN` (a bare 401, never a challenge, for API routes whose
clients handle the 401 themselves).
