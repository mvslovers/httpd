/* HTTPCONS.C
*/
#include "httpd.h"

static int process(char *buf);

static int display(char *buf);
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

#define HTTPD199I \
    "HTTPD199I -------------------------------------------------------"

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
        wtof("HTTPD100I CONS(%u) START", cib->cibconid);
        break;
    case CIBMODFY:
        wtof("HTTPD100I CONS(%u) \"%-*.*s\"",
            cib->cibconid, cib->cibdatln, cib->cibdatln, cib->cibdata);
        buf = calloc(1, cib->cibdatln + 2);
        if (buf) {
            memcpy(buf, cib->cibdata, cib->cibdatln);
            process(buf);
        }
        break;
    case CIBSTOP:
        wtof("HTTPD100I CONS(%u) STOP", cib->cibconid);
        rc = 1;
        break;
    case CIBMOUNT:
        wtof("HTTPD100I CONS(%u) MOUNT", cib->cibconid);
        break;
    }

quit:
    if (buf) free(buf);
    http_exit("httpcons(), rc=%d\n", rc);
    return rc;
}

static char *usage[] = {
	"Display Login",
	"    display users that are logged into the HTTPD server",
	"Display Memory xxxxxx[,nnn] (D M xxxxxx)",
	"    displays memory at given address xxxxxx",
    "Display Ports (D P)",
    "    displays the port number this server is listening on.",
    "Display Stats n [Months|Days|Hours|Minutes|all]",
    "    displays HTTPD client statistics.",
    "Display Threads (D T)",
    "    displays information about the server threads.",
    "Display TIme [-|+][tzoffset] (D TI)",
    "    displays current time in GMT and local time.",
    "Display Version (D V)",
    "    displays server version.",
    " ",
    "Set Login [all,cgi,get,head,post,none] (S L ...)",
    "    set the login option.",
    "Set MAxtask n (S MAxtask n)",
    "    set the worker maximum thread count.",
    "Set MIntask n (S MIntask n)",
    "    set the worker minimum thread count.",
    "Set Stats ON|OFF [Save|Clear|Free|Reset]",
    "    set client statistics recording ON or OFF.",
    " ",
    "Example: /F HTTPD,D P",
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

    wtof("HTTPD101E unknown console command \"%s\"", token);

help:
    wtof("HTTPD000I MODIFY commands are:");
    for(i=0; usage[i]; i++) {
        wtof("HTTPD000I %s", usage[i]);
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

    wtof("HTTPD101E unknown console command \"SET %s\"", token);

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

    wtof("HTTPD101E unknown console command \"DISPLAY %s\"", token);

quit:
    return rc;
}

static int
d_login_cred(CRED *cred)
{
    int         rc      = 0;
    CREDID		id;

	credid_dec(&cred->id, &id);
	
	wtof("HTTPD071I User:%-8.8s IP:%u.%u.%u.%u ACEE(%06X)",
		id.userid, 
		id.addr >> 24 & 0xFF,
		id.addr >> 16 & 0xFF,
		id.addr >> 8  & 0xFF,
		id.addr       & 0xFF,
		cred->acee);

quit:
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
	wtof("HTTPD070I Users Logged In: %u", count);

	for(n=1; n <= count; n++) {
		cred = array_get(array, n);
		if (!cred) continue;

		lock(cred, LOCK_SHR);
		rc = d_login_cred(cred);
		unlock(cred, LOCK_SHR);
	}
	
quit:
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

    wtof("HTTPD102I HTTPD server listening on port %d", httpd->port);

quit:
    return rc;
}

static int
d_stats(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;

	wtof("HTTPD411I SMF recording: %s",
		(httpd->client & HTTPD_CLIENT_STATS) ? "ON" : "OFF");
	wtof("HTTPD412I Requests: %u  Errors: %u  Bytes: %u  Active: %u",
		httpd->total_requests, httpd->total_errors,
		httpd->total_bytes_sent, httpd->active_connections);

	return 0;
}


static int
d_version(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;

    wtof("HTTPD140I HTTPD Server version %s", httpd->version);
    
	return 0;
}

static int 
d_memory(char *buf)
{
	char		*next = NULL;
	char		*mem = (char *) strtoul(buf, &next, 16);
	int			len = next ? (int) strtoul(next+1, NULL, 0) : 256;
	unsigned    rc;
	
	/* sanity check length */
	if (len <= 0) len = 256;
	if (len > 4096) len = 4096;
	
	/* sanity check memory address */
	if (mem > (char*)0x00FFFFFF){
		wtof("Invalid memory address 0x%08X", mem);
		goto quit;
	}

	/* call wtodumpf() from ESTAE protected try() */
	try(wtodumpf, mem, len, "DISPLAY MEMORY");
	rc = tryrc();
	if (rc==0) {
		wtof("End of memory dump for 0x%06X", mem);
		goto quit;
	}
	
	if (rc < 0) {
		rc *= -1;	/* make rc positive value */
		wtof("ESTAE CREATE failure, RC=0x%08X", rc);
		goto quit;
	}
	
	if (rc > 0xFFF) {
		/* system ABEND occured */
		wtof("ABEND S%03X occured for DISPLAY MEMORY 0x%06X", 
			(rc >> 12) & 0xFFF, mem);
	}
	else {
		/* user ABEND occured */
		wtof("ABEND U%04d occured for DISPLAY MEMORY 0x%06X", 
			rc & 0xFFF, mem);
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
	int 		minutes 	= strtol(buf, &next, 10);
	time64_t	gmt 		= time64(NULL);
	time64_t	lot;
	unsigned    offset;
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
	strftime(tbuf, sizeof(tbuf), "HTTPD142I time %Y/%m/%d %H:%M:%S GMT", &tm);
	wtof("%s", tbuf);

	gmtime64_r(&lot, &tm);
	strftime(tbuf, sizeof(tbuf), "HTTPD143I time %Y/%m/%d %H:%M:%S Local", &tm);
	wtof("%s TZOFFSET=%s%d", tbuf, sign < 0 ? "-" : "+", minutes);

quit:
	return 0;
}

static int
d_cthdtask(CTHDTASK *task)
{
    if (!task) goto quit;

    wtof("HTTPD104I .....EYE %-8.8s  .....TCB %08X  ..OWNTCB %08X",
        task->eye, task->tcb, task->owntcb);
    wtof("HTTPD105I .TERMECB %08X  ......RC %08X  STACKSZE %08X",
        task->termecb, task->rc, task->stacksize);
    wtof("HTTPD106I ....FUNC %08X  ....ARG1 %08X  ....ARG2 %08X",
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

    wtof("HTTPD107I .....EYE %-8.8s  ....TASK %08X  .WAITECB %08X",
        mgr->eye, mgr->task, mgr->wait);
    wtof("HTTPD108I ....FUNC %08X  ...UDATA %08X  STACKSZE %08X",
        mgr->func, mgr->udata, mgr->stacksize);
    wtof("HTTPD109I ..WORKER %08X  ...QUEUE %08X  ...STATE %08X %s",
        mgr->worker, mgr->queue, mgr->state, state);
    wtof("HTTPD110I .MINTASK %8u  .MAXTASK %8u",
        mgr->mintask, mgr->maxtask);
    wtof("HTTPD111I .WORKERS %8u  .DISPCNT %llu",
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
            wtof("HTTPD118I PROTOCOL HTTP      ....USER %-9.9s ...GROUP %s", user, group);
            wtof("HTTPD119I ..REMOTE CLIENT    ....PORT %8d  ......IP %s",
                httpc->port, ip);
            addrlen = sizeof(addr);
            getpeername(httpc->socket, &addr, &addrlen);
            ntoa(in->sin_addr.s_addr, ip);
            wtof("HTTPD120I ..SOCKET %8d  ....PORT %8d  ......IP %s",
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

    wtof("HTTPD112I ..WORKER %08X  .....EYE %-8.8s  .WAITECB %08X",
        work, work->eye, work->wait);
    wtof("HTTPD113I .....MGR %08X  ....TASK %08X  ...QUEUE %08X",
        work->mgr, work->task, work->queue);
    wtof("HTTPD114I ...STATE %08X %s",
        work->state, state);
    wtof("HTTPD115I ...START %016llX  %s",
        work->start_time, work->start_time.u64 ? ctime64(&work->start_time) : "");
    wtof("HTTPD116I ....WAIT %016llX  %s",
        work->wait_time, work->wait_time.u64 ? ctime64(&work->wait_time) : "");
    wtof("HTTPD117I ....DISP %016llX  %s",
        work->disp_time, work->disp_time.u64 ? ctime64(&work->disp_time) : "");
    wtof("HTTPD141I .DISPCNT %llu",
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
    CTHDWORK    *work;
    unsigned    count;
    unsigned    n;

    /* obtain a shared lock on httpd */
    lock(httpd,1);

    if (task) {
        wtof("HTTPD103I ..THREAD %08X main server thread", task);
        d_cthdtask(task);
        wtof("HTTPD120I ...RNAME %-28.28s",
            httpd->rname);
        wtof(HTTPD199I);
    }

    wtof("HTTPD103I ..THREAD %08X socket thread", httpd->socket_thread);
    if (httpd->socket_thread) {
        d_cthdtask(httpd->socket_thread);
        wtof("HTTPD119I ..SOCKET %8d  ....PORT %8d  (HTTP Protocol)",
            httpd->listen, httpd->port);
        wtof(HTTPD199I);
    }

    if (httpd->mgr) {
        mgr = httpd->mgr;

        lock(mgr,1);

        task = mgr->task;
        if (task) {
            wtof("HTTPD103I ..THREAD %08X dispatcher thread", task);
            d_cthdtask(task);
        }
        d_cthdmgr(mgr);
        wtof(HTTPD199I);

        count = array_count(&mgr->worker);
        for(n=0; n < count; n++) {
            CTHDWORK    *work = mgr->worker[n];

            if (!work) continue;

            task = work->task;
            if (!task) continue;

            wtof("HTTPD103I ..THREAD %08X worker thread", task);
            d_cthdtask(task);
            d_cthdwork(work);
            wtof(HTTPD199I);
        }

        unlock(mgr,1);
    }

quit:
    unlock(httpd,1);
    return rc;
}

static int
s_maxtask(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    CTHDTASK    *task   = cthread_self();
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
            wtof("HTTPD103I ..THREAD %08X dispatcher thread", task);
            d_cthdtask(task);
        }
#endif
        d_cthdmgr(mgr);
        wtof(HTTPD199I);

        unlock(mgr,1);
    }

quit:
    unlock(httpd,1);
    return rc;
}

static int
s_mintask(char *buf)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = 0;
    CTHDTASK    *task   = cthread_self();
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
            wtof("HTTPD103I ..THREAD %08X dispatcher thread", task);
            d_cthdtask(task);
        }
#endif
        d_cthdmgr(mgr);
        wtof(HTTPD199I);

        unlock(mgr,1);
    }

quit:
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
		wtof("HTTPD048W Missing LOGIN (all,cgi,head,get,post,none) value");
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
		wtof("HTTPD048E Invalid SET LOGIN value \"%s\"", p);
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

    /* obtain a exclusive lock on httpd */
    lock(httpd,LOCK_EXC);

	if (!in) {
		wtof("HTTPD048W Missing STATS ON|OFF [RESET]");
		goto quit;
	}

	p = strtok(in, " ,");
	if (!p) {
		wtof("HTTPD048W Missing STATS ON|OFF [RESET]");
		goto quit;
	}

	if (http_cmp(p, "ON")==0) {
		httpd->client |= HTTPD_CLIENT_STATS;
		wtof("HTTPD411I SMF recording is %s", "ON");
	}
	else if (http_cmp(p, "OFF")==0) {
		httpd->client &= ~HTTPD_CLIENT_STATS;
		wtof("HTTPD411I SMF recording is %s", "OFF");
	}
	else {
		wtof("HTTPD411E Invalid SET STATS value \"%s\"", p);
		goto quit;
	}

	// Check for RESET option
	next = strtok(NULL, " ,");
	if (next && http_cmpn(next, "RESET", strlen(next))==0) {
		httpd->total_requests = 0;
		httpd->total_errors = 0;
		httpd->total_bytes_sent = 0;
		wtof("HTTPD411I Statistics counters reset");
	}


quit:
    unlock(httpd,LOCK_EXC);
    return rc;
}
