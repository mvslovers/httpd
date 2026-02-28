include config.mk

# C source code files
C_FILES := src/abend0c1.c \
           src/asc2ebc.c \
           src/cgihttpc.c \
           src/cgihttpd.c \
           src/cgistart.c \
           src/dbgdump.c \
           src/dbgenter.c \
           src/dbgexit.c \
           src/dbgf.c \
           src/dbgs.c \
           src/dbgtime.c \
           src/dbgw.c \
           src/ebc2asc.c \
           src/ftpcdsc.c \
           src/ftpcgets.c \
           src/ftpcmsg.c \
           src/ftpcopen.c \
           src/ftpcprtf.c \
           src/ftpcrecv.c \
           src/ftpcsend.c \
           src/ftpcterm.c \
           src/ftpdabor.c \
           src/ftpdcwd.c \
           src/ftpddele.c \
           src/ftpdlist.c \
           src/ftpdmkd.c \
           src/ftpdnoop.c \
           src/ftpdpass.c \
           src/ftpdpasv.c \
           src/ftpdpc.c \
           src/ftpdpcs.c \
           src/ftpdport.c \
           src/ftpdpwd.c \
           src/ftpdquit.c \
           src/ftpdretr.c \
           src/ftpdrmd.c \
           src/ftpdsecs.c \
           src/ftpdstor.c \
           src/ftpdsyst.c \
           src/ftpdterm.c \
           src/ftpdtype.c \
           src/ftpduser.c \
           src/ftpfopen.c \
           src/ftpfqn.c \
           src/ftpslist.c \
           src/hello.c \
           src/http1123.c \
           src/httpacgi.c \
           src/httpatoe.c \
           src/httpauth.c \
           src/httpclos.c \
           src/httpcmd.c \
           src/httpcmp.c \
           src/httpcmpn.c \
           src/httpconf.c \
           src/httpcons.c \
           src/httpcred.c \
           src/httpd.c \
           src/httpd048.c \
           src/httpdbug.c \
           src/httpdeco.c \
           src/httpdel.c \
           src/httpdenv.c \
           src/httpdm.c \
           src/httpdmtt.c \
           src/httpdone.c \
           src/httpdsl.c \
           src/httpdsld.c \
           src/httpdsle.c \
           src/httpdsli.c \
           src/httpdsll.c \
           src/httpdslp.c \
           src/httpdslx.c \
           src/httpdsrv.c \
           src/httpetoa.c \
           src/httpfcgi.c \
           src/httpfenv.c \
           src/httpfile.c \
           src/httpgenv.c \
           src/httpget.c \
           src/httpgetc.c \
           src/httpgets.c \
           src/httpgsna.c \
           src/httpgtod.c \
           src/httphead.c \
           src/httpin.c \
           src/httpisb.c \
           src/httpjes2.c \
           src/httplink.c \
           src/httplua.c \
           src/httpluax.c \
           src/httpmime.c \
           src/httpnenv.c \
           src/httpntoa.c \
           src/httpopen.c \
           src/httppars.c \
           src/httppc.c \
           src/httppcgi.c \
           src/httppcs.c \
           src/httppost.c \
           src/httpprtf.c \
           src/httpprtv.c \
           src/httppubf.c \
           src/httpput.c \
           src/httprbz.c \
           src/httprepo.c \
           src/httprese.c \
           src/httpresp.c \
           src/httprexx.c \
           src/httprise.c \
           src/httprnf.c \
           src/httprnim.c \
           src/httpsay.c \
           src/httpsbz.c \
           src/httpsecs.c \
           src/httpsend.c \
           src/httpsenv.c \
           src/httpshen.c \
           src/httpsndb.c \
           src/httpsndt.c \
           src/httpsqen.c \
           src/httpstat.c \
           src/httpstrt.c \
           src/httpsubt.c \
           src/httptest.c \
           src/httpx.c \
           src/jespr.c \
           src/jesst.c \
           src/lauxlib.c \
           src/liolib.c \
           src/loadlib.c \
           src/stck2tv.c

# Assembler source files
A_FILES :=

SUBDIRS := credentials

include rules.mk

$(SUBDIRS):
	@$(MAKE) -C $@
.PHONY: $(SUBDIRS)

link: all
	@$(ROOT_DIR)scripts/mvslink $(if $(MODULES),$(MODULES),--all)
.PHONY: link

# Generate compile_commands.json for clangd
compiledb:
	@echo "[" > compile_commands.json
	@first=1; \
	for f in $(C_FILES); do \
		if [ $$first -eq 0 ]; then echo "," >> compile_commands.json; fi; \
		first=0; \
		echo "  {" >> compile_commands.json; \
		echo "    \"directory\": \"$(CURDIR)\"," >> compile_commands.json; \
		echo "    \"file\": \"$$f\"," >> compile_commands.json; \
		echo "    \"command\": \"clang -c $(DEFS) $(INCS) -std=c89 $$f\"" >> compile_commands.json; \
		echo "  }" >> compile_commands.json; \
	done
	@echo "]" >> compile_commands.json
	@echo "Generated compile_commands.json"
.PHONY: compiledb
