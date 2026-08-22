/*
** HTTPD Parmlib Configuration Parser
**
** Replaces the Lua-based httpconf.c with a line-based KEY=VALUE parser.
** Reads configuration from DD:HTTPPRM (FB-80 dataset).
** Missing DD → server starts with defaults (warning issued).
**
** Follows the same pattern as FTPD (ftpd#cfg.c) and UFSD (ufsd#cfg.c).
*/
#include "httpd.h"
#include "httpdmsg.h"       /* operator message catalog                    */
#include "httprlm.h"        /* realm default + REALM gate (#191, #193)     */
#include "httpxlat.h"
#include "clibdsab.h"       /* get_dsab()  -- DD -> DSAB                    */
#include "ieftiot.h"        /* TIOTDD      -- DSAB -> TIOT entry            */
#include "osjfcb.h"         /* JFCB        -- TIOT entry -> dsname(member)  */

/* libc370 ships sleep() (src/clib/sleep.c) and __tzget() (src/clib/@@tzget.c)
** but declares neither in any header, so both were implicit declarations here.
** Declared locally until libc370 exports them (libc370 #70); the signatures are
** copied from those definitions, so a later header will agree rather than
** conflict. */
extern int sleep(unsigned seconds);
extern int __tzget(void);       /* src/clib/@@tzget.c -- returns crt->crttzoff */

/* parmlib_name() - resolve what the HTTPPRM DD actually points at, e.g.
** "SYS2.PARMLIB(HTTPPRM0)".  The STC PROC makes the member a startup choice
** (S HTTPD,M=HTTPPRM1), so "which member is this server running?" is a real
** operator question -- and until #166 nothing answered it: HTTPD404E named the
** CONFIG= parm, which has been ignored since the Parmlib migration.
**
** Read-only pointer chasing (DSAB -> TIOT entry -> JFCB), no SVC and no
** allocation; falls back to the DD name if any link is missing, so a caller
** always gets something printable.  The 3-byte TIOEJFCB is a SWA address,
** cast per byte because the field is char and its high bit is data.
**
** No longer static: F HTTPD,D CONFIG reports the same value on demand, and
** re-deriving the DD there would have been the identical 40 lines (#184). */
const char *
parmlib_name(char *buf, size_t size)
{
    DSAB    *dsab;
    TIOTDD  *tiotdd;
    JFCB    *jfcb;
    char     dsn[45];
    char     mem[9];
    int      i;

    dsab = get_dsab(NULL, HTTPD_PARMLIB_DD);    /* NULL == this task's TCB */
    if (!dsab) goto fallback;

    tiotdd = dsab->dsabtiot;
    if (!tiotdd) goto fallback;

    jfcb = (JFCB *)(((unsigned)(unsigned char)tiotdd->TIOEJFCB[0] << 16 |
                     (unsigned)(unsigned char)tiotdd->TIOEJFCB[1] << 8  |
                     (unsigned)(unsigned char)tiotdd->TIOEJFCB[2]) + 16);
    if (!jfcb) goto fallback;

    /* JFCB text fields are blank-padded, not NUL-terminated */
    for (i = 0; i < 44 && jfcb->jfcbdsnm[i] > ' '; i++)
        dsn[i] = jfcb->jfcbdsnm[i];
    dsn[i] = '\0';
    if (!dsn[0]) goto fallback;

    for (i = 0; i < 8 && jfcb->jfcbelnm[i] > ' '; i++)
        mem[i] = jfcb->jfcbelnm[i];
    mem[i] = '\0';

    if (mem[0]) snprintf(buf, size, "%s(%s)", dsn, mem);
    else        snprintf(buf, size, "%s", dsn);
    return buf;

fallback:
    snprintf(buf, size, "DD:%s", HTTPD_PARMLIB_DD);
    return buf;
}

/* The settled Basic realm / server name (#193) lives in the HTTPD block, as
** httpd->cfg_realm_val, with cfg_realm pointing at it -- the REALM keyword's
** value, or the httprlm() result when the Parmlib names none.  It used to be
** a file-scope buffer here, which is module storage: fetched from an
** APF-authorized or LNKLST library the module lands in key-0 storage and the
** key-8 store abends S0C4 (#197).  Still inline rather than malloc'd, so no
** allocation can fail on this path. */

/* Forward declarations */
static void set_defaults(HTTPD *httpd);
static void parse_line(HTTPD *httpd, char *line);
static void parse_keyvalue(HTTPD *httpd, const char *key, const char *value);
static void parse_mod(HTTPD *httpd, const char *value);
static void parse_loc(HTTPD *httpd, const char *value);
static void report_login_retired(HTTPD *httpd, const char *value);
static void report_tzoffset_retired(HTTPD *httpd);
static int  do_bind(HTTPD *httpd);
static void close_stale_port(int port);
static char *trim(char *s);

__asm__("\n&FUNC    SETC 'http_config'");
int
http_config(HTTPD *httpd, const char *member)
{
    CLIBCRT *crt = __crtget();
    FILE    *fp;
    char     line[256];
    int      rc;

    /* CONFIG= has been read-and-discarded since the Parmlib migration, which
       is worse than not accepting it: an operator who passes it believes it
       took effect.  Say it did not, and name the mechanism that does (#166). */
    if (member && *member) {
        wtof(MSG_CFG_IGNORED, member, HTTPD_PARMLIB_DD);
        wtof(MSG_CFG_USE_MEMBER);
    }

    /* store HTTPD pointer in CRT for CGI modules */
    crt->crtapp1 = httpd;

    /* set defaults */
    set_defaults(httpd);

    /* read configuration from DD:HTTPPRM */
    fp = fopen("DD:" HTTPD_PARMLIB_DD, "r");
    if (!fp) {
        wtof(MSG_CFG_NO_PARMLIB, HTTPD_PARMLIB_DD);
    } else {
        /* The member is NOT announced here.  It used to be, so a parse error
        ** below would already be attributed -- but every error this loop can
        ** raise names the offending line itself, and F HTTPD,D CONFIG reports
        ** the member on demand (HTTPD133I). */
        while (fgets(line, (int)sizeof(line), fp)) {
            parse_line(httpd, line);
        }
        fclose(fp);
    }

    /* The Basic realm / server name: the REALM keyword's value, or the
       system's SMF ID when the Parmlib names none (#193, #191).  Settled once
       here, so every consumer -- the WWW-Authenticate challenge, the login
       form, D CONFIG -- reports the same name, and none needs a fallback. */
    if (!httpd->cfg_realm) {
        httpd->cfg_realm = httprlm((const char *)__smfid(),
                                   httpd->cfg_realm_val,
                                   sizeof(httpd->cfg_realm_val));
    }

    /* A route that asked for an auth policy but did not get one is fail-open
       authorization: the route is simply absent, so nothing gates its requests
       at all -- for a LOC= prefix that serves the whole protected subtree to
       anyone.  Refuse to start rather than run a server whose gates are not
       the ones the Parmlib configured.  Checked before do_bind() so the port
       is never opened.  A retired LOGIN= that used to require a login reaches
       this the same way (see report_login_retired()). */
    if (httpd->flag & HTTPD_FLAG_CFGERR) {
        wtof(MSG_ROUTE_POLICY_BAD);
        return 8;
    }

    /* initialize codepage translation tables */
    http_xlate_init(httpd, httpd->codepage[0] ? httpd->codepage : NULL);

    /* initialize UFS if enabled */
    if (httpd->ufs_enabled) {
        httpd->ufssys = ufs_sys_new();
        if (!httpd->ufssys) {
            wtof(MSG_UFS_FAILED);
        }
        /* The document root is not announced here: HTTPD001I names it when
           the server is actually ready to serve it, and F HTTPD,D CONFIG
           reports it on demand (#184). */
    }

    /* Stats counters initialized to zero by calloc */

    /* open debug file if DEBUG=1 */
    if (httpd->dbg_enabled && !httpd->dbg) {
        httpd->dbg = fopen("DD:HTTPDBG", "w");
        if (!httpd->dbg) {
            wtof(MSG_CFG_DBG_FAILED, errno);
            wtof(MSG_CFG_DBG_STDERR);
            httpd->dbg = stderr;
        }
    }

    /* bind HTTP listener socket.  do_bind() writes HTTPD054I on success. */
    rc = do_bind(httpd);
    if (rc) return rc;

    /* The settled configuration used to be echoed here in four lines nobody
       had asked for.  It is available on demand as F HTTPD,D CONFIG instead
       (#184) -- the UFSD/FTPD shape: a quiet banner, full detail when the
       operator wants it.  What stays is the one line below, because it is not
       a value report but a warning about a combination that loses data. */

    /* the reaper must not free a credential still borrowed by an in-flight
       request: SESSION_TIMEOUT (min) must exceed the longest request
       (CLIENT_TIMEOUT, sec, and worst-case CGI runtime).  Warn on an unsafe
       config (M2). */
    if (httpd->cfg_session_timeout &&
        (long)httpd->cfg_session_timeout * 60 <= httpd->cfg_client_timeout) {
        wtof(MSG_CFG_SESSION_UNSAFE,
             httpd->cfg_session_timeout, httpd->cfg_client_timeout);
    }

    /* the max-age reaps on the same path and so carries the same invariant
       (#118) */
    if (httpd->cfg_session_maxage &&
        (long)httpd->cfg_session_maxage * 60 <= httpd->cfg_client_timeout) {
        wtof(MSG_CFG_MAXAGE_UNSAFE,
             httpd->cfg_session_maxage, httpd->cfg_client_timeout);
    }

    return 0;
}

/* ====================================================================
** Set default configuration values
** ================================================================= */
static void
set_defaults(HTTPD *httpd)
{
    httpd->port             = 8080;
    httpd->cfg_maxtask      = 9;
    httpd->cfg_mintask      = 3;
    httpd->cfg_client_timeout = 10;
    httpd->smf_level        = SMF_LEVEL_NONE;
    httpd->smf_type         = SMF_TYPE_HTTPD_DEFAULT;
    httpd->cfg_cgictx       = HTTPD_CGICTX_MAX;
    httpd->client           = HTTPD_CLIENT_INMSG | HTTPD_CLIENT_INDUMP
                            | HTTPD_CLIENT_STATS;
    httpd->ufs_enabled      = 1;
    httpd->bind_tries       = 10;
    httpd->bind_sleep       = 10;
    httpd->listen_queue     = 5;
    strcpy(httpd->docroot, "/www");
    httpd->codepage[0]      = '\0';
    httpd->xlate            = &http_cp037;  /* until CODEPAGE is parsed */
    httpd->cfg_realm_val[0] = '\0';
    httpd->dbg_enabled      = 0;
    httpd->cfg_keepalive_timeout = 5;
    httpd->cfg_keepalive_max     = 100;
    httpd->cfg_session_timeout   = 30;      /* credential idle TTL (min), 0=off */
    httpd->cfg_realm             = NULL;    /* settled after the parse (#193) */

    /* Hard max-age (#118): an actively used session outlives the idle TTL
       forever, so without this a credential cached at login stays valid --
       and with it a RACF identity that may since have been REVOKEd or had its
       password changed.  8 hours is deliberately generous: every reap calls
       racf_logout(), which clears ASXBSENV (see docs/identity-redesign.md §1),
       so reaping often is its own hazard until that plank lands. */
    httpd->cfg_session_maxage    = 480;     /* credential max-age (min), 0=off */

    /* Inherit the offset libc370 already resolved for this task rather than
    ** defaulting to 0 (issue #145).  __tzget() returns crt->crttzoff, which
    ** tzset() filled in from the TZ environment variable, or from the system's
    ** CVTTZ when TZ is absent -- see httpstrt.c, which calls tzset() right
    ** after loadenv().
    **
    ** Leaving this at 0 made httpd carry a second, disagreeing notion of the
    ** timezone: 0 here against CVTTZ in every task's CRT, so on any system
    ** whose CVTTZ is not zero the server's own DISPLAY TIME output was offset
    ** from what localtime()/ctime64() produced in the same address space --
    ** five hours apart on the reference system, with nothing configured.  The
    ** 0 was never a chosen default; the HTTPD block is static storage and
    ** nothing wrote the field.
    **
    ** This is now the only writer: the TZOFFSET keyword is retired, so nothing
    ** can override it from the Parmlib.  To choose a different offset, set TZ
    ** in the STC's SYSENV or ENVIRON DD -- tzset() picks it up in every task,
    ** and this reads the result.  See report_tzoffset_retired(). */
    httpd->tzoffset         = __tzget();
}

/* ====================================================================
** Trim leading and trailing whitespace in place
** ================================================================= */
static char *
trim(char *s)
{
    char *end;

    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '\0')
        return s;

    end = s + strlen(s) - 1;
    while (end > s && (*end == ' ' || *end == '\t' || *end == '\n'
           || *end == '\r'))
        *end-- = '\0';

    return s;
}

/* ====================================================================
** Parse a single configuration line
** ================================================================= */
static void
parse_line(HTTPD *httpd, char *line)
{
    char *p;
    char *key;
    char *value;

    p = trim(line);

    /* skip empty lines and comments */
    if (*p == '\0' || *p == '#' || *p == '*')
        return;

    /* find '=' separator */
    key = p;
    value = strchr(p, '=');
    if (!value) {
        wtof(MSG_CFG_BAD_LINE, p);
        return;
    }

    *value = '\0';
    value++;

    key   = trim(key);
    value = trim(value);

    /* convert key to uppercase */
    for (p = key; *p; p++)
        *p = (char)toupper((unsigned char)*p);

    parse_keyvalue(httpd, key, value);
}

/* ====================================================================
** Process a KEY=VALUE pair
** ================================================================= */
static void
parse_keyvalue(HTTPD *httpd, const char *key, const char *value)
{
    int i;

    if (strcmp(key, "PORT") == 0) {
        i = atoi(value);
        if (i < 1 || i > 65535) {
            wtof(MSG_CFG_BAD_PORT, i);
        } else {
            httpd->port = i;
        }
    }
    else if (strcmp(key, "MAXTASK") == 0) {
        i = atoi(value);
        if (i > 0) {
            if (i > 255) i = 255;
            httpd->cfg_maxtask = (UCHAR)i;
        }
    }
    else if (strcmp(key, "MINTASK") == 0) {
        i = atoi(value);
        if (i > 0) {
            if (i > 255) i = 255;
            httpd->cfg_mintask = (UCHAR)i;
            if (httpd->cfg_mintask > httpd->cfg_maxtask)
                httpd->cfg_maxtask = httpd->cfg_mintask;
        }
    }
    else if (strcmp(key, "CLIENT_TIMEOUT") == 0) {
        i = atoi(value);
        if (i > 0) {
            if (i > 255) i = 255;
            httpd->cfg_client_timeout = (UCHAR)i;
        }
    }
    else if (strcmp(key, "SESSION_TIMEOUT") == 0) {
        /* credential idle TTL in minutes; 0 disables reaping (M2) */
        i = atoi(value);
        if (i < 0) i = 0;
        if (i > 65535) i = 65535;
        httpd->cfg_session_timeout = (USHRT)i;
    }
    else if (strcmp(key, "SESSION_MAXAGE") == 0) {
        /* credential hard max-age in minutes, counted from login and never
           refreshed by activity; 0 disables it (#118) */
        i = atoi(value);
        if (i < 0) i = 0;
        if (i > 65535) i = 65535;
        httpd->cfg_session_maxage = (USHRT)i;
    }
    else if (strcmp(key, "LOGIN") == 0) {
        report_login_retired(httpd, value);
    }
    else if (strcmp(key, "REALM") == 0) {
        /* Basic realm / server name (#193), defaulting to the SMF ID (#191).
           The value lands inside the quoted-string of the WWW-Authenticate
           challenge and in the login form's HTML, so httprlm_ok() refuses
           the characters that would break either framing (see httprlm.h)
           rather than escaping them per consumer. */
        if (httprlm_ok(value)) {
            /* httprlm_ok() bounds the length to HTTP_REALM_CFG_MAX */
            strcpy(httpd->cfg_realm_val, value);
            httpd->cfg_realm = httpd->cfg_realm_val;
        }
        else {
            wtof(MSG_CFG_BAD_REALM, value);
        }
    }
    else if (strcmp(key, "TZOFFSET") == 0) {
        /* Retired (issue #145).  It is accepted and ignored rather than
           rejected, so an existing Parmlib still starts the server. */
        report_tzoffset_retired(httpd);
    }
    else if (strcmp(key, "DEBUG") == 0) {
        i = atoi(value);
        httpd->dbg_enabled = (i > 0) ? 1 : 0;
    }
    else if (strcmp(key, "UFS") == 0) {
        i = atoi(value);
        httpd->ufs_enabled = (i > 0) ? 1 : 0;
    }
    else if (strcmp(key, "DOCROOT") == 0) {
        if (value[0] == '/') {
            int len = strlen(value);
            /* strip trailing slash */
            if (len > 1 && value[len - 1] == '/')
                len--;
            if (len >= (int)sizeof(httpd->docroot))
                len = (int)sizeof(httpd->docroot) - 1;
            memcpy(httpd->docroot, value, len);
            httpd->docroot[len] = '\0';
        }
    }
    else if (strcmp(key, "CODEPAGE") == 0) {
        strncpy(httpd->codepage, value, sizeof(httpd->codepage) - 1);
        httpd->codepage[sizeof(httpd->codepage) - 1] = '\0';
    }
    else if (strcmp(key, "BIND_TRIES") == 0) {
        i = atoi(value);
        if (i < 1) i = 1;
        if (i > 100) i = 100;
        httpd->bind_tries = i;
    }
    else if (strcmp(key, "BIND_SLEEP") == 0) {
        i = atoi(value);
        if (i < 1) i = 1;
        if (i > 100) i = 100;
        httpd->bind_sleep = i;
    }
    else if (strcmp(key, "LISTEN_QUEUE") == 0) {
        i = atoi(value);
        if (i < 1) i = 1;
        if (i > 100) i = 100;
        httpd->listen_queue = i;
    }
    else if (strcmp(key, "MOD") == 0) {
        parse_mod(httpd, value);
    }
    else if (strcmp(key, "CGI") == 0) {
        wtof(MSG_ROUTE_CGI_DEPR);
        parse_mod(httpd, value);
    }
    else if (strcmp(key, "LOC") == 0) {
        parse_loc(httpd, value);
    }
    else if (strcmp(key, "CLIENT_TIMEOUT_MSG") == 0) {
        if (atoi(value) > 0) httpd->client |= HTTPD_CLIENT_INMSG;
        else                 httpd->client &= ~HTTPD_CLIENT_INMSG;
    }
    else if (strcmp(key, "CLIENT_TIMEOUT_DUMP") == 0) {
        if (atoi(value) > 0) httpd->client |= HTTPD_CLIENT_INDUMP;
        else                 httpd->client &= ~HTTPD_CLIENT_INDUMP;
    }
    else if (strcmp(key, "CLIENT_STATS") == 0) {
        if (atoi(value) > 0) httpd->client |= HTTPD_CLIENT_STATS;
        else                 httpd->client &= ~HTTPD_CLIENT_STATS;
    }
    else if (strcmp(key, "SMF") == 0) {
        // Parse level (first word of value)
        int vlen = 0;
        char *type_arg;
        while (value[vlen] && value[vlen] != ' ') vlen++;
        if (http_cmpn(value, "NONE", vlen) == 0) {
            httpd->smf_level = SMF_LEVEL_NONE;
        }
        else if (http_cmpn(value, "ERROR", vlen) == 0) {
            httpd->smf_level = SMF_LEVEL_ERROR;
        }
        else if (http_cmpn(value, "AUTH", vlen) == 0) {
            httpd->smf_level = SMF_LEVEL_AUTH;
        }
        else if (http_cmpn(value, "ALL", vlen) == 0) {
            httpd->smf_level = SMF_LEVEL_ALL;
        }
        else {
            wtof(MSG_SMF_BAD_LEVEL, value);
        }
        // Check for TYPE=nnn option
        type_arg = strstr(value, "TYPE=");
        if (type_arg) {
            int t = atoi(type_arg + 5);
            if (t >= 128 && t <= 255)
                httpd->smf_type = (UCHAR)t;
            else
                wtof(MSG_SMF_BAD_TYPE, t);
        }
    }
    else if (strcmp(key, "KEEPALIVE_TIMEOUT") == 0) {
        i = atoi(value);
        if (i < 1) i = 1;
        if (i > 255) i = 255;
        httpd->cfg_keepalive_timeout = (UCHAR)i;
    }
    else if (strcmp(key, "KEEPALIVE_MAX") == 0) {
        i = atoi(value);
        if (i < 1) i = 1;
        if (i > 255) i = 255;
        httpd->cfg_keepalive_max = (UCHAR)i;
    }
    else if (strcmp(key, "CGI_CONTEXT_POINTERS") == 0) {
        i = atoi(value);
        if (i >= HTTPD_CGICTX_MIN) {
            if (i > HTTPD_CGICTX_MAX) i = HTTPD_CGICTX_MAX;
            httpd->cfg_cgictx = (UCHAR)i;
        }
    }
    else {
        wtof(MSG_CFG_BAD_KEY, key);
    }
}

/* ====================================================================
** Per-route auth policy, shared by MOD= (CGI) and LOC= (static prefix).
** ================================================================= */
typedef struct {
    UCHAR   auth;               /* HTTP_AUTH_* (NONE until AUTH= seen)      */
    UCHAR   has_auth;           /* an AUTH= keyword was read (see below)    */
    UCHAR   resattr;            /* RACF attr (RACF_ATTR_READ when RES= set) */
    UCHAR   failed;             /* policy could not be built (no storage)   */
    char   *resclass;           /* strdup'd RACF class (NULL = no authz)    */
    char   *resname;            /* strdup'd RACF resource name              */
} ROUTE_POLICY;

/* has_auth exists because "the line said AUTH=NONE" and "the line said nothing"
** are the same HTTP_AUTH_NONE once the policy reaches the route, and RES= has
** to tell them apart: a resource always requires an identity to check against,
** so RES= without AUTH= implies authentication (documented since #98), while
** AUTH=NONE with RES= is the contradiction that NONE wins.  The distinction is
** resolved here in the parser and never reaches the route -- auth_gate() sees
** four concrete modes and no "unset" state.  That was HTTP_AUTH_DEFAULT's job,
** and it is the whole reason the runtime no longer needs it (#105). */

/* policy_binds() - true if this route asked for a gate that is lost with it.
** A route that is not registered does not make its path disappear: the path
** keeps being served, now with nothing gating it at all -- for a LOC= prefix
** that is the whole subtree the RES= was meant to protect, handed out
** unauthenticated.  So a *binding* policy that cannot be built is a
** configuration error, not a warning -- see the HTTPD_FLAG_CFGERR check in
** http_config().
**
** AUTH=NONE is the one policy losing the route cannot weaken (an unregistered
** path is public, which is what NONE asked for), so a lost AUTH=NONE route
** stays a warning.  Since #105 a route with no AUTH= at all carries NONE too,
** and for exactly the same reason it is equally harmless to lose. */
static int
policy_binds(const ROUTE_POLICY *pol)
{
    return pol->failed
        || pol->resclass != NULL
        || pol->auth != HTTP_AUTH_NONE;
}

/* tokenize() - split s in place into whitespace-delimited tokens.  Returns the
** count, filling tok[0..count) with pointers into s (each NUL-terminated). */
static int
tokenize(char *s, char **tok, int max)
{
    int n = 0;

    while (*s && n < max) {
        while (*s == ' ' || *s == '\t') s++;    /* skip leading blanks */
        if (!*s) break;
        tok[n++] = s;
        while (*s && *s != ' ' && *s != '\t') s++;
        if (*s) *s++ = '\0';                    /* terminate the token */
    }
    return n;
}

/* is_route_kv() - true if tok is a trailing route option
** (AUTH=.../RES=..., or the retired RECLAIM=) rather than a positional path
** token. */
static int
is_route_kv(const char *tok)
{
    return http_cmpn(tok, "AUTH=", 5) == 0
        || http_cmpn(tok, "RES=", 4) == 0
        || http_cmpn(tok, "RECLAIM=", 8) == 0;
}

/* parse_kv_tail() - parse the trailing AUTH=/RES= tokens (tok[start..ntok)),
** shared by parse_mod() and parse_loc().  Unknown tokens are warned + ignored;
** strdup'd RES= strings are handed to the route by apply_policy(). */
static void
parse_kv_tail(HTTPD *httpd, char **tok, int start, int ntok, ROUTE_POLICY *pol)
{
    int i;

    for (i = start; i < ntok; i++) {
        char *t = tok[i];

        if (http_cmpn(t, "AUTH=", 5) == 0) {
            char *v = t + 5;

            if (http_cmp(v, "NONE") == 0)       pol->auth = HTTP_AUTH_NONE;
            else if (http_cmp(v, "FORM") == 0)  pol->auth = HTTP_AUTH_FORM;
            else if (http_cmp(v, "BASIC") == 0) pol->auth = HTTP_AUTH_BASIC;
            else if (http_cmp(v, "TOKEN") == 0) pol->auth = HTTP_AUTH_TOKEN;
            else {
                wtof(MSG_ROUTE_BAD_AUTH, v);
                continue;                       /* not a policy the line set */
            }
            pol->has_auth = 1;
        }
        else if (http_cmpn(t, "RES=", 4) == 0) {
            char *v     = t + 4;                /* class:resource */
            char *colon = strchr(v, ':');

            if (colon && colon[1]) {
                *colon = '\0';
                free(pol->resclass);
                free(pol->resname);
                pol->resclass = strdup(v);
                pol->resname  = strdup(colon + 1);
                pol->resattr  = RACF_ATTR_READ;

                /* resclass and resname are both-or-neither: auth_gate() needs
                   resclass to force authentication and both to authorize, so a
                   half-built pair would authenticate the request and then never
                   check it against the resource.  Drop the pair and mark the
                   policy failed -- the route is refused, not weakened. */
                if (!pol->resclass || !pol->resname) {
                    free(pol->resclass);
                    free(pol->resname);
                    pol->resclass = NULL;
                    pol->resname  = NULL;
                    pol->resattr  = 0;
                    pol->failed   = 1;
                    wtof(MSG_ROUTE_NO_RES_MEM, v, colon + 1);
                    break;      /* a later RES= must not clear the failure */
                }
            }
            else {
                wtof(MSG_ROUTE_BAD_RES, v);
            }
        }
        else if (http_cmpn(t, "RECLAIM=", 8) == 0) {
            /* Retired (#174): the reclaim is unconditional now.  Accepted so
               existing members keep loading, warned so the member gets
               cleaned up -- the value is ignored either way. */
            wtof(MSG_ROUTE_RETIRED, t);
        }
        else {
            wtof(MSG_ROUTE_BAD_OPT, t);
        }
    }

    /* An explicit AUTH=NONE with a resource is contradictory: a public route
       has no ACEE to check against.  Warn and let NONE win -- the gate skips
       authz.  Tested on has_auth, not on the mode: since #105 a line that
       named no AUTH= at all also carries NONE, and that is the case just
       below, not this one. */
    if (pol->has_auth && pol->auth == HTTP_AUTH_NONE && pol->resclass) {
        wtof(MSG_ROUTE_NONE_RES, pol->resclass, pol->resname);
    }

    /* RES= without AUTH=: the resource check needs an identity, so the route
       authenticates.  BASIC is the challenge -- it is the one mode that serves
       both audiences a resource-gated route has (a browser pops its native
       dialog, curl/Zowe read the 401 they already expect), and unlike FORM it
       never answers an API client with an HTML page.  Naming AUTH= explicitly
       is still the better member; this only keeps a RES=-only line from being
       silently public, which is what it would otherwise become now that there
       is no global LOGIN left for it to inherit. */
    if (!pol->has_auth && pol->resclass) {
        pol->auth = HTTP_AUTH_BASIC;
    }
}

/* apply_policy() - move the parsed policy onto a freshly registered route
** (the strdup'd RES= storage becomes AS-lifetime, like path/pgm), or release
** it if the route could not be registered.  This is the only writer of
** route->resclass/resname, so the both-or-neither invariant parse_kv_tail
** establishes holds for every registered route. */
static void
apply_policy(HTTPROUTE *route, ROUTE_POLICY *pol)
{
    if (route) {
        route->auth     = pol->auth;
        route->resattr  = pol->resattr;
        route->resclass = pol->resclass;
        route->resname  = pol->resname;
    }
    else {
        free(pol->resclass);
        free(pol->resname);
    }
}

/* route_policy_lost() - a route that carried (or may have carried) a binding
** auth policy could not be registered.  Never continue: without the route the
** path is served with nothing gating it at all, which is weaker than anything
** the Parmlib asked for.  Flag the configuration so http_config() refuses to
** start the server. */
static void
route_policy_lost(HTTPD *httpd, const char *kind, const char *path)
{
    /* %.40s because the caller in the untokenizable case passes the raw
       Parmlib line, not a path -- same bound parse_line() uses for HTTPD020W */
    wtof(MSG_ROUTE_LOST, kind, path ? path : "(NULL)");
    httpd->flag |= HTTPD_FLAG_CFGERR;
}

/* route_malformed() - a MOD=/LOC= line was rejected before a route could be
** built, because the positional token it needs (program name / path) is missing
** and an AUTH=/RES= option stands in its place.  A dropped line is not
** harmless: the prefix it was meant to gate is then served ungated, so
** `LOC=AUTH=BASIC RES=FACILITY:HTTPD.ADMIN` -- the path forgotten --
** publishes exactly the subtree it named (issue #164).  Unlike the
** allocation failures this shares its reporting with, a typo reaches it.
**
** The remaining tokens are parsed only to classify the line: a binding policy
** makes it a configuration error, AUTH=NONE or no policy stays a warning
** (see policy_binds()).  Nothing is registered either way, so any RES= storage
** the parse allocated is released again. */
static void
route_malformed(HTTPD *httpd, const char *kind, char **tok, int ntok,
                const char *value)
{
    ROUTE_POLICY pol;
    int          binds;

    memset(&pol, 0, sizeof(pol));               /* auth = HTTP_AUTH_NONE    */
    parse_kv_tail(httpd, tok, 0, ntok, &pol);   /* every token is an option */
    binds = policy_binds(&pol);
    apply_policy(NULL, &pol);                   /* release RES= strings */

    if (binds)
        route_policy_lost(httpd, kind, value);
}

/* ====================================================================
** Parse MOD=PROGRAM [pattern] [AUTH=mode] [RES=class:resource]
** If pattern is omitted, derive *.<lowercase program> and use DOCROOT.
** ================================================================= */
static void
parse_mod(HTTPD *httpd, const char *value)
{
    char  program[9];
    char  auto_pattern[16];
    char *tok[8];
    char *tmp;
    char *path;
    int   ntok;
    int   ti;
    int   j;
    int   binds;
    ROUTE_POLICY pol;
    HTTPROUTE *route;

    /* the line cannot even be tokenized, so whether it carried an AUTH=/RES=
       policy is unknowable -- assume it did rather than start a server with a
       route the Parmlib asked for silently missing */
    tmp = strdup(value);
    if (!tmp) {
        route_policy_lost(httpd, "MOD", value);
        return;
    }

    memset(&pol, 0, sizeof(pol));               /* auth = HTTP_AUTH_NONE    */

    ntok = tokenize(tmp, tok, 8);
    if (ntok < 1) {                             /* no program name */
        wtof(MSG_MOD_NO_PGM);
        free(tmp);
        return;
    }

    /* An AUTH=/RES= option where the program name belongs means the name was
       omitted.  Without this the option would be folded into an 8-char module
       name ("AUTH=BAS") and registered against a derived "*.auth=bas" pattern
       -- a route that can never load, built out of a typo. */
    if (is_route_kv(tok[0])) {
        wtof(MSG_MOD_NO_PGM);
        route_malformed(httpd, "MOD", tok, ntok, value);
        free(tmp);
        return;
    }

    /* first token = program name, folded to uppercase, max 8 chars */
    for (j = 0; j < 8 && tok[0][j]; j++)
        program[j] = (char)toupper((unsigned char)tok[0][j]);
    program[j] = '\0';

    /* optional second token = path pattern (unless it's already AUTH=/RES=) */
    ti = 1;
    if (ti < ntok && !is_route_kv(tok[ti])) {
        path = tok[ti];
        ti++;
    }
    else {
        /* no explicit pattern -> derive *.<lowercase program> */
        auto_pattern[0] = '*';
        auto_pattern[1] = '.';
        for (j = 0; j < 8 && program[j]; j++)
            auto_pattern[2 + j] = (char)tolower((unsigned char)program[j]);
        auto_pattern[2 + j] = '\0';
        path = auto_pattern;
    }

    /* remaining tokens = per-route AUTH=/RES= policy */
    parse_kv_tail(httpd, tok, ti, ntok, &pol);

    binds = policy_binds(&pol);

    if (pol.failed) {
        route = NULL;                             /* refuse the route outright */
        apply_policy(NULL, &pol);
    }
    else if (program[0]) {
        /* the 4th argument is the retired login flag (#105).  It stays in the
           signature because http_add_route sits in the httpx vector at 0x104
           and modules are compiled against it; httpacgi() ignores it. */
        route = http_add_route(httpd, program, path, 0);
        apply_policy(route, &pol);
        if (route)
            wtof(MSG_MOD_REGISTERED, program, path);
        else
            wtof(MSG_MOD_NOT_REG, program, path);
    }
    else {
        route = NULL;
        apply_policy(NULL, &pol);               /* release RES= strings */
    }

    if (!route && binds)
        route_policy_lost(httpd, "MOD", path);

    free(tmp);
}

/* ====================================================================
** Parse LOC=PATH [AUTH=mode] [RES=class:resource]
** A program-less route: a static path prefix that carries the same auth
** policy as MOD= but falls through to httpget (pgm == NULL) instead of
** dispatching a CGI.  This is what static/SPA deployments need to protect a
** whole subtree behind a login or a RACF/RAKF profile.
** ================================================================= */
static void
parse_loc(HTTPD *httpd, const char *value)
{
    char *tok[8];
    char *tmp;
    char *path;
    int   ntok;
    int   binds;
    ROUTE_POLICY pol;
    HTTPROUTE *route;

    /* see parse_mod(): an untokenizable line is treated as policy-bearing */
    tmp = strdup(value);
    if (!tmp) {
        route_policy_lost(httpd, "LOC", value);
        return;
    }

    memset(&pol, 0, sizeof(pol));               /* auth = HTTP_AUTH_NONE    */

    ntok = tokenize(tmp, tok, 8);
    if (ntok < 1 || is_route_kv(tok[0])) {      /* first token must be a path */
        wtof(MSG_LOC_NO_PATH);
        if (ntok >= 1)
            route_malformed(httpd, "LOC", tok, ntok, value);
        free(tmp);
        return;
    }

    path = tok[0];

    /* remaining tokens = per-route AUTH=/RES= policy */
    parse_kv_tail(httpd, tok, 1, ntok, &pol);

    binds = policy_binds(&pol);

    if (pol.failed) {
        route = NULL;                             /* refuse the route outright */
        apply_policy(NULL, &pol);
    }
    else {
        /* pgm == NULL registers a program-less (static) route */
        route = http_add_route(httpd, NULL, path, 0);
        apply_policy(route, &pol);
        if (route)
            wtof(MSG_LOC_REGISTERED, path);
        else
            wtof(MSG_LOC_NOT_REG, path);
    }

    if (!route && binds)
        route_policy_lost(httpd, "LOC", path);

    free(tmp);
}

/* ====================================================================
** LOGIN is retired (#105) -- AUTH= on each MOD=/LOC= route is the only
** authentication policy now.
**
** How loudly depends on what the operand said, because the two cases fail in
** opposite directions.  LOGIN NONE (and a bare LOGIN, which parsed to the same
** thing) required no login of anything: dropping it changes nothing, so the
** line is noise and a warning is enough.  Any other operand -- ALL, CGI, GET,
** HEAD, POST -- REQUIRED a login for requests that no longer name one
** themselves, and every route in the member that carries no AUTH= of its own
** would silently become public.  That is the same fail-open shape HTTPD419E
** refuses to start on, so it is refused here too, and for the same reason:
** the operator must convert the routes, not discover the hole in a log.
**
** Deliberately NOT a partial translation into per-route AUTH= modes.  LOGIN
** gated by HTTP method, AUTH= gates by route, and the two do not map: LOGIN
** GET says nothing about which routes it meant.  A guess here would produce a
** configuration the Parmlib never asked for.
** ================================================================= */
static void
report_login_retired(HTTPD *httpd, const char *value)
{
    char *tmp;
    char *tok;
    int   required = 0;

    /* LOGIN= with nothing after the '=' arrives as an empty value, which
       parse_login() read as "no bits set" -- same as NONE, so warn and stop.
       (A bare LOGIN with no '=' at all never gets here: parse_line() rejects
       a line without a separator as malformed.) */
    if (!value || !*value) {
        wtof(MSG_LOGIN_RETIRED);
        return;
    }

    tmp = strdup(value);
    if (!tmp) {
        /* Out of storage before the operand could be classified.  Assume it
           required a login: refusing to start on a LOGIN NONE is an operator
           annoyance, starting anyway on a LOGIN ALL is a published server. */
        wtof(MSG_LOGIN_RETIRED_E, value);
        httpd->flag |= HTTPD_FLAG_CFGERR;
        return;
    }

    /* Any token other than NONE gated something.  An unknown token counts as
       gating too -- parse_login() rejected it with a warning and carried on,
       but here it is the difference between a warning and a refusal, and the
       operator's intent behind a misspelled operand is not ours to guess. */
    for (tok = strtok(tmp, ","); tok; tok = strtok(NULL, ",")) {
        while (*tok == ' ') tok++;
        if (*tok && http_cmp(tok, "NONE") != 0) required = 1;
    }

    if (required) {
        wtof(MSG_LOGIN_RETIRED_E, value);
        httpd->flag |= HTTPD_FLAG_CFGERR;
    }
    else {
        wtof(MSG_LOGIN_RETIRED);
    }

    free(tmp);
}

/* ====================================================================
** TZOFFSET is retired (issue #145) -- say so and name the replacement.
**
** It had exactly one effect left: the default offset for the DISPLAY TIME
** command, which takes an offset as an argument anyway.  Its two documented
** purposes were never real -- the Date: header goes through gmtime64()
** (http1123.c) and the SMF record through localtime() (httprepo.c), neither of
** which ever read this field -- and the JES2 API stopped reading it in #151.
**
** It was also a trap.  "TZOFFSET +02:00" reads like a display preference, but
** it asserts that the machine's TOD clock runs at UTC+2; set on a system at
** UTC-5 it silently skewed every JES2 timestamp by seven hours.  And its side
** effects were misleading: __tzset() reached only the task that parsed the
** Parmlib, while setenvi("TZOFFSET") published a name nothing reads, since
** tzset() looks at TZ.
**
** The offset now always comes from __tzget() (set_defaults()), i.e. what
** tzset() resolved for this task from TZ or the system's CVTTZ.  To override it
** deliberately, set TZ in the STC's SYSENV or ENVIRON DD: both httpstrt.c and
** cgistart.c call tzset() after loadenv(), so that reaches every task --
** server, workers and modules -- which is what this keyword never did.
** ================================================================= */
static void
report_tzoffset_retired(HTTPD *httpd)
{
    int  sec  = httpd->tzoffset;
    int  sign = sec < 0 ? -1 : 1;
    int  hour, min;

    sec *= sign;
    hour = sec / 3600;
    sec -= hour * 3600;
    min  = sec / 60;

    wtof(MSG_CFG_TZOFFSET, sign < 0 ? "-" : "+", hour, min);
    wtof(MSG_CFG_TZ_HOWTO);
}

/* ====================================================================
** Bind HTTP listener socket with retry logic
** ================================================================= */
static int
do_bind(HTTPD *httpd)
{
    int                 sock;
    int                 rc;
    int                 i;
    struct sockaddr_in  serv_addr;

    /* close sockets that are using this port */
    close_stale_port(httpd->port);

    /* create listener socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        wtof(MSG_CFG_SOCKET, sock, errno);
        return 8;
    }

    /* bind socket to IP address and port */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family      = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port        = htons(httpd->port);

    rc = bind(sock, &serv_addr, sizeof(serv_addr));
    if (rc < 0) {
        int error = errno;
        wtof(MSG_CFG_BIND, rc, errno);
        if (error == EADDRINUSE) {
            for (i = 0; i < httpd->bind_tries; i++) {
                wtof(MSG_CFG_BIND_RETRY, httpd->port);
                sleep(httpd->bind_sleep);
                rc = bind(sock, &serv_addr, sizeof(serv_addr));
                if (rc >= 0) break;
            }
        }
        if (rc < 0) {
            wtof(MSG_CFG_BIND, rc, errno);
            closesocket(sock);
            close_stale_port(httpd->port);
            return 8;
        }
    }

    /* listen for connections */
    rc = listen(sock, httpd->listen_queue);
    if (rc < 0) {
        wtof(MSG_CFG_LISTEN, rc, errno);
        closesocket(sock);
        return 8;
    }

    httpd->listen = sock;

    /* The bind is unconditionally INADDR_ANY, so there is no interface to
       name -- "ANY" is the literal truth rather than a placeholder. */
    wtof(MSG_LISTENING, "ANY", httpd->port);

    return 0;
}

/* ====================================================================
** Close sockets bound to a given port (stale from previous instance)
** ================================================================= */
static void
close_stale_port(int port)
{
    int                 rc;
    int                 i;
    int                 addrlen;
    struct sockaddr     addr;

    for (i = 1; i < FD_SETSIZE; i++) {
        addrlen = sizeof(addr);
        rc = getsockname(i, &addr, &addrlen);
        if (rc == 0) {
            struct sockaddr_in *in = (struct sockaddr_in *)&addr;
            if (in->sin_port == port) {
                wtof(MSG_STALE_SOCKET, i, in->sin_port);
                closesocket(i);
                sleep(2);
                break;
            }
        }
    }
}
