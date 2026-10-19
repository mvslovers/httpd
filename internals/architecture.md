# HTTPD -- architecture notes

Reference material moved verbatim from `CLAUDE.md` on 2026-10-19: the 4.0.0
decisions, the HTTP/1.1 framing design and the source file map. Read it before
changing response framing or when looking for where something lives.

## HTTPD 4.0.0 — confirmed changes

These decisions are final:

- **HTTP/1.1 support** — Chunked Transfer Encoding, Content-Length, Persistent Connections (Keep-Alive)
- **Parmlib configuration** — Replace Lua config engine with line-based Parmlib parser (DD:HTTPPRM, FB-80)
- **UFSD-only docroot** — Remove DD-based file serving (`/DD:HTML(...)` paths), static files only from UFS
- **Remove embedded FTPD** — FTP functionality available separately
- **Remove MQTT telemetry** — HTTPT struct, telemetry_thread, mqtt370 dependency
- **Extract HTTPLUA/HTTPREXX** — Lua and REXX CGI handlers moved to separate projects (mvslovers/httplua, mvslovers/httprexx), lua370 dependency removed
- **CGIs disabled by default** — No CGIs are registered unless explicitly configured in Parmlib.

## HTTP/1.1 Design (implemented)

**Response body framing — decision logic:**
0. If the status is body-less (`1xx`, `204`, `304`) → never set Content-Length or
   Transfer-Encoding and never emit a body (RFC 7230 §3.3.1). The chunked fallback in
   `httpprtv.c` skips these; otherwise the `0\r\n\r\n` trailer from `httpdone.c` is an
   illegal body that strict parsers (llhttp/Node) reject.
1. If handler/CGI set Content-Length (e.g. mvsMF's `sendDefaultHeaders()`) → use as-is
2. If static file with known size (UFS `filesize`) → send Content-Length
3. Otherwise → send `Transfer-Encoding: chunked`

**Chunked encoding — transparent in httpsend.c:**
When `httpc->chunked == 1`, every `http_send()` call is automatically wrapped in chunk framing. The terminating `0\r\n\r\n` is sent in `httpdone.c`.

**Keep-Alive:**
- HTTP/1.1: default keep-alive, `Connection: close` to opt out
- HTTP/1.0: default close, `Connection: keep-alive` to opt in
- Configurable idle timeout and max requests per connection

**Impact on CGI modules (mvsMF):** Zero changes needed. mvsMF already sets Content-Length via `sendDefaultHeaders()` for JSON responses. Streaming responses get chunked encoding transparently.

## Key Source Files

**Core HTTP engine:**
- **httpd.c**: Main entry, initialization, socket_thread, worker_thread, operator commands.
- **httpin.c**: Read HTTP request line and headers from socket.
- **httppars.c**: Parse method, URI, query parameters, POST body.
- **httppc.c**: Request dispatch — CGI match → http_link, or static file → http_get.
- **httpget.c**: Static file serving. MIME detection via httpmime.c.
- **httpresp.c**: HTTP response line + standard headers.
- **httpsend.c**: Low-level socket send.
- **httprese.c**: Reset for the next request on a kept-alive connection, or close.
- **httpclos.c**: Release client handle, close socket, free UFS handles.
- **httpopen.c**: File open — UFS only; the DD path is gone (4.0.0).
- **httpmime.c**: MIME type detection from file extension.
- **httpauth.c / httpcred.c**: RACF authentication and credential management.
- **httplink.c**: CGI module loading via MVS LINK SVC.

**Configuration:**
- **httpprm.c**: Parmlib parser — reads `DD:HTTPPRM` (`HTTPD_PARMLIB_DD` in httpd.h). Replaced the Lua-based httpconf.c, which is gone. Startup does not echo the settled configuration; `F HTTPD,D CONFIG` reports it on demand.
- **httpdmsg.h**: the operator message catalog. Every WTO literal, once. See *Conventions* in `CLAUDE.md`.

**CGI infrastructure:**
- **cgistart.c**: Custom `__start` for CGI modules.
- **httpx.c**: HTTPX vector table initialization.

**Built-in CGI modules** — the four `[module.*]` entries besides HTTPD itself:
- **httpdsrv.c**: Server + control-block display (`/.dsrv`)
- **httpdm.c / httpdmtt.c**: Storage display (`/.dm`) and Master Trace Table (`/.dmtt`)
- **abend0c1.c**: Deliberate-abend test CGI

None is registered unless the Parmlib says so.

**Subsystems:**
- **httprepo.c**: SMF recording + simple counters (format in `docs/smf-records.md`)

**Gone, do not look for them:** httpconf.c and the Lua runtime (→
mvslovers/httplua), httprexx.c (→ mvslovers/httprexx), the FTP daemon (`ftp*.c`
→ mvslovers/ftpd), the MQTT telemetry (HTTPT, telemetry_thread), and the
hello.c / test.c demos. httpjes2.c and httpdsl.c live in `tbd/`, outside the
build — mvsMF's jobs and dataset APIs replaced them.
