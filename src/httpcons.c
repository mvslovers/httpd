/* HTTPCONS.C
*/
#include "httpd.h"
#include "httpdmsg.h"     /* operator message catalog */
#include <buildstamp.h>     /* MBT_COMMIT for DISPLAY VERSION */

static int process(char *buf);

static int display(char *buf);
static int d_config(char *buf);
static int d_login(char *buf);
static int d_port(char *buf);
static int d_thread(char *buf);
static int d_time(char *buf);
static int d_memory(char *buf);
static int d_version(char *buf);
static int d_stats(char *buf);

static int set(char *buf);
static int s_maxtask(char *buf);
static int s_mintask(char *buf);
static int s_login(char *buf);
static int s_stats(char *in);

/* http_console() */
int
httpcons(CIB *cib)
{
    int     rc      = 0;
    char    *buf    = NULL;

    http_enter("httpcons(), cib=%08X\n", cib);

    if (!cib) goto quit;

    switch(cib->cibverb) {
    case CIBSTART:
        wtof(MSG_CONS_STATE, cib->cibconid, "START");
        break;
    case CIBMODFY:
        wtof(MSG_CONS_MODIFY,
            cib->cibconid, cib->cibdatln, cib->cibdatln, cib->cibdata);
        buf = calloc(1, cib->cibdatln + 2);
        if (buf) {
            memcpy(buf, cib->cibdata, cib->cibdatln);
            process(buf);
        }
        break;
    case CIBSTOP:
        wtof(MSG_CONS_STATE, cib->cibconid, "STOP");
        rc = 1;
        break;
    case CIBMOUNT:
        wtof(MSG_CONS_STATE, cib->cibconid, "MOUNT");
        break;
    }

quit:
    if (buf) free(buf);
    http_exit("httpcons(), rc=%d\n", rc);
    return rc;
}

/* The help text is console output, so it follows the same upper case rule as
** every other WTO.  Abbreviations keep the capital/lower split that shows how
** far a command can be shortened -- "DISPLAY MAXTASK" is matched on "MA", so
** printing it MAxtask is the only way the line says so. */
static char *usage[] = {
    "DISPLAY Config (D C)",
    "    DISPLAYS THE CONFIGURATION THIS SERVER IS RUNNING",
	"DISPLAY Login (D L)",
	"    DISPLAYS USERS THAT ARE LOGGED INTO THE HTTPD SERVER",
	"DISPLAY Memory xxxxxx[,nnn] (D M xxxxxx)",
	"    DISPLAYS MEMORY AT THE GIVEN ADDRESS xxxxxx",
    "DISPLAY Ports (D P)",
    "    DISPLAYS THE PORT NUMBER THIS SERVER IS LISTENING ON",
    "DISPLAY Stats (D S)",
    "    DISPLAYS HTTPD CLIENT STATISTICS",
    "DISPLAY Threads (D T)",
    "    DISPLAYS INFORMATION ABOUT THE SERVER THREADS",
    "DISPLAY TIme [-|+][minutes] (D TI)",
    "    DISPLAYS THE CURRENT TIME IN GMT AND LOCAL TIME",
    "DISPLAY Version (D V)",
    "    DISPLAYS THE SERVER VERSION AND BUILD",
    " ",
    "SET Login [ALL,CGI,GET,HEAD,POST,NONE] (S L ...)",
    "    SETS THE LOGIN OPTION",
    "SET MAxtask n (S MA n)",
    "    SETS THE WORKER MAXIMUM THREAD COUNT",
    "SET MIntask n (S MI n)",
    "    SETS THE WORKER MINIMUM THREAD COUNT",
    "SET Stats NONE|ERROR|AUTH|ALL [RESET]",
    "    SETS THE SMF RECORDING LEVEL",
    " ",
    "EXAMPLE: /F HTTPD,D C",
    NULL
};

static int
process(char *buf)
{
    int     rc      = 0;
    int     i;
    int     len;
    char    *token;
    char    *rest;

    token   = strtok(buf, " ,");
    if (!token) goto quit;

    len     = strlen(token);
    rest    = strtok(NULL, "");
    if (http_cmpn(token, "DISPLAY", len)==0) {
        rc = display(rest);
        goto quit;
    }

    if (http_cmpn(token, "SET", len)==0) {
        rc = set(rest);
        goto quit;
    }

    if (http_cmpn(token, "?", len)==0) goto help;
    if (http_cmpn(token, "HELP", len)==0) goto help;

    wtof(MSG_CONS_UNKNOWN, token);

help:
    wtof(MSG_HELP_HEADER);
    for(i=0; usage[i]; i++) {
        wtof(MSG_HELP_LINE, usage[i]);
    }

quit:
    return rc;
}

static int
set(char *buf)
{
    int     rc      = 0;
    int     len;
    char    *token;
    char    *rest;

    token   = strtok(buf, " ,");
    if (!token) goto quit;

    len     = strlen(token);
    rest    = strtok(NULL, "");

    if (http_cmpn(token, "MAXTASK", len)==0) {
        rc = s_maxtask(rest);
        goto quit;
    }

    if (http_cmpn(token, "MINTASK", len)==0) {
        rc = s_mintask(rest);
        goto quit;
    }
    
    if (http_cmpn(token, "LOGIN", len)==0) {
		rc = s_login(rest);
		goto quit;
	}

    if (http_cmpn(token, "STATS", len)==0) {
		rc = s_stats(rest);
		goto quit;
	}

    wtof(MSG_CONS_UNKNOWN_S, token);

quit:
    return rc;
}


static int
display(char *buf)
{
    int     rc      = 0;
    int     len;
    char    *token;
    char    *rest;

    token   = strtok(buf, " ,");
    if (!token) goto quit;

    len     = strlen(token);
    rest    = strtok(NULL, "");

    /* CONFIG before LOGIN: both are unique at one character in the operator's
       mind but "C" only matches this one, so the order is cosmetic -- keep the
       list alphabetical so a new command has an obvious home. */
    if (http_cmpn(token, "CONFIG", len)==0) {
        rc = d_config(rest);
        goto quit;
    }

	if (http_cmpn(token, "LOGIN", len)==0) {
		rc = d_login(rest);
		goto quit;
	}

    if (http_cmpn(token, "MEMORY", len)==0) {
        rc = d_memory(rest);
        goto quit;
    }

    if (http_cmpn(token, "PORTS", len)==0) {
        rc = d_port(rest);
        goto quit;
    }

    if (http_cmpn(token, "STATS", len)==0) {
        rc = d_stats(rest);
        goto quit;
    }

    if (http_cmpn(token, "THREADS", len)==0) {
        rc = d_thread(rest);
        goto quit;
    }

    if (http_cmpn(token, "TIME", len)==0) {
        rc = d_time(rest);
        goto quit;
    }

    if (http_cmpn(token, "VERSION", len)==0) {
        rc = d_version(rest);
        goto quit;
    }

    wtof(MSG_CONS_UNKNOWN_D, token);

quit:
    return rc;
}

/* DISPLAY CONFIG -- the settled configuration, on demand.
**
** Startup used to echo all of this whether anyone wanted it or not.  It is
** here instead (#184): UFSD and FTPD both keep the banner short and answer
** questions when asked, and an operator who wants to know what the server
** actually parsed can now ask at any point in its life, not only by finding
** the start of the job log.
**
** Values keep their case -- the document root is a UFS path and the codepage
** name is a value; only the labels are upper case (httpdmsg.h rule 3). */
static int
d_config(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    static const char *levels[] = {"NONE","ERROR","AUTH","ALL"};
    char        name[64];

    lock(httpd, LOCK_SHR);

    wtof(MSG_D_CFG_TASKS, httpd->port,
        (int)httpd->cfg_mintask, (int)httpd->cfg_maxtask);
    wtof(MSG_D_CFG_TIMES, (int)httpd->cfg_client_timeout,
        (int)httpd->cfg_keepalive_timeout, (int)httpd->cfg_keepalive_max);
    wtof(MSG_D_CFG_SESSION, (int)httpd->cfg_session_timeout,
        httpd->cfg_session_timeout ? "" : " (REAPER DISABLED)");
    wtof(MSG_D_CFG_DOCROOT,
        httpd->docroot[0] ? httpd->docroot : "(NONE)");
    wtof(MSG_D_CFG_CODEPAGE,
        httpd->codepage[0] ? httpd->codepage : "CP037");
    wtof(MSG_D_CFG_SMF, (int)httpd->smf_type, levels[httpd->smf_level]);
    wtof(MSG_D_CFG_SOURCE, parmlib_name(name, sizeof(name)));

    unlock(httpd, LOCK_SHR);

    return 0;
}

static int
d_login_cred(CRED *cred)
{
    int         rc      = 0;
    CREDID		id;

	credid_dec(&cred->id, &id);
	
	wtof(MSG_D_LOGIN_USER,
		id.userid, 
		id.addr >> 24 & 0xFF,
		id.addr >> 16 & 0xFF,
		id.addr >> 8  & 0xFF,
		id.addr       & 0xFF,
		cred->acee);

	return rc;
}

static int
d_login(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    CRED 		***array= cred_array();
    CRED		*cred;
    unsigned	count, n;

	lock(httpd, LOCK_SHR);
	rc = httpd048(httpd);

	lock(array, LOCK_SHR);

	count = array_count(array);
	wtof(MSG_D_LOGIN_COUNT, count);

	for(n=1; n <= count; n++) {
		cred = array_get(array, n);
		if (!cred) continue;

		lock(cred, LOCK_SHR);
		rc = d_login_cred(cred);
		unlock(cred, LOCK_SHR);
	}
	
	unlock(array, LOCK_SHR);
	unlock(httpd, LOCK_SHR);
    return rc;
}

static int
d_port(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;

    wtof(MSG_D_PORT, httpd->port);

    return rc;
}

static int
d_stats(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
	static const char *levels[] = {"NONE","ERROR","AUTH","ALL"};

	wtof(MSG_D_STATS_SMF,
		levels[httpd->smf_level], (int)httpd->smf_type);
	wtof(MSG_D_STATS_COUNT,
		httpd->total_requests, httpd->total_errors,
		httpd->total_bytes_sent, httpd->active_connections);

	return 0;
}


static int
d_version(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    char        vers[24];
    char        commit[24];

    wtof(MSG_D_VERSION,
        http_upcase(vers, sizeof(vers), httpd->version),
        http_upcase(commit, sizeof(commit), MBT_COMMIT));
    
	return 0;
}

static int 
d_memory(char *buf)
{
	char		*next = NULL;
	char		*mem;
	int			len;
	unsigned    rc;

	/* D M with no address would strtoul(NULL) -> deref NULL on the console
	   thread; an address is required */
	if (!buf) {
		wtof(MSG_D_MEM_USAGE);
		return 0;
	}

	mem = (char *) strtoul(buf, &next, 16);
	len = next ? (int) strtoul(next+1, NULL, 0) : 256;

	/* sanity check length */
	if (len <= 0) len = 256;
	if (len > 4096) len = 4096;
	
	/* sanity check memory address */
	if (mem > (char*)0x00FFFFFF){
		wtof(MSG_D_MEM_INVALID, mem);
		goto quit;
	}

	/* call wtodumpf() from ESTAE protected try() */
	try(wtodumpf, mem, len, "DISPLAY MEMORY");
	rc = tryrc();
	if (rc==0) {
		wtof(MSG_D_MEM_END, mem);
		goto quit;
	}
	
	if (rc < 0) {
		rc *= -1;	/* make rc positive value */
		wtof(MSG_D_MEM_ESTAE, rc);
		goto quit;
	}
	
	if (rc > 0xFFF) {
		/* system ABEND occured */
		wtof(MSG_D_MEM_ABEND_S, (rc >> 12) & 0xFFF, mem);
	}
	else {
		/* user ABEND occured */
		wtof(MSG_D_MEM_ABEND_U, rc & 0xFFF, mem);
	}
	
quit:
	return 0;
}

static int 
d_time(char *buf)
{
    CLIBGRT     *grt    	= __grtget();
    HTTPD       *httpd  	= grt->grtapp1;
	int			tzoffset	= httpd->tzoffset;
	int			sign		= tzoffset < 0 ? -1 : 1;
	char		*next 		= NULL;
	/* "D TI" with no argument is valid (show time at the configured offset);
	   strtol(NULL) would deref NULL, so default to 0 (= use tzoffset below) */
	int 		minutes 	= buf ? strtol(buf, &next, 10) : 0;
	time64_t	gmt 		= time64(NULL);
	time64_t	lot;
	struct tm	tm;
	char		tbuf[128];

	if (minutes) {
		/* sanity check minutes */
		if (minutes > (12*60)) minutes = (12*60);
		if (minutes < -(12*60)) minutes = -(12*60);

		if (minutes < 0) {
			sign = -1;
			minutes *= sign;	/* make positive */
		}
		else {
			sign = 1;
		}
	
		tzoffset = minutes * 60;
	}
	else {
		/* calculate minutes from tzoffset value */
		tzoffset *= sign;	/* make positive value */
		minutes = tzoffset / 60;
	}


	if (sign < 0) {
		__64_sub_i32(&gmt, tzoffset, &lot);
	}
	else {
		__64_add_i32(&gmt, tzoffset, &lot);
	}
	
	gmtime64_r(&gmt, &tm);
	strftime(tbuf, sizeof(tbuf), MSG_D_TIME_GMT, &tm);
	wtof("%s", tbuf);

	gmtime64_r(&lot, &tm);
	strftime(tbuf, sizeof(tbuf), MSG_D_TIME_LOCAL, &tm);
	/* "OFFSET=" and not "TZOFFSET=": the Parmlib keyword of that name is
	   retired (#145), and labelling the value after it suggested a setting that
	   no longer exists.  What is shown is the offset in minutes -- the system's
	   unless D TI was given one as an argument. */
	wtof("%s OFFSET=%s%d", tbuf, sign < 0 ? "-" : "+", minutes);

	return 0;
}

static int
d_cthdtask(CTHDTASK *task)
{
    if (!task) goto quit;

    wtof(MSG_D_TASK1,
        task->eye, task->tcb, task->owntcb);
    wtof(MSG_D_TASK2,
        task->termecb, task->rc, task->stacksize);
    wtof(MSG_D_TASK3,
        task->func, task->arg1, task->arg2);

quit:
    return 0;
}

static int
d_cthdmgr(CTHDMGR *mgr)
{
    const char  *state  = "unknown";
    if (!mgr) goto quit;

    switch(mgr->state) {
    case CTHDMGR_STATE_INIT:        state = "INIT";         break;
    case CTHDMGR_STATE_RUNNING:     state = "RUNNING";      break;
    case CTHDMGR_STATE_QUIESCE:     state = "QUIESCE";      break;
    case CTHDMGR_STATE_STOPPED:     state = "STOPPED";      break;
    case CTHDMGR_STATE_WAITING:     state = "WAITING";      break;
    }

    wtof(MSG_D_MGR1,
        mgr->eye, mgr->task, mgr->wait);
    wtof(MSG_D_MGR2,
        mgr->func, mgr->udata, mgr->stacksize);
    wtof(MSG_D_MGR3,
        mgr->worker, mgr->queue, mgr->state, state);
    wtof(MSG_D_MGR4,
        mgr->mintask, mgr->maxtask);
    wtof(MSG_D_MGR5,
        array_count(&mgr->worker), mgr->dispatched);

quit:
    return 0;
}

static int
ntoa(unsigned addr, char *buf)
{
    unsigned char *p = (unsigned char*)&addr;

    return sprintf(buf, "%u.%u.%u.%u", p[0], p[1], p[2], p[3]);
}

static int
d_queue(CTHDWORK *work)
{
    CTHDQUE *q = work->queue;
    if (q->data) {
        HTTPC       *httpc = q->data;
        struct sockaddr addr;
        struct sockaddr_in *in = (struct sockaddr_in*)&addr;
        int         addrlen;
        char        ip[20] = "";
        char        user[12] = "ANONYMOUS";
        char        group[12] = "";
        if (strcmp(httpc->eye, HTTPC_EYE)==0) {
			CRED	*cred = httpc->cred;
			if (cred) {
				CREDID id;
				
				credid_dec(&cred->id, &id);
                strncpy(user, id.userid, sizeof(user));

				if (cred->acee) {
					memcpyp(user, sizeof(user), &cred->acee->aceeuser[1], cred->acee->aceeuser[0], 0);
					memcpyp(group, sizeof(group), &cred->acee->aceegrp[1], cred->acee->aceegrp[0], 0);
				}
            }
            ntoa(httpc->addr, ip);
            wtof(MSG_D_CLIENT, user, group);
            wtof(MSG_D_REMOTE,
                httpc->port, ip);
            addrlen = sizeof(addr);
            getpeername(httpc->socket, &addr, &addrlen);
            ntoa(in->sin_addr.s_addr, ip);
            wtof(MSG_D_SOCKET,
                httpc->socket, in->sin_port, ip);
        }
    }
    return 0;
}

static int
d_cthdwork(CTHDWORK *work)
{
    const char  *state = "unknown";

    if (!work) goto quit;

    switch(work->state) {
    case CTHDWORK_STATE_INIT:       state = "INIT";         break;
    case CTHDWORK_STATE_RUNNING:    state = "RUNNING";      break;
    case CTHDWORK_STATE_WAITING:    state = "WAITING";      break;
    case CTHDWORK_STATE_DISPATCH:   state = "DISPATCHED";   break;
    case CTHDWORK_STATE_SHUTDOWN:   state = "SHUTDOWN";     break;
    case CTHDWORK_STATE_STOPPED:    state = "STOPPED";      break;
    }

    wtof(MSG_D_WORK1,
        work, work->eye, work->wait);
    wtof(MSG_D_WORK2,
        work->mgr, work->task, work->queue);
    wtof(MSG_D_WORK3,
        work->state, state);
    wtof(MSG_D_WORK4,
        work->start_time, work->start_time.u64 ? ctime64(&work->start_time) : "");
    wtof(MSG_D_WORK5,
        work->wait_time, work->wait_time.u64 ? ctime64(&work->wait_time) : "");
    wtof(MSG_D_WORK6,
        work->disp_time, work->disp_time.u64 ? ctime64(&work->disp_time) : "");
    wtof(MSG_D_DISPCNT,
        work->dispatched);

    if (work->state==CTHDWORK_STATE_RUNNING && work->queue) {
        /* since we're not holding any lock, we want to use try() to handle unexpected failures */
        try(d_queue, work);
    }

quit:
    return 0;
}

static int
d_thread(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    CTHDTASK    *task   = cthread_self();
    CTHDMGR     *mgr;
    unsigned    count;
    unsigned    n;

    /* obtain a shared lock on httpd */
    lock(httpd,1);

    if (task) {
        wtof(MSG_D_THREAD, task, "MAIN SERVER THREAD");
        d_cthdtask(task);
        wtof(MSG_D_RNAME, httpd->rname);
        wtof(MSG_SEPARATOR);
    }

    wtof(MSG_D_THREAD, httpd->socket_thread, "SOCKET THREAD");
    if (httpd->socket_thread) {
        d_cthdtask(httpd->socket_thread);
        wtof(MSG_D_LISTENER, httpd->listen, httpd->port);
        wtof(MSG_SEPARATOR);
    }

    if (httpd->mgr) {
        mgr = httpd->mgr;

        lock(mgr,1);

        task = mgr->task;
        if (task) {
            wtof(MSG_D_THREAD, task, "DISPATCHER THREAD");
            d_cthdtask(task);
        }
        d_cthdmgr(mgr);
        wtof(MSG_SEPARATOR);

        count = array_count(&mgr->worker);
        for(n=0; n < count; n++) {
            CTHDWORK    *work = mgr->worker[n];

            if (!work) continue;

            task = work->task;
            if (!task) continue;

            wtof(MSG_D_THREAD, task, "WORKER THREAD");
            d_cthdtask(task);
            d_cthdwork(work);
            wtof(MSG_SEPARATOR);
        }

        unlock(mgr,1);
    }

    unlock(httpd,1);
    return rc;
}

static int
s_maxtask(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    CTHDMGR     *mgr;
	char		*next = NULL;
	unsigned    value = strtoul(buf, &next, 10);

	if (value < 1) value = 1;
	if (value > 100) value = 100;

    /* obtain a shared lock on httpd */
    lock(httpd,1);

    if (httpd->mgr) {
        mgr = httpd->mgr;

        lock(mgr,1);

		mgr->maxtask = value;
		if (value < mgr->mintask) mgr->mintask = value;

		/* wake up the thread manager thread */
		rc = cthread_post(&mgr->wait, CTHDMGR_POST_DATA);
#if 0		
        task = mgr->task;
        if (task) {
            wtof(MSG_D_THREAD, task, "DISPATCHER THREAD");
            d_cthdtask(task);
        }
#endif
        d_cthdmgr(mgr);
        wtof(MSG_SEPARATOR);

        unlock(mgr,1);
    }

    unlock(httpd,1);
    return rc;
}

static int
s_mintask(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    CTHDMGR     *mgr;
	char		*next = NULL;
	unsigned    value = strtoul(buf, &next, 10);

	if (value < 1) value = 1;
	if (value > 100) value = 100;

    /* obtain a shared lock on httpd */
    lock(httpd,1);

    if (httpd->mgr) {
        mgr = httpd->mgr;

        lock(mgr,1);

		mgr->mintask = value;
		if (value > mgr->maxtask) mgr->maxtask = value;

		/* wake up the thread manager thread */
		rc = cthread_post(&mgr->wait, CTHDMGR_POST_DATA);
#if 0		
        task = mgr->task;
        if (task) {
            wtof(MSG_D_THREAD, task, "DISPATCHER THREAD");
            d_cthdtask(task);
        }
#endif
        d_cthdmgr(mgr);
        wtof(MSG_SEPARATOR);

        unlock(mgr,1);
    }

    unlock(httpd,1);
    return rc;
}

static int
s_login(char *in)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    char 		*p		= NULL;
	char		*next 	= NULL;

    /* obtain a shared lock on httpd */
    lock(httpd,LOCK_SHR);

	if (!in) {
		wtof(MSG_LOGIN_MISSING);
		goto quit;
	}

	p = strtok(in, " (,");
	next = strtok(NULL, "");
#if 0
	wtof("%s: p=\"%s\", next=\"%s\"", __func__, p, next ? next : "(null)");
#endif
    for(; p ; p = strtok(next," ,)"), next = strtok(NULL, "")) {
#if 0
		wtof("%s: LOGIN: p=\"%s\", next=\"%s\"", __func__, p, next ? next : "(null)");
#endif
		if (http_cmp(p, "ALL")==0) {
			httpd->login |= HTTPD_LOGIN_ALL;
			continue;
		}
		if (http_cmp(p, "CGI")==0) {
			httpd->login |= HTTPD_LOGIN_CGI;
			continue;
		}
		if (http_cmp(p, "GET")==0) {
			httpd->login |= HTTPD_LOGIN_GET;
			continue;
		}
		if (http_cmp(p, "HEAD")==0) {
			httpd->login |= HTTPD_LOGIN_HEAD;
			continue;
		}
		if (http_cmp(p, "POST")==0) {
			httpd->login |= HTTPD_LOGIN_POST;
			continue;
		}
		if (http_cmp(p, "NONE")==0) {
			httpd->login &= 0xFF - HTTPD_LOGIN_ALL;
			continue;
		}
		/* not one of our LOGIN= values */
		wtof(MSG_S_LOGIN_INVALID, p);
        break;
	}

#if 0
	wtof("%s: httpd->login 0x%02X", __func__, httpd->login);
	wtof("%s: NONE test 0x%02X", __func__, httpd->login & HTTPD_LOGIN_ALL);
#endif

	rc = httpd048(httpd);

quit:
    unlock(httpd,LOCK_SHR);
    return rc;
}

static int
s_stats(char *in)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    char 		*p		= NULL;
	char		*next 	= NULL;
	static const char *levels[] = {"NONE","ERROR","AUTH","ALL"};

    /* obtain a exclusive lock on httpd */
    lock(httpd,LOCK_EXC);

	if (!in) {
		wtof(MSG_S_STATS_MISSING);
		goto quit;
	}

	p = strtok(in, " ,");
	if (!p) {
		wtof(MSG_S_STATS_MISSING);
		goto quit;
	}

	if (http_cmp(p, "NONE")==0) {
		httpd->smf_level = SMF_LEVEL_NONE;
	}
	else if (http_cmp(p, "ERROR")==0) {
		httpd->smf_level = SMF_LEVEL_ERROR;
	}
	else if (http_cmp(p, "AUTH")==0) {
		httpd->smf_level = SMF_LEVEL_AUTH;
	}
	else if (http_cmp(p, "ALL")==0) {
		httpd->smf_level = SMF_LEVEL_ALL;
	}
	else if (http_cmp(p, "RESET")==0) {
		httpd->total_requests = 0;
		httpd->total_errors = 0;
		httpd->total_bytes_sent = 0;
		wtof(MSG_S_STATS_RESET);
		goto quit;
	}
	else {
		wtof(MSG_S_STATS_INVALID, p);
		goto quit;
	}
	wtof(MSG_S_STATS_LEVEL, levels[httpd->smf_level]);

	// Check for RESET option
	next = strtok(NULL, " ,");
	if (next && http_cmpn(next, "RESET", strlen(next))==0) {
		httpd->total_requests = 0;
		httpd->total_errors = 0;
		httpd->total_bytes_sent = 0;
		wtof(MSG_S_STATS_RESET);
	}

quit:
    unlock(httpd,LOCK_EXC);
    return rc;
}
