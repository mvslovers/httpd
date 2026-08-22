#ifndef HTTPDMSG_H
#define HTTPDMSG_H

/**
 * @file httpdmsg.h
 * @brief Operator message catalog (WTO)
 *
 * Every wtof() literal in HTTPD lives here, exactly once.  Adding a message
 * means adding a line to this file, which is what makes an id collision or a
 * duplicated text visible -- inline literals hid both, and had hidden three
 * by the time this file was written (see "What the catalog found" below).
 *
 * Rules, in order of how often they are broken:
 *
 * 1. A WTO is written only for what an operator can act on or must know.
 *    Anything the client caused (a bad request, an unknown route, a file
 *    that is not there) is reported in the HTTP response and nowhere else.
 *    The console line would help nobody, and every line written here also
 *    lands in the Master Trace Table and the hardcopy log.
 *
 * 2. One text per id.  If two call sites need different wording they need
 *    different ids; if they say the same thing with a varying detail, the
 *    detail becomes an argument (see MSG_APF_AUTHORIZED).
 *
 * 3. Literals are upper case -- 3270 console convention.  Substituted values
 *    are passed through unchanged: most are MVS names and already upper
 *    case, but URL paths, UFS paths and free user text must not be folded.
 *    Runtime strings that arrive lower case (MBT_VERSION, MBT_COMMIT,
 *    libc370_version()) go through http_upcase() at the call site.
 *
 * 4. Severity is I, W or E.  No D, no T.  HTTPD900D/902D/903D were debug
 *    counters for issue #159 and are gone with it (#186) -- 903D wrote a
 *    census line every 60 seconds of a healthy server's life, because the
 *    switch it claimed to be behind was on.  Anything claiming to be a debug
 *    probe has to actually be compiled out, or the claim is decoration.
 *
 * 5. No trailing "\n".  wtof() writes one console line; the newline was a
 *    stray character on a 3270 and is gone from every format here.
 *
 * 6. A single-line WTO truncates around 70-90 characters.  Uppercasing is
 *    length-neutral, rewording is not -- keep new text inside the old.
 *
 * Id ranges:
 *
 *   HTTPD0xx   server lifecycle: start, identity, APF, listener, threads, stop
 *   HTTPD1xx   operator console -- MODIFY / DISPLAY output
 *   HTTPD4xx   configuration (DD:HTTPPRM), routes and SMF
 *   HTTPD9xx   diagnostics: abends, recovery, storage, internal faults
 *
 * With one wart, stated rather than tidied away: configuration ALSO occupies
 * HTTPD020-HTTPD048, which predate the 4xx block.  Renumbering them would
 * churn every id an operator has ever seen in a log for no gain, so they stay
 * where they are and both halves are filed under "configuration" below.  If
 * you are looking an id up, search the file -- do not trust the range.
 *
 * The catalog is documented for operators in docs/messages.md; keep the two
 * in step.
 *
 * ---------------------------------------------------------------------
 * What the catalog found (issue #184)
 *
 * Collapsing the literals into one file surfaced three id collisions that
 * had been invisible while the text lived at the call site:
 *
 *   - HTTPD070/071 served BOTH the codepage banner (httpxlat.c) and the
 *     DISPLAY LOGIN output (httpcons.c).  Console output moved to 1xx.
 *   - HTTPD411I carried three unrelated texts (SMF level, counters reset,
 *     SMF level set).  Split across 1xx.
 *   - HTTPD415W was both "SMF type configured but SMF inactive" and
 *     "LOC= requires a path".  SMF moved to 43x.
 *
 * HTTPD020W likewise carried three configuration texts and HTTPD024W two.
 * Both are split below.
 * ---------------------------------------------------------------------
 */

/* =====================================================================
** HTTPD0xx -- server lifecycle
** ================================================================== */

/** HTTPD000I startup banner: version and commit of this build */
#define MSG_STARTING		"HTTPD000I HTTPD %s (%s) STARTING"

/** HTTPD001I the listener is up and the server is accepting requests */
#define MSG_READY		"HTTPD001I HTTPD %s READY - SERVING %s"

/** HTTPD001I as MSG_READY, but no document root: a CGI-only server (mvsMF
 *  and nothing else) is an ordinary deployment, so this is not a warning --
 *  a W on a healthy configuration only teaches operators to ignore Ws. */
#define MSG_READY_NODOC		"HTTPD001I HTTPD %s READY - NO DOCUMENT ROOT"

/** HTTPD002I the STC was already authorized at entry -- the job step was
 *  authorized when program fetch ran, so MVS took the job pack area in
 *  subpool 252 key 0 and the module's own storage is read-only to it.
 *  The key is INFERRED from the route, not measured (issue #197). */
#define MSG_APF_BY_LIB		"HTTPD002I AUTHORIZED BY LIBRARY (MODULE KEY 0)"

/** HTTPD002I the STC authorized itself via SVC 244, which happens after
 *  program fetch and so cannot relabel storage: the module stays key 8. */
#define MSG_APF_BY_SVC		"HTTPD002I AUTHORIZED BY SVC (MODULE KEY 8)"

/** HTTPD003W RACINIT needs key 0 and the STC could not get there */
#define MSG_STCID_NOKEY		"HTTPD003W RACINIT SKIPPED, CANNOT ENTER SUPERVISOR STATE"

/** HTTPD004I the STC dropped its default STC/STCGROUP identity (issue #177) */
#define MSG_STCID_SET		"HTTPD004I STC IDENTITY SET TO %s/%s VIA RACINIT"

/** HTTPD004W logon failed; the server keeps the inherited STC identity */
#define MSG_STCID_FAILED	"HTTPD004W RACINIT ENVIR=CREATE FAILED FOR %s/%s RC=%d"

/** HTTPD005I which C runtime this module actually linked against */
#define MSG_LIBC_VERSION	"HTTPD005I %s"

/** HTTPD006W the tree had uncommitted tracked changes when this was built */
#define MSG_BUILD_DIRTY		"HTTPD006W BUILT FROM A MODIFIED WORKING TREE"

/** HTTPD007I a client completed the form login */
#define MSG_LOGIN_OK		"HTTPD007I USER %-8.8s IP %u.%u.%u.%u LOGIN SUCCESSFUL ACEE(%06X)"

/** HTTPD008I a client's credential was released */
#define MSG_LOGOUT_OK		"HTTPD008I USER %-8.8s IP %u.%u.%u.%u LOGOUT SUCCESSFUL"

/** HTTPD008W logout for a credential that was not found */
#define MSG_LOGOUT_FAILED	"HTTPD008W USER %-8.8s IP %u.%u.%u.%u LOGOUT FAILED"

/*
 * HTTPD010I/011I/013I are no longer written on the healthy path (#184
 * follow-up): that APF was obtained is only interesting when it FAILS, and
 * HTTPD012E covers that.  UFSD and FTPD report the same way.  The ids stay
 * defined -- httpauth.c still references them, and reserving them costs
 * nothing next to reusing one for something else later.
 */

/** HTTPD010I %s is the module name; it was already authorized */
#define MSG_APF_AUTHORIZED	"HTTPD010I %s IS APF AUTHORIZED"

/** HTTPD011I %s is the module name; authorization came from SVC 244 */
#define MSG_APF_VIA_SVC244	"HTTPD011I %s WAS APF AUTHORIZED VIA SVC 244"

/** HTTPD012E without APF the LINK SVC and the RACF services are unavailable */
#define MSG_APF_FAILED		"HTTPD012E %s UNABLE TO DYNAMICALLY OBTAIN APF AUTHORIZATION"

/** HTTPD013I the STEPLIB was authorized so CGI modules can be linked */
#define MSG_APF_STEPLIB		"HTTPD013I STEPLIB IS NOW APF AUTHORIZED"

/*
 * HTTPD014-019 -- __start DD checks (httpstrt.c).  HTTPD writes its own
 * STDOUT/STDERR/STDIN to private DDs so a CGI cannot scribble into the JES
 * data sets the operator reads.  One line per condition; the six-line
 * asterisk banners these replaced said no more and cost six WTOs each.
 */

/** HTTPD014E SYSPRINT present; HTTPD would write STDOUT into it */
#define MSG_DD_SYSPRINT		"HTTPD014E SYSPRINT DD NOT ALLOWED, HTTPD USES HTTPDOUT FOR STDOUT"

/** HTTPD015E SYSTERM present; HTTPD would write STDERR into it */
#define MSG_DD_SYSTERM		"HTTPD015E SYSTERM DD NOT ALLOWED, HTTPD USES HTTPDERR FOR STDERR"

/** HTTPD016E SYSIN present; HTTPD would read STDIN from it */
#define MSG_DD_SYSIN		"HTTPD016E SYSIN DD NOT ALLOWED, HTTPD USES HTTPDIN FOR STDIN"

/** HTTPD017W no HTTPDOUT; STDOUT goes nowhere */
#define MSG_DD_NO_STDOUT	"HTTPD017W HTTPDOUT DD NOT DEFINED, STDOUT IS NOT AVAILABLE"

/** HTTPD018W no HTTPDERR; STDERR goes nowhere */
#define MSG_DD_NO_STDERR	"HTTPD018W HTTPDERR DD NOT DEFINED, STDERR IS NOT AVAILABLE"

/** HTTPD019W no HTTPDIN; STDIN goes nowhere */
#define MSG_DD_NO_STDIN		"HTTPD019W HTTPDIN DD NOT DEFINED, STDIN IS NOT AVAILABLE"

/** HTTPD027I a socket from a previous instance was still bound to the port */
#define MSG_STALE_SOCKET	"HTTPD027I CLOSING STALE SOCKET %d ON PORT %u"

/** HTTPD033E without the socket thread nothing is ever accepted */
#define MSG_NO_SOCKET_THREAD	"HTTPD033E UNABLE TO CREATE SOCKET THREAD"

/** HTTPD034W the pool is empty; only static files can still be served */
#define MSG_NO_WORKERS		"HTTPD034W UNABLE TO CREATE WORKER THREADS, DYNAMIC DOCUMENTS DISABLED"

/** HTTPD040I shutdown is waiting on the socket thread; %d counts the tries */
#define MSG_WAIT_SOCKET_THREAD	"HTTPD040I WAITING FOR SOCKET THREAD TO TERMINATE (%d)"

/** HTTPD041I the socket thread did not end in time and is being detached */
#define MSG_FORCE_DETACH	"HTTPD041I FORCE DETACHING SOCKET THREAD"

/** HTTPD044W UFS did not initialize; static file serving is unavailable */
#define MSG_UFS_FAILED		"HTTPD044W UNABLE TO INITIALIZE FILE SYSTEM"

/** HTTPD047E without the key no credential can be sealed; login is dead */
#define MSG_CREDKEY_FAILED	"HTTPD047E UNABLE TO INITIALIZE SERVER CREDENTIAL KEY, RC=%d"

/** HTTPD050E select() was handed an empty descriptor set */
#define MSG_MAXSOCK_ZERO	"HTTPD050E MAXSOCK IS ZERO, THE LISTENER IS IN AN INVALID STATE"

/** HTTPD051E the accept loop cannot continue */
#define MSG_SELECT_FAILED	"HTTPD051E SELECTEX() FAILED, RC=%d ERRNO=%d"

/** HTTPD052E one connection was lost; the listener keeps running */
#define MSG_ACCEPT_FAILED	"HTTPD052E ACCEPT() FAILED, RC=%d ERRNO=%d"

/** HTTPD053E a blocking socket would stall its worker indefinitely */
#define MSG_NONBLOCK_FAILED	"HTTPD053E UNABLE TO SET NON-BLOCKING I/O FOR SOCKET %d"

/** HTTPD054I the bind/listen succeeded; %s is the interface or ANY */
#define MSG_LISTENING		"HTTPD054I LISTENING ON %s PORT %d"

/** HTTPD060I a server thread ended; same layout as MSG_THREAD_START */
#define MSG_THREAD_STOP		"HTTPD060I SHUTDOWN %-16.16s TCB(%06X) TASK(%06X) STACKSIZE(%u)"

/** HTTPD061I a server thread started; %s is "SOCKET THREAD" or "WORKER(xxxxxx)" */
#define MSG_THREAD_START	"HTTPD061I STARTING %-16.16s TCB(%06X) TASK(%06X) STACKSIZE(%u)"

/** HTTPD062E the worker's ESTAE caught an abend; the worker is recycled */
#define MSG_WORKER_ABEND	"HTTPD062E ABEND %08X IN WORKER(%06X) CLIENT(%08X) SOCKET(%d)"

/** HTTPD070E the configured codepage is unknown; CP037 is used instead */
#define MSG_CODEPAGE_UNKNOWN	"HTTPD070E UNKNOWN CODEPAGE \"%s\", USING CP037"

/** HTTPD090E no console interface means no MODIFY and no STOP */
#define MSG_CONSOLE_FAILED	"HTTPD090E UNABLE TO INITIALIZE CONSOLE INTERFACE"

/** HTTPD098I P HTTPD was accepted; the quiesce has begun */
#define MSG_SHUTTING_DOWN	"HTTPD098I HTTPD SHUTTING DOWN"

/** HTTPD099I everything is released; the address space is about to end */
#define MSG_SHUTDOWN_DONE	"HTTPD099I HTTPD SHUTDOWN COMPLETE"

/* =====================================================================
** HTTPD1xx -- operator console (MODIFY / DISPLAY)
**
** Everything here is written because an operator asked for it, so rule 1
** does not apply: the console line IS the answer.
** ================================================================== */

/** HTTPD100I the MODIFY text as received, echoed before it is parsed */
#define MSG_CONS_MODIFY		"HTTPD100I CONS(%u) \"%-*.*s\""

/** HTTPD101E the verb is not one of the DISPLAY/SET/HELP set */
#define MSG_CONS_UNKNOWN	"HTTPD101E UNKNOWN CONSOLE COMMAND \"%s\""

/** HTTPD101E DISPLAY with an unknown operand */
#define MSG_CONS_UNKNOWN_D	"HTTPD101E UNKNOWN CONSOLE COMMAND \"DISPLAY %s\""

/** HTTPD101E SET with an unknown operand */
#define MSG_CONS_UNKNOWN_S	"HTTPD101E UNKNOWN CONSOLE COMMAND \"SET %s\""

/** HTTPD102I DISPLAY PORTS */
#define MSG_D_PORT		"HTTPD102I HTTPD IS LISTENING ON PORT %d"

/** HTTPD103I DISPLAY THREADS -- one per thread, %s names its role */
#define MSG_D_THREAD		"HTTPD103I ..THREAD %08X %s"

/* CTHDTASK field dump (DISPLAY THREADS) */
#define MSG_D_TASK1		"HTTPD104I .....EYE %-8.8s  .....TCB %08X  ..OWNTCB %08X"
#define MSG_D_TASK2		"HTTPD105I .TERMECB %08X  ......RC %08X  STACKSZE %08X"
#define MSG_D_TASK3		"HTTPD106I ....FUNC %08X  ....ARG1 %08X  ....ARG2 %08X"

/* CTHDMGR field dump (DISPLAY THREADS) */
#define MSG_D_MGR1		"HTTPD107I .....EYE %-8.8s  ....TASK %08X  .WAITECB %08X"
#define MSG_D_MGR2		"HTTPD108I ....FUNC %08X  ...UDATA %08X  STACKSZE %08X"
#define MSG_D_MGR3		"HTTPD109I ..WORKER %08X  ...QUEUE %08X  ...STATE %08X %s"
#define MSG_D_MGR4		"HTTPD110I .MINTASK %8u  .MAXTASK %8u"
#define MSG_D_MGR5		"HTTPD111I .WORKERS %8u  .DISPCNT %llu"

/* Worker field dump (DISPLAY THREADS) */
#define MSG_D_WORK1		"HTTPD112I ..WORKER %08X  .....EYE %-8.8s  .WAITECB %08X"
#define MSG_D_WORK2		"HTTPD113I .....MGR %08X  ....TASK %08X  ...QUEUE %08X"
#define MSG_D_WORK3		"HTTPD114I ...STATE %08X %s"
#define MSG_D_WORK4		"HTTPD115I ...START %016llX  %s"
#define MSG_D_WORK5		"HTTPD116I ....WAIT %016llX  %s"
#define MSG_D_WORK6		"HTTPD117I ....DISP %016llX  %s"

/* Connected-client detail (DISPLAY THREADS) */
#define MSG_D_CLIENT		"HTTPD118I PROTOCOL HTTP      ....USER %-9.9s ...GROUP %s"
#define MSG_D_REMOTE		"HTTPD119I ..REMOTE CLIENT    ....PORT %8d  ......IP %s"
#define MSG_D_LISTENER		"HTTPD119I ..SOCKET %8d  ....PORT %8d  (HTTP PROTOCOL)"
#define MSG_D_SOCKET		"HTTPD120I ..SOCKET %8d  ....PORT %8d  ......IP %s"
#define MSG_D_RNAME		"HTTPD120I ...RNAME %-28.28s"

/*
 * HTTPD121-122 -- DISPLAY LOGIN.  These were HTTPD070I/071I, which the
 * codepage banner already owned; console output belongs in 1xx.
 */

/** HTTPD121I DISPLAY LOGIN header */
#define MSG_D_LOGIN_COUNT	"HTTPD121I USERS LOGGED IN: %u"

/** HTTPD122I one logged-in credential */
#define MSG_D_LOGIN_USER	"HTTPD122I USER %-8.8s IP %u.%u.%u.%u ACEE(%06X)"

/*
 * HTTPD123-126 -- DISPLAY/SET STATS.  These were three different texts on
 * HTTPD411I plus one on HTTPD412I.
 */

/** HTTPD123I DISPLAY STATS, SMF part */
#define MSG_D_STATS_SMF		"HTTPD123I SMF: %s (TYPE %d)"

/** HTTPD124I DISPLAY STATS, counter part */
#define MSG_D_STATS_COUNT	"HTTPD124I REQUESTS: %u  ERRORS: %u  BYTES: %u  ACTIVE: %u"

/** HTTPD125I SET STATS ... RESET completed */
#define MSG_S_STATS_RESET	"HTTPD125I STATISTICS COUNTERS RESET"

/** HTTPD126I SET STATS changed the recording level */
#define MSG_S_STATS_LEVEL	"HTTPD126I SMF LEVEL SET TO %s"

/** HTTPD126E SET STATS with an operand that is not a level */
#define MSG_S_STATS_INVALID	"HTTPD126E INVALID SET STATS VALUE \"%s\""

/** HTTPD127W SET STATS without the required operand */
#define MSG_S_STATS_MISSING	"HTTPD127W MISSING STATS NONE|ERROR|AUTH|ALL [RESET]"

/*
 * HTTPD128-129 -- DISPLAY CONFIG (issue #184).  The startup path used to
 * echo all of this unasked; it is on demand now, which is the UFSD/FTPD
 * shape: a quiet banner, full detail when the operator asks.
 */

/** HTTPD128I DISPLAY CONFIG, listener and worker pool */
#define MSG_D_CFG_TASKS		"HTTPD128I PORT %d  MINTASK %d  MAXTASK %d"

/** HTTPD129I DISPLAY CONFIG, timeouts */
#define MSG_D_CFG_TIMES		"HTTPD129I CLIENT_TIMEOUT %d  KEEPALIVE %d/%d"

/** HTTPD130I DISPLAY CONFIG, credential reaper; 0 disables it */
#define MSG_D_CFG_SESSION	"HTTPD130I SESSION_TIMEOUT %d MIN%s"

/** HTTPD131I DISPLAY CONFIG, document root */
#define MSG_D_CFG_DOCROOT	"HTTPD131I DOCROOT %s"

/** HTTPD132I DISPLAY CONFIG, SMF recording */
#define MSG_D_CFG_SMF		"HTTPD132I SMF TYPE %d LEVEL %s"

/** HTTPD133I DISPLAY CONFIG, which Parmlib member was read */
#define MSG_D_CFG_SOURCE	"HTTPD133I CONFIG FROM %s"

/** HTTPD134I DISPLAY CONFIG, codepage in effect */
#define MSG_D_CFG_CODEPAGE	"HTTPD134I CODEPAGE %s"

/** HTTPD135I DISPLAY CONFIG, credential hard max-age; 0 disables it (#118) */
#define MSG_D_CFG_MAXAGE	"HTTPD135I SESSION_MAXAGE %d MIN%s"

/** HTTPD136I DISPLAY CONFIG, Basic realm / server name (#193): the REALM
 *  keyword's value, or the system's SMF ID when the Parmlib names none */
#define MSG_D_CFG_REALM		"HTTPD136I REALM %s"

/** HTTPD140I DISPLAY VERSION */
#define MSG_D_VERSION		"HTTPD140I HTTPD %s (%s)"

/** HTTPD141I worker dispatch count (DISPLAY THREADS) */
#define MSG_D_DISPCNT		"HTTPD141I .DISPCNT %llu"

/* HTTPD142I/143I are built by strftime(), so the id is part of the format
** string there rather than a literal here.  Named for the catalog: */
/** HTTPD142I DISPLAY TIME, GMT */
#define MSG_D_TIME_GMT		"HTTPD142I TIME %Y/%m/%d %H:%M:%S GMT"
/** HTTPD143I DISPLAY TIME, local -- an "OFFSET=%s%d" suffix is appended */
#define MSG_D_TIME_LOCAL	"HTTPD143I TIME %Y/%m/%d %H:%M:%S LOCAL"

/*
 * HTTPD144-147 -- DISPLAY MEMORY.  These had no id at all.
 */

/** HTTPD144E the address is above the 16 MB line, so it cannot be read */
#define MSG_D_MEM_INVALID	"HTTPD144E INVALID MEMORY ADDRESS 0x%08X"

/** HTTPD145I the dump completed normally */
#define MSG_D_MEM_END		"HTTPD145I END OF MEMORY DUMP FOR 0x%06X"

/** HTTPD146E the dump could not be run under recovery, so it was not run */
#define MSG_D_MEM_ESTAE		"HTTPD146E ESTAE CREATE FAILURE, RC=0x%08X"

/** HTTPD147E reading that storage abended; recovery caught it */
#define MSG_D_MEM_ABEND_S	"HTTPD147E ABEND S%03X FOR DISPLAY MEMORY 0x%06X"

/** HTTPD147E as above, user abend */
#define MSG_D_MEM_ABEND_U	"HTTPD147E ABEND U%04d FOR DISPLAY MEMORY 0x%06X"

/** HTTPD148I DISPLAY MEMORY without an address */
#define MSG_D_MEM_USAGE		"HTTPD148I USAGE: DISPLAY MEMORY XXXXXX[,NNN] (D M XXXXXX)"

/** HTTPD150I MODIFY help header */
#define MSG_HELP_HEADER		"HTTPD150I MODIFY COMMANDS ARE:"

/** HTTPD151I one MODIFY help line */
#define MSG_HELP_LINE		"HTTPD151I %s"

/** HTTPD199I separator between DISPLAY THREADS blocks */
#define MSG_SEPARATOR		"HTTPD199I -------------------------------------------------------"

/* =====================================================================
** HTTPD4xx -- configuration (DD:HTTPPRM), routes and SMF
** ================================================================== */

/** HTTPD020W no Parmlib member; the server starts on built-in defaults */
#define MSG_CFG_NO_PARMLIB	"HTTPD020W CANNOT OPEN DD:%s -- USING DEFAULTS"

/** HTTPD020E the debug DD is present but unusable */
#define MSG_CFG_DBG_FAILED	"HTTPD020E FOPEN FOR DD:HTTPDBG FAILED, ERRNO=%d"

/** HTTPD021I where the debug/trace output went instead */
#define MSG_CFG_DBG_STDERR	"HTTPD021I DEBUG/TRACE OUTPUT WILL BE TO DD:SYSTERM"

/** HTTPD023E PORT outside 1-65535; the default is kept */
#define MSG_CFG_BAD_PORT	"HTTPD023E INVALID PORT VALUE (%d)"

/** HTTPD024W the CONFIG= parameter is from the retired Lua engine */
#define MSG_CFG_IGNORED		"HTTPD024W CONFIG=%.32s IS IGNORED, CONFIGURATION COMES FROM THE %s DD"

/** HTTPD024I how to do what CONFIG= used to do */
#define MSG_CFG_USE_MEMBER	"HTTPD024I USE S HTTPD,M=MEMBER TO SELECT A DIFFERENT MEMBER"

/** HTTPD025W TZOFFSET is retired (#145); the system offset applies */
#define MSG_CFG_TZOFFSET	"HTTPD025W TZOFFSET IS NO LONGER USED, THE SYSTEM OFFSET GMT %s%02d:%02d APPLIES"

/** HTTPD025I where to set the timezone instead */
#define MSG_CFG_TZ_HOWTO	"HTTPD025I SET TZ IN THE SYSENV OR ENVIRON DD TO OVERRIDE IT FOR ALL TASKS"

/** HTTPD026W a line that is not KEYWORD VALUE; was one of three on 020W */
#define MSG_CFG_BAD_LINE	"HTTPD026W UNRECOGNIZED CONFIGURATION LINE: %.40s"

/** HTTPD026E the reaper would free a credential an in-flight request still holds */
#define MSG_CFG_SESSION_UNSAFE	"HTTPD026E SESSION_TIMEOUT (%d MIN) <= CLIENT_TIMEOUT (%d SEC), RAISE IT"

/** HTTPD032E same hazard as HTTPD026E, reached through the hard max-age (#118) */
#define MSG_CFG_MAXAGE_UNSAFE	"HTTPD032E SESSION_MAXAGE (%d MIN) <= CLIENT_TIMEOUT (%d SEC), RAISE IT"

/** HTTPD028E the listener socket could not be created at all */
#define MSG_CFG_SOCKET		"HTTPD028E SOCKET() FAILED, RC=%d ERRNO=%d"

/** HTTPD029W a known-shaped line with an unknown keyword */
#define MSG_CFG_BAD_KEY		"HTTPD029W UNKNOWN CONFIGURATION KEYWORD: %s"

/** HTTPD030E the port is taken or the stack refused the bind */
#define MSG_CFG_BIND		"HTTPD030E BIND() FAILED FOR HTTP PORT, RC=%d ERRNO=%d"

/** HTTPD030I a previous instance still holds the port; retrying */
#define MSG_CFG_BIND_RETRY	"HTTPD030I EADDRINUSE, WAITING FOR TCPIP TO RELEASE HTTP PORT %d"

/** HTTPD031E the socket was bound but will not accept */
#define MSG_CFG_LISTEN		"HTTPD031E LISTEN() FAILED, RC=%d ERRNO=%d"

/** HTTPD035W the region ran out of storage while the Parmlib was being read.
 *  Nothing else reaches this: there is no route-table limit (array_add() grows)
 *  and duplicate patterns are never detected.  The path is NOT dark -- it is
 *  served statically under the global LOGIN default, without the CGI.  Alone it
 *  also says the route bound no auth policy; one that did brings HTTPD419E and
 *  HTTPD420E and the server does not start (#220). */
#define MSG_MOD_NOT_REG		"HTTPD035W UNABLE TO REGISTER MODULE %s FOR %s"

/** HTTPD036I a CGI route is active */
#define MSG_MOD_REGISTERED	"HTTPD036I MODULE %s REGISTERED FOR %s"

/** HTTPD048W LOGIN NONE, or LOGIN with no operand.  The keyword is retired
 *  (#105), but NONE was its default, so the member behaves as it always did
 *  and the line is only noise. */
#define MSG_LOGIN_RETIRED	"HTTPD048W LOGIN IS RETIRED -- USE AUTH= ON EACH MOD=/LOC= ROUTE"

/** HTTPD048E LOGIN with an operand that used to REQUIRE authentication, so
 *  ignoring it would publish every route that carries no AUTH= of its own.
 *  Fatal for the same reason HTTPD419E is: a policy silently weakened is worse
 *  than a server that will not start.  %s is the operand as written. */
#define MSG_LOGIN_RETIRED_E	"HTTPD048E LOGIN %s IS RETIRED -- ROUTES WITHOUT AUTH= WOULD BECOME PUBLIC"

/** HTTPD400E the configuration had errors the server will not start on */
#define MSG_CFG_ERRORS		"HTTPD400E ERRORS OCCURRED PROCESSING THE CONFIGURATION"

/** HTTPD410W CGI= was the 3.3.x spelling */
#define MSG_ROUTE_CGI_DEPR	"HTTPD410W CGI= IS DEPRECATED, USE MOD= INSTEAD"

/** HTTPD411W AUTH= with a mode that is not NONE/BASIC/FORM/DEFAULT */
#define MSG_ROUTE_BAD_AUTH	"HTTPD411W IGNORING UNKNOWN AUTH MODE '%s'"

/** HTTPD412W RES= must be class:resource */
#define MSG_ROUTE_BAD_RES	"HTTPD412W IGNORING MALFORMED RES= '%s' (NEED CLASS:RESOURCE)"

/** HTTPD413W an option this build does not know */
#define MSG_ROUTE_BAD_OPT	"HTTPD413W IGNORING UNKNOWN ROUTE OPTION '%s'"

/** HTTPD414W a public route cannot also demand a RACF profile */
#define MSG_ROUTE_NONE_RES	"HTTPD414W AUTH=NONE IGNORES RES=%s:%s (PUBLIC ROUTE)"

/** HTTPD415W LOC= with no path operand */
#define MSG_LOC_NO_PATH		"HTTPD415W LOC= REQUIRES A PATH (E.G. LOC /ADMIN/* AUTH=BASIC)"

/** HTTPD416I the request/error/byte totals, written once at shutdown */
#define MSG_STATS		"HTTPD416I STATS: %u REQUESTS, %u ERRORS, %u BYTES"

/** HTTPD417I a program-less static prefix is active */
#define MSG_LOC_REGISTERED	"HTTPD417I LOCATION %s REGISTERED"

/** HTTPD418E RES= parsed but its storage could not be obtained */
#define MSG_ROUTE_NO_RES_MEM	"HTTPD418E NO STORAGE FOR RES=%.16s:%.40s"

/** HTTPD419E the route is gone and its auth policy with it -- fatal */
#define MSG_ROUTE_LOST		"HTTPD419E %s=%.40s COULD NOT BE REGISTERED -- ITS AUTH POLICY IS LOST"

/** HTTPD420E at least one route would have answered without its policy */
#define MSG_ROUTE_POLICY_BAD	"HTTPD420E ROUTE AUTHORIZATION POLICY INCOMPLETE -- HTTPD WILL NOT START"

/** HTTPD421W MOD= with no program operand */
#define MSG_MOD_NO_PGM		"HTTPD421W MOD= REQUIRES A PROGRAM NAME (E.G. MOD=MVSMF /ZOSMF/* AUTH=BASIC)"

/** HTTPD422W RECLAIM= is retired (#175); %s is the keyword as written */
#define MSG_ROUTE_RETIRED	"HTTPD422W %s IS RETIRED -- CGI STORAGE RECLAIM IS ALWAYS ON"

/** HTTPD423W as HTTPD035W, for a LOC= prefix: out of storage, with no table
 *  limit and no duplicate check behind it.  The prefix keeps being served from
 *  the document root, under the global LOGIN default instead of its own
 *  policy (#220). */
#define MSG_LOC_NOT_REG		"HTTPD423W UNABLE TO REGISTER LOCATION %s"

/** HTTPD424W REALM refused by httprlm_ok() (#193); the SMF ID default stays */
#define MSG_CFG_BAD_REALM	"HTTPD424W INVALID REALM VALUE: %.40s"

/** HTTPD425W a RES= route names a resource no profile covers (#137).  The
 *  route still serves -- SAF calls an unprotected resource allowed -- so the
 *  authorization stage does nothing and only the AUTH= stage is left.  Args:
 *  class, resource, route path. */
#define MSG_RES_NO_PROFILE	"HTTPD425W NO PROFILE FOR %.8s:%.32s -- %.24s NOT GATED"

/*
 * HTTPD43x -- SMF.  Split off HTTPD415W, which "LOC= requires a path"
 * also used, and off HTTPD028W, which carried both SMF operand errors.
 */

/** HTTPD430I SMF recording is configured and SMF is up */
#define MSG_SMF_ACTIVE		"HTTPD430I SMF TYPE %d LEVEL %s"

/** HTTPD430W records would be written into a void; nothing is recorded */
#define MSG_SMF_INACTIVE	"HTTPD430W SMF TYPE %d CONFIGURED BUT SMF INACTIVE"

/** HTTPD431W SMF level operand is not NONE/ERROR/AUTH/ALL */
#define MSG_SMF_BAD_LEVEL	"HTTPD431W INVALID SMF LEVEL \"%s\""

/** HTTPD432W SMF record type outside the user range */
#define MSG_SMF_BAD_TYPE	"HTTPD432W INVALID SMF TYPE=%d (128-255)"

/* =====================================================================
** HTTPD9xx -- diagnostics, abend recovery, storage
**
** Nothing here is routine.  A 9xx line means the server hit a state its
** own code did not expect, and every one of them is worth a bug report.
** ================================================================== */

/** HTTPD901E a client sat in the same state past every deadline */
#define MSG_SPIN			"HTTPD901E SPIN IN SERVE_CLIENT: CLIENT(%08X) STATE(%d) SOCKET(%d) REQ(%u) WORKER(%08X) -- FORCING CLOSE"

/*
 * HTTPD900D/902D/903D are retired (#186).  They were #159's probes; that issue
 * is closed, and 903D was writing a console line a minute on an idle server.
 * The ids stay reserved rather than reused -- a log from an older build still
 * means what it meant.
 */

/** HTTPD904E the request cannot be completed without the variable */
#define MSG_ENV_NO_STORAGE	"HTTPD904E NO STORAGE FOR ENVIRONMENT VARIABLE %.32s CLIENT(%08X)"

/** HTTPD905E the value exceeds what an HTTPV can hold */
#define MSG_ENV_TOO_LARGE	"HTTPD905E ENVIRONMENT VARIABLE %.32s TOO LARGE CLIENT(%08X)"

/** HTTPD906W a CGI's subpool survived it; storage leaks until restart */
#define MSG_RECLAIM_FAILED	"HTTPD906W STORAGE RECLAIM FOR %s FAILED, RC=%d"

/** HTTPD907W a busy-list entry outlived its request; cleared and resumed */
#define MSG_STALE_BUSY		"HTTPD907W STALE BUSY ENTRY FOR CLIENT(%08X) STATE(%d) SOCKET(%d) REQ(%u) -- CLEARED"

/** HTTPD908E the CGI could not be linked; %s says why */
#define MSG_CGI_FAILED		"HTTPD908E EXTERNAL PROGRAM %s %s"

/** HTTPD909E a handler asked for a status code this build has no phrase for */
#define MSG_BAD_STATUS		"HTTPD909E UNSUPPORTED HTTP STATUS %d REQUESTED, SENT 500"

/** HTTPD910E the main task's recovery caught a system abend */
#define MSG_ABEND_S		"HTTPD910E ABEND S%03X DETECTED"

/** HTTPD910E as above, user abend */
#define MSG_ABEND_U		"HTTPD910E ABEND U%04d DETECTED"

/** HTTPD911E a lock could not be taken; %s names the function */
#define MSG_LOCK_FAILED		"HTTPD911E %s LOCK FAILURE"

/** HTTPD912E the shutdown path itself abended */
#define MSG_TERM_FAILED		"HTTPD912E TERMINATE FAILED WITH RC=%08X"

/** HTTPD913E http_get() reached a content subtype it has no branch for */
#define MSG_BAD_SUBTYPE		"HTTPD913E UNEXPECTED SUBTYPE %d IN HTTP_GET()"

/** HTTPD914E the send buffer position went below zero */
#define MSG_SEND_UNDERFLOW	"HTTPD914E HTTP_SEND() POSITION UNDERFLOW %d"

/** HTTPD915E the file reader's length accounting disagrees with itself */
#define MSG_FILE_LENGTH		"HTTPD915E %s LENGTH INCONSISTENCY, LEN=%d BUFLEN=%d"

/** HTTPD916E the client could not be marked busy, so it cannot be served */
#define MSG_SET_BUSY_FAILED	"HTTPD916E HTTP_SET_BUSY() FAILED"

/** HTTPD920E a CGI module was run from TSO or batch instead of by HTTPD */
#define MSG_NOT_UNDER_HTTPD	"HTTPD920E %s MUST BE CALLED BY THE HTTPD SERVER"

/** HTTPD921W ABEND0C1 is about to abend on purpose; it is a test module */
#define MSG_ABEND_TEST		"HTTPD921W ABEND0C1 ALLOCATED %u KB IN %u BLOCKS, ABENDING WITHOUT FREEING"

/** HTTPD922E cgi_httpd() found neither an argument nor a GRT anchor; %s is the caller */
#define MSG_NO_HTTPD		"HTTPD922E HTTPD CONTROL BLOCK NOT FOUND BY %s"

/** HTTPD923E as above for the client block.  Its call site is #if 0 today --
 *  the id exists so re-enabling it does not reintroduce an id-less WTO. */
#define MSG_NO_HTTPC		"HTTPD923E HTTPC CONTROL BLOCK NOT FOUND BY %s"

/** HTTPD999E the address space is out of storage; the connection is dropped */
#define MSG_OUT_OF_MEMORY	"HTTPD999E OUT OF MEMORY"

#endif /* HTTPDMSG_H */
