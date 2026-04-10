/* HTTPD.C
** HTTP Server
*/
#ifndef VERSION
#define VERSION	"0.0.0-unknown"
#endif
#define HTTP_VERSION	VERSION

#include "httpd.h"
#include "clibgrt.h"
#include "clibsock.h"

static int socket_thread(void *arg1, void *arg2);
static int worker_thread(void *udata, CTHDWORK *work);

unsigned __stklen = 64*1024;

int main(int argc, char **argv);

#if 0
static void
close_stale_port(int port)
{
	int					rc;
	int					i;
    int                 addrlen;
    struct sockaddr     addr;
    struct sockaddr_in  serv_addr;

	for(i = 1; i < FD_SETSIZE; i++) {
		addrlen = sizeof(addr);
		rc = getsockname(i, &addr, &addrlen);
		if (rc==0) {
			struct sockaddr_in *in = (struct sockaddr_in*)&addr;
			if (in->sin_port == port) {
				wtof("HTTPD027I Closing stale socket %d on port:%u\n",
					i, in->sin_port);
				closesocket(i);
				sleep(2);	/* give the underlying host a chance to cleanup the socket */
				break;
			}
		}
	}
}
#endif

static void
initialize(int argc, char **argv)
{
    CLIBGRT             *grt        = __grtget();
    CLIBPPA             *ppa        = __ppaget();
    HTTPD               *httpd      = grt->grtapp1;
    unsigned            *psa        = (unsigned *)0;
    unsigned            *ascb       = (unsigned *)psa[0x224/4]; /* PSAAOLD  */
    unsigned            asid        = ((ascb[0x24/4]) >> 16);   /* ASCBASID */
    int                 rc;
    int					i;
    CTHDTASK            *task;
    const char 			*config		= NULL;

    /* if something goes all wrong, capture it! */
    abendrpt(ESTAE_CREATE, DUMP_DEFAULT);

	/* look for "CONFIG=" parm */
	for(i=0; i < argc; i++) {
		// wtof("%s: argv[%d]=\"%s\"", __func__, i, argv[i]);
		if (memcmp(argv[i], "CONFIG=", 7)==0) {
			config = &argv[i][7];
			while(*config=='=' || isspace(*config)) config++; 
			if (!*config) config = "CONFIG";
		}
	}

    /* initialize our server data area */
    memset(httpd, 0, sizeof(HTTPD));
    strcpy(httpd->eye, HTTPD_EYE);
    httpd->flag |= HTTPD_FLAG_INIT;
    sprintf(httpd->rname, "HTTPD.%04X", asid);

    /* lock the httpd while we're making changes and creating threads */
    lock(httpd,0);

    /* httpd <--> httpx */
    httpd->httpx = httpx;
    
    /* set the server up time */
    httpd->uptime = time64(NULL);

	/* httpd -> httpluax */
    httpd->luax		= &httpluax;
    
    /* Server version */
    httpd->version	= VERSION;

	/* Get configuration from member/dataset/NULL */
	rc = http_config(httpd, config);
	if (rc) {
		wtof("HTTPD404E Errors occured processing %s", config ? config : "(null)");
		if (httpd->listen) {
			closesocket(httpd->listen);
			httpd->listen = 0;
		}
		goto quit;
	}

	if (httpd->client & HTTPD_CLIENT_STATS) {
		if (smf_active())
			wtof("HTTPD415I SMF Type %d recording active", SMF_TYPE_HTTPD);
		else
			wtof("HTTPD415W SMF recording is inactive");
	}

    /* create socket thread */
    httpd->socket_thread = cthread_create_ex(socket_thread,
        httpd, httpx, 32*1024);
    if (!httpd->socket_thread) {
        wtof("HTTPD033E Unable to create socket thread\n");
        closesocket(httpd->listen);
        httpd->listen = 0;
        goto quit;
    }

    httpd->flag |= HTTPD_FLAG_LISTENER;

    /* update the main() thread information */
    task = cthread_self();
    if (task) {
        httpd->self = task;
        task->func = main;
        task->arg1 = (void*)argc;
        task->arg2 = argv;
        task->stacksize = ppa->ppastkln;
    }

	/* check configuration max and min task values */
    if (!(httpd->cfg_maxtask)) httpd->cfg_maxtask = 9;
    if (!(httpd->cfg_mintask)) httpd->cfg_mintask = 3;
    if (httpd->cfg_mintask > httpd->cfg_maxtask) {
		httpd->cfg_maxtask = httpd->cfg_mintask;
	}

    /* create 1 dispatcher and max 9 worker threads (576K total storage stack) */
    /* Note: the thread manager will only start 3 threads initially */
    httpd->mgr = cthread_manager_init(httpd->cfg_maxtask, worker_thread, httpd, 64*1024);
    if (!httpd->mgr) {
        /* gripe and continue */
        wtof("HTTPD034W Unable to create worker threads. "
             "Dynamic documents are disabled.");
    }

	/* set the min task for the thread manager */
    if (httpd->mgr) {
		CTHDMGR *mgr = httpd->mgr;

        lock(mgr,1);

		mgr->mintask = httpd->cfg_mintask;
		if (httpd->cfg_mintask > mgr->maxtask) mgr->maxtask = httpd->cfg_mintask;

		/* wake up the thread manager thread */
		rc = cthread_post(&mgr->wait, CTHDMGR_POST_DATA);

        unlock(mgr,1);
    }
    
    /* Initialize CGI context pointers array */
    i = (httpd->cfg_cgictx + 1) * sizeof(void *);
    httpd->cgictx = (void **) calloc(1, (size_t)i);

quit:
	/* initialize our credential key */
	rc = cred_init(httpd, sizeof(HTTPD));
	if (rc) {
		wtof("HTTPD047E Unable to initialize server credential key, rc=%d", rc);
	}

    /* unlock the httpd */
    unlock(httpd,0);

    /* remove ESTAE */
    abendrpt(ESTAE_DELETE, DUMP_DEFAULT);

    http_exit("initialize()\n");
    return;
}

#if 0
static int
report_abend(unsigned abcode)
{
    int     rc;

    http_enter("report_abend()\n");

    abcode = (unsigned) rc;
    if (abcode > 4095) {
        /* system abend */
        wtof("ABEND S%03X detected", abcode >> 12);
        rc = 12;
    }
    else {
        /* user abend */
        wtof("ABEND U%04d detected", abcode);
        rc = 12;
    }

    http_exit("report_abend()\n");
    return rc;
}
#endif // 0

static void
close_fd_set(void)
{
    CLIBGRT *grt    = __grtget();
    int i;

    http_enter("close_fd_set()\n");
    if (grt->grtsock) {
        unsigned count  = array_count(&grt->grtsock);
        while(count > 0) {
            count--;
            CLIBSOCK *s = grt->grtsock[count];
            if (!s) continue;
            if (s->socket >= 0) {
                closesocket(s->socket);
            }
        }
    }
    http_exit("close_fd_set()\n");
}

static void
terminate(void)
{
    CLIBGRT *grt        = __grtget();
    HTTPD   *httpd      = grt->grtapp1;
    HTTPC   *httpc;
    int     i;
    unsigned count;
    unsigned n;

    http_enter("terminate()\n");

    httpd->flag |= HTTPD_FLAG_QUIESCE;
    httpd->flag |= HTTPD_FLAG_SHUTDOWN;

    if (httpd->socket_thread) {
        CTHDTASK *task = httpd->socket_thread;

        for(i=0; (i < 10) && (!(task->termecb & 0x40000000)); i++) {
            if (i) {
                wtof("HTTPD040I Waiting for socket thread to terminate (%d)\n",
                    i);
            }
            /* we need to wait 1 second */
            __asm__("STIMER WAIT,BINTVL==F'100'   1 seconds");
        }

        if(!(task->termecb & 0x40000000)) {
            wtof("HTTPD041I Force detaching socket thread\n");
        }
        cthread_delete(&httpd->socket_thread);
    }

    /* close the listener socket */
    if (httpd->listen > 0) {
        http_dbgf("closing socket(%d)\n", httpd->listen);
        closesocket(httpd->listen);
        httpd->listen = 0;
    }

    /* terminate the worker threads */
    cthread_manager_term(&httpd->mgr);

    if (httpd->httpc) {
        for(httpc=httpd->httpc[0]; httpc; httpc=httpd->httpc[0]) {
            http_dbgf("closing socket(%d)\n", httpc->socket);
            http_close(httpc);
        }
        array_free(&httpd->httpc);
    }

    /* cleanup the CGI array */
    if (httpd->httpcgi) {
        count = array_count(&httpd->httpcgi);

        for(n=0; n < count; n++) {
            HTTPCGI *cgi = httpd->httpcgi[n];
			if (!cgi) continue;
			if (cgi->pgm) 	free(cgi->pgm);
			if (cgi->path) 	free(cgi->path); 
            free(cgi);
        }

        array_free(&httpd->httpcgi);
    }

    /* httpd->ufs removed — per-client sessions freed in httpclos.c */

    if (httpd->ufssys) {
        ufs_sys_term(&httpd->ufssys);
        httpd->ufssys = NULL;
    }

	if (httpd->client & HTTPD_CLIENT_STATS) {
		wtof("HTTPD416I Stats: %u requests, %u errors, %u bytes",
			httpd->total_requests, httpd->total_errors,
			httpd->total_bytes_sent);
	}

    /* just in case we missed something */
    close_fd_set();

    wtof("HTTPD002I Server is SHUTDOWN\n");

    http_exit("terminate()\n");
}

static int
build_fd_set(fd_set *read, fd_set *write, fd_set *excp)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         maxsock = 0;
    unsigned    count   = array_count(&httpd->httpc);
    unsigned    n;
    HTTPC       *httpc;
#if 0
    http_enter("build_fd_set()\n");
#endif
    if (read && httpd->listen >= 0) {
        memset(read, 0, sizeof(fd_set));
        FD_SET(httpd->listen, read);
        if (httpd->listen > maxsock) maxsock = httpd->listen;
    }
    if (write) {
        memset(write, 0, sizeof(fd_set));
    }
    if (excp) {
        memset(excp, 0, sizeof(fd_set));
    }

    for(n=0; n < count; n++) {
        httpc = httpd->httpc[n];
        if (!httpc) continue;           /* no client handle? */

        if (httpc->socket < 0) continue;   /* no socket? */

        if (httpc->socket > maxsock) maxsock = httpc->socket;
        if (read)   FD_SET(httpc->socket, read);
        if (write)  FD_SET(httpc->socket, write);
        if (excp)   FD_SET(httpc->socket, excp);
    }

quit:
    if (maxsock) maxsock++;
#if 0
    http_exit("build_fd_set(), maxsock=%d\n", maxsock);
#endif
    return maxsock;
}

static int
process_clients(fd_set *read, fd_set *write, fd_set *excp)
{
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    int         rc      = http_process_clients();
    int         lockrc;
    unsigned    count;
    unsigned    n;
    HTTPC       *httpc;

    /* process all the http clients (does its own locking) */
    rc = http_process_clients();

    /* get lock so we can cleanup any clients that are closing */
    lockrc = lock(httpd, 0);
    if (lockrc==0) goto httpd_locked;   /* we have the lock */
    if (lockrc==8) goto httpd_locked;   /* we already had the lock */
    wtof("%s lock failure", __func__);
    goto quit;

httpd_locked:
    /* look for any clients that need to be closed */
    count = array_count(&httpd->httpc);
    for(n=0; n < count; n++) {
        httpc = httpd->httpc[n];
        if (!httpc) continue;           /* no client handle? */

        if (httpc->state==CSTATE_CLOSE) {
            /* we close a single client at a time,
            ** other clients will be closed on next pass.
            */
            http_dbgf("closing client(%u) socket(%d)\n",
                n+1, httpc->socket);
            http_close(httpc);
            break;
        }
    }
    if (lockrc==0) unlock(httpd, 0);

quit:
    return 0;
}

static int
socket_thread(void *arg1, void *arg2)
{
	CLIBCRT     *crt    = __crtget();
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    unsigned    *psa    = (unsigned *)0;
    unsigned    *tcb    = (unsigned *)psa[0x21C/4]; /* A(TCB) from PSATOLD  */
    unsigned    *ascb   = (unsigned *)psa[0x224/4]; /* A(ASCB) from PSAAOLD */
    CTHDTASK    *task   = cthread_self();
    CTHDMGR     *mgr    = httpd->mgr;
    int         rc;
    int         sock;
    int         len;
    HTTPC       *httpc;
    timeval     wait;
    int         maxsock;
    unsigned    count;
    fd_set      read;
    struct sockaddr_in addr;
    struct sockaddr_in *a = (struct sockaddr_in *)&addr;

    http_enter("socket_thread()\n");

    wtof("HTTPD061I STARTING socket thread    TCB(%06X) TASK(%06X) STACKSIZE(%u)",
        tcb, task, task->stacksize);

    /* if something goes all wrong, capture it! */
    abendrpt(ESTAE_CREATE, DUMP_DEFAULT);

    /* we loop until we're shut down */
    while(task) {
        if (httpd->flag & HTTPD_FLAG_SHUTDOWN) break;

        /* process clients */
        if (process_clients(&read, NULL, NULL)) break;

        /* build fd_set for select() */
        maxsock = build_fd_set(&read, NULL, NULL);
        if (!maxsock) {
            wtof("HTTPD050E maxsock==0, something is wrong!");
            goto quit;
        }

        /* set the wait interval */
        wait.tv_sec     = 0;
        wait.tv_usec    = 0;

        mgr = httpd->mgr;
        if (mgr) {
            /* we're using worker threads for client processing */
            wait.tv_sec = 1;

            /* if we're quiesced then break out of loop */
            if (httpd->flag & HTTPD_FLAG_QUIESCE) break;
        }
        else {
            /* we're doing client processing from this thread */
            count       = array_count(&httpd->httpc);

            if (count==0) {
                /* no clients, so wait just a little bit */
                wait.tv_sec = 1;

                /* if we're quiesced then break out of loop */
                if (httpd->flag & HTTPD_FLAG_QUIESCE) break;
            }
        }

        /* wait for something to do */
        rc = selectex(maxsock, &read, NULL, NULL, &wait, NULL);
        if (rc<0) {
            if (httpd->flag & HTTPD_FLAG_QUIESCE) continue;     /* checked at top of loop */
            if (httpd->flag & HTTPD_FLAG_SHUTDOWN) continue;    /* checked at top of loop */
            wtof("HTTPD051E selectex() failed, rc=%d, error=%d", rc, errno);
            goto quit;
        }

        /* if we're quiesced then don't accept new connections */
        if (httpd->flag & HTTPD_FLAG_QUIESCE) continue;

        /* first check for new connections */
        if (FD_ISSET(httpd->listen, &read)) {
            /* new client wants to connect */
            len = sizeof(addr);
            sock = accept(httpd->listen, &addr, &len);
            if (sock<0) {
                wtof("HTTPD052E accept() failed, rc=%d, error=%d\n",
                    sock, errno);
                http_dbgf("accept() failed, rc=%d, error=%d\n", sock, errno);
                goto quit;
            }

            http_dbgf("new connection on socket=%d\n", sock);

            len = 1;    /* set non-blocking I/O */
            if (ioctlsocket(sock, FIONBIO, &len)) {
                wtof("HTTPD053E Unable to set non-blocking I/O for socket %d\n",
                    sock);
                http_dbgf("Unable to set non-blocking I/O for socket %d\n",
                    sock);
            }

            /* we've accepted a connection */
            httpc = calloc(1, sizeof(HTTPC));
            if (!httpc) {
                wtof("HTTPD999E Out of memory!");
                goto quit;
            }
            strcpy(httpc->eye, HTTPC_EYE);
            httpc->httpd  = httpd;
            httpc->socket = sock;
            httpc->addr   = a->sin_addr.s_addr;
            httpc->port   = a->sin_port;
            httpc->state  = CSTATE_IN;
            httpsecs(&httpc->start);
            httpd->active_connections++;
            /* UFS session created lazily by http_get_ufs() */

            mgr = httpd->mgr;
            if (mgr) {
                /* we have a thread manager */
                if (mgr->state == CTHDMGR_STATE_QUIESCE ||
                    mgr->state == CTHDMGR_STATE_STOPPED) {
                    /* manager is not available */
                    http_close(httpc);
                }
                else {
                    /* queue client to thread manager work queue */
                    cthread_queue_add(mgr, httpc);
                }
            }
            else {
                /* add new client to array of clients */
                array_add(&httpd->httpc, httpc);
            }
        } /* if (FD_ISSET(httpd->listen, &read)) */

    } /* while(task) */

quit:
	wtof("HTTPD060I SHUTDOWN socket thread    TCB(%06X) TASK(%06X) STACKSIZE(%u)",
		tcb, task, task->stacksize);

    abendrpt(ESTAE_DELETE, DUMP_DEFAULT);
    http_exit("socket_thread()\n");
    return 0;
}

static int
worker_thread(void *udata, CTHDWORK *work)
{
    CLIBCRT     *crt    = __crtget();           /* A(CLIBCRT) from TCBUSER  */
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    unsigned    *psa    = (unsigned *)0;
    unsigned    *tcb    = (unsigned *)psa[0x21C/4]; /* A(TCB) from PSATOLD  */
    unsigned    *ascb   = (unsigned *)psa[0x224/4]; /* A(ASCB) from PSAAOLD */
    CTHDTASK    *task   = cthread_self();
    CTHDMGR     *mgr    = work->mgr;
    int         rc      = 0;
    char        *data   = NULL;
    HTTPC       *httpc  = NULL;

    http_enter("worker_thread()\n");

    /* if something goes all wrong, capture it! */
    abendrpt(ESTAE_CREATE, DUMP_DEFAULT);

    wtof("HTTPD061I STARTING worker(%06X)   TCB(%06X) TASK(%06X) STACKSIZE(%u)",
        work, tcb, task, task->stacksize);

    while(task) {
        if (work->state == CTHDWORK_STATE_SHUTDOWN) break;

        rc = cthread_worker_wait(work,&data);
        if (rc == CTHDWORK_POST_SHUTDOWN) break;

        if (rc == CTHDWORK_POST_REQUEST) {
            /* process request */
            httpc = (HTTPC*)data;
            if (httpc && strcmp(httpc->eye, HTTPC_EYE)==0) {
                while(httpc->state != CSTATE_CLOSE) {
                    http_process_client(httpc);
                    if (work->state == CTHDWORK_STATE_SHUTDOWN) break;
                }
                http_dbgf("closing client(%08X) socket(%d)\n",
                    httpc, httpc->socket);
                http_close(httpc);
            }
            else {
                wtodumpf(data, 16, "%s unknown request", __func__);
            }
        }
        else if (rc == CTHDWORK_POST_TIMER) {
            /* process timer */
        }
    }

quit:
	wtof("HTTPD060I SHUTDOWN worker(%06X)   TCB(%06X) TASK(%06X) STACKSIZE(%u)",
		work, tcb, task, task->stacksize);

    abendrpt(ESTAE_DELETE, DUMP_DEFAULT);

    http_exit("worker_thread()\n");
    return 0;
}

static unsigned
build_ecblist(COM *com, unsigned **ecblist)
{
    unsigned        pos     = 0;
    unsigned        *ecbp;

    if (com->comecbpt) {
        ecblist[pos++] = com->comecbpt;
    }

quit:
    if (pos) {
        ecbp = ecblist[pos-1];
        ecblist[pos-1] = (unsigned*)((unsigned)ecbp | 0x80000000);
    }

    return pos;
}

#include "cde.h"
static CDE *
find_cde(const char *name)
{
    unsigned    *psa    = 0;                        /* low core == PSA      */
    unsigned    *tcb    = (unsigned*)psa[0x21c/4];  /* TCB      == PSATOLD  */
    unsigned    *jstcb  = (unsigned*)tcb[0x7c/4];   /* JSTCB    == TCBJSTCB */
    CDE         *cde    = (CDE*)jstcb[0x2c/4];      /* CDE      == TCBJPQ   */
    int         i;
    char        temp[9] = "        ";

    for(i=0; i < 8 && name[i]; i++) {
        temp[i] = name[i];
    }

    while(cde) {
        if (memcmp(cde->CDNAME, temp, 8)==0) {
            return cde;
            break;
        }
        cde = cde->CDCHAIN;
    }

    return NULL;
}

static int
auth_cde(CDE *cde)
{
    int     rc = 0;

    if (cde) {
        __asm__("MODESET KEY=ZERO,MODE=SUP\n" : : : "0", "1", "14", "15");
        cde->CDATTR2 |= (CDSYSLIB | CDAUTH);
        __asm__("MODESET KEY=NZERO,MODE=PROB" : : : "0", "1", "14", "15");
    }

    return rc;
}

static int
auth_name(const char *name)
{
    int     rc = 0;
    CDE     *cde = find_cde(name);

    if (cde) {
        /* wtodumpf(cde, sizeof(CDE), "%s %s CDE before auth_cde()", __func__, name); */
        rc = auth_cde(cde);
        /* wtodumpf(cde, sizeof(CDE), "%s %s CDE after auth_cde()", __func__, name); */
    }
    else {
        /* wtof("%s %s CDE was not found", __func__, name); */
        rc = 4;
    }

    return rc;
}

static int
identify_cthread(void)
{
    int     rc = 0;

    /* CTHREAD does not already exist, identify it now */
    /* wtof("%s preparing to IDENTIFY CTHREAD", __func__); */

    __asm__("L     1,=V(CTHREAD)    A(thread driver routine)\n\t"
            "LA    0,=CL8'CTHREAD'\n\t"
            "IDENTIFY EPLOC=(0),ENTRY=(1)\n\t"
            "ST    15,0(,%0)\n" : : "r" (&rc): "0", "1", "14", "15");
    __asm__("\n*\n");

    /* wtof("%s IDENTIFY CTHREAD rc=%d", __func__, rc); */

    rc = auth_name("CTHREAD");

    return rc;
}

static int auth_setup(const char *name)
{
    int     rc = 0;

    wtof("HTTPD010I %s is APF authorized", name);

    return rc;
}

static int unauth_setup(const char *name)
{
    int         rc      = 0;
    CLIBCRT     *crt    = __crtget();   /* A(CLIBCRT)               */

    /* this task is not currently APF authorized */
    rc = __autask();    /* APF authorize this task  */
    /* wtof("%s __autask() rc=%d", __func__, rc); */
    if (rc==0) {
        if (crt->crtauth & CRTAUTH_ON) {
            wtof("HTTPD011I %s was APF authorized via SVC 244", name);
            /* we want the STEPLIB APF authorized as well */
            rc = __austep();    /* APF authorize the STEPLIB */
            if (rc==0) {
                if (crt->crtauth & CRTAUTH_STEPLIB) {
                    wtof("HTTPD013I STEPLIB is now APF authorized");
                    rc = auth_name(name);
                    /* wtof("%s auth_name(%s) rc=%d", __func__, name, rc); */
                    rc = identify_cthread();
                    /* wtof("%s identify_cthread() rc=%d", __func__, rc); */
                }
            }
        }
        else {
            wtof("HTTPD011I %s is APF authorized", name);
            rc = auth_name(name);
            /* wtof("%s auth_name(%s) rc=%d", __func__, name, rc); */
            rc = identify_cthread();
            /* wtof("%s identify_cthread() rc=%d", __func__, rc); */
        }
    }
    else {
        wtof("HTTPD012E %s unable to dynamically obtain APF "
            "authorization", name);
    }

    return rc;
}

int
main(int argc, char **argv)
{
    CLIBPPA     *ppa    = __ppaget();   /* A(CLIBPPA)               */
    CLIBCRT     *crt    = __crtget();   /* A(CLIBCRT)               */
    CLIBGRT     *grt    = __grtget();   /* A(CLIBGRT)               */
    HTTPD       *httpd  = 0;
#if 0
    unsigned    *psa    = (unsigned *)0;
    unsigned    *tcb    = (unsigned *)psa[0x21C/4]; /* A(TCB) from PSATOLD  */
    unsigned    *ascb   = (unsigned *)psa[0x224/4]; /* A(ASCB) from PSAAOLD */
    unsigned    *asxb   = (unsigned *)ascb[0x6C/4]; /* A(ASXB) from ASCBASXB*/
    unsigned    *acee   = (unsigned *)asxb[0xC8/4]; /* A(ACEE) from ASXBSENV*/
#endif
    COM         *com    = __gtcom();
    CIB         *cib;
    int         i;
    int         rc;
    unsigned    count;
    unsigned    *ecblist[10];
    char        buf[1024];
    HTTPD       server;

    /* If this message has a leading '+' then we're not APF authorized yet. */
    /* in any case, we want to get the server version into the log in case we have problems */
    wtof("HTTPD000I HTTPD Server %s starting", VERSION);

    grt->grtapp1    = &server;
    httpd           = grt->grtapp1;

    if (crt->crtopts & CRTOPTS_AUTH) {
        /* this task was previously authorized */
        rc = auth_setup(argv[0]);
    }
    else {
        /* this task has not been authorized yet */
        rc = unauth_setup(argv[0]);
    }
    if (rc) goto quit;

#if 0
    for(i=0;i<argc;i++) {
        wtof("arg#%d=\"%s\"", i, argv[i] ? argv[i] : "(null)");
    }
#endif

#if 0
    wtof("main ASCB(%08X) ASXB(%08X) ASXBSENV(%08X)", ascb, asxb, acee);
    if (acee) {
        wtof("ACEEACEE=%-4.4s", acee);
    }
#endif

    if (!com) {
        wtof("HTTPD090E Unable to initialize console interface\n");
        goto quit;
    }

#if 0
    rc = try(initialize,argc,argv);
    if (rc) {
        /* report abend and exit */
        rc = report_abend((unsigned)rc);
        goto cleanup;
    }
#else
    /* Note: All the heavy lifting occurs in the threads created by initialize() */
    initialize(argc,argv);
#endif

    if (!httpd->listen) {
        /* no listener socket, exit */
        goto cleanup;
    }

#if 0
    /* issue "maxrates" command to the hercules console */
    httpcmd("maxrates", buf, sizeof(buf));
    wtof("%s", buf);
#endif

    /* set number of modify commands we'll queue */
    __cibset(5);

    httpd->flag |= HTTPD_FLAG_READY;
    wtof("HTTPD001I Server is READY\n");

    /* Note: The main thread (this thread) waits for console messages
     * like modify "/F jobname,..." and stop "/P jobname".
     *
     * The socket_thread() waits for client connections and
     * gives those clients to the thread manager for processing
     * by one of the worker_thread.
     */

    /* wait for a console command and process as needed */
    for(;;) {
        /* check for console command */
        if (*(com->comecbpt) & 0x40000000) {
            cib = __cibget();
            if (cib) {
                /* process console command */
                i = http_console(cib);
                __cibdel(cib);
                if (i) break;	/* must be a stop "/P jobname" */
            }
        }

        memset(ecblist, 0, sizeof(ecblist));
        count = build_ecblist(com, ecblist);
        if (count) {
            __asm__("WAIT ECBLIST=(%0)" : : "r"(ecblist));
        }
        else {
            /* we need to wait 1 second */
            __asm__("STIMER WAIT,BINTVL==F'100'   1 seconds");
        }
    }

cleanup:
    httpd->flag |= HTTPD_FLAG_QUIESCE;
    wtof("HTTPD002I Server is QUIESCE\n");

    i = try(terminate,0);
    if (i) wtof("terminate failed with rc=%08X", i);

quit:
    if (crt) {
        if (crt->crtauth & CRTAUTH_STEPLIB) {
            __uastep();    /* reset STEPLIB APF authorization  */
        }
        if (crt->crtauth & CRTAUTH_ON) {
            __uatask();    /* reset APF authorization  */
        }
    }

    return rc;
}

#if 0
__asm__("IKJTCB LIST=YES");
__asm__("IHAASCB");
__asm__("IHAASXB");
__asm__("CSECT ,");
#endif
