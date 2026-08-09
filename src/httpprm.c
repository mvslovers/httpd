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
#include "httpxlat.h"

/* libc370 ships sleep() (src/clib/sleep.c) and __tzget() (src/clib/@@tzget.c)
** but declares neither in any header, so both were implicit declarations here.
** Declared locally until libc370 exports them (libc370 #70); the signatures are
** copied from those definitions, so a later header will agree rather than
** conflict. */
extern int sleep(unsigned seconds);
extern int __tzget(void);       /* src/clib/@@tzget.c -- returns crt->crttzoff */

/* Forward declarations */
static void set_defaults(HTTPD *httpd);
static void parse_line(HTTPD *httpd, char *line);
static void parse_keyvalue(HTTPD *httpd, const char *key, const char *value);
static void parse_mod(HTTPD *httpd, const char *value);
static void parse_loc(HTTPD *httpd, const char *value);
static void parse_login(HTTPD *httpd, const char *value);
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

    (void)member;   /* CONFIG= parm ignored — we always read DD:HTTPPRM */

    /* store HTTPD pointer in CRT for CGI modules */
    crt->crtapp1 = httpd;

    /* set defaults */
    set_defaults(httpd);

    /* read configuration from DD:HTTPPRM */
    fp = fopen("DD:HTTPPRM", "r");
    if (!fp) {
        wtof("HTTPD020W Cannot open DD:HTTPPRM -- using defaults");
    } else {
        while (fgets(line, (int)sizeof(line), fp)) {
            parse_line(httpd, line);
        }
        fclose(fp);
    }

    /* A route that asked for an auth policy but did not get one is fail-open
       authorization: the route is simply absent, so its requests fall back to
       the global LOGIN default -- for a LOC= prefix under LOGIN NONE that
       serves the whole protected subtree to anyone.  Refuse to start rather
       than run a server whose gates are not the ones the Parmlib configured.
       Checked before do_bind() so the port is never opened. */
    if (httpd->flag & HTTPD_FLAG_CFGERR) {
        wtof("HTTPD420E Route authorization policy incomplete "
             "-- HTTPD will not start");
        return 8;
    }

    /* initialize codepage translation tables */
    http_xlate_init(httpd->codepage[0] ? httpd->codepage : NULL);

    /* initialize UFS if enabled */
    if (httpd->ufs_enabled) {
        httpd->ufssys = ufs_sys_new();
        if (!httpd->ufssys) {
            wtof("HTTPD044W Unable to initialize file system");
        } else {
            if (httpd->docroot[0])
                wtof("HTTPD047I Document root: %s", httpd->docroot);
        }
    }

    /* Stats counters initialized to zero by calloc */

    /* open debug file if DEBUG=1 */
    if (httpd->dbg_enabled && !httpd->dbg) {
        httpd->dbg = fopen("DD:HTTPDBG", "w");
        if (!httpd->dbg) {
            wtof("HTTPD020E fopen for DD:HTTPDBG failed, error=%d", errno);
            wtof("HTTPD021I DEBUG/TRACE output will be to DD:SYSTERM");
            httpd->dbg = stderr;
        }
    }

    /* bind HTTP listener socket */
    rc = do_bind(httpd);
    if (rc) return rc;

    /* log final configuration */
    wtof("HTTPD032I Listening for HTTP request on port %d", httpd->port);
    wtof("HTTPD033I MINTASK=%d MAXTASK=%d CLIENT_TIMEOUT=%d",
         httpd->cfg_mintask, httpd->cfg_maxtask,
         httpd->cfg_client_timeout);
    wtof("HTTPD034I KEEPALIVE_TIMEOUT=%d KEEPALIVE_MAX=%d",
         httpd->cfg_keepalive_timeout, httpd->cfg_keepalive_max);
    wtof("HTTPD035I SESSION_TIMEOUT=%d min%s", httpd->cfg_session_timeout,
         httpd->cfg_session_timeout ? "" : " (reaper disabled)");

    /* the reaper must not free a credential still borrowed by an in-flight
       request: SESSION_TIMEOUT (min) must exceed the longest request
       (CLIENT_TIMEOUT, sec, and worst-case CGI runtime).  Warn on an unsafe
       config (M2). */
    if (httpd->cfg_session_timeout &&
        (long)httpd->cfg_session_timeout * 60 <= httpd->cfg_client_timeout) {
        wtof("HTTPD035W SESSION_TIMEOUT (%d min) <= CLIENT_TIMEOUT (%d sec); "
             "raise it well above the longest request",
             httpd->cfg_session_timeout, httpd->cfg_client_timeout);
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
    httpd->login            = 0;            /* NONE */
    httpd->client           = HTTPD_CLIENT_INMSG | HTTPD_CLIENT_INDUMP
                            | HTTPD_CLIENT_STATS;
    httpd->ufs_enabled      = 1;
    httpd->bind_tries       = 10;
    httpd->bind_sleep       = 10;
    httpd->listen_queue     = 5;
    httpd->unused_80        = NULL;
    strcpy(httpd->docroot, "/www");
    httpd->codepage[0]      = '\0';
    httpd->dbg_enabled      = 0;
    httpd->cfg_keepalive_timeout = 5;
    httpd->cfg_keepalive_max     = 100;
    httpd->cfg_session_timeout   = 30;      /* credential idle TTL (min), 0=off */

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
        wtof("HTTPD020W Unrecognized config line: %.40s", p);
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
            wtof("HTTPD023E Invalid PORT value (%d)", i);
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
    else if (strcmp(key, "LOGIN") == 0) {
        parse_login(httpd, value);
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
        wtof("HTTPD410W CGI= is deprecated, use MOD= instead");
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
            wtof("HTTPD028W Invalid SMF level \"%s\"", value);
        }
        // Check for TYPE=nnn option
        type_arg = strstr(value, "TYPE=");
        if (type_arg) {
            int t = atoi(type_arg + 5);
            if (t >= 128 && t <= 255)
                httpd->smf_type = (UCHAR)t;
            else
                wtof("HTTPD028W Invalid SMF TYPE=%d (128-255)", t);
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
        wtof("HTTPD020W Unknown config key: %s", key);
    }
}

/* ====================================================================
** Per-route auth policy, shared by MOD= (CGI) and LOC= (static prefix).
** ================================================================= */
typedef struct {
    UCHAR   auth;               /* HTTP_AUTH_* (DEFAULT until AUTH= seen)   */
    UCHAR   resattr;            /* RACF attr (RACF_ATTR_READ when RES= set) */
    UCHAR   failed;             /* policy could not be built (no storage)   */
    char   *resclass;           /* strdup'd RACF class (NULL = no authz)    */
    char   *resname;            /* strdup'd RACF resource name              */
} ROUTE_POLICY;

/* policy_binds() - true if this route asked for a gate the fallback cannot
** supply.  A route that is not registered does not disappear: the request falls
** back to the global LOGIN policy, and for a LOC= prefix that is exactly the
** weaker policy the RES= was meant to replace (LOGIN NONE serves the whole
** subtree unauthenticated).  So a *binding* policy that cannot be built is a
** configuration error, not a warning -- see the HTTPD_FLAG_CFGERR check in
** http_config().  AUTH=NONE is the one policy the fallback can only tighten,
** so a lost AUTH=NONE route is not fail-open and stays a warning. */
static int
policy_binds(const ROUTE_POLICY *pol)
{
    return pol->failed
        || pol->resclass != NULL
        || (pol->auth != HTTP_AUTH_DEFAULT && pol->auth != HTTP_AUTH_NONE);
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

/* is_route_kv() - true if tok is a trailing route option (AUTH=.../RES=...)
** rather than a positional path token. */
static int
is_route_kv(const char *tok)
{
    return http_cmpn(tok, "AUTH=", 5) == 0 || http_cmpn(tok, "RES=", 4) == 0;
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
            else
                wtof("HTTPD411W ignoring unknown AUTH mode '%s'", v);
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
                    wtof("HTTPD418E No storage for RES=%.16s:%.40s",
                         v, colon + 1);
                    break;      /* a later RES= must not clear the failure */
                }
            }
            else {
                wtof("HTTPD412W ignoring malformed RES= '%s' "
                     "(need class:resource)", v);
            }
        }
        else {
            wtof("HTTPD413W ignoring unknown route option '%s'", t);
        }
    }

    /* AUTH=NONE with a resource is contradictory: a public route has no ACEE
       to check against.  Warn and let NONE win -- the gate skips authz. */
    if (pol->auth == HTTP_AUTH_NONE && pol->resclass) {
        wtof("HTTPD414W AUTH=NONE ignores RES=%s:%s (public route)",
             pol->resclass, pol->resname);
    }
}

/* apply_policy() - move the parsed policy onto a freshly registered route
** (the strdup'd RES= storage becomes AS-lifetime, like path/pgm), or release
** it if the route could not be registered.  This is the only writer of
** cgi->resclass/resname, so the both-or-neither invariant parse_kv_tail
** establishes holds for every registered route. */
static void
apply_policy(HTTPCGI *cgi, ROUTE_POLICY *pol)
{
    if (cgi) {
        cgi->auth     = pol->auth;
        cgi->resattr  = pol->resattr;
        cgi->resclass = pol->resclass;
        cgi->resname  = pol->resname;
    }
    else {
        free(pol->resclass);
        free(pol->resname);
    }
}

/* route_policy_lost() - a route that carried (or may have carried) a binding
** auth policy could not be registered.  Never continue: without the route the
** request is served under the global LOGIN default, which is the weaker policy
** the Parmlib asked to replace.  Flag the configuration so http_config()
** refuses to start the server. */
static void
route_policy_lost(HTTPD *httpd, const char *kind, const char *path)
{
    /* %.40s because the caller in the untokenizable case passes the raw
       Parmlib line, not a path -- same bound parse_line() uses for HTTPD020W */
    wtof("HTTPD419E %s=%.40s could not be registered -- its auth policy is lost",
         kind, path ? path : "(null)");
    httpd->flag |= HTTPD_FLAG_CFGERR;
}

/* route_malformed() - a MOD=/LOC= line was rejected before a route could be
** built, because the positional token it needs (program name / path) is missing
** and an AUTH=/RES= option stands in its place.  A dropped line is not
** harmless: the prefix it was meant to gate is then served under the global
** LOGIN default, so `LOC=AUTH=BASIC RES=FACILITY:HTTPD.ADMIN` -- the path
** forgotten -- publishes exactly the subtree it named (issue #164).  Unlike the
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

    memset(&pol, 0, sizeof(pol));               /* auth = HTTP_AUTH_DEFAULT */
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
    int   login = httpd->login & HTTPD_LOGIN_CGI;
    ROUTE_POLICY pol;
    HTTPCGI *cgi;

    /* the line cannot even be tokenized, so whether it carried an AUTH=/RES=
       policy is unknowable -- assume it did rather than start a server with a
       route the Parmlib asked for silently missing */
    tmp = strdup(value);
    if (!tmp) {
        route_policy_lost(httpd, "MOD", value);
        return;
    }

    memset(&pol, 0, sizeof(pol));               /* auth = HTTP_AUTH_DEFAULT */

    ntok = tokenize(tmp, tok, 8);
    if (ntok < 1) {                             /* no program name */
        wtof("HTTPD421W MOD= requires a program name "
             "(e.g. MOD=MVSMF /zosmf/* AUTH=BASIC)");
        free(tmp);
        return;
    }

    /* An AUTH=/RES= option where the program name belongs means the name was
       omitted.  Without this the option would be folded into an 8-char module
       name ("AUTH=BAS") and registered against a derived "*.auth=bas" pattern
       -- a route that can never load, built out of a typo. */
    if (is_route_kv(tok[0])) {
        wtof("HTTPD421W MOD= requires a program name "
             "(e.g. MOD=MVSMF /zosmf/* AUTH=BASIC)");
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
        cgi = NULL;                             /* refuse the route outright */
        apply_policy(NULL, &pol);
    }
    else if (program[0]) {
        cgi = http_add_cgi(httpd, program, path, login);
        apply_policy(cgi, &pol);
        if (cgi)
            wtof("HTTPD036I Module %s registered for %s", program, path);
        else
            wtof("HTTPD035W Unable to register module %s for %s",
                 program, path);
    }
    else {
        cgi = NULL;
        apply_policy(NULL, &pol);               /* release RES= strings */
    }

    if (!cgi && binds)
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
    HTTPCGI *cgi;

    /* see parse_mod(): an untokenizable line is treated as policy-bearing */
    tmp = strdup(value);
    if (!tmp) {
        route_policy_lost(httpd, "LOC", value);
        return;
    }

    memset(&pol, 0, sizeof(pol));               /* auth = HTTP_AUTH_DEFAULT */

    ntok = tokenize(tmp, tok, 8);
    if (ntok < 1 || is_route_kv(tok[0])) {      /* first token must be a path */
        wtof("HTTPD415W LOC= requires a path (e.g. LOC /admin/* AUTH=BASIC)");
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
        cgi = NULL;                             /* refuse the route outright */
        apply_policy(NULL, &pol);
    }
    else {
        /* pgm == NULL registers a program-less (static) route */
        cgi = http_add_cgi(httpd, NULL, path, 0);
        apply_policy(cgi, &pol);
        if (cgi)
            wtof("HTTPD417I Location %s registered", path);
        else
            wtof("HTTPD416W Unable to register location %s", path);
    }

    if (!cgi && binds)
        route_policy_lost(httpd, "LOC", path);

    free(tmp);
}

/* ====================================================================
** Parse LOGIN value: NONE, ALL, CGI, GET, HEAD, POST (comma-separated)
** ================================================================= */
static void
parse_login(HTTPD *httpd, const char *value)
{
    char *tmp;
    char *tok;

    tmp = strdup(value);
    if (!tmp) return;

    httpd->login = 0;

    for (tok = strtok(tmp, ","); tok; tok = strtok(NULL, ",")) {
        while (*tok == ' ') tok++;
        if (http_cmp(tok, "ALL") == 0)
            httpd->login |= HTTPD_LOGIN_ALL;
        else if (http_cmp(tok, "CGI") == 0)
            httpd->login |= HTTPD_LOGIN_CGI;
        else if (http_cmp(tok, "GET") == 0)
            httpd->login |= HTTPD_LOGIN_GET;
        else if (http_cmp(tok, "HEAD") == 0)
            httpd->login |= HTTPD_LOGIN_HEAD;
        else if (http_cmp(tok, "POST") == 0)
            httpd->login |= HTTPD_LOGIN_POST;
        else if (http_cmp(tok, "NONE") == 0)
            httpd->login = 0;
        else
            wtof("HTTPD048W Invalid LOGIN value: %s", tok);
    }

    httpd048(httpd);
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

    wtof("HTTPD025W TZOFFSET is no longer used; the system offset "
         "GMT %s%02d:%02d applies", sign < 0 ? "-" : "+", hour, min);
    wtof("HTTPD025W Set TZ in the SYSENV or ENVIRON DD to override it for "
         "all tasks");
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
        wtof("HTTPD028E socket() failed, rc=%d, error=%d", sock, errno);
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
        wtof("HTTPD030E bind() failed for HTTP port, rc=%d, error=%d",
             rc, errno);
        if (error == EADDRINUSE) {
            for (i = 0; i < httpd->bind_tries; i++) {
                wtof("HTTPD030I EADDRINUSE, waiting for TCPIP to "
                     "release HTTP port=%d", httpd->port);
                sleep(httpd->bind_sleep);
                rc = bind(sock, &serv_addr, sizeof(serv_addr));
                if (rc >= 0) break;
            }
        }
        if (rc < 0) {
            wtof("HTTPD030E bind() failed for HTTP port, rc=%d, error=%d",
                 rc, errno);
            closesocket(sock);
            close_stale_port(httpd->port);
            return 8;
        }
    }

    /* listen for connections */
    rc = listen(sock, httpd->listen_queue);
    if (rc < 0) {
        wtof("HTTPD031E listen() failed, rc=%d, error=%d", rc, errno);
        closesocket(sock);
        return 8;
    }

    httpd->listen = sock;
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
                wtof("HTTPD027I Closing stale socket %d on port:%u",
                     i, in->sin_port);
                closesocket(i);
                sleep(2);
                break;
            }
        }
    }
}
