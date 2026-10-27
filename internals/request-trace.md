# HTTPD -- request trace (#350)

A server-side record of what each request did and where it waited, to attribute
a stalled request to a worker and a phase. Off unless the Parmlib says
`TRACE=n` (16 to 1024 records). Operator view: `docs/books/ref/ref-prm.typ`
(keyword) and `ref-dm.typ` (target `TRACE`).

## Design

- One block from the heap, anchored in `HTTPD.trace` (`0x50`, was `unused_50`).
  Nothing is writable module storage (#197). The size is `trc_size(slots)`:
  32 bytes, a hash of `2*slots` words, `slots` records of 128 bytes. At
  `TRACE=256` that is 34,848 bytes. The block is freed after the worker pool and
  the client loop are gone (`httptrc_stop`).
- Every hook is `if (HTTP_TRACING(httpc)) ...`: one load and a branch when the
  trace is off. `HTTPX` and `HTTPC` do not change. The switch is read at the
  start, so comparing the behaviour with and without the trace means a restart.
- A record is claimed with compare-and-swap (`TRS_FREE/DONE` -> `TRS_LIVE`),
  never taken while `LIVE`. A finished request that took `TRC_SLOW` ticks (about
  1 s) goes to `TRS_KEEP` and is only taken when no `FREE/DONE` record is left,
  so a 58 s stall is still there to be read after thousands of fast requests.
  When all records are live, the request is not traced and `lost` counts it.
- The HTTPC has no free field, so a record is found by a small open-addressing
  hash keyed on the HTTPC address (`TRC_PROBE` = 8). An entry is stale when its
  record is not `LIVE` or names another entry in `slot`; a stale entry is
  overwritten.
- A kept-alive connection gets one record per request: `httprese` calls
  `httptrc_next`, which ends the record and opens the next one on the same
  worker. The age of a later request is measured from its request line
  (`TM_LINE1`), so client think time is not counted as slow.

## The marks

`TM_OPEN` (accept, queue) `TM_RUN` (a worker took it) `TM_LINE1` `TM_REQ`
`TM_PARSE` `TM_CRED` `TM_ROUTE` `TM_LINK0` `TM_LINK1` `TM_SND1` `TM_SNDL`
`TM_DONE` `TM_END` `TM_CLOSE`. A mark is set once (`trc_mark`) except `TM_SNDL`,
which follows the last `send()`. Counters per request: `polls` (0.5 s read
waits in `http_gets`), `txn` / `txms` (calls and time in `send()`), `txst`
(pauses in `send_raw_all` when the window is closed).

The hypotheses the marks separate: a wait before a worker takes the connection
(`run` large), reading the request (`polls`, `line1`, `req`), the credential
(`cred`), the module (`lnk0` to `lnk1`, which includes a fetch with
`RESIDENT=NO`), or sending (`txms`, `txst`, `snd1` to `sndl`).

## Clock

`STCK`, shifted right by 22 bits and kept in 32 bits: one tick is 2^10 us =
1.024 ms. The clock itself resolves 1 us; the tick is the coarser of the two.
The 32-bit tick wraps after about 51 days; only differences of two ticks are
used. `trc_ms` converts without overflow (`ticks/1000*1024 + ...`).

## Files

- `include/httptrc.h`, `src/httptrcf.c` -- table, marks and line format. Free
  of `httpd.h` and of the clock: `test/tsttrc.c` runs on the host (`mbt test`)
  and on MVS. `trc_cas` is supplied by the caller (CS on the target, a plain
  compare in the test).
- `src/httptrc.c` -- the clock, `HTTPD`/`HTTPC` side of the hooks, allocation.
- Hooks: `httpd.c` (open, run), `httpgets.c`, `httpin.c`, `httppc.c`,
  `httplink.c`, `httpsend.c`, `httpdone.c`, `httprese.c`, `httpclos.c`.
- Readout: `httpdsrv.c`, `display_trace()`. A record is copied and the copy is
  dropped when its `seq` changed meanwhile (torn read).

## Reading it

`/.dsrv?target=TRACE[&min=ms]` gives `text/plain`. `tests/soak.py
--trace-sec N` polls it during a run (`OUT.trace`); `tests/soak-trace.py` joins
the stalls to the records by client port and time (`wall` and `ago` in the
output give the wall clock of a record's start).

`w=` is the address of the worker's `CTHDWORK`. `tests/soak-workers.py` lists
the workers in creation order with the number of connections dispatched to
each (from `/.dsrv?target=MGR`), which is the denominator a per-worker stall
rate needs: the trace table keeps slow records longer than fast ones, so
counting records per worker overstates the workers that stall.
