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

/* Forward declarations */
static void set_defaults(HTTPD *httpd);
static void parse_line(HTTPD *httpd, char *line);
static void parse_keyvalue(HTTPD *httpd, const char *key, const char *value);
static void parse_login(HTTPD *httpd, const char *value);
static void parse_tzoffset(HTTPD *httpd, const char *value);
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

    /* try to open DD:HTTPSTAT */
    httpd->stats = fopen("DD:HTTPSTAT", "w");
    if (!httpd->stats) {
        wtof("HTTPD026W Unable to open DD:HTTPSTAT, errno=%d", errno);
    }

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
    httpd->cfg_st_month_max = 24;
    httpd->cfg_st_day_max   = 60;
    httpd->cfg_st_hour_max  = 48;
    httpd->cfg_st_min_max   = 120;
    httpd->cfg_cgictx       = HTTPD_CGICTX_MAX;
    httpd->login            = 0;            /* NONE */
    httpd->client           = HTTPD_CLIENT_INMSG | HTTPD_CLIENT_INDUMP
                            | HTTPD_CLIENT_STATS;
    httpd->ufs_enabled      = 1;
    httpd->bind_tries       = 10;
    httpd->bind_sleep       = 10;
    httpd->listen_queue     = 5;
    httpd->cgilua_dataset   = NULL;
    httpd->cgilua_path      = NULL;
    httpd->cgilua_cpath     = NULL;
    httpd->st_dataset       = NULL;
    httpd->docroot[0]       = '\0';
    httpd->codepage[0]      = '\0';
    httpd->dbg_enabled      = 0;
    httpd->cfg_keepalive_timeout = 5;
    httpd->cfg_keepalive_max     = 100;
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
    else if (strcmp(key, "LOGIN") == 0) {
        parse_login(httpd, value);
    }
    else if (strcmp(key, "TZOFFSET") == 0) {
        parse_tzoffset(httpd, value);
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
    else if (strcmp(key, "CGI") == 0) {
        /* CGI=PROGRAM /path/* */
        char program[9];
        char *path;
        char *tmp;
        int  j;
        int  login = httpd->login & HTTPD_LOGIN_CGI;

        tmp = strdup(value);
        if (!tmp) return;

        /* first token = program name */
        path = tmp;
        while (*path && *path != ' ' && *path != '\t')
            path++;
        if (*path) {
            *path = '\0';
            path++;
            while (*path == ' ' || *path == '\t')
                path++;
        }

        /* fold program name to uppercase, max 8 chars */
        for (j = 0; j < 8 && tmp[j]; j++)
            program[j] = (char)toupper((unsigned char)tmp[j]);
        program[j] = '\0';

        if (program[0] && path[0]) {
            if (!http_add_cgi(httpd, program, path, login)) {
                wtof("HTTPD035W Unable to register CGI %s for %s",
                     program, path);
            } else {
                wtof("HTTPD036I CGI %s registered for %s", program, path);
            }
        }

        free(tmp);
    }
    else if (strcmp(key, "CGILUA_DATASET") == 0) {
        httpd->cgilua_dataset = strdup(value);
    }
    else if (strcmp(key, "CGILUA_PATH") == 0) {
        httpd->cgilua_path = strdup(value);
    }
    else if (strcmp(key, "CGILUA_CPATH") == 0) {
        httpd->cgilua_cpath = strdup(value);
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
    else if (strcmp(key, "CLIENT_STATS_MONTH_MAX") == 0) {
        i = atoi(value);
        if (i > 0) {
            if (i > 255) i = 255;
            httpd->cfg_st_month_max = (UCHAR)i;
        }
    }
    else if (strcmp(key, "CLIENT_STATS_DAY_MAX") == 0) {
        i = atoi(value);
        if (i > 0) {
            if (i > 255) i = 255;
            httpd->cfg_st_day_max = (UCHAR)i;
        }
    }
    else if (strcmp(key, "CLIENT_STATS_HOUR_MAX") == 0) {
        i = atoi(value);
        if (i > 0) {
            if (i > 255) i = 255;
            httpd->cfg_st_hour_max = (UCHAR)i;
        }
    }
    else if (strcmp(key, "CLIENT_STATS_MINUTE_MAX") == 0) {
        i = atoi(value);
        if (i > 0) {
            if (i > 255) i = 255;
            httpd->cfg_st_min_max = (UCHAR)i;
        }
    }
    else if (strcmp(key, "CLIENT_STATS_DATASET") == 0) {
        if (*value) httpd->st_dataset = strdup(value);
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
** Parse LOGIN value: NONE, ALL, CGI, GET, HEAD, POST (comma-separated)
** ================================================================= */
static void
parse_login(HTTPD *httpd, const char *value)
{
    char *tmp;
    char *tok;
    char *saveptr;

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
** Parse TZOFFSET value: integer (hours) or +HH:MM format
** ================================================================= */
static void
parse_tzoffset(HTTPD *httpd, const char *value)
{
    int  sec, hour, min;
    int  sign;

    if (!value || !*value) return;

    /* try simple integer (hours offset) first */
    httpd->tzoffset = atoi(value) * 60;     /* convert minutes to seconds */

    /* try +HH:MM or -HH:MM format */
    if (strchr(value, ':')) {
        sign = 1;
        if (*value == '-') { sign = -1; value++; }
        else if (*value == '+') value++;

        hour = atoi(value);
        value = strchr(value, ':');
        min = value ? atoi(value + 1) : 0;

        httpd->tzoffset = sign * (hour * 3600 + min * 60);
    } else {
        httpd->tzoffset = atoi(value) * 60; /* treat as minutes */
    }

    __tzset(httpd->tzoffset);
    setenvi("TZOFFSET", httpd->tzoffset, 1);

    /* log it */
    sec = httpd->tzoffset;
    sign = sec < 0 ? -1 : 1;
    sec *= sign;
    hour = sec / 3600;
    sec -= hour * 3600;
    min = sec / 60;
    sec -= min * 60;
    wtof("HTTPD025I Time zone offset set to GMT %s%02d:%02d:%02d",
         sign < 0 ? "-" : "+", hour, min, sec);
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
