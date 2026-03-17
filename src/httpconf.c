#include "httpd.h"

static int setLuaPath(lua_State* L, const char* path);
static int setHttpdDefaults(lua_State *L);
static void dumpstack (lua_State *L, const char *funcname);

#if 0
static int http_lua_wto(lua_State *L);
#endif

static int process_configuration(lua_State *L, HTTPD *httpd);
static int process_httpd(lua_State *L, HTTPD *httpd);
static int process_cgi(lua_State *L, HTTPD *httpd);
static int process_ftpd(lua_State *L, HTTPD *httpd);
static int process_mqtc(lua_State *L, HTTPD *httpd);
static void close_stale_port(int port);

int 
http_config(HTTPD *httpd, const char *member)
{
    CLIBCRT		*crt 	= __crtget();
	int			rc		= -1;
	int			i;
	lua_State	*L		= NULL;
	char 		mem[9];
	char 		dataset[56];

	if (member) {
		if (strchr(member, '.')) {
			/* member is likely a datset name */
			for(i=0; i < sizeof(dataset); i++) {
				dataset[i] = toupper(member[i]);
				if (member[i]==0) break;
			}
			dataset[sizeof(dataset)-1]=0;
		}
		else {
			/* build configuration dataset name */
			for(i=0; i < sizeof(mem); i++) {
				mem[i] = toupper(member[i]);
				if (member[i]==0) break;
			}
			mem[8] = 0;
			snprintf(dataset, sizeof(dataset),"DD:HTTPDLUA(%s)", mem);
		}
	}
	
	// wtof("%s: enter", __func__);

	/* put our HTTPD pointer in the CRT */
	crt->crtapp1 = httpd;

	/* create new Lua state */
	L = luaL_newstate();
	if (!L) goto quit;

	/* save our Lua state handle */
	httpd->config = L;

	/* Open Lua libraries */
	luaL_openlibs(L);

	/* Set the search path for require commands */
	setLuaPath(L,"DD:HTTPDLUA(?);DD:LUALIB(?)");

	/* make WTO available */
#if 1
	luaL_dostring(L, "wto = os.wto");
#else
	lua_pushcfunction(L, http_lua_wto);
	lua_setglobal(L, "wto");
#endif

	/* set default */
	setHttpdDefaults(L);	// create "httpd" amd "ftpd" global tables

#if 0 /* debugging */
	// dumpstack(L, __func__);
	luaL_dostring(L, "wto('Global Variables') for n in pairs(_G) do wto(n) end");	
	luaL_dostring(L, "wto('-------------------------------')");	
	luaL_dostring(L, "wto('HTTPD Variables') for n,v in pairs(httpd) do wto(n..'='..v) end");	
	luaL_dostring(L, "wto('-------------------------------')");	
	luaL_dostring(L, "wto('CGI Variables') for n,v in pairs(cgi) do wto(n..'='..v) end");	
	luaL_dostring(L, "wto('-------------------------------')");	
	luaL_dostring(L, "wto('FTPD Variables') for n,v in pairs(ftpd) do wto(n..'='..v) end");	
	luaL_dostring(L, "wto('-------------------------------')");	
#endif

	/* did the user provide a member or dataset to execute? */
	if (member) {
		/* yes, execute Lua script */
		rc = luaL_dofile(L, dataset);
		if (rc) {
			/* Something went wrong, dump stack to console */
			dumpstack(L,__func__);
		}
	}
	
	/* Process the configuration */
	rc = process_configuration(L, httpd);

quit:	
	// wtof("%s: exit rc=%d", __func__, rc);
	return rc;
}

static int 
process_configuration(lua_State *L, HTTPD *httpd) 
{
	int			rc;
	
    rc = process_mqtc(L, httpd);    // MQTT Client
    if (rc) goto quit;

	rc = process_httpd(L, httpd);
	if (rc) goto quit;

	rc = process_ftpd(L, httpd);	// FTPD listener
	if (rc) goto quit;

	rc = process_cgi(L, httpd);		// CGI pgm to path
	if (rc) goto quit;

quit:
	return rc;
}

static int 
process_mqtc(lua_State *L, HTTPD *httpd) 
{
    HTTPT       *httpt;
	int			rc	= 0;
    int         i;
    const char  *p;
    int         sockfd;

    /* allocate HTTPD Telemetry Handle */
    httpt = calloc(1, sizeof(HTTPT)); 
    if (!httpt) {
        wtof("HTTPD999E Out of memory!");
        rc = 8;
        goto quit;
    }
    httpd->httpt = httpt;
    strcpy(httpt->eye, HTTPT_EYE);
    
    httpt->smfid = calloc(1, 5);
    if (!httpt->smfid) {
        wtof("HTTPD999E Out of memory!");
        rc = 8;
        goto quit;
    }
    memcpy(httpt->smfid, __smfid(), 4);
    httpt->smfid[4] = 0;
    strtok(httpt->smfid, " ");
    
    httpt->jobname = calloc(1, 9);
    if (!httpt->jobname) {
        wtof("HTTPD999E Out of memory!");
        rc = 8;
        goto quit;
    }
    memcpy(httpt->jobname, __jobname(), 8);
    httpt->jobname[8] = 0;
    strtok(httpt->jobname, " ");

	lua_getglobal(L, "mqtt");	// push mqtc table on top of stack

	lua_getfield(L,-1,"start");
	i = (int) lua_tointeger(L, -1);
    if (i) httpt->flag |= HTTPT_FLAG_START;
	lua_pop(L,1);

	lua_getfield(L,-1,"datetime_gmt");
	i = (int) lua_tointeger(L, -1);
    if (i) httpt->flag |= HTTPT_FLAG_GMT;
	lua_pop(L,1);

	lua_getfield(L,-1,"broker_host");
	p = luaL_checkstring(L, -1);
    httpt->broker_host = strdup(p);
	lua_pop(L,1);

	lua_getfield(L,-1,"broker_port");
	p = luaL_checkstring(L, -1);
    httpt->broker_port = strdup(p);
	lua_pop(L,1);

	lua_getfield(L,-1,"prefix");
	p = luaL_checkstring(L, -1);
    while(*p=='/' || *p==' ') p++;  /* skip leading '/' or space characters */
    if (*p) {
        /* not an empty string */
        i = strlen(p);
        httpt->prefix = calloc(1, i+4);
        if (!httpt->prefix) {
            wtof("HTTPD999E Out of memory!");
            rc = 8;
            goto quit;
        }
        strcpy(httpt->prefix, p);
        if (httpt->prefix[i-1] != '/') {
            httpt->prefix[i] = '/'; /* append '/' */
        }
    }
    else {
        /* empty string, create default "smfid/jobname/" prefix */
        i = strlen(httpt->smfid) + strlen(httpt->jobname) + 4;
        httpt->prefix = calloc(1, i);
        if (!httpt->prefix) {
            wtof("HTTPD999E Out of memory!");
            rc = 8;
            goto quit;
        }
        sprintf(httpt->prefix, "%s/%s/", httpt->smfid, httpt->jobname);
    }
	lua_pop(L,1);

	lua_getfield(L,-1,"telemetry");
	i = (int) lua_tointeger(L, -1);
    if (i < 0) i = 30;
    httpt->telemetry = (unsigned)i;
	lua_pop(L,1);


    rc = 0;
    if (!(httpt->flag & HTTPT_FLAG_START)) {
        wtof("HTTPD420I MQTT mqtt.start not enabled in config");
        goto quit;
    }

    /* Create MQTT Client Handle */
    httpt->mqtc = mqtc_new_client();
    if (!httpt->mqtc) {
        wtof("HTTPD422E MQTT Unable to allocate client handle");
        closesocket(sockfd);
        goto quit;
    }

    /* Open socket connection with MQTT Broker */
    rc = mqtc_open_client(httpt->mqtc, httpt->broker_host, httpt->broker_port);
    if (rc) {
        wtof("HTTPD421E MQTT Failed to open socket. Broker: %s Port: %s",
            httpt->broker_host, httpt->broker_port);
        mqtc_free_client(&httpt->mqtc);
        goto quit;
    }
    wtof("HTTPD421I MQTT Broker: %s Port: %s", httpt->broker_host, httpt->broker_port);

    /* start MQTT processing thread */
    rc = mqtc_create_thread(httpt->mqtc);
    if (rc) {
        wtof("HTTPD424E MQTT Unable to create MQTT processing thread");
        goto quit;
    }

    /* Send CONNECT packet to Broker */
    rc = mqtc_connect(httpt->mqtc);
    if (rc) {
        wtof("HTTPD425E MQTT Unable to connect with MQTT broker");
        goto quit;
    }

    rc = 0;
    wtof("HTTPD426I MQTT Topic Prefix: %s", httpt->prefix);
    
    /* Publish to MQTT Broker */
    http_pub_datetime(httpd, "start");
    http_pubf(httpd, "version", "%s", httpd->version);
    http_pubf(httpd, "state", "start");

    /* Delete these topics */
    http_pubf(httpd, "shutdown", "");
    http_pubf(httpd, "quiesce", "");
    http_pubf(httpd, "ready", "");
    

quit:
	lua_pop(L,1);	// pop mqtc table off of stack
	return rc;
}


static int 
process_cgi(lua_State *L, HTTPD *httpd) 
{
	int			rc	= 0;
	int			login = httpd->login & HTTPD_LOGIN_CGI;
	int			i;
	const char 	*p;
	char		program[9];
	const char 	*path;

	lua_getglobal(L, "cgi");	// push cgi table on top of stack
	lua_pushnil(L);				// Push nil (nil program name)
	
	while(lua_next(L, -2) != 0) {
		/* get program name */
		p   	= lua_tostring(L,-2);
		/* fold to upper case */
		for(i=0; i < sizeof(program); i++) {
			program[i] = toupper(p[i]);
			if (p[i]==0) break;
		}
		program[8] = 0;
		
		/* get path name */
		path	= lua_tostring(L,-1);
		// wtof("%s: %s=%s", __func__, program, path);
		if (!http_add_cgi(httpd, program, path, login)) {
			wtof("HTTPD035W Unable to create CGI array for path=\"%s\" program=\"%s\"", path, program);
		}

		lua_pop(L, 1); 	// Pop path from stack
	}

quit:
	lua_pop(L,1);	// pop cgi table off of stack
	return rc;
}

static int process_ftpd_bind(lua_State *L, HTTPD *httpd);

static int 
process_ftpd(lua_State *L, HTTPD *httpd) 
{
	int			rc	= 0;

	lua_getglobal(L, "ftpd");	// push ftpd table on top of stack

	rc = process_ftpd_bind(L, httpd);
	if (rc) goto quit;

quit:
	lua_pop(L,1);	// pop ftpd table off of stack
	return rc;
}

static int 
process_ftpd_bind(lua_State *L, HTTPD *httpd)
{
	int					rc	 	= 0;
	FTPD				*ftpd	= httpd->ftpd;
	int					i;
	int					bind_tries;
	int					bind_sleep;
	int					listen_queue;
	int					sock;
	int					port;
	const char			*p;
    int                 addrlen;
    struct sockaddr     addr;
    struct sockaddr_in  serv_addr;
    unsigned char       *a;

	if (!ftpd) goto quit;

	/* get the ftpd.port value */
	lua_getfield(L,-1,"port");
	port = (int) lua_tointeger(L, -1);
	// wtof("%s: port=%d", __func__, port);
	lua_pop(L,1);

	if (!port) port = 8021;
	
	ftpd->port = port;

	/* close sockets that are using this port */
	close_stale_port(port);

	lua_getfield(L,-1,"bind_tries");
	bind_tries = (int) lua_tointeger(L, -1);
	// wtof("%s: bind_tries=%d", __func__, bind_tries);
	lua_pop(L,1);
	if (!bind_tries) bind_tries = 10;
	if (bind_tries < 1) bind_tries = 1;
	if (bind_tries > 100) bind_tries = 100;
	
	lua_getfield(L,-1,"bind_sleep");
	bind_sleep = (int) lua_tointeger(L, -1);
	// wtof("%s: bind_sleep=%d", __func__, bind_sleep);
	lua_pop(L,1);
	if (!bind_sleep) bind_sleep = 10;
	if (bind_sleep < 1) bind_sleep = 1;
	if (bind_sleep > 100) bind_sleep = 100;
	
	lua_getfield(L,-1,"listen_queue");
	listen_queue = (int) lua_tointeger(L, -1);
	// wtof("%s: listen=%d", __func__, listen_queue);
	lua_pop(L,1);
	if (!listen_queue) listen_queue = 5;
	if (listen_queue < 1) listen_queue = 1;
	if (listen_queue > 100) listen_queue = 100;
	
    /* create listener socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        /* failed */
		wtof("FTPD1002E setsockopt() failed, rc=%d, error=%d", sock, errno);
        goto quit;
    }

    /* bind socket to IP address and port */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = INADDR_ANY;
    serv_addr.sin_port          = htons(ftpd->port);

    rc = bind(sock, &serv_addr, sizeof(serv_addr));
    if (rc < 0) {
		int error = errno;
		wtof("FTPD0003E bind() failed for FTP port, rc=%d, error=%d", rc, errno);
        if (error==EADDRINUSE) for (i=0; i < bind_tries; i++) {
			wtof("FTPD0003I EADDRINUSE, waiting for TCPIP to release FTP port=%d", ftpd->port);
			sleep(bind_sleep);
			rc = bind(sock, &serv_addr, sizeof(serv_addr));
			if (rc >= 0) break;
			error = errno;
		}
		if (rc < 0) {
			wtof("FTPD0003E bind() failed for FTP port, rc=%d, error=%d", rc, errno);
			closesocket(sock);
			close_stale_port(ftpd->port);
			goto quit;
		}
    }

    /* listen for connection */
    rc = listen(sock,listen_queue);
    if (rc < 0) {
		wtof("FTPD0004E listen() failed, rc=%d, error=%d", rc, errno);
        closesocket(sock);
        goto quit;
    }

    /* success */
	ftpd->listen = sock;
	wtof("FTPD0005I Listening for FTP  request on port %d", ftpd->port);

    /* get our host IP address */
    if (httpd->httpt && httpd->httpt->mqtc) {
        /* we have to use a socket that is connected to obtain the IP address */
        addrlen = sizeof(serv_addr);
        rc = getsockname(httpd->httpt->mqtc->sock, (struct sockaddr*)&serv_addr, &addrlen);
        if (rc >= 0) {
            ftpd->addr = serv_addr.sin_addr.s_addr;
        }
    }
 
    /* Publish to MQTT Broker */
    a = (unsigned char *)&ftpd->addr;
    http_pubf(httpd, "thread/listener/ftp", 
        "{ \"address\" : \"%u.%u.%u.%u\" "
        ", \"port\" : %u "
        ", \"socket\" : %d "
        "}", 
        a[0], a[1], a[2], a[3], 
        ftpd->port, ftpd->listen);

	if (httpd->ufssys) {
		ftpd->sys = httpd->ufssys;
	}

	rc = 0;
	
quit:
	return rc;
}


static int process_httpd_settings(lua_State *L, HTTPD *httpd);
static int process_httpd_debug(lua_State *L, HTTPD *httpd);
static int process_httpd_bind(lua_State *L, HTTPD *httpd);
static int process_httpd_login(lua_State *L, HTTPD *httpd);
static int process_httpd_tzoffset(lua_State *L, HTTPD *httpd);
static int process_httpd_ftp(lua_State *L, HTTPD *httpd);
static int process_httpd_ufs(lua_State *L, HTTPD *httpd);

static int 
process_httpd(lua_State *L, HTTPD *httpd) 
{
	int			rc	 = 0;
	int			i;
	const char  *p;

	lua_getglobal(L, "httpd");	// push httpd table on top of stack

	rc = process_httpd_settings(L,httpd);
	if (rc) goto quit;
	rc = process_httpd_debug(L,httpd);
	if (rc) goto quit;
	rc = process_httpd_tzoffset(L, httpd);
	if (rc) goto quit;
	rc = process_httpd_login(L,httpd);
	if (rc) goto quit;
	rc = process_httpd_ftp(L, httpd);
	if (rc) goto quit;
	rc = process_httpd_ufs(L, httpd);
	if (rc) goto quit;
	rc = process_httpd_bind(L, httpd);
	if (rc) goto quit;

	/* try to open DD:HTTPSTAT */
	httpd->stats = fopen("DD:HTTPSTAT", "w");
	if (!httpd->stats) {
		wtof("HTTPD026W Unable to open DD:HTTPSTAT, errno=%d", errno);
	}

quit:
	lua_pop(L,1);	// pop httpd table off of stack
	return rc;
}

static int
process_httpd_ufs(lua_State *L, HTTPD *httpd)
{
	CLIBCRT 			*crt = __crtget();
	int					rc	 = 0;
	int					i;
    UFS                *ufs;

	lua_getfield(L,-1,"ufs");
	i = (int) lua_tointeger(L, -1);
	lua_pop(L,1);

	if (i<=0) goto quit;

    /* Stub system handle — signals UFS availability to FTPD */
    httpd->ufssys = ufs_sys_new();
    if (!httpd->ufssys) {
        wtof("HTTPD044W Unable to initialize file system");
        goto quit;
    }

    if (httpd->ftpd) httpd->ftpd->sys = httpd->ufssys;

    /* Open server-level UFS session to UFSD STC */
    httpd->ufs = ufs = ufsnew();
    if (!ufs) {
        wtof("HTTPD044W Unable to open UFSD session");
        ufs_sys_term(&httpd->ufssys);
        httpd->ufssys = NULL;
        goto quit;
    }

    wtof("HTTPD046I UFS session opened via UFSD subsystem");

    if (crt) crt->crtufs = ufs;

    /* Read document root prefix (optional) */
    lua_getfield(L, -1, "docroot");
    {
        const char *dr = lua_tostring(L, -1);
        if (dr && dr[0] == '/') {
            int drlen = strlen(dr);
            /* strip trailing slash */
            if (drlen > 1 && dr[drlen - 1] == '/')
                drlen--;
            if (drlen >= (int)sizeof(httpd->docroot))
                drlen = (int)sizeof(httpd->docroot) - 1;
            memcpy(httpd->docroot, dr, drlen);
            httpd->docroot[drlen] = '\0';
            wtof("HTTPD047I Document root: %s", httpd->docroot);
        }
    }
    lua_pop(L, 1);

quit:
	return rc;
}

static int 
process_httpd_ftp(lua_State *L, HTTPD *httpd)
{
	int					rc	 = 0;
	int					ftp;
    FTPD                *ftpd;

	lua_getfield(L,-1,"ftp");
	ftp = (int) lua_tointeger(L, -1);
	// wtof("%s: ftp=%d", __func__, ftp);
	lua_pop(L,1);

	if (!ftp) {
		lua_getfield(L,-1,"ftpd");
		ftp = (int) lua_tointeger(L, -1);
		// wtof("%s: ftp=%d", __func__, ftp);
		lua_pop(L,1);
	}

	if (ftp) {
		ftpd = (FTPD*) calloc(1, sizeof(FTPD));
		if (ftpd) {
			strcpy(ftpd->eye, FTPD_EYE);
			httpd->ftpd = ftpd;
		}
	}
	
	return rc;
}

static int 
process_httpd_tzoffset(lua_State *L, HTTPD *httpd)
{
	int					rc	 = 0;
	int					tzoffset;
    int                 hour;
    int                 min;
    int                 sec;
    int                 sign;
	const char			*p;
    HTTPT               *httpt = httpd->httpt;

	/* get the httpd.tzoffsetvalue */
	lua_getfield(L,-1,"tzoffset");
	tzoffset = (int) lua_tointeger(L, -1);
	// wtof("%s: tzoffset=%d", __func__, tzoffset);
	lua_pop(L,1);

    httpd->tzoffset = tzoffset * 60; /* convert to seconds */

	/* What we really want is to update the crt time zone
	** offset with this calculated time zone offset.
	*/
    __tzset(httpd->tzoffset);	/* set tz offset for crt */
    /* And just for fun, set the TZOFFSET environment
    ** variable just in case we need it later.
    */
    setenvi("TZOFFSET", httpd->tzoffset, 1);

    /* get time zone offset */
    if (!tzoffset) {
		p = getenvi("TZ");
		if (p) {
			int neg = 0;
			int	tmp;
			
			/* assume TZ is "[-]HH[:MM][:SS]" format */
	
			while (*p=='-') {
				neg = 1;
				p++;
			}

			if (isdigit(*p)) {
				httpd->tzoffset = 0;
				
				/* hours */
				tmp = atoi(p);
				httpd->tzoffset += tmp * 60 * 60;
				p = strchr(p, ':');

				if (p) {
					/* minutes */
					p++;
					tmp = atoi(p);
					httpd->tzoffset += tmp * 60;
					p = strchr(p, ':');
				}

				if (p) {
					/* seconds */
					p++;
					tmp = atoi(p);
					httpd->tzoffset += tmp;
				}

				if (neg) httpd->tzoffset = 0 - httpd->tzoffset;
				/* create a TZOFFSET environment variable */
                setenvi("TZOFFSET", httpd->tzoffset, 1);
			}
		}
	} 

    /* tell them what time zone offset we're using */
    sec = httpd->tzoffset;
    sign = httpd->tzoffset < 0 ? -1 : 1;
    /* calc hour, min, sec */
    sec *= sign;            /* make positive */
    hour = sec / 3600;      /* calc hour */
    sec -= hour * 3600;
    min = sec / 60;
    sec -= min * 60;
    wtof("HTTPD025I Time zone offset set to GMT %s%02d:%02d:%02d",
        sign < 0 ? "-":"+", hour, min, sec);

    http_pubf(httpd, "timezone", "GMT %s%02d:%02d:%02d",
        sign < 0 ? "-":"+", hour, min, sec);

	return rc;
}

static int 
process_httpd_bind(lua_State *L, HTTPD *httpd)
{
	int					rc	 = 0;
	int					i;
	int					bind_tries;
	int					bind_sleep;
	int					listen_queue;
	int					sock;
	int					port;
	const char			*p;
    int                 addrlen;
    struct sockaddr     addr;
    struct sockaddr_in  serv_addr;
    unsigned char       *a;

	/* get the httpd.port string value */
	lua_getfield(L,-1,"port");
	port = (int) lua_tointeger(L, -1);
	// wtof("%s: port=%d", __func__, port);
	lua_pop(L,1);

	if (port < 1 OR port > 65535) {
		wtof("HTTPD023E Invalid PORT value (%d)", port);
		rc = EINVAL;
		goto quit;
	}

    httpd->port = port;

	/* close sockets that are using this port */
	close_stale_port(port);

	lua_getfield(L,-1,"bind_tries");
	bind_tries = (int) lua_tointeger(L, -1);
	// wtof("%s: bind_tries=%d", __func__, bind_tries);
	lua_pop(L,1);
	if (bind_tries < 1) bind_tries = 1;
	if (bind_tries > 100) bind_tries = 100;
	
	lua_getfield(L,-1,"bind_sleep");
	bind_sleep = (int) lua_tointeger(L, -1);
	// wtof("%s: bind_sleep=%d", __func__, bind_sleep);
	lua_pop(L,1);
	if (bind_sleep < 1) bind_sleep = 1;
	if (bind_sleep > 100) bind_sleep = 100;
	
	lua_getfield(L,-1,"listen_queue");
	listen_queue = (int) lua_tointeger(L, -1);
	// wtof("%s: listen=%d", __func__, listen_queue);
	lua_pop(L,1);
	if (listen_queue < 1) listen_queue = 1;
	if (listen_queue > 100) listen_queue = 100;
	
    /* create listener socket */
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        /* failed */
        wtof("HTTPD028E socket() failed, rc=%d, error=%d", sock, errno);
        goto quit;
    }

    /* bind socket to IP address and port */
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family        = AF_INET;
    serv_addr.sin_addr.s_addr   = INADDR_ANY;
    serv_addr.sin_port          = htons(httpd->port);

    rc = bind(sock, &serv_addr, sizeof(serv_addr));
    if (rc < 0) {
		int error = errno;
        wtof("HTTPD030E bind() failed for HTTP port, rc=%d, error=%d", rc, errno);
        if (error==EADDRINUSE) for (i=0; i < bind_tries; i++) {
			wtof("HTTPD030I EADDRINUSE, waiting for TCPIP to release HTTP port=%d", httpd->port);
			sleep(bind_sleep);
			rc = bind(sock, &serv_addr, sizeof(serv_addr));
			if (rc >= 0) break;
			error = errno;
		}
		if (rc < 0) {
			wtof("HTTPD030E bind() failed for HTTP port, rc=%d, error=%d", rc, errno);
			closesocket(sock);
			close_stale_port(httpd->port);
			goto quit;
		}
    }

    /* listen for connection */
    rc = listen(sock,listen_queue);
    if (rc < 0) {
        wtof("HTTPD031E listen() failed, rc=%d, error=%d", rc, errno);
        closesocket(sock);
        goto quit;
    }

    /* success */
    wtof("HTTPD032I Listening for HTTP request on port %d", httpd->port);
    httpd->listen = sock;

    /* get our host IP address */
    if (httpd->httpt && httpd->httpt->mqtc) {
        /* we have to use a socket that is connected to obtain the IP address */
        addrlen = sizeof(serv_addr);
        rc = getsockname(httpd->httpt->mqtc->sock, (struct sockaddr*)&serv_addr, &addrlen);
        if (rc >= 0) {
            httpd->addr = serv_addr.sin_addr.s_addr;
        }
    }

    /* Publish to MQTT Broker */
    a = (unsigned char *)&httpd->addr;
    http_pubf(httpd, "thread/listener/http", 
        "{ \"address\" : \"%u.%u.%u.%u\" "
        ", \"port\" : %u "
        ", \"socket\" : %d "
        "}", 
        a[0], a[1], a[2], a[3], 
        httpd->port, httpd->listen);

	rc = 0;
	
quit:
	return rc;
}

static int 
process_httpd_login(lua_State *L, HTTPD *httpd)
{
	int			rc	 = 0;
	const char	*p;
	char		*next;

	/* get the httpd.login value */
	lua_getfield(L,-1,"login");
	p = luaL_checkstring(L, -1);
	// wtof("%s: login=\"%s\"", __func__, p);
	if (p) {
		char 	*tmp = strdup(p);
		if (tmp) {
			for(p = strtok(tmp, "(,)"); p ; p = strtok(next,",)")) {
				next = strtok(NULL, "");
				// wtof("%s p=\"%s\" next=%p", __func__, p, next);
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
					httpd->login = 0;
					continue;
				}
				/* not one of our LOGIN= values */
				wtof("HTTPD048W Invalid httpd.login=\"%s\" value", p);
				continue;
			}	/* for() */
			httpd048(httpd);
			free(tmp);
		}
	}
	lua_pop(L,1);

	return rc;
}

static int 
process_httpd_settings(lua_State *L, HTTPD *httpd) 
{
	int			rc	 = 0;
	int			i;
	const char  *p;

	lua_getfield(L,-1,"maxtask");
	i = (int) lua_tointeger(L, -1);
	if (i>0) {
		if (i > 255) i = 255;
		httpd->cfg_maxtask = (UCHAR)i;
	}
	else httpd->cfg_maxtask = 9;	// default
	lua_pop(L,1);

	lua_getfield(L,-1,"mintask");
	i = (int) lua_tointeger(L, -1);
	if (i>0) {
		if (i > 255) i = 255;
		httpd->cfg_mintask = (UCHAR)i;
	}
	else httpd->cfg_mintask = 3;	// default
	if (httpd->cfg_mintask > httpd->cfg_maxtask) {
		httpd->cfg_maxtask = httpd->cfg_mintask;
	}
	lua_pop(L,1);

	lua_getfield(L,-1,"client_timeout");
	i = (int) lua_tointeger(L, -1);
	if (i>0) {
		if (i > 255) i = 255;
		httpd->cfg_client_timeout = (UCHAR)i;
	}
	else httpd->cfg_client_timeout = 10;	// default
	lua_pop(L,1);

	lua_getfield(L,-1,"client_timeout_msg");
	i = (int) lua_tointeger(L, -1);
	if (i>0) httpd->client |= HTTPD_CLIENT_INMSG;
	else     httpd->client &= ~HTTPD_CLIENT_INMSG;
	lua_pop(L,1);

	lua_getfield(L,-1,"client_timeout_dump");
	i = (int) lua_tointeger(L, -1);
	if (i>0) httpd->client |= HTTPD_CLIENT_INDUMP;
	else     httpd->client &= ~HTTPD_CLIENT_INDUMP;
	lua_pop(L,1);
	
	lua_getfield(L,-1,"client_stats");
	i = (int) lua_tointeger(L, -1);
	if (i>0) httpd->client |= HTTPD_CLIENT_STATS;
	else     httpd->client &= ~HTTPD_CLIENT_STATS;
	lua_pop(L,1);
	
	lua_getfield(L,-1,"client_stats_month_max");
	i = (int) lua_tointeger(L, -1);
	if (i>0) {
		if (i > 255) i = 255;
		httpd->cfg_st_month_max = (UCHAR)i;
	}
	else httpd->cfg_st_month_max = 24;	// default
	lua_pop(L,1);

	lua_getfield(L,-1,"client_stats_day_max");
	i = (int) lua_tointeger(L, -1);
	if (i>0) {
		if (i > 255) i = 255;
		httpd->cfg_st_day_max = (UCHAR)i;
	}
	else httpd->cfg_st_day_max = 60;	// default
	lua_pop(L,1);

	lua_getfield(L,-1,"client_stats_hour_max");
	i = (int) lua_tointeger(L, -1);
	if (i>0) {
		if (i > 255) i = 255;
		httpd->cfg_st_hour_max = (UCHAR)i;
	}
	else httpd->cfg_st_hour_max = 48;	// default
	lua_pop(L,1);

	lua_getfield(L,-1,"client_stats_minute_max");
	i = (int) lua_tointeger(L, -1);
	if (i>0) {
		if (i > 255) i = 255;
		httpd->cfg_st_min_max = (UCHAR)i;
	}
	else httpd->cfg_st_min_max = 120;	// default
	lua_pop(L,1);

	lua_getfield(L,-1, "client_stats_dataset");
	p = lua_tostring(L, -1);
	if (p && *p) {
		httpd->st_dataset = strdup(p);
	}
	lua_pop(L,1);
	
	lua_getfield(L,-1, "cgilua_dataset");
	p = lua_tostring(L, -1);
	if (p && *p) {
		httpd->cgilua_dataset = strdup(p);
	}
	lua_pop(L,1);
	
	lua_getfield(L,-1, "cgilua_path");
	p = lua_tostring(L, -1);
	if (p && *p) {
		httpd->cgilua_path = strdup(p);
	}
	lua_pop(L,1);

	lua_getfield(L,-1, "cgilua_cpath");
	p = lua_tostring(L, -1);
	if (p && *p) {
		httpd->cgilua_cpath = strdup(p);
	}
	lua_pop(L,1);
    
    lua_getfield(L,-1, "cgi_context_pointers");
	i = (int) lua_tointeger(L, -1);
	if (i>=HTTPD_CGICTX_MIN) {  // 0
		if (i > HTTPD_CGICTX_MAX) i = HTTPD_CGICTX_MAX; // 255
		httpd->cfg_cgictx = (UCHAR)i;
	}
	else httpd->cfg_cgictx = HTTPD_CGICTX_MIN;	// default 0
	lua_pop(L,1);

	return rc;
}

static int 
process_httpd_debug(lua_State *L, HTTPD *httpd) 
{
	int			rc	 = 0;
	int			i;
	const char  *p;

	/* if we're not already in debug mode */
    if (!httpd->dbg) {
		/* get the httpd.debug string value */
		lua_getfield(L,-1,"debug");
		i = (int) lua_tointeger(L, -1);
		if (i>0) {
			httpd->dbg = fopen("DD:HTTPDBG", "w");
			if (!httpd->dbg) {
				wtof("HTTPD020E fopen for DD:HTTPDBG failed, error=%d", errno);
				wtof("HTTPD021I DEBUG/TRACE output will be to DD:SYSTERM");
				httpd->dbg = stderr;    /* STDERR = DD:SYSTERM */
			}
		}
		lua_pop(L,1);
	}
	
	return rc;
}

static int 
setLuaPath(lua_State* L, const char* path)
{
    CLIBCRT		*crt 	= __crtget();
	HTTPD		*httpd 	= crt->crtapp1;
	char 		*p;
	
	lua_getglobal( L, "package" );

	// wtof("%s: new path=\"%s\"", __func__, path);

    lua_pushstring( L, path ); // push the new one
    lua_setfield( L, -2, "path" ); // set the field "path" in table at -2 with value at top of stack
    lua_remove( L, -1 ); // get rid of package table from top of stack

    return 0; // all done!
}

static int
setHttpdDefaults(lua_State *L) 
{
    CLIBCRT		*crt 	= __crtget();
	HTTPD		*httpd 	= crt->crtapp1;

	// Create new table for "httpd"
	lua_newtable(L);			// create new table

	lua_pushstring(L, "0");
	lua_setfield(L,-2,"debug");		// table.debug="no"

	lua_pushstring(L,"8080");
	lua_setfield(L,-2,"port");		// table.port="8080"
	
	lua_pushstring(L,"none");
	lua_setfield(L,-2,"login");		// table.login="none"
	
	lua_pushinteger(L, 1);
	lua_setfield(L,-2,"ftp");		// table.ftp=1

	lua_pushinteger(L, 0);
	lua_setfield(L,-2,"tzoffset");	// table.tzoffset=0

	lua_pushstring(L, "5");
	lua_setfield(L,-2,"listen_queue");// table.listen_queue="5"

	lua_pushstring(L, "10");
	lua_setfield(L,-2,"bind_tries");// table.bind_tries="10"
	
	lua_pushstring(L, "10");
	lua_setfield(L,-2,"bind_sleep");// table.bind_sleep="10"
	
	lua_pushstring(L, "10");
	lua_setfield(L,-2,"client_timeout");// table.client_timeout="10"
	
	lua_pushstring(L, "1");
	lua_setfield(L,-2,"client_timeout_msg"); // table.client_timeout_msg="1"

	lua_pushstring(L, "1");
	lua_setfield(L,-2,"client_timeout_dump"); // table.client_timeout_dump="1"

	lua_pushstring(L, "1");
	lua_setfield(L,-2,"client_stats"); 	// table.client_stats="1"

	lua_pushstring(L, "24");
	lua_setfield(L,-2,"client_stats_month_max"); 	// table.client_stats_month_max="24"

	lua_pushstring(L, "60");
	lua_setfield(L,-2,"client_stats_day_max"); 	// table.client_stats_day_max="60"

	lua_pushstring(L, "48");
	lua_setfield(L,-2,"client_stats_hour_max"); 	// table.client_stats_hour_max="48"

	lua_pushstring(L, "120");
	lua_setfield(L,-2,"client_stats_minute_max"); 	// table.client_stats_minute_max="120"

	lua_pushstring(L, "");
	lua_setfield(L,-2,"client_stats_dataset");		// table.client_stats_dataset=""

	lua_pushstring(L, "3");	
	lua_setfield(L,-2,"mintask");	// table.mintask="3"
	
	lua_pushstring(L, "9");
	lua_setfield(L,-2,"maxtask");	// table.maxtask="9"
	
	lua_pushstring(L, "1");
	lua_setfield(L,-2,"ufs");		// table.ufs="1"

	lua_pushstring(L, "");
	lua_setfield(L,-2,"docroot");	// table.docroot=""
	
	lua_pushstring(L, "HTTPD.CGILUA");
	lua_setfield(L,-2,"cgilua_dataset");	// table.cgilua_dataset="HTTPD.CGILUA"

	lua_pushstring(L, "");
	lua_setfield(L,-2,"cgilua_path");	// table.cgilua_path=""

	lua_pushstring(L, "");
	lua_setfield(L,-2,"cgilua_cpath");	// table.cgilua_cpath=""
    
    lua_pushstring(L, "255");
    lua_setfield(L,-2,"cgi_context_pointers");  // table.cgi_context_pointers="255"

	lua_setglobal(L, "httpd");	// httpd = table

	// Create new table for httpd.cgi
	lua_newtable(L);			// create new table
	
	lua_pushstring(L,"/hello");
	lua_setfield(L,-2,"HELLO");		// table.HELLO="/hello"
	
	lua_pushstring(L, "/abend0c1");
	lua_setfield(L,-2,"ABENDOC1");	// table.ABEND0C1="/abend0c1"

	lua_pushstring(L, "/jes/*");
	lua_setfield(L,-2,"HTTPJES2");	// table.HTTPJES2="/jes/*"
	
	lua_pushstring(L, "/rexx/*");
	lua_setfield(L,-2,"HTTPREXX");	// table.HTTPREXX="/rexx/*"
		
	lua_pushstring(L, "/lua/*");
	lua_setfield(L,-2,"HTTPLUA");	// table.HTTPLUA="/lua/*"
    
    lua_pushstring(L, "/.dm");
    lua_setfield(L,-2,"HTTPDM");    // table.HTTPDM="/.dm"
	
    lua_pushstring(L, "/.dmtt");
    lua_setfield(L,-2,"HTTPDMTT");  // table.HTTPDMTT="/.dmtt"
	
    lua_pushstring(L, "/.dsrv");
    lua_setfield(L,-2,"HTTPDSRV");  // table.HTTPDSRV="/.dsrv"

    lua_pushstring(L, "/dsl/*");
    lua_setfield(L,-2,"HTTPDSL");   // table.HTTPDSL="/dsl/*"
	
	lua_setglobal(L, "cgi");	// cgi = table

	// Create new table for "ftpd"
	lua_newtable(L);			// create new table

	lua_pushstring(L, "8021");
	lua_setfield(L,-2,"port");		// table.port="8080"
	
	lua_pushstring(L, "5");
	lua_setfield(L,-2,"listen_queue");// table.listen_queue="5"

	lua_pushstring(L, "10");
	lua_setfield(L,-2,"bind_tries");// table.bind_tries="10"
	
	lua_pushstring(L, "10");
	lua_setfield(L,-2,"bind_sleep");// table.bind_sleep="10"

	lua_setglobal(L, "ftpd");	// ftpd = table

    // Create new table for "mqtt"
	lua_newtable(L);			// create new table

	lua_pushstring(L, "0");
	lua_setfield(L,-2,"start");		// table.start="0"

	lua_pushstring(L, "0");
	lua_setfield(L,-2,"datetime_gmt"); // table.datetime_gmt="0"

	lua_pushstring(L, "test.mosquitto.org");
	lua_setfield(L,-2,"broker_host");// table.broker_host=""
    
	lua_pushstring(L, "1883");
	lua_setfield(L,-2,"broker_port");// table.broker_port="1883"

	lua_pushstring(L, "");
	lua_setfield(L,-2,"prefix");	// table.prefix=""

	lua_pushstring(L, "30");
	lua_setfield(L,-2,"telemetry");	// table.telemetry="30"

	lua_setglobal(L, "mqtt");	// mqtt = table

}

static void 
dumpstack (lua_State *L, const char *funcname) 
{
    CLIBCRT		*crt 	= __crtget();
	HTTPD		*httpd 	= crt->crtapp1;
	int 		top		= lua_gettop(L);
	int			i;
	int			j;
	char		buf[256];
	
	wtof("%s Stack Dump (%d)", funcname, top);
	for (i=1, j=-top; i <= top; i++, j++) {
		const char *typename = luaL_typename(L,i);
		sprintf(buf, "%3d (%d) %.12s", i, j, luaL_typename(L,i));
		switch (lua_type(L, i)) {
			case LUA_TNUMBER:
				wtof("%s %g", buf, lua_tonumber(L,i));
				break;
			case LUA_TSTRING:
				wtof("%s %s", buf, lua_tostring(L,i));
				break;
			case LUA_TBOOLEAN:
				wtof("%s %s", buf, (lua_toboolean(L, i) ? "true" : "false"));
				break;
			case LUA_TNIL:
				wtof("%s %s", buf, "nil");
				break;
			default:
				wtof("%s %p", buf ,lua_topointer(L,i));
				break;
		}
	}
}

#if 0
static int 
http_lua_wto(lua_State *L) 
{
    CLIBCRT	*crt 		= __crtget();
	HTTPD	*httpd 		= crt->crtapp1;
	int 	top			= lua_gettop(L);  /* number of arguments */
	int i;
	int		pos			= 0;
	char 	buf[1024]	= "";

	for (i = 1; i <= top; i++) {  /* for each argument */
		const char *s = lua_tostring(L, i);  /* convert it to string */
		// wtof("%s: %3d \"%s\"", __func__, i, s);
		if (i > 1) { /* not the first element? */
			strcpy(&buf[pos], "\t");  /* add a tab before it */
			pos++;
		}
		strcpy(&buf[pos], s);  /* print it */
		// lua_pop(L, 1);  /* pop result */
	}
	wtof("%s", buf);

	lua_settop(L, 0);	/* clear the stack */
	return 0;	/* returns nothing on the stack */
}
#endif

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
