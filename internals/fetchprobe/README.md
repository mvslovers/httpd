# Program fetch probe

A tool for measuring how reliable MVS program fetch is on a stand, without
HTTPD. It was written for issue #300 (HTTPD lost 82 pages of storage on every
failed fetch of MVSMF, `IEA703I 106-0F`). The results are kept in that issue,
not here.

## What it does

`LDLP` (`ldlp.asm`, IFOX00) takes `PARM='NAME,COUNT'`. It issues
`LOAD EPLOC=NAME,ERRET=…` followed by `DELETE`, COUNT times, so every load is
a fresh program fetch. Each load counts as one of:

| count | meaning |
|---|---|
| FAIL | fetch failed and took the ERRET exit. The first ten are reported with the R1/R15 the exit received (`R1=00000106 R15=0000000F` for `106-0F`). |
| BAD | fetch succeeded, at the same address as the first load, but the checksum over the module's storage (CDE extent list) differs: wrong contents with no error at all. A word-by-word diff against a kept copy of the first image follows (count, first and last differing offset), then 16 bytes of the good and of the bad image at the first difference. The first ten are reported. |
| MOVED | the module landed at another address, so the sums are not compared and the new address becomes the baseline. After a FAIL this is the storage of the failed fetch, which MVS never gives back. |
| NOCDE | no CDE was found after a successful LOAD. |

Messages go to the job log (WTO, `ROUTCDE=11`). RC is 0 when clean, 4 for
MOVED/NOCDE only, 8 for FAIL or BAD, and 16 for a PARM error.

Use a module **without RLD** (relocation dictionary) if BAD is to mean
anything: relocated adcons change the sum whenever the load address moves.
IEMRA, a PL/I (F) phase on TK5, has no RLD and was the module used for #300.

## Running it

The job is assembled and link-edited on the stand, so all it needs is IFOX00
and `SYS1.MACLIB`.

```
python3 -I probe.py TARGET NAME:COUNT[:DSN] ...
```

- One GO step per argument. A `DSN` is concatenated to that step's STEPLIB
  after the probe's own library; without one the module comes from the link
  list.
- Alternate the sources within one job (`A B A B …`). Rates measured in
  different jobs are not comparable: the stands share one host, and the load
  from the other stands changes the rates.
- `DRYRUN=1` prints the JCL instead of submitting it.
- `TARGET` is a `[target.NAME]` of `~/.mbt/targets.toml`. The password comes
  from there or from the keychain and goes to curl via `-K -`, never on the
  command line. For a throwaway container, `@FILE` names a TOML file holding
  just `url`, `user` and `password` (write it with `umask 077`).
- At about 1.5 ms per load (4 ms through the link list), 250,000 loads take
  6–17 minutes. Fetch failures were in the range of 1 per 100,000 loads, so
  plan on a million loads per side before reading a difference.

Helpers, same conventions:

| script | does |
|---|---|
| `peek.py TARGET JOBNAME JOBID` | job status and LDLP lines of a running job |
| `subjcl.py TARGET FILE.jcl` | submit any job, wait, print the whole spool |
| `xfer.py get\|put TARGET DSN FILE [VOLUME]` | binary download/upload of an XMIT file, for moving a test library between stands |
| `resp.py HLQ` | writes the LINK reuse probe job (below) to stdout |

## Does LINK reuse a loaded copy? (`resp.asm`)

A second probe, written for #250: whether a module held by LOAD is found by
the LINK that HTTPD issues per request. `RESP` LOADs a test module, deletes
its member from the library (`STOW … D`, checked with BLDL), and LINKs to it:
a LINK that still works used the copy in storage, one that fails `806-4` went
to the library. Five cases, each with its own module (`RPRBA`–`RPRBE`): no
LOAD; `LOAD EP=`; the same with a module linked without REUS; LINK from a
subtask (`rprbs.asm`); and the `BLDL` + `LOAD DE=` path of libc370's
`__load()`.

```
python3 -I resp.py HLQ > resp.jcl
python3 -I subjcl.py TARGET resp.jcl
```

The job allocates `HLQ.RESPPRB.XLIB` and deletes it again in its last step.
Every result is a `RESP` or `RPRBS` line in the job log. The results are in
#250.

## Reading the DASD images

Whether a module fails can depend on where its records sit on the track
(#300). Three read-only tools read a compressed CCKD image (`CKD_C370`)
directly on the Hercules host:

| script | does |
|---|---|
| `trkdump.py IMAGE CYL HEAD [NTRK]` | every record (R, KL, DL, first 8 data bytes) on the given tracks |
| `trkfind.py IMAGE HEXPATTERN` | every record whose data starts with the pattern, e.g. a module's first text bytes |
| `dircount.py IMAGE CYL HEAD` | the number of directory blocks of the PDS whose extent starts there |

They only read the image. A track that Hercules still holds in its cache
can be older on disk than the running system sees it, so read freshly
written data sets after a moment, or not at all.
