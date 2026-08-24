MBT_ROOT := mbt
include $(MBT_ROOT)/mk/mbt.mk

# -- Webroot disk (issue #252) --------------------------------------
# static/ ships as a formatted UFS370 image, because a UFS disk cannot
# travel any of the ways the rest of the package does: SMP has no element
# type for a DSORG=PS/RECFM=U image and must never touch site content, and
# TSO RECEIVE allocates its own target and refuses to merge into an
# existing dataset -- a first-install-only transport for something that
# gets updated.  The operator uploads the image with IND$FILE instead;
# docs/installation.md carries the procedure.
#
# There is deliberately no codepage handling here.  http_send_file()
# translates UFS files with the hard-coded IBM-1047 table, independent of
# the CODEPAGE= setting (src/httpfile.c), and IBM-1047 is what
# `ufsd-utils cp` writes -- measured byte for byte over the whole file,
# UTF-8 sequences included.  Only 0x85 and 0xF7 fail to round-trip, so a
# text file must avoid characters whose UTF-8 encoding contains them.
#
# ufsd-utils is pinned rather than taken from PATH: `create` stamps an
# owner and a timestamp into the image, so the version that builds a
# release artifact is part of what is shipped.  Release CI runs the shared
# mbt workflow and cannot install anything of its own, hence the fetch into
# .mbt/tools/.  Pass UFSD_UTILS=<path> to build against a different one.
# 1M is 256 blocks of 4096, which the inode list caps at 62 files -- the same
# 62 any disk up to 512 blocks gets, so a larger image buys space, not files.
# Owner and group are metadata: UFSD enforces the MOUNT's OWNER(), not the
# inode owner.  They are named anyway, so a release artifact does not carry
# whatever userid happened to build it.
WEBROOT_IMG   := $(DISTDIR)/httpd-webroot.img
WEBROOT_SRC   := static
WEBROOT_SIZE  := 1M
WEBROOT_OWNER := IBMUSER
WEBROOT_GROUP := SYSPROG

UFSD_UTILS_VER := 1.0.1
UFSD_UTILS_BIN := .mbt/tools/ufsd-utils-$(UFSD_UTILS_VER)
UFSD_UTILS     ?= $(UFSD_UTILS_BIN)

# Only the pinned binary is a prerequisite -- an overridden UFSD_UTILS names
# a program on PATH, which make would try (and fail) to build as a file.
WEBROOT_TOOL_DEP := $(if $(filter $(UFSD_UTILS_BIN),$(UFSD_UTILS)),$(UFSD_UTILS_BIN))

$(UFSD_UTILS_BIN):
	$(E) "[webroot] fetching ufsd-utils $(UFSD_UTILS_VER)"
	@mkdir -p $(dir $@)
	@os=`uname -s | tr '[:upper:]' '[:lower:]'`; \
	 arch=`uname -m`; \
	 case "$$arch" in \
	   x86_64|amd64)  arch=amd64 ;; \
	   arm64|aarch64) arch=arm64 ;; \
	   *) echo "[webroot] no ufsd-utils release for $$arch" >&2; exit 1 ;; \
	 esac; \
	 name=ufsd-utils-$$os-$$arch; \
	 curl -sSfL "https://github.com/mvslovers/ufsd-utils/releases/download/v$(UFSD_UTILS_VER)/$$name.tar.gz" \
	   | tar xzOf - $$name > $@ || { rm -f $@; exit 1; }
	@chmod +x $@

webroot: $(WEBROOT_IMG)

$(WEBROOT_IMG): $(shell find $(WEBROOT_SRC) -type f) $(WEBROOT_TOOL_DEP)
	$(E) "[webroot] $(notdir $@) ($(WEBROOT_SIZE), from $(WEBROOT_SRC)/)"
	@mkdir -p $(DISTDIR)
	@rm -f $@
	$(Q)$(UFSD_UTILS) create $@ --size $(WEBROOT_SIZE) --blksize 4096 \
	    --owner $(WEBROOT_OWNER) --group $(WEBROOT_GROUP) > /dev/null
	$(Q)$(UFSD_UTILS) cp -r $(WEBROOT_SRC)/ $@:/

# Both read it: 'package' ships it in the archive ([distribution] extra),
# and 'dist' re-renders that archive on its own.
package dist: webroot

.PHONY: webroot
