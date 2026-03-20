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
static int telemetry_thread(void *arg1, void *arg2);
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

	if ((httpd->client & HTTPD_CLIENT_STATS) && httpd->st_dataset) {
		wtof("HTTPD415I Loading stats from %s", httpd->st_dataset);
		httpstat_load(httpd, httpd->st_dataset);
	}

    if (httpd->httpt) {
        HTTPT   *httpt = httpd->httpt;
        
        /* create telemetry thread */
        if ((httpt->flag & HTTPT_FLAG_START) && (httpt->telemetry > 0)) {
            httpt->telemetry_thread = cthread_create_ex(telemetry_thread,
                httpd, httpt, 32*1024);
            if (!httpt->telemetry_thread) {
                wtof("HTTPD033E Unable to telemetry thread\n");
                goto quit;
            }
        }
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
    FTPD    *ftpd       = httpd->ftpd;
    HTTPC   *httpc;
    int     i;
    unsigned count;
    unsigned n;

    http_enter("terminate()\n");

    if (ftpd) {
        ftpdterm(&ftpd);
        httpd->ftpd = 0;
        http_pubf(httpd, "thread/listener/ftp", "");
    }

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
        http_pubf(httpd, "thread/listener/http", "");
    }

    /* terminate the worker threads */
    cthread_manager_term(&httpd->mgr);
    http_pubf(httpd, "thread/worker", "");

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
        wtof("HTTPD047I Terminating File System");
        ufs_sys_term(&httpd->ufssys);
        httpd->ufssys = NULL;
    }

	if ((httpd->client & HTTPD_CLIENT_STATS) && httpd->st_dataset) {
		wtof("HTTPD416I Saving stats to %s", httpd->st_dataset);
		httpstat_save(httpd, httpd->st_dataset);
	}

    /* Publish to MQTT Broker */
    http_pubf(httpd, "state", "shutdown");
    http_pub_datetime(httpd, "shutdown");
    
    if (httpd->httpt) {     /* HTTPD Telemetry (MQTT) */
        HTTPT *httpt = httpd->httpt;

        if (httpt->telemetry_thread) {
            CTHDTASK *task = httpt->telemetry_thread;

            /* wake up the telemetry thread */
       		cthread_post(&httpt->telemetry_ecb, 0);

            for(i=0; (i < 10) && (!(task->termecb & 0x40000000)); i++) {
                if (i) {
                    wtof("HTTPD040I Waiting for telemetry thread to terminate (%d)", i);
                }
                /* we need to wait 1 second */
                __asm__("STIMER WAIT,BINTVL==F'100'   1 seconds");
            }

            if(!(task->termecb & 0x40000000)) {
                wtof("HTTPD041I Force detaching telemetry thread\n");
            }
            cthread_delete(&httpt->telemetry_thread);
            http_pubf_nocache(httpd, "thread/telemetry/cache", "");
        }

        if (httpt->mqtc) {
            wtof("HTTPD429I Terminating MQTT");

            mqtc_close_client(httpt->mqtc);

            cthread_yield();
    
            mqtc_free_client(&httpt->mqtc);
        }

        if (httpt->broker_host) free(httpt->broker_host);
        if (httpt->broker_port) free(httpt->broker_port);
        if (httpt->prefix) free(httpt->prefix);
        if (httpt->smfid) free(httpt->smfid);
        if (httpt->jobname) free(httpt->jobname);

        if (httpt->httptc) {
            /* free the telemetry cache array */
            count = array_count(&httpt->httptc);
            
            for(n=count; n > 0; n--) {
                HTTPTC *httptc = array_del(&httpt->httptc, n);
                
                if (!httptc) continue;

                /* free the telemetry cache record */
                if (httptc->data) free(httptc->data);
                if (httptc->topic) free(httptc->topic);
                free(httptc);
            }
            
            array_free(&httpt->httptc);
        }
        
        free(httpt);
        httpd->httpt = NULL;
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
    FTPD        *ftpd   = httpd->ftpd;
    int         maxsock = 0;
    unsigned    count   = array_count(&httpd->httpc);
    unsigned    n;
    HTTPC       *httpc;
    FTPC        *ftpc;
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

    if (ftpd && ftpd->listen >= 0) {
        /* include the FTPD listener socket */
        if (read && ftpd->listen) {
            FD_SET(ftpd->listen, read);
            if (ftpd->listen > maxsock) maxsock = ftpd->listen;
        }
#if 0   /* clients are processed only in the worker threads */
        /* include any FTPD client sockets */
        count = array_count(&ftpd->ftpc);
        for(n=0; n < count; n++) {
            ftpc = ftpd->ftpc[n];
            if (!ftpc) continue;           /* no client handle? */

            if (ftpc->client_socket >= 0) {
                /* the clients control channel socket */
                if (ftpc->client_socket > maxsock) maxsock = ftpc->client_socket;
                if (read)   FD_SET(ftpc->client_socket, read);
                if (write)  FD_SET(ftpc->client_socket, write);
                if (excp)   FD_SET(ftpc->client_socket, excp);
            }
            if (ftpc->data_socket >= 0) {
                /* the clients data channel socket */
                if (ftpc->data_socket > maxsock) maxsock = ftpc->data_socket;
                if (read)   FD_SET(ftpc->data_socket, read);
                if (write)  FD_SET(ftpc->data_socket, write);
                if (excp)   FD_SET(ftpc->data_socket, excp);
            }
        }
#endif
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
#if 0
    FTPD        *ftpd   = httpd->ftpd;
#endif
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

#if 0 /* clients are processed only in the worker threads */
    if (!ftpd) goto quit;

    /* process ftp clients (does its own locking */
    rc = ftpd_process_clients();
#endif

quit:
    return 0;
}

static int
socket_thread(void *arg1, void *arg2)
{
	CLIBCRT     *crt    = __crtget();
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    FTPD        *ftpd   = httpd->ftpd;
    unsigned    *psa    = (unsigned *)0;
    unsigned    *tcb    = (unsigned *)psa[0x21C/4]; /* A(TCB) from PSATOLD  */
    unsigned    *ascb   = (unsigned *)psa[0x224/4]; /* A(ASCB) from PSAAOLD */
    CTHDTASK    *task   = cthread_self();
    CTHDMGR     *mgr    = httpd->mgr;
    int         rc;
    int         sock;
    int         len;
    HTTPC       *httpc;
    FTPC        *ftpc;
    timeval     wait;
    int         maxsock;
    unsigned    count;
    fd_set      read;
    struct sockaddr_in addr;
    struct sockaddr_in *a = (struct sockaddr_in *)&addr;
    char        *timebuf;

    http_enter("socket_thread()\n");

    wtof("HTTPD061I STARTING socket thread    TCB(%06X) TASK(%06X) STACKSIZE(%u)",
        tcb, task, task->stacksize);

    /* Publish to MQTT Broker */
    timebuf = http_get_timebuf();
    if (timebuf) {
        lock(timebuf, LOCK_EXC);
        http_pubf(httpd, "thread/listener", 
            "{ \"datetime\" : \"%s\" "
            ", \"tcb\" : \"%06X\" "
            ", \"task\" : \"%06X\" "
            ", \"stacksize\" : %u "
            "}",
            http_get_datetime(httpd), tcb, task, task->stacksize);
        unlock(timebuf, LOCK_EXC);
    }

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

        if (ftpd) {
            /* check for new connections */
            if (FD_ISSET(ftpd->listen, &read)) {
                /* new client wants to connect */
                len = sizeof(addr);
                sock = accept(ftpd->listen, &addr, &len);
                if (sock<0) {
                    wtof("FTPD0052E accept() failed, rc=%d, error=%d\n",
                        sock, errno);
                    http_dbgf("accept() failed, rc=%d, error=%d\n", sock, errno);
                    goto quit;
                }

                http_dbgf("new connection on socket=%d\n", sock);
                /* wtof("%s new connection on socket=%d", __func__, sock); */

#if 0 /* we want the FTP control socket to block */
                len = 1;    /* set non-blocking I/O */
                if (ioctlsocket(sock, FIONBIO, &len)) {
                    wtof("FTPD0053E Unable to set non-blocking I/O for socket %d\n",
                        sock);
                    http_dbgf("Unable to set non-blocking I/O for socket %d\n",
                        sock);
                }
#endif
                /* we've accepted a connection */
                ftpc = calloc(1, sizeof(FTPC));
                if (!ftpc) {
                    wtof("FTPD0999E Out of memory!");
                    goto quit;
                }
                strcpy(ftpc->eye, FTPC_EYE);
                ftpc->ftpd          = ftpd;
                ftpc->client_socket = sock;
                ftpc->client_addr   = a->sin_addr.s_addr;
                ftpc->client_port   = a->sin_port;
                ftpc->data_socket   = -1;
                ftpc->state         = FTPSTATE_INITIAL;
                ftpdsecs(&ftpc->start);

                mgr = httpd->mgr;
                if (mgr) {
                    /* we have a thread manager */
                    if (mgr->state == CTHDMGR_STATE_QUIESCE ||
                        mgr->state == CTHDMGR_STATE_STOPPED) {
                        /* manager is not available */
                        ftpcterm(&ftpc);
                    }
                    else {
                        /* queue client to thread manager work queue */
                        cthread_queue_add(mgr, ftpc);
                    }
                }
                else {
                    /* add new client to array of clients */
                    array_add(&ftpd->ftpc, ftpc);
                }
            }   /* if (FD_ISSET(ftpd->listen, &read)) */
        }   /* if (ftpd) */
    } /* while(task) */

quit:
	wtof("HTTPD060I SHUTDOWN socket thread    TCB(%06X) TASK(%06X) STACKSIZE(%u)",
		tcb, task, task->stacksize);

    /* Publish to MQTT Broker */
    http_pubf(httpd, "thread/listener", "");

    abendrpt(ESTAE_DELETE, DUMP_DEFAULT);
    http_exit("socket_thread()\n");
    return 0;
}

typedef struct static_process_telemetry_ping {
    time64_t    ping_time;
} SPTP;
static SPTP  *static_process_telemetry_ping;

static int
http_process_telemetry_ping(HTTPD *httpd)
{
    int         rc      = 0;
    SPTP        *sptp   = __wsaget(&static_process_telemetry_ping, sizeof(SPTP));
    HTTPT       *httpt  = httpd->httpt;
    MQTC        *mqtc;
    time64_t    now;
    char        timebuf[26];
    unsigned    n;
    unsigned    count;
    unsigned    bytes   = 0;

    if (!httpt) goto quit;
    if (!httpt->mqtc) goto quit;

    time64(&now);

    if (__64_cmp(&sptp->ping_time, &now)== __64_LARGER) goto quit;

    if(!__64_is_zero(&sptp->ping_time)) {
        /* send PING request */
        rc = mqtc_send_ping(httpt->mqtc);
    }

    /* update next PING time */
    __64_add_u32(&now, 60, &sptp->ping_time);

quit:
    return rc;
}




typedef struct static_process_telemetry_cache {
    unsigned    records;
    unsigned    bytes;
} SPTC;
static SPTC  *static_process_telemetry_cache;

static int
http_process_telemetry_cache(HTTPD *httpd)
{
    int         rc      = 0;
    int         lockrc  = 8;
    SPTC        *sptc   = __wsaget(&static_process_telemetry_cache, sizeof(SPTC));
    HTTPT       *httpt  = httpd->httpt;
    HTTPTC      *httptc;
    MQTC        *mqtc;
    time64_t    last;
    char        timebuf[26];
    unsigned    n;
    unsigned    count;
    unsigned    bytes   = 0;

    if (!httpt) goto quit;
    if (!httpt->mqtc) goto quit;

    __64_init(&last);

    lockrc = lock(&httpt->httptc, LOCK_SHR);

    count = array_count(&httpt->httptc);
    for(n=0; n < count; n++) {
        httptc = httpt->httptc[n];
        
        if (!httptc) continue;
        bytes += sizeof(HTTPTC);
        bytes += (httptc->topic_len + 1 + 7) & 0xFFFFFFF8;
        bytes += (httptc->data_len  + 1 + 7) & 0xFFFFFFF8;
        
        if (__64_cmp(&httptc->last, &last) == __64_LARGER) {
            last = httptc->last;
        }
    }

    if (sptc->records == count && sptc->bytes == bytes) {
        /* and the same record count and byte count */
        goto quit;
    }
    
    sptc->records = count;
    sptc->bytes = bytes;

    if (__64_is_zero(&last)) time64(&last);

    /* don't cache this topic */
    http_pubf_nocache(httpd, "thread/telemetry/cache", 
        "{ \"datetime\" : \"%s\" "
        ", \"records\" : %u "
        ", \"bytes\" : %u "
        "}",
        http_fmt_datetime(httpd, &last, timebuf),
        count,
        bytes);


quit:
    if (lockrc==0) unlock(&httpt->httptc, LOCK_SHR);
    return rc;
}

typedef struct static_process_telemetry {
    __64        dispatched;
    unsigned    worker_count;
} SPT;
static SPT  *static_process_telemetry;

static int
http_process_telemetry(HTTPD *httpd, CTHDMGR *mgr)
{
    int         rc      = 0;
    SPT         *spt    = __wsaget(&static_process_telemetry, sizeof(SPT));
    HTTPT       *httpt  = httpd->httpt;
    MQTC        *mqtc;
    time64_t    now;
    char        *timebuf = http_get_timebuf();
    CTHDTASK    *task;
    CTHDWORK    *work;
    unsigned    n;
    unsigned    worker_count;
    unsigned    queue_count;
    const char  *state;

    if (!httpt) goto quit;
    if (!httpt->mqtc) goto quit;


    /* get current time */
    time64(&now);

    task            = mgr->task;
    worker_count    = array_count(&mgr->worker);
    queue_count     = array_count(&mgr->queue);

    if (__64_cmp(&spt->dispatched, &mgr->dispatched) == __64_EQUAL) {
        /* the dispatch count hasn't changed */
        if (spt->worker_count == worker_count) {
            /* and the same number of workers */
            goto quit;
        }
    }
    
    spt->dispatched = mgr->dispatched;
    spt->worker_count = worker_count;

    switch(mgr->state) {
        case CTHDMGR_STATE_INIT:    state = "INIT"; break;
        case CTHDMGR_STATE_RUNNING: state = "RUNNING"; break;
        case CTHDMGR_STATE_QUIESCE: state = "QUIESCE"; break;
        case CTHDMGR_STATE_STOPPED: state = "STOPPED"; break;
        case CTHDMGR_STATE_WAITING: state = "WAITING"; break;
        default:                    state = "UNKNOWN"; break;
    }

    if (timebuf) {
        lock(timebuf, LOCK_EXC);
        http_pubf(httpd, "thread/worker", 
            "{ \"datetime\" : \"%s\" "
            ", \"dispatched\" : %llu "
            ", \"state\" : \"%s\" "
            ", \"mgr\" : \"%06X\" "
            ", \"task\" : \"%06X\" "
            ", \"tcb\" : \"%06X\" "
            ", \"mintask\" : %u "
            ", \"maxtask\" : %u "
            ", \"stacksize\" : %u "
            ", \"workers\" : %u "
            ", \"queued\" : %u "
            "}",
            http_get_datetime(httpd), 
            mgr->dispatched,
            state,
            mgr, 
            task, 
            task->tcb, 
            mgr->mintask,
            mgr->maxtask,
            task->stacksize,
            worker_count,
            queue_count);
        unlock(timebuf, LOCK_EXC);
    }
    
    for(n=0; n < worker_count; n++) {
        char    topic[40];
        
        work = mgr->worker[n];

        sprintf(topic, "thread/worker/%u", n+1);
        
        if (!work) goto discard;            /* no worker            */

        task = work->task;
        if (!task) goto discard;            /* no task              */
        if (!task->tcb) goto discard;       /* no tcb               */
        if (task->termecb) goto discard;    /* tcb terminated       */

        switch(work->state) {
            case CTHDWORK_STATE_INIT:       state = "INIT";     break;
            case CTHDWORK_STATE_RUNNING:    state = "RUNNING";  break;
            case CTHDWORK_STATE_WAITING:    state = "WAITING";  break;
            case CTHDWORK_STATE_DISPATCH:   state = "DISPATCH"; break;
            case CTHDWORK_STATE_SHUTDOWN:   state = "SHUTDOWN"; break;
            case CTHDWORK_STATE_STOPPED:    state = "STOPPED";  break;
            default:                        state = "UNKNOWN";  break;
        }

        lock(timebuf, LOCK_EXC);
        http_pubf(httpd, topic, 
            "{ \"datetime\" : \"%s\" "
            ", \"dispatched\" : %llu "
            ", \"state\" : \"%s\" "
            ", \"task\" : \"%06X\" "
            ", \"tcb\" : \"%06X\" "
            ", \"stacksize\" : %u "
            "}",
            http_fmt_datetime(httpd, &work->start_time, timebuf),
            work->dispatched,
            state,
            task, 
            task->tcb, 
            task->stacksize);
        unlock(timebuf, LOCK_EXC);

        continue;

discard:
        http_pubf(httpd, topic, "");
        continue;  /* tcb has ended        */
    }
    
quit:
    return rc;
}

static int
telemetry_thread(void *udata1, void *udata2)
{
    HTTPD       *httpd  = udata1;                   /* Server handle */
    HTTPT       *httpt  = httpd->httpt;             /* Telemetry handle */
    unsigned    *psa    = (unsigned *)0;
    unsigned    *tcb    = (unsigned *)psa[0x21C/4]; /* A(TCB) from PSATOLD  */
    unsigned    *ascb   = (unsigned *)psa[0x224/4]; /* A(ASCB) from PSAAOLD */
    CTHDTASK    *task   = cthread_self();
    CTHDMGR     *mgr;
    int         rc      = 0;
    char        *timebuf;

    wtof("HTTPD061I STARTING telemetry thread TCB(%06X) TASK(%06X) STACKSIZE(%u)", 
        tcb, task, task->stacksize);

    /* Publish to MQTT Broker */
    timebuf = http_get_timebuf();
    if (timebuf) {
        lock(timebuf, LOCK_EXC);

        if (httpt && httpt->mqtc && httpt->mqtc->task) {
            MQTC        *mqtc       = httpt->mqtc;
            CTHDTASK    *subtask    = mqtc->task;
            CTHDTASK    *self       = httpd->self;
            unsigned    addr, port;
            
            http_pubf(httpd, "thread/main",
                "{ \"datetime\" : \"%s\" "
                ", \"tcb\" : \"%06X\" "
                ", \"task\" : \"%06X\" "
                ", \"stacksize\" : %u "
                "}",
                http_get_datetime(httpd), self->tcb, self, self->stacksize);

            http_pubf(httpd, "thread/mqtt", 
                "{ \"datetime\" : \"%s\" "
                ", \"tcb\" : \"%06X\" "
                ", \"task\" : \"%06X\" "
                ", \"stacksize\" : %u "
                "}",
                http_get_datetime(httpd), subtask->tcb, subtask, subtask->stacksize);

            http_pubf(httpd, "thread/mqtt/broker",
                "{ \"host\" : \"%s\" "
                ", \"port\" : \"%s\" "
                ", \"socket\" : %d "
                "}",
                mqtc->broker_host, mqtc->broker_port, mqtc->sock);
        }

        
        http_pubf(httpd, "thread/telemetry", 
            "{ \"datetime\" : \"%s\" "
            ", \"tcb\" : \"%06X\" "
            ", \"task\" : \"%06X\" "
            ", \"stacksize\" : %u "
            "}",
            http_get_datetime(httpd), tcb, task, task->stacksize);
        unlock(timebuf, LOCK_EXC);
    }

    /* if something goes all wrong, capture it! */
    abendrpt(ESTAE_CREATE, DUMP_DEFAULT);

    while(httpd) {
        unsigned    wait;

        httpt = httpd->httpt;              /* Telemetry handle */
        if (!httpt) break;

        wait = httpt->telemetry * 100;        
        
        /* wait for ecb post or timer expire */
        cthread_timed_wait(&httpt->telemetry_ecb, wait, 0);

        if (httpd->flag & HTTPD_FLAG_SHUTDOWN) break;

        /* terminate() may shutdown the thread manager before us */
        mgr = httpd->mgr;
        if (!mgr) break;
        
        http_process_telemetry(httpd, mgr);
        http_process_telemetry_cache(httpd);
        http_process_telemetry_ping(httpd);
    }

    abendrpt(ESTAE_DELETE, DUMP_DEFAULT);

	wtof("HTTPD060I SHUTDOWN telemetry thread TCB(%06X) TASK(%06X) STACKSIZE(%u)",
		tcb, task, task->stacksize);

    http_pubf(httpd, "thread/telemetry", "");
    http_pubf(httpd, "thread/mqtt/broker", "");
    http_pubf(httpd, "thread/mqtt", "");
    http_pubf(httpd, "thread/main", "");
    
    return 0;
}


static int
worker_thread(void *udata, CTHDWORK *work)
{
    CLIBCRT     *crt    = __crtget();           /* A(CLIBCRT) from TCBUSER  */
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    FTPD        *ftpd   = httpd->ftpd;
    HTTPT       *httpt  = httpd->httpt;         /* Telemtry handle */
    unsigned    *psa    = (unsigned *)0;
    unsigned    *tcb    = (unsigned *)psa[0x21C/4]; /* A(TCB) from PSATOLD  */
    unsigned    *ascb   = (unsigned *)psa[0x224/4]; /* A(ASCB) from PSAAOLD */
    CTHDTASK    *task   = cthread_self();
    CTHDMGR     *mgr    = work->mgr;
    int         rc      = 0;
    char        *data   = NULL;
    HTTPC       *httpc  = NULL;
    FTPC        *ftpc   = NULL;
    char        *timebuf;
    char        topic[40];
    unsigned    n, count;

    http_enter("worker_thread()\n");

    /* if something goes all wrong, capture it! */
    abendrpt(ESTAE_CREATE, DUMP_DEFAULT);

    wtof("HTTPD061I STARTING worker(%06X)   TCB(%06X) TASK(%06X) STACKSIZE(%u)",
        work, tcb, task, task->stacksize);

    if (httpt && httpt->telemetry_thread) {
        cthread_post(&httpt->telemetry_ecb, 0);
    }

#if 0
    /* Publish to MQTT Broker */
    sprintf(topic, "thread/worker(%06X)", work);

    timebuf = http_get_timebuf();
    if (timebuf) {
        lock(timebuf, LOCK_EXC);
        http_pubf(httpd, topic, 
            "{ \"datetime\" : \"%s\" "
            ", \"tcb\" : \"%06X\" "
            ", \"task\" : \"%06X\" "
            ", \"mgr\" : \"%06X\" "
            ", \"crt\" : \"%06X\" "
            "}",
            http_get_datetime(httpd), tcb, task, mgr, crt);
        unlock(timebuf, LOCK_EXC);
    }
#endif

    while(task) {
        if (work->state == CTHDWORK_STATE_SHUTDOWN) break;

        rc = cthread_worker_wait(work,&data);
        if (rc == CTHDWORK_POST_SHUTDOWN) break;

        if (rc == CTHDWORK_POST_REQUEST) {
            /* process request */
            httpc = (HTTPC*)data;
            ftpc  = (FTPC*)data;
            if (httpc && strcmp(httpc->eye, HTTPC_EYE)==0) {
                while(httpc->state != CSTATE_CLOSE) {
                    http_process_client(httpc);
                    if (work->state == CTHDWORK_STATE_SHUTDOWN) break;
                }
                http_dbgf("closing client(%08X) socket(%d)\n",
                    httpc, httpc->socket);
                http_close(httpc);
            }
            else if (ftpc && strcmp(ftpc->eye, FTPC_EYE)==0) {
                while(ftpc->state != FTPSTATE_TERMINATE) {
                    ftpd_process_client(ftpc);
                    if (work->state == CTHDWORK_STATE_SHUTDOWN) break;
                }
                ftpcterm(&ftpc);
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

#if 0
    /* Delete topic from MQTT Broker */
    http_pubf(httpd, topic, "");
#endif

    count = array_count(&mgr->worker);
    for(n=0; n < count; n++) {
        CTHDWORK *w = mgr->worker[n];
        
        if (w == work) {
            sprintf(topic, "thread/worker/%u", n+1);
            http_pubf(httpd, topic, "");
            break;
        }
    }

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
    
    /* Publish to MQTT Broker */
    http_pub_datetime(httpd, "ready");
    http_pubf(httpd, "state", "ready");

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

    http_pubf(httpd, "state", "quiesce");
    http_pub_datetime(httpd, "quiesce");

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
