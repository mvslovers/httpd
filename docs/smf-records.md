# HTTPD SMF Records

HTTPD writes SMF records for request auditing, performance analysis, and session tracking. Records use the standard 18-byte SMF base header followed by HTTPD-specific fields.

## Configuration

```
SMF=ALL TYPE=243
```

See [configuration.md](configuration.md) for the level matrix (NONE / ERROR / AUTH / ALL) and the configurable record type.

## Record Types

| Subtype | Name | Written When | Purpose |
|---------|------|-------------|---------|
| 1 | Request | After each completed HTTP request | What was called, by whom, how long it took |
| 2 | Session | When a TCP connection closes | How efficiently the client used Keep-Alive |

## SMF Base Header (18 bytes)

All SMF records start with the standard MVS header. Note that SVC 83 strips the first 4 bytes (RDW: reclen + segdesc) when writing to SYS1.MANx. Records read from SYS1.MANx start at offset 0x04 of the struct.

| Offset | Length | Field | Description |
|--------|--------|-------|-------------|
| 0x00 | 2 | reclen | Record length (including this field) |
| 0x02 | 2 | segdesc | Segment descriptor (always 0) |
| 0x04 | 1 | sysiflags | System indicator flags (0x02) |
| 0x05 | 1 | rectype | Record type (default 243, configurable) |
| 0x06 | 4 | time | Time since midnight in 1/100 seconds |
| 0x0A | 1 | dtepref | Date prefix (1 = year >= 2000) |
| 0x0B | 3 | date | Date in packed decimal 0CYYDDD |
| 0x0E | 4 | sysid | System ID from SMCA (e.g. "MVSC") |

## Subtype 1: Request Record

Written after each completed HTTP request (when the SMF level permits).

| Offset | Length | Type | Field | Description |
|--------|--------|------|-------|-------------|
| 0x12 | 8 | char | subsys | Subsystem ID, padded with spaces. `"HTTPD   "` for core, `"MVSMF   "` for mvsMF, `"HTTPLUA "` for Lua module. |
| 0x1A | 2 | short | subtype | Always 1 for request records. |
| 0x1C | 8 | char | userid | RACF user ID (space-padded). Blank if no authentication. |
| 0x24 | 4 | uint | client_addr | Client IPv4 address in network byte order. |
| 0x28 | 4 | uint | resp_code | HTTP response status code (e.g. 200, 404). |
| 0x2C | 4 | uint | bytes_sent | Response body bytes sent to client. |
| 0x30 | 4 | uint | duration_us | Request duration in microseconds (from first byte received to last byte sent). |
| 0x34 | 8 | char | method | HTTP method (e.g. `"GET     "`, `"POST    "`), space-padded. |
| 0x3C | 64 | char | uri | Request URI, truncated to 64 bytes if longer. |

Total record size: 124 bytes (including 18-byte base header).

### Example: Decoded Request Record

```
Subtype 1 (REQUEST)  06:53:55
  subsys="HTTPD   " resp=200 bytes=13512 duration=271.4ms
  method=GET uri=/jesst.html ip=192.168.0.23 userid=(none)
```

## Subtype 2: Session Record

Written when a TCP connection is closed (only at SMF level ALL).

| Offset | Length | Type | Field | Description |
|--------|--------|------|-------|-------------|
| 0x12 | 8 | char | subsys | Always `"HTTPD   "`. |
| 0x1A | 2 | short | subtype | Always 2 for session records. |
| 0x1C | 8 | char | userid | Last authenticated RACF user on this connection. |
| 0x24 | 4 | uint | client_addr | Client IPv4 address. |
| 0x28 | 4 | uint | connect_time | Time of TCP connect (1/100s since midnight). |
| 0x2C | 4 | uint | duration_us | Total session duration in microseconds. |
| 0x30 | 4 | uint | request_count | Number of HTTP requests served on this connection. |
| 0x34 | 4 | uint | total_bytes | Total response bytes sent across all requests. |

Total record size: 56 bytes (including 18-byte base header).

### Example: Decoded Session Record

```
Subtype 2 (SESSION)  06:54:01
  conn@06:53:55 reqs=3 bytes=64976 duration=6000.0ms
  ip=192.168.0.23 userid=(none)
```

A session with `request_count=1` means the client did not use Keep-Alive (or only made one request). Sessions with higher counts show effective Keep-Alive usage. The duration includes idle time between requests (up to `KEEPALIVE_TIMEOUT`).

## Reading SMF Records

SMF records are written to SYS1.MANx via SVC 83. To read them:

1. Dump SYS1.MANx with `IFASMFDP` (SMF Dump Program)
2. Records in the dump start at struct offset 0x04 (RDW is stripped by SVC 83)
3. Filter by record type (default 243) and subsystem ID

### Byte Order

All multi-byte fields are in big-endian (network byte order), which is native on S/370. IPv4 addresses are in standard network byte order (e.g. `C0A80017` = 192.168.0.23).

### Decoding Time Fields

The `time` field in the base header is hundredths of a second since midnight:

```
hours   = time / 360000
minutes = (time / 6000) % 60
seconds = (time / 100) % 60
```

### Decoding Date Fields

The `date` field is packed decimal 0CYYDDD where C is the century indicator (0 = 1900s, 1 = 2000s), YY is the year, and DDD is the day of year. Check `dtepref` to determine if the century bit is set.
