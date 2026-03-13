/* HTTPDSRV.C - CGI Program, Display Server */
#include "httpd.h"
#include "osdcb.h"      /* DCB -- was transitively included via ufs.h */

#define httpx   (httpd->httpx)

#if 0
static int getself(char *jobname, char *jobid);
#endif

static int display_httpd(HTTPD *httpd, HTTPC *httpc);
static int display_cgi(HTTPD *httpd, HTTPC *httpc);
static int display_file(HTTPD *httpd, HTTPC *httpc);
static int display_fs(HTTPD *httpd, HTTPC *httpc);
static int display_ftpd(HTTPD *httpd, HTTPC *httpc);
static int display_mgr(HTTPD *httpd, HTTPC *httpc);
static int display_task(HTTPD *httpd, HTTPC *httpc);
static int display_ufsdisks(HTTPD *httpd, HTTPC *httpc);
static int display_ufsio(HTTPD *httpd, HTTPC *httpc);
static int display_ufspagers(HTTPD *httpd, HTTPC *httpc);
static int display_ufssb(HTTPD *httpd, HTTPC *httpc);
static int display_help(HTTPD *httpd, HTTPC *httpc);
static int display_ufsvdisks(HTTPD *httpd, HTTPC *httpc);
static int display_workers(HTTPD *httpd, HTTPC *httpc);

static int display_ufs(HTTPD *httpd, HTTPC *httpc, UFS *ufs);
static int display_ufssys(HTTPD *httpd, HTTPC *httpc, UFSSYS *sys);
static int display_cgi_row(HTTPD *httpd, HTTPC *httpc, HTTPCGI *cgi, unsigned n);
static int display_worker_row(HTTPD *httpd, HTTPC *httpc, CTHDWORK *worker, unsigned n);
static int display_queue_data(HTTPD *httpd, HTTPC *httpc, CTHDQUE *q);
#if 0 /* ufs370 internal types -- not available with libufs stub */
static int display_ufsdisk(HTTPD *httpd, HTTPC *httpc, UFSDISK *disk, unsigned n);
static int display_ufsdisk_table(HTTPD *httpd, HTTPC *httpc, UFSDISK *disk);
static int display_ufspager_table(HTTPD *httpd, HTTPC *httpc, UFSPAGER *pager, unsigned n);
static int display_ufsio_table(HTTPD *httpd, HTTPC *httpc, UFSIO *io, unsigned n);
static int display_ufsvdisk(HTTPD *httpd, HTTPC *httpc, UFSVDISK *vdisk, unsigned n);
static int display_ufsvdisk_table(HTTPD *httpd, HTTPC *httpc, UFSVDISK *vdisk);
#endif

static int display_memory(HTTPD *httpd, HTTPC *httpc, const char *title, void *memory, int length, int chunk);
static int try_memory(HTTPD *httpd, HTTPC *httpc, const char *title, void *memory, int length, int chunk);

static int send_resp(HTTPD *httpd, HTTPC *httpc, int resp);
static int send_last(HTTPD *httpd, HTTPC *httpc);

int main(int argc, char **argv)
{
    int         rc      = 0;
    CLIBPPA     *ppa    = __ppaget();
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    char        *target = NULL;
    unsigned    len;    

    if (!httpd) {
        wtof("This program %s must be called by the HTTPD web server%s", argv[0], "");
        /* TSO callers might not see a WTO message, so we send a STDOUT message too */
        printf("This program %s must be called by the HTTPD web server%s", argv[0], "\n");
        return 12;
    }

    /* Get the query variables from the URL */
    if (!target) target = http_get_env(httpc, "QUERY_TARGET");
    if (!target) target = http_get_env(httpc, "QUERY_T");
    if (!target) target = "HTTPD";

    len = strlen(target);
    if (http_cmpn(target, "CGI", len)==0) {
        display_cgi(httpd, httpc);
        goto quit;
    }
    
    if (http_cmpn(target, "FILE", len)==0) {
        display_file(httpd, httpc);
        goto quit;
    }

    if (http_cmpn(target, "FS", len)==0) {
        display_fs(httpd, httpc);
        goto quit;
    }

    if (http_cmpn(target, "FTPD", len)==0) {
        display_ftpd(httpd, httpc);
        goto quit;
    }

    if (http_cmpn(target, "MGR", len)==0) {
        display_mgr(httpd, httpc);
        goto quit;
    }

    if (http_cmpn(target, "HTTPD", len)==0) {
        display_httpd(httpd, httpc);
        goto quit;
    }

    if (http_cmpn(target, "TASK", len)==0) {
        display_task(httpd, httpc);
        goto quit;
    }

#if 0 /* ufs370 internal display -- not available with libufs stub */
    if (http_cmpn(target, "UFSDISKS", len)==0) {
        display_ufsdisks(httpd, httpc);
        goto quit;
    }

    if (http_cmpn(target, "UFSIO", len)==0) {
        display_ufsio(httpd, httpc);
        goto quit;
    }

    if (http_cmpn(target, "UFSPAGERS", len)==0) {
        display_ufspagers(httpd, httpc);
        goto quit;
    }

    if (http_cmpn(target, "UFSSB", len)==0) {
        display_ufssb(httpd, httpc);
        goto quit;
    }

    if (http_cmpn(target, "UFSVDISKS", len)==0) {
        display_ufsvdisks(httpd, httpc);
        goto quit;
    }
#endif

    if (http_cmpn(target, "WORKERS", len)==0) {
        display_workers(httpd, httpc);
        goto quit;
    }

    display_help(httpd, httpc);

quit:
    return 0;
}

static int 
send_resp(HTTPD *httpd, HTTPC *httpc, int resp)
{
    int     rc;

    if (rc = http_resp(httpc, resp) < 0) goto quit;
    if (rc = http_printf(httpc, "Cache-Control: no-store\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "Content-Type: %s\r\n", "text/html") < 0) goto quit;
    if (rc = http_printf(httpc, "Access-Control-Allow-Origin: *\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "\r\n") < 0) goto quit;

    if (rc = http_printf(httpc, "<!DOCTYPE html>\n") < 0) goto quit;
    if (rc = http_printf(httpc, "<html>\n") < 0) goto quit;

    if (rc = http_printf(httpc, "<head>\n") < 0) goto quit;

    if (rc = http_printf(httpc, "<title>HTTPDSRV</title>\n") < 0) goto quit;

    if (rc = http_printf(httpc, "<style>\n") < 0) goto quit;
    if (rc = http_printf(httpc, ".shadowbox {\n") < 0) goto quit;
    if (rc = http_printf(httpc, "  width: 45em;\n") < 0) goto quit;
    if (rc = http_printf(httpc, "  border: 1px solid #333;\n") < 0) goto quit;
    if (rc = http_printf(httpc, "  box-shadow: 8px 8px 5px #444;\n") < 0) goto quit;
    if (rc = http_printf(httpc, "  padding: 8px 12px;\n") < 0) goto quit;
    // if (rc = http_printf(httpc, "  background-image: linear-gradient(180deg, #fff, #ddd 40%%, #ccc);\n") < 0) goto quit;
    if (rc = http_printf(httpc, "  background-image: linear-gradient(270deg, #eee, #ddd 10%%, #ccc);\n") < 0) goto quit;
    if (rc = http_printf(httpc, "}\n") < 0) goto quit;
    if (rc = http_printf(httpc, "</style>\n") < 0) goto quit;

    if (rc = http_printf(httpc, "</head>\n") < 0) goto quit;

    if (rc = http_printf(httpc, "<body>\n") < 0) goto quit;
    if (rc = http_printf(httpc, "<div class=\"shadowbox\">\n") < 0) goto quit;

 
quit:
    return rc;
}

static int 
send_last(HTTPD *httpd, HTTPC *httpc)
{
    int     rc;

    if (rc = http_printf(httpc, "</div>\n") < 0) goto quit;
    if (rc = http_printf(httpc, "</body>\n") < 0) goto quit;
    if (rc = http_printf(httpc, "</html>\n") < 0) goto quit;

quit:
    return rc;
}

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(httpd->a) - (unsigned)httpd)

static int
display_httpd(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    UCHAR       *u;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    http_printf(httpc, "<h2>HTTPD Handle %p</h2>\n", httpd);

#if 0
    http_printf(httpc, 
        "<embed type=\"text/html\" src=\"/.dm?t=HTTPD&m=%p&l=%u\" width=\"800\" height=\"230\">\n",
        httpd, sizeof(HTTPD));
#endif
    display_memory(httpd, httpc, "HTTPD", httpd, sizeof(HTTPD), 16);
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, 
        "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), httpd->eye);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"/.dm?t=HTTPX&m=%p&l=%u\">httpd->httpx</a></td>"
        "<td>Function Pointers for CGI modules</td>"
        "<td>%p</td></tr>\n", 
        8, httpx, sizeof(HTTPX), httpx);

    u = (UCHAR*)&httpd->addr;
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->addr</td>"
        "<td>HTTP Address</td>"
        "<td>%u.%u.%u.%u</td></tr>\n", 
        O(addr), u[0], u[1], u[2], u[3]);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->port</td>"
        "<td>HTTP Port</td>"
        "<td>%u</td></tr>\n", 
        O(port), httpd->port);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->listen</td>"
        "<td>HTTP Listen Socket</td>"
        "<td>%d</td></tr>\n", 
        O(listen), httpd->listen);

    if (httpd->stats) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td><a href=\"?target=FILE&m=%p\">httpd->stats</a></td>"
            "<td>HTTP Statistics File Handle</td>"
            "<td>%p</td></tr>\n", 
            O(stats), httpd->stats, httpd->stats);
    }

    if (httpd->dbg) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td><a href=\"?target=FILE&m=%p\">httpd->dbg</a></td>"
            "<td>HTTP Debug File Handle</td>"
            "<td>%p</td></tr>\n", 
            O(dbg), httpd->dbg, httpd->dbg);
    }

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->tzoffset</td>"
        "<td>Time Zone Offset (seconds)</td>"
        "<td>%d</td></tr>\n", 
        O(tzoffset), httpd->tzoffset);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->busy</td>"
        "<td>HTTP Busy Client Array (%u)</td>"
        "<td>%p</td></tr>\n", 
        O(busy), array_count(&httpd->busy), httpd->busy);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->flag</td>"
        "<td>Processing Flag</td>"
        "<td>%02X",
        O(flag), httpd->flag);
    if (httpd->flag & HTTPD_FLAG_INIT)      http_printf(httpc, " INIT");
    if (httpd->flag & HTTPD_FLAG_LISTENER)  http_printf(httpc, " LISTENER");
    if (httpd->flag & HTTPD_FLAG_READY)     http_printf(httpc, " READY");
    if (httpd->flag & HTTPD_FLAG_QUIESCE)   http_printf(httpc, " QUIESCE");
    if (httpd->flag & HTTPD_FLAG_SHUTDOWN)  http_printf(httpc, " SHUTDOWN");
    http_printf(httpc, "</td></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->login</td>"
        "<td>Login Requirements</td>"
        "<td>%02X",
        O(login), httpd->login);
    if (httpd->login == HTTPD_LOGIN_ALL) {
        http_printf(httpc, " ALL");
    }
    else if (httpd->login == 0) {
        http_printf(httpc, " NONE");
    }
    else {
        if (httpd->login & HTTPD_LOGIN_CGI) {
            http_printf(httpc, " CGI");
        }
        if (httpd->login & HTTPD_LOGIN_GET) {
            http_printf(httpc, " GET");
        }
        if (httpd->login & HTTPD_LOGIN_HEAD) {
            http_printf(httpc, " HEAD");
        }
        if (httpd->login & HTTPD_LOGIN_POST) {
            http_printf(httpc, " POST");
        }
    }
    http_printf(httpc, "</td></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->client</td>"
        "<td>Client Options</td>"
        "<td>%02X",
        O(client), httpd->client);
    if (httpd->client & HTTPD_CLIENT_INMSG) {
        http_printf(httpc, " TIMEOUT");
    }
    if (httpd->client & HTTPD_CLIENT_INDUMP) {
        http_printf(httpc, " DUMP");
    }
    if (httpd->client & HTTPD_CLIENT_STATS) {
        http_printf(httpc, " STATS");
    }
    http_printf(httpc, "</td></tr>\n");

    if (httpd->socket_thread) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td><a href=\"?target=TASK&m=%p\">httpd->socket_thread</a></td>"
            "<td>Socket Thread Handle</td>"
            "<td>%p</td></tr>\n", 
            O(socket_thread), httpd->socket_thread, httpd->socket_thread);
    }

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"?target=MGR&m=%p\">httpd->mgr</a></td>"
        "<td>Thread Manager Handle</td>"
        "<td>%p</td></tr>\n", 
        O(mgr), httpd->mgr, httpd->mgr);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->rname</td>"
        "<td>Resource Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(rname), httpd->rname);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"?target=CGI&m=%p\">httpd->httpcgi</a></td>"
        "<td>Common Gateway Interface Array</td>"
        "<td>%p</td></tr>\n", 
        O(httpcgi), httpd->httpcgi, httpd->httpcgi);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->uptime</td>"
        "<td>Server Up Time</td>"
        "<td>%-24.24s</td></tr>\n", 
        O(uptime), ctime64(&httpd->uptime));

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"?target=FTPD\">httpd->ftpd</a></td>"
        "<td>Embedded FTP Server Handle</td>"
        "<td>%p</td></tr>\n", 
        O(ftpd), httpd->ftpd);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"?target=FS\">httpd->ufssys</a></td>"
        "<td>UFS System Handle</td>"
        "<td>%p</td></tr>\n", 
        O(ufssys), httpd->ufssys);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"/.dm?t=LUAX&m=%p&l=%u\">httpd->luax</a></td>"
        "<td>Lua Function Vector</td>"
        "<td>%p</td></tr>\n", 
        O(luax), httpd->luax, sizeof(LUAX), httpd->luax);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->version</td>"
        "<td>Version String</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(version), httpd->version);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->config</td>"
        "<td>Lua Configuration State</td>"
        "<td>%p</td></tr>\n", 
        O(config), httpd->config);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cfg_maxtask</td>"
        "<td>Config Max Task</td>"
        "<td>%u</td></tr>\n", 
        O(cfg_maxtask), httpd->cfg_maxtask);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cfg_mintask</td>"
        "<td>Config Min Task</td>"
        "<td>%u</td></tr>\n", 
        O(cfg_mintask), httpd->cfg_mintask);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cfg_client_timeout</td>"
        "<td>Config Client Timeout Seconds</td>"
        "<td>%u</td></tr>\n", 
        O(cfg_client_timeout), httpd->cfg_client_timeout);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cfg_st_month_max</td>"
        "<td>Config Statistics Months</td>"
        "<td>%u</td></tr>\n", 
        O(cfg_st_month_max), httpd->cfg_st_month_max);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cfg_st_day_max</td>"
        "<td>Config Statistics Days</td>"
        "<td>%u</td></tr>\n", 
        O(cfg_st_day_max), httpd->cfg_st_day_max);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cfg_st_hour_max</td>"
        "<td>Config Statistics Hours</td>"
        "<td>%u</td></tr>\n", 
        O(cfg_st_hour_max), httpd->cfg_st_hour_max);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cfg_st_min_max</td>"
        "<td>Config Statistics Minutes</td>"
        "<td>%u</td></tr>\n", 
        O(cfg_st_min_max), httpd->cfg_st_min_max);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cfg_cgictx</td>"
        "<td>Config CGI Context Pointers</td>"
        "<td>%u</td></tr>\n", 
        O(cfg_cgictx), httpd->cfg_cgictx);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->st_month</td>"
        "<td>Statistics Month Array</td>"
        "<td>%p</td></tr>\n", 
        O(st_month), httpd->st_month);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->st_day</td>"
        "<td>Statistics Day Array</td>"
        "<td>%p</td></tr>\n", 
        O(st_day), httpd->st_day);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->st_hour</td>"
        "<td>Statistics Hour Array</td>"
        "<td>%p</td></tr>\n", 
        O(st_hour), httpd->st_hour);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->st_min</td>"
        "<td>Statistics Minute Array</td>"
        "<td>%p</td></tr>\n", 
        O(st_min), httpd->st_min);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->st_dataset</td>"
        "<td>Statistics Dataset Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(st_dataset), httpd->st_dataset);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cgilua_dataset</td>"
        "<td>CGI Lua Dataset Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(cgilua_dataset), httpd->cgilua_dataset);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cgilua_path</td>"
        "<td>CGI Lua Path Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(cgilua_path), httpd->cgilua_path);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->cgilua_cpath</td>"
        "<td>CGI Lua CPath Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(cgilua_cpath), httpd->cgilua_cpath);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->ufs</td>"
        "<td>Unix \"like\" File System Handle</td>"
        "<td>%p</td></tr>\n", 
        O(ufs), httpd->ufs);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->httpt</td>"
        "<td>Telemetry (MQTT) Handle</td>"
        "<td>%p</td></tr>\n", 
        O(httpt), httpd->httpt);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"?target=TASK&m=%p\">httpd->self</a></td>"
        "<td>HTTPD Main Thread Handle</td>"
        "<td>%p</td></tr>\n", 
        O(self), httpd->self, httpd->self);
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"/.dm?t=CGICTX&m=%p&l=%u\">httpd->cgictx</a></td>"
        "<td>CGI Context Pointers Array</td>"
        "<td>%p</td></tr>\n", 
        O(cgictx), httpd->cgictx, (HTTPD_CGICTX_MAX+1)*4, httpd->cgictx);

done:
    http_printf(httpc, "</table>\n");
    send_last(httpd, httpc);
	
quit:
	return 0;
}

static int
display_fs(HTTPD *httpd, HTTPC *httpc)
{
    char        *memory = NULL;
    UFSSYS      *sys    = NULL;
    UFS         *ufs    = NULL;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        sys = httpd->ufssys;
        display_ufssys(httpd, httpc, sys);
        goto quit;
    }

    /* convert string to memory pointer */
    memory = (char *) strtoul(memory, NULL, 16);
    if (memcmp(memory, "LIBUFSSY", 8)==0) {
        sys = (UFSSYS*) strtoul(memory, NULL, 16);
        display_ufssys(httpd, httpc, sys);
        goto quit;
    }
    if (memcmp(memory, "LIBUFSUF", 8)==0) {
        ufs = (UFS*) strtoul(memory, NULL, 16);
        display_ufs(httpd, httpc, ufs);
        goto quit;
    }

quit:
    return 0;
}

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(sys->a) - (unsigned)sys)

static int
display_ufssys(HTTPD *httpd, HTTPC *httpc, UFSSYS *sys)
{
    int         rc      = 0;

    if (!sys) {
        /* default to HTTPD ufssys handle */
        sys = httpd->ufssys;
    }

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    http_printf(httpc, "<h2>UFS System Handle %p</h2>\n", sys);

    display_memory(httpd, httpc, "UFSSYS", sys, sizeof(UFSSYS), 16);

    http_printf(httpc, "<table>\n");

    http_printf(httpc,
        "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc,
        "<tr><td>+%04X</td>"
        "<td>sys->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%-8.8s\"</td></tr>\n",
        O(eye), sys->eye);

    http_printf(httpc,
        "<tr><td colspan=\"4\">"
        "Disk and inode state is owned by the UFSD STC."
        "</td></tr>\n");

done:
    http_printf(httpc, "</table>\n");
    send_last(httpd, httpc);

quit:
    return 0;
}

#if 0
typedef struct ufs_sys  UFSSYS;
struct ufs_sys {
    char        eye[8];                 /* 00 eye catcher for dumps             */
#define UFSSYS_EYE  "*UFSSYS*"          /* ... eye catcher for dumps            */
    UFSDISK     **disks;                /* 08 array of disk handles             */
    UFSPAGER    **pagers;               /* 0C array of pager handles            */
    UFSIO       **io;                   /* 10 array of io handles               */
    UFSVDISK    **vdisks;               /* 14 array of virtual disk handles     */
    UFSDEV      **devs;                 /* 18 array of devices                  */
    UINT32      next_dvnum;             /* 1C next device number to assign      */
    UFSMIN      *fsroot;                /* 20 file system root inode "/"        */
    UFSNAME     **names;                /* 24 array of name to vdisk            */
    UFSCWD      **cwds;                 /* 28 array of current working directory*/
    UFSFILE     **files;                /* 2C opened file descriptors           */
    UFSMIN      **mountpoint;           /* 30 array directories with mounted vdisk */
};                                      /* 40 (64 bytes)                        */
#endif

#if 0 /* ufs370 internal display -- not available with libufs stub */

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(vdisk->a) - (unsigned)vdisk)

static int
display_ufsvdisks(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    char        *memory = NULL;
    UFSVDISK    **array = NULL;
    UFSVDISK    *vdisk;
    UCHAR       *u;
    unsigned    n, count;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        http_printf(httpc, "<h2>Missing memory (&m=nnnnnnnn)</h2>\n");
        send_last(httpd, httpc);
        goto quit;
    }

    array = (UFSVDISK **) strtoul(memory, NULL, 16);
    count = array_count(&array);

    http_printf(httpc, "<h2>UFSVDISK Array %p</h2>\n", array);

    display_memory(httpd, httpc, "UFSVDISK Array", array, count*4, 16);
    
    for(n=0; n < count; n++) {
        vdisk = array[n];
        
        if (!vdisk) continue;
        display_ufsvdisk(httpd, httpc, vdisk, n);
    }

quit:
	return 0;
}

static int
display_ufsvdisk(HTTPD *httpd, HTTPC *httpc, UFSVDISK *vdisk, unsigned n)
{
    http_printf(httpc, "<h3>UFSVDISK #%u %p</h3>\n", n, vdisk);

    display_memory(httpd, httpc, "UFSVDISK", vdisk, sizeof(UFSVDISK), 16);
    
    display_ufsvdisk_table(httpd, httpc, vdisk);

quit:
    return 0;
}

static int
display_ufsvdisk_table(HTTPD *httpd, HTTPC *httpc, UFSVDISK *vdisk)
{
    unsigned    count;
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, 
        "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>vdisk->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), vdisk->eye);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>vdisk->disk</td>"
        "<td>Disk Dataset Handle</td>"
        "<td>%p</td></tr>\n", 
        O(disk), vdisk->disk);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>vdisk->io</td>"
        "<td>Disk I/O Function Pointers</td>"
        "<td>%p</td></tr>\n", 
        O(io), vdisk->io);

    count = array_count(&vdisk->minodes);
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>vdisk->minodes</td>"
        "<td>Array Of %u Inodes</td>"
        "<td>%p</td></tr>\n", 
        O(minodes), count, vdisk->minodes);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>vdisk->mounted_minode</td>"
        "<td>Mounted On This Inode</td>"
        "<td>%p</td></tr>\n", 
        O(mounted_minode), vdisk->mounted_minode);

    http_printf(httpc, "<table>\n");

    http_printf(httpc, "<br/>\n");
    
    display_ufsdisk_table(httpd, httpc, vdisk->disk);


quit:
    return 0;
}
#if 0
struct ufsvdisk {
    char        eye[8];             /* 00 eye catcher for dumps             */
#define UFSVDISK_EYE "UFSVDISK"     /* ... eye catcher for dumps            */
    UFSDISK     *disk;              /* 08 disk dataset, blksize, super block*/
    UFSIO       *io;                /* 0C I/O function pointers             */
    UFSMIN      **minodes;          /* 10 inode cache                       */
    UFSMIN      *mounted_minode;    /* 14 mounted on this inode             */
};                                  /* 18 (24 bytes)                        */
#endif

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(disk->a) - (unsigned)disk)

static int
display_ufsdisks(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    char        *memory = NULL;
    UFSDISK     **array = NULL;
    UFSDISK     *disk;
    UCHAR       *u;
    unsigned    n, count;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        http_printf(httpc, "<h2>Missing memory (&m=nnnnnnnn)</h2>\n");
        send_last(httpd, httpc);
        goto quit;
    }

    array = (UFSDISK **) strtoul(memory, NULL, 16);
    count = array_count(&array);

    http_printf(httpc, "<h2>UFSDISK Array %p</h2>\n", array);

    display_memory(httpd, httpc, "UFSDISK Array", array, count*4, 16);
    
    for(n=0; n < count; n++) {
        disk = array[n];
        
        if (!disk) continue;
        display_ufsdisk(httpd, httpc, disk, n);
    }

quit:
	return 0;
}

static int
display_ufsdisk(HTTPD *httpd, HTTPC *httpc, UFSDISK *disk, unsigned n)
{
    http_printf(httpc, "<h3>UFSDISK #%u %p</h3>\n", n, disk);

    display_memory(httpd, httpc, "UFSDISK", disk, sizeof(UFSDISK), 16);
    
    display_ufsdisk_table(httpd, httpc, disk);

quit:
    return 0;
}

static int
display_ufsdisk_table(HTTPD *httpd, HTTPC *httpc, UFSDISK *disk)
{
    UFSBOOT     *boot;
    UFSBOOTE    *boote;
    char        *type;
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, 
        "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>disk->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), disk->eye);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>disk->dcb</td>"
        "<td>DCB For Opened Disk Dataset</td>"
        "<td>%p</td></tr>\n", 
        O(dcb), disk->dcb);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>disk->blksize</td>"
        "<td>Physical Block Size</td>"
        "<td>%u</td></tr>\n", 
        O(blksize), disk->blksize);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>disk->readonly</td>"
        "<td>Disk Read Only</td>"
        "<td>%s</td></tr>\n", 
        O(readonly), disk->readonly ? "Yes" : "No");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>disk->buf</td>"
        "<td>Array Of %u Disk Buffers</td>"
        "<td>%p</td></tr>\n", 
        O(buf), array_count(&disk->buf), disk->buf);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>disk->ddname</td>"
        "<td>Dataset DD Name</td>"
        "<td>%s</td></tr>\n", 
        O(ddname), disk->ddname);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>disk->dsname</td>"
        "<td>Dataset Name</td>"
        "<td>%s</td></tr>\n", 
        O(dsname), disk->dsname);

    boot = &disk->boot;
    switch(boot->type) {
    case UFS_DISK_TYPE_RAW:     type = "RAW";       break;
    case UFS_DISK_TYPE_UNKNOWN: type = "UNKNOWN";   break;
    case UFS_DISK_TYPE_UFS:     type = "UFS";       break;
    default:                    type = "INVALID";   break;
    }
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>disk->boot</td>"
        "<td>Boot Block For Disk Dataset</td>"
        "<td>Type:%s Check:%04X Blksize:%u</td></tr>\n", 
        O(boot), type, boot->check, boot->blksize);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"?target=UFSSB&m=%p\">disk->sb</a></td>"
        "<td>Super Block</td>"
        "<td>%p</td></tr>\n", 
        O(sb), &disk->sb, &disk->sb);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>disk->boote</td>"
        "<td>Boot Extension Address</td>"
        "<td>%p</td></tr>\n", 
        O(boote), disk->boote);


#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(boote->a) - (unsigned)boote)
    
    boote = disk->boote;
    if (boote) {
        http_printf(httpc, "<tr><td><br/></td></tr>\n");
        
        http_printf(httpc, 
            "<tr><th>Offset</th>"
            "<th>Data Name</th>"
            "<th>Description</th>"
            "<th>Contents</th></tr>\n");

        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td>boote->create_time</td>"
            "<td>Disk Created</td>"
            "<td>%-24.24s</td></tr>\n", 
            O(create_time), ctime64(&boote->create_time));
        
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td>boote->update_time</td>"
            "<td>Disk Updated</td>"
            "<td>%-24.24s</td></tr>\n", 
            O(update_time), ctime64(&boote->update_time));
        
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td>boote->version</td>"
            "<td>Version</td>"
            "<td>%u</td></tr>\n", 
            O(version), boote->version);
        
        http_printf(httpc, "<table>\n");
    }

    http_printf(httpc, "<table>\n");


quit:
    return 0;
}


#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(io->a) - (unsigned)io)

static int
display_ufsio(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    char        *memory = NULL;
    UFSIO       **array = NULL;
    UFSIO       *io;
    UCHAR       *u;
    unsigned    n, count;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        http_printf(httpc, "<h2>Missing memory (&m=nnnnnnnn)</h2>\n");
        send_last(httpd, httpc);
        goto quit;
    }

    array = (UFSIO **) strtoul(memory, NULL, 16);
    count = array_count(&array);

    http_printf(httpc, "<h2>UFSIO Array %p</h2>\n", array);

    display_memory(httpd, httpc, "UFSIO Array", array, count*4, 16);
    
    for(n=0; n < count; n++) {
        io = array[n];
        
        if (!io) continue;
        display_ufsio_table(httpd, httpc, io, n);
    }

quit:
	return 0;
}

static int
display_ufsio_table(HTTPD *httpd, HTTPC *httpc, UFSIO *io, unsigned n)
{
    http_printf(httpc, "<h3>UFSIO #%u %p</h3>\n", n, io);

    display_memory(httpd, httpc, "UFSIO", io, sizeof(UFSIO), 16);
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, 
        "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>io->ctx</td>"
        "<td>Function Context</td>"
        "<td>%p</td></tr>\n", 
        O(ctx), io->ctx);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>io->read_fn</td>"
        "<td>Read Function Pointer</td>"
        "<td>%p</td></tr>\n", 
        O(read_fn), io->read_fn);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>io->read_n_fn</td>"
        "<td>Read One Or More Function Pointer</td>"
        "<td>%p</td></tr>\n", 
        O(read_n_fn), io->read_n_fn);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>io->write_fn</td>"
        "<td>Write Function Pointer</td>"
        "<td>%p</td></tr>\n", 
        O(write_fn), io->write_fn);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>io->write_n_fn</td>"
        "<td>Write One Or More Function Pointer</td>"
        "<td>%p</td></tr>\n", 
        O(write_n_fn), io->write_n_fn);



    http_printf(httpc, "<table>\n");

quit:
    return 0;
}


#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(pager->a) - (unsigned)pager)

static int
display_ufspagers(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    char        *memory = NULL;
    UFSPAGER    **array = NULL;
    UFSPAGER    *pager;
    UCHAR       *u;
    unsigned    n, count;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        http_printf(httpc, "<h2>Missing memory (&m=nnnnnnnn)</h2>\n");
        send_last(httpd, httpc);
        goto quit;
    }

    array = (UFSPAGER **) strtoul(memory, NULL, 16);
    count = array_count(&array);

    http_printf(httpc, "<h2>UFSPAGER Array %p</h2>\n", array);

    display_memory(httpd, httpc, "UFSPAGER Array", array, count*4, 16);
    
    for(n=0; n < count; n++) {
        pager = array[n];
        
        if (!pager) continue;
        display_ufspager_table(httpd, httpc, pager, n);
    }

quit:
	return 0;
}

static int
display_ufspager_table(HTTPD *httpd, HTTPC *httpc, UFSPAGER *pager, unsigned n)
{
    UFSBOOT     *boot;
    char        *type;
    
    http_printf(httpc, "<h3>UFSPAGER #%u %p</h3>\n", n, pager);

    display_memory(httpd, httpc, "UFSPAGER", pager, sizeof(UFSPAGER), 16);
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, 
        "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>pager->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), pager->eye);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>pager->disk</td>"
        "<td>Disk Handle</td>"
        "<td>%p</td></tr>\n", 
        O(disk), pager->disk);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>pager->maxpages</td>"
        "<td>Maximum Number Of Pages</td>"
        "<td>%u</td></tr>\n", 
        O(maxpages), pager->maxpages);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>pager->pages</td>"
        "<td>Page Cache %u Array</td>"
        "<td>%p</td></tr>\n", 
        O(pages), array_count(&pager->pages), pager->pages);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>pager->cachehits</td>"
        "<td>Number Of Cache Hits</td>"
        "<td>%u</td></tr>\n", 
        O(cachehits), pager->cachehits);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>pager->reads</td>"
        "<td>Number Of Read Request</td>"
        "<td>%u</td></tr>\n", 
        O(reads), pager->reads);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>pager->writes</td>"
        "<td>Number Of Write Request</td>"
        "<td>%u</td></tr>\n", 
        O(writes), pager->writes);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>pager->dreads</td>"
        "<td>Number Of Disk Reads</td>"
        "<td>%u</td></tr>\n", 
        O(dreads), pager->dreads);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>pager->dwrites</td>"
        "<td>Number Of Disk Writes</td>"
        "<td>%u</td></tr>\n", 
        O(dwrites), pager->dwrites);

    http_printf(httpc, "<table>\n");

    // http_printf(httpc, "<h4>Pager Disk (pager->disk %p)</h4>\n", pager->disk);
    http_printf(httpc, "<br/>\n");
    
    display_ufsdisk_table(httpd, httpc, pager->disk);

quit:
    return 0;
}

#endif /* ufs370 internal display */

#if 0 /* ufs370 internal display -- not available with libufs stub */
#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(sb->a) - (unsigned)sb)

static int
display_ufssb(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    char        *memory = NULL;
    UFSSB       *sb;
    UCHAR       *u;
    unsigned    n, count;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        http_printf(httpc, "<h2>Missing memory (&m=nnnnnnnn)</h2>\n");
        send_last(httpd, httpc);
        goto quit;
    }

    sb = (UFSSB *) strtoul(memory, NULL, 16);

    http_printf(httpc, "<h2>UFSSB Super Block %p</h2>\n", sb);

    display_memory(httpd, httpc, "UFSSB", sb, sizeof(UFSSB), 16);
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, 
        "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->datablock_start_sector</td>"
        "<td>Data Block Start Sector</td>"
        "<td>%u</td></tr>\n", 
        O(datablock_start_sector), sb->datablock_start_sector);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->volume_size</td>"
        "<td>Disk Volume Size In Disk Sectors</td>"
        "<td>%u</td></tr>\n", 
        O(volume_size), sb->volume_size);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->lock_freeblock</td>"
        "<td>Free Block Lock Byte</td>"
        "<td>%u</td></tr>\n", 
        O(lock_freeblock), sb->lock_freeblock);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->lock_freeinode</td>"
        "<td>Free Inode Lock Byte</td>"
        "<td>%u</td></tr>\n", 
        O(lock_freeinode), sb->lock_freeinode);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->modified</td>"
        "<td>Disk Modified</td>"
        "<td>%s</td></tr>\n", 
        O(modified), sb->modified ? "Yes" : "No");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->readonly</td>"
        "<td>Disk Read Only</td>"
        "<td>%s</td></tr>\n", 
        O(readonly), sb->readonly ? "Yes" : "No");

    if (sb->update_time) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td>sb->update_time</td>"
            "<td>Update Time</td>"
            "<td>%-24.24s</td></tr>\n", 
            O(update_time), ctime(&sb->update_time));
    }

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->total_freeblock</td>"
        "<td>Total Free Blocks</td>"
        "<td>%u</td></tr>\n", 
        O(total_freeblock), sb->total_freeblock);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->total_freeinode</td>"
        "<td>Total Free Inodes</td>"
        "<td>%u</td></tr>\n", 
        O(total_freeinode), sb->total_freeinode);

    if (sb->create_time) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td>sb->create_time</td>"
            "<td>Create Time</td>"
            "<td>%-24.24s</td></tr>\n", 
            O(create_time), ctime(&sb->create_time));
    }
    
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->nfreeblock</td>"
        "<td>Free Blocks In Super Block Cache</td>"
        "<td>%u</td></tr>\n", 
        O(nfreeblock), sb->nfreeblock);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->freeblock</td>"
        "<td>Free Blocks Cache</td>"
        "<td>%p %u bytes</td></tr>\n", 
        O(freeblock), sb->freeblock, UFS_MAX_FREEBLOCK*4);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->nfreeinode</td>"
        "<td>Free Inodes In Super Block Cache</td>"
        "<td>%u</td></tr>\n", 
        O(nfreeinode), sb->nfreeinode);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->freeinode</td>"
        "<td>Free Inodes Cache</td>"
        "<td>%p %u bytes</td></tr>\n", 
        O(freeinode), sb->freeinode, UFS_MAX_FREEINODE*4);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->inodes_per_block</td>"
        "<td>Inodes Per Block</td>"
        "<td>%u</td></tr>\n", 
        O(inodes_per_block), sb->inodes_per_block);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->blksize_shift</td>"
        "<td>Block Size Shift Bits</td>"
        "<td>%u</td></tr>\n", 
        O(blksize_shift), sb->blksize_shift);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>sb->ilist_sector</td>"
        "<td>Inode List Starts Here</td>"
        "<td>%u</td></tr>\n", 
        O(ilist_sector), sb->ilist_sector);

    http_printf(httpc, "</table>\n");
    send_last(httpd, httpc);

quit:
	return 0;
}

#endif /* ufs370 internal display */

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(ufs->a) - (unsigned)ufs)

static int
display_ufs(HTTPD *httpd, HTTPC *httpc, UFS *ufs)
{
    int         rc      = 0;
    UCHAR       *u;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    http_printf(httpc, "<h2>UFS Handle %p</h2>\n", ufs);

    display_memory(httpd, httpc, "UFS", ufs, sizeof(UFS), 16);
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, 
        "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>httpd->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%-8.8s\"</td></tr>\n", 
        O(eye), ufs->eye);



done:
    http_printf(httpc, "</table>\n");
    send_last(httpd, httpc);
	
quit:
	return 0;
}
#if 0
/* Private "user" handle */
struct ufs {
    char        eye[8];             /* 00 eye catcher                       */
#define UFSEYE  "**UFS**"           /* ... eye catcher                      */
    UFSSYS      *sys;               /* 08 system handle                     */
    UFSCWD      *cwd;               /* 0C Current working directory         */
    ACEE        *acee;              /* 10 user security handle              */
    UINT32      flags;              /* 14 flags                             */
#define UFS_ACEE_DEFAULT    0x80000000  /* address space acee handle        */
#define UFS_ACEE_USER       0x40000000  /* user set acee handle             */
#define UFS_ACEE_SIGNON     0x20000000  /* ufs_signon() acee handle         */
    UINT32      create_perm;        /* 18 file creation permission          */
    UINT32      unused;             /* 1C unused                            */
};                                  /* 20 (32 bytes)                        */
#endif

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(ftpd->a) - (unsigned)ftpd)

static int
display_ftpd(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    FTPD        *ftpd   = httpd->ftpd;
    unsigned    n, count;
    UCHAR       *u;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    http_printf(httpc, "<h2>FTPD Handle %p</h2>\n", ftpd);

#if 0
    http_printf(httpc, 
        "<embed type=\"text/html\" src=\"/.dm?t=FTPD&m=%p&l=%u\" width=\"800\" height=\"230\">\n",
        ftpd, sizeof(FTPD));
#endif
    display_memory(httpd, httpc, "FTPD", ftpd, sizeof(FTPD), 16);
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, 
        "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>ftpd->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), ftpd->eye);

    if (ftpd->ftpd_thread) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td><a href=\"?target=TASK&m=%p\">ftpd->ftpd_thread</a></td>"
            "<td>Subtask Thread Handle</td>"
            "<td>%p</td></tr>\n", 
            O(ftpd_thread), ftpd->ftpd_thread, ftpd->ftpd_thread);
    }
    else {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td>ftpd->ftpd_thread</td>"
            "<td>Subtask Thread Handle</td>"
            "<td>%p</td></tr>\n", 
            O(ftpd_thread), ftpd->ftpd_thread);
    }

    count = array_count(&ftpd->ftpc);
    if (count) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td><a href=\"?target=FTPC&m=%p\">ftpd->ftpc</a></td>"
            "<td>FTP Clients Array</td>"
            "<td>%p</td></tr>\n", 
            O(ftpc), ftpd->ftpc, ftpd->ftpc);
    }
    else {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td>ftpd->ftpc</td>"
            "<td>FTP Clients Array</td>"
            "<td>%p</td></tr>\n", 
            O(ftpc), ftpd->ftpc);
    }

    u = (unsigned char*)&ftpd->addr;
    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>ftpd->addr</td>"
        "<td>FTPD Listen Address</td>"
        "<td>%u.%u.%u.%u</td></tr>\n", 
        O(addr), u[0], u[1], u[2], u[3]);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>ftpd->port</td>"
        "<td>FTPD Listen Port</td>"
        "<td>%d</td></tr>\n", 
        O(port), ftpd->port);

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>ftpd->listen</td>"
        "<td>FTPD Listen Socket</td>"
        "<td>%d</td></tr>\n", 
        O(listen), ftpd->listen);

    if (ftpd->stats) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td><a href=\"?target=FILE&m=%p\">ftpd->stats</a></td>"
            "<td>FTP Statistics File</td>"
            "<td>%p</td></tr>\n", 
            O(stats), ftpd->stats, ftpd->stats);
    }
    else {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td>ftpd->stats</td>"
            "<td>FTPD Statistics File</td>"
            "<td>%p</td></tr>\n", 
            O(stats), ftpd->stats);
    }

    if (ftpd->dbg) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td><a href=\"?target=FILE&m=%p\">ftpd->dbg</a></td>"
            "<td>FTP Debug File</td>"
            "<td>%p</td></tr>\n", 
            O(dbg), ftpd->dbg, ftpd->dbg);
    }
    else {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td>ftpd->dbg</td>"
            "<td>FTPD Debug File</td>"
            "<td>%p</td></tr>\n", 
            O(dbg), ftpd->dbg);
    }

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td>ftpd->flags</td>"
        "<td>FTPD Processing Flags</td>"
        "<td>%02X", 
        O(flags), ftpd->flags);
    if (ftpd->flags & FTPD_FLAG_INIT)       http_printf(httpc, " INIT");
    if (ftpd->flags & FTPD_FLAG_LISTENER)   http_printf(httpc, " LISTENER");
    if (ftpd->flags & FTPD_FLAG_READY)      http_printf(httpc, " READY");
    if (ftpd->flags & FTPD_FLAG_QUIESCE)    http_printf(httpc, " QUIESCE");
    if (ftpd->flags & FTPD_FLAG_SHUTDOWN)   http_printf(httpc, " SHUTDOWN");
    http_printf(httpc, "</td></tr>\n");

    http_printf(httpc, 
        "<tr><td>+%04X</td>"
        "<td><a href=\"?target=UFSSYS&m=%p\">ftpd->sys</a></td>"
        "<td>Unix \"like\" File System Handle</td>"
        "<td>%p</td></tr>\n", 
        O(sys), ftpd->sys, ftpd->sys);

done:
    http_printf(httpc, "</table>\n");
    send_last(httpd, httpc);
	
quit:
	return 0;
}


#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(cgi->a) - (unsigned)cgi)

static int
display_cgi(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    HTTPCGI     *cgi    = NULL;
    HTTPCGI     **array = NULL;
    char        *memory = NULL;
    unsigned    n, count;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        http_printf(httpc, "<h2>Missing memory address (&m=nnnnnnnn)</h2>\n");
        goto done;
    }

    array = (HTTPCGI**) strtoul(memory, NULL, 16);
    count = array_count(&array);

    http_printf(httpc, "<h2>CGI Array %p</h2>", array);
#if 0
    http_printf(httpc, 
        "<embed type=\"text/html\" src=\"/.dm?t=%s&m=%p&l=%u\" width=\"800\" height=\"140\">\n",
        "CGI%20Array", array, count*sizeof(HTTPCGI*));
#endif
    display_memory(httpd, httpc, "CGI Array", array, count*sizeof(HTTPCGI*), 16);
    

    for(n=0; n < count; n++) {
        cgi = array[n];
        
        if (!cgi) continue;
        display_cgi_row(httpd, httpc, cgi, n);
    }

done:
    send_last(httpd, httpc);
	
quit:
	return 0;
}

static int 
display_cgi_row(HTTPD *httpd, HTTPC *httpc, HTTPCGI *cgi, unsigned n)
{
    char        title[40];
    
    sprintf(title, "CGI #%u", n);

    http_printf(httpc, "<h3>%s</h3>\n", title);

    display_memory(httpd, httpc, title, cgi, sizeof(HTTPCGI), 16);

    http_printf(httpc, "<table>\n");

    http_printf(httpc, "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");
    
    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>cgi->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), cgi->eye);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>cgi->wild</td>"
        "<td>Is Wildcard</td>"
        "<td>%u</td></tr>\n", 
        O(wild), cgi->wild);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>cgi->login</td>"
        "<td>Login Required</td>"
        "<td>%u</td></tr>\n", 
        O(login), cgi->login);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>cgi->len</td>"
        "<td>Path Length</td>"
        "<td>%u</td></tr>\n", 
        O(len), cgi->len);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>cgi->path</td>"
        "<td>Path Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(path), cgi->path);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>cgi->pgm</td>"
        "<td>Program Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(pgm), cgi->pgm);

#if 0
    http_printf(httpc, "<tr><td>----------</td>"
        "<td>-------------</td>"
        "<td>------------------------</td>"
        "<td>------------------------</td></tr>\n");
#endif

    http_printf(httpc, "</table>\n");

    return 0;
}


#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(fp->a) - (unsigned)fp)

static int
display_file(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    FILE        *fp     = NULL;
    char        *memory = NULL;
    UCHAR       type;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        http_printf(httpc, "<h2>Missing memory address (&m=nnnnnnnn)</h2>\n");
        goto done;
    }
    else {
        fp = (FILE*) strtoul(memory, NULL, 16);
    }

    http_printf(httpc, "<h2>FILE Handle %p</h2>\n", fp);
#if 0
    http_printf(httpc, 
        "<embed type=\"text/html\" src=\"/.dm?t=%s&m=%p&l=%u\" width=\"800\" height=\"260\">\n",
        "FILE%20Handle", fp, sizeof(FILE));
#endif
    display_memory(httpd, httpc, "FILE Handle", fp, sizeof(FILE), 16);

    http_printf(httpc, "<table>\n");

    http_printf(httpc, "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), fp->eye);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td><a href=\"/.dm?title=%s&m=%p&l=%u\">fp->dcb</a></td>"
        "<td>Data Control Block</td>"
        "<td>%p</td></tr>\n", 
        O(dcb), "FILE%20Handle%20DCB", fp->dcb, sizeof(DCB), fp->dcb);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->asmbuf</td>"
        "<td>Buffer For Assembler Routines</td>"
        "<td>%p</td></tr>\n", 
        O(asmbuf), fp->asmbuf);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->lrecl</td>"
        "<td>Logical Record Length</td>"
        "<td>%u</td></tr>\n", 
        O(lrecl), fp->lrecl);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->blksize</td>"
        "<td>Physical Block Size</td>"
        "<td>%u</td></tr>\n", 
        O(blksize), fp->blksize);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->filepos</td>"
        "<td>Character Offset In File</td>"
        "<td>%u</td></tr>\n", 
        O(filepos), fp->filepos);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td><a href=\"/.dm?title=%s&m=%p&l=%u\">fp->buf</a></td>"
        "<td>File Buffer</td>"
        "<td>%p</td></tr>\n", 
        O(buf), "FILE%20Handle%20Buffer", fp->buf, fp->lrecl+8, fp->buf);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->upto</td>"
        "<td>Current Position In Buffer</td>"
        "<td>%u</td></tr>\n", 
        O(upto), fp->upto - fp->buf);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->endbuf</td>"
        "<td>End Of Buffer</td>"
        "<td>%u</td></tr>\n", 
        O(endbuf), fp->endbuf - fp->buf);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->flags</td>"
        "<td>Processing Flags</td>"
        "<td>%04X",
        O(flags), fp->flags);
    if (fp->flags & _FILE_FLAG_DYNAMIC) http_printf(httpc, " DYNAMIC");
    if (fp->flags & _FILE_FLAG_OPEN)    http_printf(httpc, " OPEN");
    if (fp->flags & _FILE_FLAG_READ)    http_printf(httpc, " READ");
    if (fp->flags & _FILE_FLAG_WRITE)   http_printf(httpc, " WRITE");
    if (fp->flags & _FILE_FLAG_APPEND)  http_printf(httpc, " APPEND");
    if (fp->flags & _FILE_FLAG_BINARY)  http_printf(httpc, " BINARY");
    if (fp->flags & _FILE_FLAG_RECORD)  http_printf(httpc, " RECORD");
    if (fp->flags & _FILE_FLAG_BSAM)    http_printf(httpc, " BSAM");
    if (fp->flags & _FILE_FLAG_TERM)    http_printf(httpc, " TERM");
    if (fp->flags & _FILE_FLAG_ERROR)   http_printf(httpc, " ERROR");
    if (fp->flags & _FILE_FLAG_EOF)     http_printf(httpc, " EOF");
    http_printf(httpc, "</td></tr>\n");

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->recfm</td>"
        "<td>Record Format</td>"
        "<td>%02X",
        O(recfm), fp->recfm);
    
    type = fp->recfm & _FILE_RECFM_TYPE;
    if (type == _FILE_RECFM_F)          http_printf(httpc, " FIXED");
    if (type == _FILE_RECFM_V)          http_printf(httpc, " VARIABLE");
    if (type == _FILE_RECFM_U)          http_printf(httpc, " UNDEFINED");
    
    if (fp->recfm & _FILE_RECFM_O)      http_printf(httpc, " OVERFLOW");
    if (fp->recfm & _FILE_RECFM_B)      http_printf(httpc, " BLOCKED");
    if (fp->recfm & _FILE_RECFM_S)      http_printf(httpc, " SPANNED");
    
    if (fp->recfm & _FILE_RECFM_A)      http_printf(httpc, " ASA");
    if (fp->recfm & _FILE_RECFM_M)      http_printf(httpc, " MACHINE");
    if (fp->recfm & _FILE_RECFM_L)      http_printf(httpc, " KEYLEN");
    http_printf(httpc, "</td></tr>\n");
    
    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->ddname</td>"
        "<td>DD Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(ddname), fp->ddname);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->member</td>"
        "<td>Member Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(member), fp->member);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->dataset</td>"
        "<td>Dataset Name</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(dataset), fp->dataset);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>fp->mode</td>"
        "<td>Mode String</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(mode), fp->mode);

    http_printf(httpc, "</table>\n");

done:
    send_last(httpd, httpc);
	
quit:
	return 0;
}

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(task->a) - (unsigned)task)

static int
display_task(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    CTHDTASK    *task   = NULL;
    char        *memory = NULL;
    UCHAR       type;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        http_printf(httpc, "<h2>Missing memory address (&m=nnnnnnnn)</h2>\n");
        goto done;
    }
    else {
        task = (CTHDTASK*) strtoul(memory, NULL, 16);
    }

    http_printf(httpc, "<h2>TASK Handle %p</h2>\n", task);
#if 0
    http_printf(httpc, 
        "<embed type=\"text/html\" src=\"/.dm?t=%s&m=%p&l=%u\" width=\"800\" height=\"160\">\n",
        "TASK%20Handle", task, sizeof(CTHDTASK));
#endif
    display_memory(httpd, httpc, "TASK Handle", task, sizeof(CTHDTASK), 16);
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>task->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), task->eye);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>task->tcb</td>"
        "<td>MVS Task Control Block</td>"
        "<td>%p</td></tr>\n", 
        O(tcb), task->tcb);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>task->owntcb</td>"
        "<td>Owner MVS Task Control Block</td>"
        "<td>%p</td></tr>\n", 
        O(owntcb), task->owntcb);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>task->termecb</td>"
        "<td>Termination Event Control Block</td>"
        "<td>%p</td></tr>\n", 
        O(termecb), task->termecb);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>task->rc</td>"
        "<td>Return Code</td>"
        "<td>%d</td></tr>\n", 
        O(rc), task->rc);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>task->stacksize</td>"
        "<td>Stack Size</td>"
        "<td>%u</td></tr>\n", 
        O(stacksize), task->stacksize);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>task->func</td>"
        "<td>Function Address</td>"
        "<td>%p</td></tr>\n", 
        O(func), task->func);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>task->arg1</td>"
        "<td>Function Parameter #1 Address</td>"
        "<td>%p</td></tr>\n", 
        O(arg1), task->arg1);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>task->arg2</td>"
        "<td>Function Parameter #2 Address</td>"
        "<td>%p</td></tr>\n", 
        O(arg2), task->arg2);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td><a href=\"/.dm?title=%s&m=%p&l=%u\">task->stack</a></td>"
        "<td>Function Stack</td>"
        "<td>%p</td></tr>\n", 
        O(stack), "TASK%20Stack", task->stack, task->stacksize, task->stack);

    http_printf(httpc, "</table>\n");

done:
    send_last(httpd, httpc);
	
quit:
	return 0;
}

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(mgr->a) - (unsigned)mgr)

static int
display_mgr(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    CTHDMGR     *mgr    = httpd->mgr;
    char        *s;
    unsigned    n, count;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    http_printf(httpc, "<h2>HTTPD Thread Manager %p</h2>", mgr);
#if 0
    http_printf(httpc, 
        "<embed type=\"text/html\" src=\"/.dm?t=%s&m=%p&l=%u\" width=\"800\" height=\"170\">\n",
        "HTTPD%20Thread%20Manager", mgr, sizeof(CTHDMGR));
#endif
    display_memory(httpd, httpc, "HTTPD Thread Manager", mgr, sizeof(CTHDMGR), 16);

    if (strcmp(mgr->eye, CTHDMGR_EYE)!=0) {
        http_printf(httpc, "<h2>Invalid Eye Catcher</h2>\n");
        goto done;
    }

    http_printf(httpc, "<table>\n");

    http_printf(httpc, "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), mgr->eye);

    if (mgr->task) {
        http_printf(httpc, 
            "<tr><td>+%04X</td>"
            "<td><a href=\"?target=TASK&m=%p\">mgr->task</a></td>"
            "<td>Thread Manager Task Handle</td>"
            "<td>%p</td></tr>\n", 
            O(task), mgr->task, mgr->task);
    }

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->wait</td>"
        "<td>Wait For Work ECB</td>"
        "<td>%p</td></tr>\n", 
        O(wait), mgr->wait);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->func</td>"
        "<td>Thread Function Address</td>"
        "<td>%p</td></tr>\n", 
        O(func), mgr->func);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->udata</td>"
        "<td>Thread User Data</td>"
        "<td>%p</td></tr>\n", 
        O(udata), mgr->udata);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->stacksize</td>"
        "<td>Thread Stack Size</td>"
        "<td>%u</td></tr>\n", 
        O(stacksize), mgr->stacksize);

    if (mgr->worker) {
        http_printf(httpc, "<tr><td>+%04X</td>"
            "<td><a href=\"?target=WORKERS&m=%p\">mgr->worker</a></td>"
            "<td>Worker Threads Array</td>"
            "<td>%p</td></tr>\n", 
            O(worker), mgr->worker, mgr->worker);
    }
    
    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->queue</td>"
        "<td>Worker Queue Array</td>"
        "<td>%p</td></tr>\n", 
        O(queue), mgr->queue);

    switch(mgr->state) {
    case CTHDMGR_STATE_INIT:    s = "INIT";     break;
    case CTHDMGR_STATE_RUNNING: s = "RUNNING";  break;
    case CTHDMGR_STATE_QUIESCE: s = "QUIESCE";  break;
    case CTHDMGR_STATE_STOPPED: s = "STOPPED";  break;
    case CTHDMGR_STATE_WAITING: s = "WAITING";  break;
    default:                    s = "UNKNOWN";  break;
    }
    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->state</td>"
        "<td>Current State</td>"
        "<td>%d \"%s\"</td></tr>\n", 
        O(state), mgr->state, s);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->mintask</td>"
        "<td>Minimum Worker Threads</td>"
        "<td>%u</td></tr>\n", 
        O(mintask), mgr->mintask);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->maxtask</td>"
        "<td>Maximum Worker Threads</td>"
        "<td>%u</td></tr>\n", 
        O(maxtask), mgr->maxtask);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->dispatched</td>"
        "<td>Dispatched Worker Threads Count</td>"
        "<td>%llu</td></tr>\n", 
        O(dispatched), mgr->dispatched);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->start</td>"
        "<td>Round Robin Start Index</td>"
        "<td>%u</td></tr>\n", 
        O(start), mgr->start);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>mgr->rname</td>"
        "<td>Resource Name String</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(rname), mgr->rname);


    http_printf(httpc, "</table>\n");

done:
    send_last(httpd, httpc);
	
quit:
	return 0;
}

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(worker->a) - (unsigned)worker)

static int
display_workers(HTTPD *httpd, HTTPC *httpc)
{
    int         rc      = 0;
    char        *memory = NULL;
    CTHDWORK    **array = NULL;
    CTHDWORK    *worker = NULL;
    unsigned    n, count;

    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    /* Get the query variables from the URL */
    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) {
        http_printf(httpc, "<h2>Missing memory address (&m=nnnnnnnn)</h2>\n");
        goto done;
    }
    else {
        array = (CTHDWORK**) strtoul(memory, NULL, 16);
    }
    
    http_printf(httpc, "<h2>Worker Threads Array %p</h2>\n", array);

    count = array_count(&array);

#if 0
    http_printf(httpc, 
        "<embed type=\"text/html\" src=\"/.dm?t=%s&m=%p&l=%u\" width=\"800\" height=\"110\">\n",
        "Worker%20Threads%20Array", array, count*4);
    http_printf(httpc, "<br/>\n");
#endif
    display_memory(httpd, httpc, "Worker Threads Array", array, count*4, 16);
    
    for(n=0; n < count; n++) {
        worker = array[n];
        
        if (!worker) continue; 
        if (memcmp(worker->eye, CTHDWORK_EYE, 8)!=0) continue;
        
        display_worker_row(httpd, httpc, worker, n);
    }

done:
    send_last(httpd, httpc);
	
quit:
	return 0;
}

static int 
display_worker_row(HTTPD *httpd, HTTPC *httpc, CTHDWORK *worker, unsigned n)
{
    char        *s;
    char        title[40];

    http_printf(httpc, "<h3>Worker Thread #%u %p</h3>\n", n, worker);

#if 0
    http_printf(httpc, 
        "<embed type=\"text/html\" src=\"/.dm?t=%s&m=%p&l=%u\" width=\"800\" height=\"140\">\n",
        "WORKER", worker, sizeof(CTHDWORK));
#endif
    sprintf(title, "Worker Thread #%u", n);
    display_memory(httpd, httpc, title, worker, sizeof(CTHDWORK), 16);

    switch(worker->state) {
    case CTHDWORK_STATE_INIT:       s = "INIT";     break;
    case CTHDWORK_STATE_RUNNING:    s = "RUNNING";  break;
    case CTHDWORK_STATE_WAITING:    s = "WAITING";  break;
    case CTHDWORK_STATE_DISPATCH:   s = "DISPATCH"; break;
    case CTHDWORK_STATE_SHUTDOWN:   s = "SHUTDOWN"; break;
    case CTHDWORK_STATE_STOPPED:    s = "STOPPED";  break;
    default:                        s = "UNKNOWN";  break;
    }

    http_printf(httpc, "<table>\n");

    http_printf(httpc, "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");
    
    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%-8.8s\"</td></tr>\n", 
        O(eye), worker->eye);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->wait</td>"
        "<td>Wait Event Control Block</td>"
        "<td>%p</td></tr>\n", 
        O(wait), worker->wait);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->mgr</td>"
        "<td>Thread Manager Handle</td>"
        "<td>%p</td></tr>\n", 
        O(mgr), worker->mgr);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td><a href=\"?target=TASK&m=%p\">worker->task</a></td>"
        "<td>Thread Task Handle</td>"
        "<td>%p</td></tr>\n", 
        O(task), worker->task, worker->task);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->queue</td>"
        "<td>Work Queue Handle</td>"
        "<td>%p</td></tr>\n", 
        O(queue), worker->queue);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->state</td>"
        "<td>Current State</td>"
        "<td>%d (%s)</td></tr>\n", 
        O(state), worker->state, s);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->opt</td>"
        "<td>Options Flags</td>"
        "<td>%p",
        O(opt), worker->opt);
    if (worker->opt & CTHDWORK_OPT_TIMER)  http_printf(httpc, " TIMER");
    if (worker->opt & CTHDWORK_OPT_NOWORK) http_printf(httpc, " NOWORK");
    http_printf(httpc, "</td></tr>\n");

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->start_time</td>"
        "<td>Start Time</td>"
        "<td>%-24.24s</td></tr>\n", 
        O(start_time), ctime64(&worker->start_time));

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->wait_time</td>"
        "<td>Wait Time</td>"
        "<td>%-24.24s</td></tr>\n", 
        O(wait_time), ctime64(&worker->wait_time));

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->disp_time</td>"
        "<td>Dispatch Time</td>"
        "<td>%-24.24s</td></tr>\n", 
        O(disp_time), ctime64(&worker->disp_time));

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>worker->dispatched</td>"
        "<td>Dispatched Count</td>"
        "<td>%llu</td></tr>\n", 
        O(dispatched), worker->dispatched);

    http_printf(httpc, "</table>\n");

    if (worker->state==CTHDWORK_STATE_RUNNING && worker->queue) {
        /* since we're not holding any lock, we want to use try() to handle unexpected failures */
        try(display_queue_data, httpd, httpc, worker->queue);
    }

    return 0;
}

static int
ntoa(unsigned addr, char *buf)
{
    unsigned char *p = (unsigned char*)&addr;

    return sprintf(buf, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
}

#ifdef O
#undef O
#endif
#define O(a) ((unsigned)&(q->a) - (unsigned)q)

static int
display_queue_data(HTTPD *httpd, HTTPC *httpc, CTHDQUE *q)
{
    HTTPC       *h          = q->data;
    FTPC        *ftpc       = q->data;
    struct sockaddr addr;
    struct sockaddr_in *in  = (struct sockaddr_in*)&addr;
    int         addrlen;
    char        ip[20]      = "";
    char        user[12]    = "ANONYMOUS";
    char        group[12]   = "";

    if (!q) goto quit;
    if (!q->data) goto quit;

    // http_printf(httpc, "<h4>Worker Queue Data %p</h4>\n", q);
    http_printf(httpc, "<br/>\n");
    
    http_printf(httpc, "<table>\n");

    http_printf(httpc, "<tr><th>Offset</th>"
        "<th>Data Name</th>"
        "<th>Description</th>"
        "<th>Contents</th></tr>\n");
    

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>queue->eye</td>"
        "<td>Eye Catcher</td>"
        "<td>\"%s\"</td></tr>\n", 
        O(eye), q->eye);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>queue->mgr</td>"
        "<td>Thread Manager Handle</td>"
        "<td>%p</td></tr>\n", 
        O(mgr), q->mgr);

    http_printf(httpc, "<tr><td>+%04X</td>"
        "<td>queue->data</td>"
        "<td>Data Handle</td>"
        "<td>%p</td></tr>\n", 
        O(data), q->data);

    http_printf(httpc, "</table>\n");

    // http_printf(httpc, "<h4>Worker Activity</h4>\n");

    if (strcmp(h->eye, HTTPC_EYE)==0) {
        CRED	*cred = h->cred;

        
        if (cred) {
            CREDID id;
				
            credid_dec(&cred->id, &id);
            strncpy(user, id.userid, sizeof(user));

            if (cred->acee) {
                memcpyp(user, sizeof(user), &cred->acee->aceeuser[1], cred->acee->aceeuser[0], 0);
                memcpyp(group, sizeof(group), &cred->acee->aceegrp[1], cred->acee->aceegrp[0], 0);
            }
        }

        addrlen = sizeof(addr);
        getsockname(h->socket, &addr, &addrlen);
        ntoa(in->sin_addr.s_addr, ip);

        http_printf(httpc, "<pre>\n");
        http_printf(httpc, "PROTOCOL        HTTP\n");
        http_printf(httpc, "USER            %-9.9s\n", user);
        http_printf(httpc, "GROUP           %s\n", group);
        http_printf(httpc, "SOCKET          %d\n", h->socket);
        http_printf(httpc, "LOCAL           %s:%d\n", ip, in->sin_port);

        addrlen = sizeof(addr);
        getpeername(h->socket, &addr, &addrlen);
        ntoa(in->sin_addr.s_addr, ip);
        http_printf(httpc, "REMOTE          %s:%d\n", ip, in->sin_port);
        http_printf(httpc, "</pre>\n");
        goto quit;
    }

    if (strcmp(ftpc->eye, FTPC_EYE)==0) {
        ACEE *acee = ftpc->acee;

        if (acee) {
            memcpyp(user, sizeof(user), &acee->aceeuser[1], acee->aceeuser[0], 0);
            memcpyp(group, sizeof(group), &acee->aceegrp[1], acee->aceegrp[0], 0);
        }

        addrlen = sizeof(addr);
        getsockname(ftpc->client_socket, &addr, &addrlen);
        ntoa(in->sin_addr.s_addr, ip);

        http_printf(httpc, "<pre>\n");
        http_printf(httpc, "PROTOCOL        FTP\n");
        http_printf(httpc, "USER            %-9.9s\n", user);
        http_printf(httpc, "GROUP           %s\n", group);
        http_printf(httpc, "SOCKET          %d\n", ftpc->client_socket);
        http_printf(httpc, "LOCAL           %s:%d\n", ip, in->sin_port);

        addrlen = sizeof(addr);
        getpeername(ftpc->client_socket, &addr, &addrlen);
        ntoa(in->sin_addr.s_addr, ip);
        http_printf(httpc, "REMOTE          %s:%d\n", ip, in->sin_port);
        http_printf(httpc, "</pre>\n");

        goto quit;
    }

quit:
    return 0;
}

static int
display_memory(HTTPD *httpd, HTTPC *httpc, const char *title, void *memory, int length, int chunk)
{
    int         rc      = 0;
	char		*mem    = memory;
    int         len     = length;
    int         size    = chunk;

    http_printf(httpc, "<form action=\"/.dm\">\n");
    if (title && title[0]) {
        http_printf(httpc, "<input type=\"hidden\" id=\"title\" name=\"title\" value=\"%s\">\n", title);
    }
       
    http_printf(httpc, "<label for\"memory\">Memory Address:</label>\n");
    http_printf(httpc, "<input type=\"text\" id=\"memory\" name=\"memory\" size=\"8\" value=\"%p\">\n", mem);
    http_printf(httpc, "<label for\"length\">Data Length:</label>\n");
    http_printf(httpc, "<input type=\"number\" id=\"length\" name=\"length\" size=\"4\" min=\"1\" max=\"4096\" value=\"%u\">\n", len);
    http_printf(httpc, "<label for=\"chunk\">Chunk Size:</label>\n");
    http_printf(httpc, "<input type=\"number\" id=\"chunk\" name=\"chunk\" size=\"2\" step=\"4\" min=\"4\" max=\"64\" value=\"%u\">\n", size);
    http_printf(httpc, "<input type=\"submit\" value=\"submit\">\n");
    http_printf(httpc, "</form>\n");
    
    http_printf(httpc, "<pre>\n");

	/* call try_memory() from ESTAE protected try() */
	try(try_memory, httpd, httpc, title, memory, length, chunk);
    rc = tryrc();
    if (rc==0) {
        // http_printf(httpc, "End of memory dump for 0x%06X\n", mem);
        goto done;
    }
	
    if (rc < 0) {
        rc *= -1;	/* make rc positive value */
        http_printf(httpc, "ESTAE CREATE failure, RC=0x%08X\n", rc);
        goto done;
    }
	
    if (rc > 0xFFF) {
        /* system ABEND occured */
        http_printf(httpc, "ABEND S%03X occured for DISPLAY MEMORY 0x%06X\n", 
            (rc >> 12) & 0xFFF, mem);
    }
    else {
        /* user ABEND occured */
        http_printf(httpc, "ABEND U%04d occured for DISPLAY MEMORY 0x%06X\n", 
            rc & 0xFFF, mem);
    }
    
done:
    http_printf(httpc, "</pre>\n");
	
quit:
	return 0;
}

static int 
try_memory(HTTPD *httpd, HTTPC *httpc, const char *title, void *memory, int length, int chunk)
{
    int             size    = length;  /* memory size */
    int             pos     = 0;
    int             i, j;
    int             ie      = 0;
    int             x       = (chunk * 2) + ((chunk / 4) - 1);
    unsigned        addr    = 0;
    unsigned char   *area   = (unsigned char*)memory;
    unsigned char   e;
    char            eChar[256];

    if (title && title[0]) {
        http_printf(httpc, "Dump of %s %08X (%d byte chunks) (%d bytes total)\n",
            title, memory, chunk, size);
    }
    else {
        http_printf(httpc, "Dump of %08X (%d byte chunks) (%d bytes total)\n",
            memory, chunk, size);
    }

    http_printf(httpc, "+00000 ");
    pos = 7;
    for(j=i=0; size > 0; i++) {
        if ( i==chunk ) {
            http_printf(httpc, ":%s:\n", eChar);
            j += i;
            http_printf(httpc, "+%05X ", j);
            ie = i = 0;
            pos = 7;
        }
        
        if ((i&3)==0) {
            addr = *(unsigned*)area;
            http_printf(httpc, "<a href=\"/.dm?&m=%p&l=%d&c=%d\">", 
                addr, length, chunk);
        }
        http_printf(httpc, "%02X", *area);
        pos += 2;

        if ((i & 3) == 3) {
            http_printf(httpc, "</a> ");
            pos += 1;
        }
        
        e = isgraph(*area) ? *area : *area==' ' ? ' ' : '.';

        switch(e) {
        case '<':
            ie += sprintf(&eChar[ie], "&lt;");
            break;
        case '>':
            ie += sprintf(&eChar[ie], "&gt;");
            break;
        default:
            ie += sprintf(&eChar[ie],"%c", e);
            break;
        }

        area++;
        size--;

        if (size==0 && ((i&3)!=3)) {
            http_printf(httpc, "</a> ");
            pos += 1;
        }
    }

    // wtof("%s: x=%d pos=%d ie=%d", __func__, x, pos, ie);
    if ( ie ) {
        i = (x+8) - pos;
        if (i < 0) goto quit;
        http_printf(httpc, "%-*.*s:%s:\n", i,i, "", eChar);
    }

quit:
    return 0;
}

static int 
display_help(HTTPD *httpd, HTTPC *httpc)
{
    int         rc;
    const char  *path   = NULL;
    const char  *host   = NULL;
    
    if (!path)  path = http_get_env(httpc, "REQUEST_PATH");
    if (!path)  path = "unknown";
    
    if (!host)  host = http_get_env(httpc, "HTTP_HOST");
    if (!host)  host = "unknown";
    
    if (rc = send_resp(httpd, httpc, 200) < 0) goto quit;

    http_printf(httpc, "<h2>HTTPDSRV Help</h2>\n");
    http_printf(httpc, "<h3>Usage: http:/%s%s?target=name[&m=nnnnnnnn]</h3>\n", host, path);
    http_printf(httpc, "<p>This CGI program uses the QUERY variable "
        "\"target\" value to control which HTTPD storage area to display.<p>\n");

    http_printf(httpc, "<dl>\n");

    http_printf(httpc, "<dt>?target=HTTPD</td>\n");
    http_printf(httpc, "<dd>Display the HTTPD storage areas. "
        "This is the default if ?target is omitted.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=CGI&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the Common Gateway Interface array at the memory address.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=FILE&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the File Handle at the memory address.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=FS</dt>\n");
    http_printf(httpc, "<dd>Display the HTTPD ... Unix \"like\" File System Handle.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=FTPD</dt>\n");
    http_printf(httpc, "<dd>Display the HTTPD ... FTPD Handle.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=HELP</dt>\n");
    http_printf(httpc, "<dd>Display this help text.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=MGR&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the Thread Manager Handle at the memory address.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=TASK&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the Thread Task Handle at the memory address.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=UFSDISKS&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the UFS Disk Array at the memory address.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=UFSIO&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the UFS Input / Output Array at the memory address.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=UFSPAGERS&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the UFS Pagers Array at the memory address.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=UFSSB&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the UFS Super Block at the memory address.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=UFSVDISK&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the UFS Virtual Disk Array at the memory address.</dd><br/>\n");

    http_printf(httpc, "<dt>?target=WORKERS&m=nnnnnnnn</dt>\n");
    http_printf(httpc, "<dd>Display the Thread Manager Workers Array at the memory address.</dd><br/>\n");

    http_printf(httpc, "</dl>\n");
    
    send_last(httpd, httpc);
    
quit:
    return 0;
}
