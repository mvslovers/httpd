-- static/ ships as a formatted UFS370 image, because a UFS disk cannot travel
-- any of the ways the rest of the package does: SMP has no element type for a
-- DSORG=PS/RECFM=U image and must never touch site content, and TSO RECEIVE
-- allocates its own target and refuses to merge into an existing dataset -- a
-- first-install-only transport for something that gets updated.  The operator
-- uploads the image with IND$FILE instead; docs/installation.md carries the
-- procedure.
--
-- There is deliberately no codepage handling here.  http_send_file()
-- translates UFS files with the hard-coded IBM-1047 table, independent of the
-- CODEPAGE= setting (src/httpfile.c), and IBM-1047 is what `ufsd-utils cp`
-- writes -- measured byte for byte over the whole file, UTF-8 sequences
-- included.  Only 0x85 and 0xF7 fail to round-trip, so a text file must avoid
-- characters whose UTF-8 encoding contains them.
--
-- The plugin's defaults are what the image always was: 1M, block size 4096,
-- owner IBMUSER, group SYSPROG, built before `mbt package` and `mbt dist`.
-- 1M is 256 blocks of 4096, which the inode list caps at 62 files -- the same
-- 62 any disk up to 512 blocks gets, so a larger image buys space, not files.
-- Owner and group are metadata: UFSD enforces the MOUNT's OWNER(), not the
-- inode owner.  They are fixed anyway, so a release artifact does not carry
-- whatever userid happened to build it.
local ufs = require("mvslovers/mbt-ufs")

ufs.webroot { from = "static", image = "build/webroot/httpd-webroot.img" }
