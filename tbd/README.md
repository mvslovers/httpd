# tbd — retired sources, kept for reference

Nothing under this directory is built. `project.toml`'s `[internal]` pool globs
`src/*.c`, so moving a file here takes it out of the build entirely: it is no
longer compiled, no longer archived, and no load module can autocall it.

These are kept rather than deleted so the code stays readable in place while the
replacements settle. Deleting them is a later step — git history would preserve
them either way, but reading a deleted file is more friction than reading this
directory.

## What is here and why

**HTTPDSL — dataset list browser (`/dsl/*`)**

```
src/httpdsl.c    root, the CGI entry point
src/httpdsld.c   DSCB -> DSLIST record
src/httpdsle.c   error responses
src/httpdsli.c   index page
src/httpdsll.c   HLQ listing (JSON + print)
src/httpdslp.c   PDS member listing
src/httpdslx.c   XMIT/download
include/httpds.h the cluster's private header
```

**HTTPJES2 — JES2 spool browser (`/jes/*`)**

```
src/httpjes2.c    root, the CGI entry point
src/jespr.c       sysout printing
src/jesst.c       status/DD listing
src/jestime.c     ISO 8601 job timestamps
include/jestime.h its header
```

Both are superseded by [mvsMF](https://github.com/mvslovers/mvsmf), whose
dataset and jobs REST APIs cover the same ground. Neither appears in the sample
Parmlib any more, and a `MOD=HTTPDSL` / `MOD=HTTPJES2` line now fails to load
the program at request time.

**stck2tv.c** — unrelated to the two above: it converts an STCK value to a
`struct timeval` and nothing in the tree has ever called it. It came out in the
same pass because the same link test found it.

## Verified before the move

Every module and test still links without these files: the build drops from 6
load modules to 4 (HTTPD, HTTPDM, HTTPDMTT, HTTPDSRV) and the internal archive
from 112 objects to 100. Nothing outside the two clusters referenced them —
`httpds_*` appears only in the DSL files, the JES API (`clibjes2.h`) only in the
JES ones.

The HTTPX vector exports none of these functions, so no external CGI (mvsMF
included) can reach them at runtime. That matters because the vector's offsets
are frozen: had one been exported, removing the source would have broken the
published ABI rather than just shrinking the build.
