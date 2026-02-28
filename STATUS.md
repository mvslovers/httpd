# HTTPD Status / Open Items

## Open

- **ZLIB370 dependency**: HTTPDSL needs ZLIB370.NCALIB (see Rayborn's LKEDDSL.JCL:
  `SYSLIB` includes `MDR.ZLIB370.NCALIB`, `INCLUDE SYSLIB(ZUTIL)`).
  We don't have zlib370 yet. HTTPDSL will link without it but SSI gzip
  compression won't work. Needs its own project or vendored build.
