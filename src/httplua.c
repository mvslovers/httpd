/* HTTPLUA.C - CGI Program, REST style CGI program to execute lua scripts */
#include "clibary.h"
#include "clibos.h"
#include "httpd.h"
#include "svc99.h"

#define httpx (httpd->httpx)

static int alloc_dummy(char *ddname);
static int alloc_temp(char *ddname, const char *tmpname);
static int free_alloc(const char *ddname);
static int process_stdout(HTTPD *httpd, HTTPC *httpc);
static int process_stderr(HTTPD *httpd, HTTPC *httpc, const char *script);
static int process_print(HTTPD *httpd, HTTPC *httpc, const char *buf);
static int main_lua(HTTPD *httpd, HTTPC *httpc, const char *script);
static void dumpstack(lua_State *L, const char *funcname);
static int setLuaPath(lua_State *L, const char *path);
static int setLuaCPath(lua_State *L, const char *path);
static char *getLuaPath(lua_State *L);

static int open_http(lua_State *L);

static int http_lua_publish(lua_State *L);

#define MAXPARMS 50 /* maximum number of arguments we can handle */

/* we want the internal label for __start as "cgistart" for use with dumps */
int __start(char *p, char *pgmname, int tsojbid, void **pgmr1) {
  CLIBGRT *grt = __grtget();
  HTTPD *httpd = NULL;
  HTTPC *httpc = NULL;
  char ddstdin[12] = "DD:xxxxxxxx";
  char ddstdout[12] = "DD:xxxxxxxx";
  char ddstderr[12] = "DD:xxxxxxxx";
  int x;
  int argc;
  unsigned u;
  char *argv[MAXPARMS + 1];
  int rc;
  int parmLen;
  int progLen;
  char parmbuf[310];

  /* we're going to process the callers parameter list first so we
     can decide is we'll bypass the opens for the permanent datasets.
  */
  if (pgmr1) {
    /* save the program parameter list values (max 10 pointers)
       note: the first pointer is always the raw EXEC PGM=...,PARM
       or CPPL (TSO) address.
    */
    for (x = 0; x < 10; x++) {
      u = (unsigned)pgmr1[x];
      /* add to array of pointers from caller */
      arrayadd(&grt->grtptrs, (void *)(u & 0x7FFFFFFF));

      if (u) {
        if (strcmp((void *)u, HTTPD_EYE) == 0) {
          /* this is a HTTPD pointer */
          httpd = (HTTPD *)(u & 0x7FFFFFFF);
          grt->grtapp1 = httpd;
        }
        if (strcmp((void *)u, HTTPC_EYE) == 0) {
          /* this is a HTTPC pointer */
          httpc = (HTTPC *)(u & 0x7FFFFFFF);
          grt->grtapp2 = httpc;
        }
      }

      if (u & 0x80000000)
        break; /* end of VL style address list */
    }
  }

  /* if we got a HTTPC and we didn't get a HTTPD,
     then use the HTTPD from the HTTPC handle */
  if (httpc && !httpd) {
    httpd = httpc->httpd;
    grt->grtapp1 = httpd;
  }

  /* need to know if this is a TSO environment straight away
     because it determines how the permanent files will be
     opened */
  parmLen = ((unsigned int)p[0] << 8) | (unsigned int)p[1];
  if ((parmLen > 0) && (p[2] == 0)) {
    grt->grtflag1 |= GRTFLAG1_TSO;
    progLen = (unsigned int)p[3];
  }

  rc = alloc_dummy(&ddstdin[3]);
  // wtof("%s: stdin %s", __func__, ddstdin);
  if (rc)
    goto quit;

  rc = alloc_temp(&ddstdout[3], "STDOUT");
  // wtof("%s: stdout %s", __func__, ddstdout);
  if (rc)
    goto quit;

  rc = alloc_temp(&ddstderr[3], "STDERR");
  // wtof("%s: stderr %s", __func__, ddstderr);
  if (rc)
    goto quit;

  stdout = fopen(ddstdout, "w");
  // wtof("%s: stdout 0x%08X", __func__, stdout);
  if (!stdout) {
    wtof("Unable to open STDOUT %s", ddstdout);
    goto quit;
  }

  stderr = fopen(ddstderr, "w");
  // wtof("%s: stderr 0x%08X", __func__, stderr);
  if (!stderr) {
    wtof("Unable to open STDERR %s", ddstderr);
    goto quit;
  }

  stdin = fopen(ddstdin, "r");
  // wtof("%s: stdin 0x%08X", __func__, stdin);
  if (!stdin) {
    stdin = fopen("'NULLFILE'", "r");
    // wtof("%s: stdin 0x%08X", __func__, stdin);
  }
  if (!stdin) {
    wtof("Unable to open STDIN %s", ddstdin);
    goto quit;
  }

  if (loadenv("dd:SYSENV")) {
    /* no SYSENV DD, try ENVIRON DD */
    loadenv("dd:ENVIRON");
  }

  /* initialize time zone offset for this thread */
  tzset();

  if (parmLen >= sizeof(parmbuf) - 2) {
    parmLen = sizeof(parmbuf) - 1 - 2;
  }
  if (parmLen < 0)
    parmLen = 0;

  /* We copy the parameter into our own area because
     the caller hasn't necessarily allocated room for
     a terminating NUL, nor is it necessarily correct
     to clobber the caller's area with NULs. */
  memset(parmbuf, 0, sizeof(parmbuf));
  if (grt->grtflag1 & GRTFLAG1_TSO) {
    parmLen -= 4;
    memcpy(parmbuf, p + 4, parmLen);
  } else {
    memcpy(parmbuf, p + 2, parmLen);
  }
  p = parmbuf;

  if (grt->grtflag1 & GRTFLAG1_TSO) {
    argv[0] = p;
    for (x = 0; x <= progLen; x++) {
      if (argv[0][x] == ' ') {
        argv[0][x] = 0;
        break;
      }
    }
    p += progLen;
  } else { /* batch or tso "call" */
    argv[0] = pgmname;
    pgmname[8] = '\0';
    pgmname = strchr(pgmname, ' ');
    if (pgmname)
      *pgmname = '\0';
  }

  while (*p == ' ')
    p++;

  x = 1;
  if (*p) {
    while (x < MAXPARMS) {
      char srch = ' ';

      if (*p == '"') {
        p++;
        srch = '"';
      }
      argv[x++] = p;
      p = strchr(p, srch);
      if (!p)
        break;

      *p = '\0';
      p++;
      /* skip trailing blanks */
      while (*p == ' ')
        p++;
      if (*p == '\0')
        break;
    }
  }
  argv[x] = NULL;
  argc = x;

  rc = main(argc, argv);

quit:
  if (stdin) {
    // wtof("%s: fclose(stdin)", __func__);
    fclose(stdin);
  }
  if (stdout) {
    // wtof("%s: fclose(stdout)", __func__);
    fclose(stdout);
  }
  if (stderr) {
    // wtof("%s: fclose(stderr)", __func__);
    fclose(stderr);
  }

  /* release allocations */
  free_alloc(&ddstdin[3]);
  free_alloc(&ddstdout[3]);
  free_alloc(&ddstderr[3]);

  __exit(rc);
  return (rc);
}

int main(int argc, char **argv) {
  int rc = 0;
  CLIBPPA *ppa = __ppaget();
  CLIBGRT *grt = __grtget();
  CLIBCRT *crt = __crtget();
  UFS *ufs = NULL;
  void *crtapp1 = NULL;
  void *crtapp2 = NULL;
  HTTPD *httpd = grt->grtapp1;
  HTTPC *httpc = grt->grtapp2;
  char *path = NULL;
  char *script = NULL;

  if (!httpd) {
    wtof("This program %s must be called by the HTTPD web server%s", argv[0],
         "");
    /* TSO callers might not see a WTO message, so we send a STDOUT message too
     */
    printf("This program %s must be called by the HTTPD web server%s", argv[0],
           "\n");
    return 12;
  }

  /* save for our exit/exterbnal programs */
  if (crt) {
    // wtof("httplua.c:%s: crt=%p crt->crtufs=%p", __func__, crt, crt->crtufs);
    crtapp1 = crt->crtapp1;
    crtapp2 = crt->crtapp2;
    ufs = crt->crtufs;
    crt->crtapp1 = httpd;
    crt->crtapp2 = httpc;
    crt->crtufs = httpc->ufs;
  }

  // wtof("%s: enter", argv[0]);

  // wtof("%s: ppa=0x%08X grt=0x%08X httpd=0x%08X httpc=0x%08X",
  // 	argv[0], ppa, grt, httpd, httpc);

  /* get the request path string */
  path = http_get_env(httpc, "REQUEST_PATH");
  script = strrchr(path, '/');

  // wtof("%s: path=\"%s\"", argv[0], path);
  // wtof("%s: script=\"%s\"", argv[0], path);

  if (script) {
    rc = main_lua(httpd, httpc, script);
  }

quit:
  if (crt) {
    /* restore crt values */
    crt->crtapp1 = crtapp1;
    crt->crtapp2 = crtapp2;
    crt->crtufs = ufs;
  }

  // wtof("%s: exit rc=%d", argv[0], rc);
  return rc;
}

static int readable(const char *filename) {
  CLIBCRT *crt = __getcrt();
  UFS *ufs = crt ? crt->crtufs : NULL;
  UFSFILE *ufp = NULL;
  FILE *f = NULL;

  // wtof("httplua.c:%s: enter \"%s\"", __func__, filename);

  if (ufs) {
    ufp = ufs_fopen(ufs, filename, "r");
    // wtof("httplua.c:%s: ufp=%p", __func__, ufp);
    if (ufp) {
      /* this filename exist and is readable */
      ufs_fclose(&ufp);
      goto okay;
    }
  }

  /* try opening as a dataset */
  f = fopen(filename, "r"); /* try to open file */
  // wtof("httplua.c:%s: f=%p", __func__, f);
  if (f == NULL)
    goto fail; /* open failed */
  fclose(f);
  goto okay;

fail:
  // wtof("httplua.c:%s: exit 0", __func__);
  return 0;

okay:
  // wtof("httplua.c:%s: exit 1", __func__);
  return 1;
}

static char *make_pathnames(const char *paths, const char *script) {
  char *pathname = NULL;
  int pathcount = 1;
  int scriptlen = strlen(script);
  int i;
  char *p;

  // wtof("httplua.c:%s: enter paths=\"%s\" script=\"%s\"", __func__, paths,
  // script);

  for (p = strchr(paths, ';'); p; p = strchr(p + 1, ';')) {
    pathcount++;
  }

  pathname = calloc(1, strlen(paths) + (scriptlen * pathcount));
  if (!pathname)
    goto quit;

  for (p = pathname; *paths; paths++) {
    *p = *paths;
    if (*p == '?') {
      strcpy(p, script);
      p += scriptlen;
    } else {
      p++;
    }
  }

quit:
  // wtof("httplua.c:%s: exit pathname=\"%s\"", __func__, pathname);
  return pathname;
}

static int main_lua(HTTPD *httpd, HTTPC *httpc, const char *script) {
  int rc = 0;
  lua_State *L = NULL;
  unsigned lines = 0;
  unsigned errors = 0;
  char *path = NULL;
  char dataset[256];

  // wtof("%s: enter script=\"%s\"", __func__, script);

  while (*script == '/')
    script++;

  /* create new Lua state */
  L = luaL_newstate();
  // wtof("%s: lua_State=0x%08X", __func__, L);

  // dumpstack(L, __func__ );

  /* Open Lua libraries */
  // wtof("%s: calling luaL_openlibs(L)", __func__);
  luaL_openlibs(L);

  // dumpstack(L, __func__ );

  /* Install http */
  // wtof("%s: calling luaL_requiref(L, \"http\", open_http, 1)", __func__);
  luaL_requiref(L, "http", open_http, 1);

  // dumpstack(L, __func__ );
  lua_settop(L, 0);

#if 0  /* debugging */
	path = getLuaPath(L);
	wtof("%s: path=\"%s\"", __func__, path);
	if (path) {
		free(path);
		path = NULL;
	}
#endif /* debugging */

  if (httpd->cgilua_path && httpd->cgilua_path[0]) {
    setLuaPath(L, httpd->cgilua_path);
  }

  if (httpd->cgilua_cpath && httpd->cgilua_cpath[0]) {
    setLuaCPath(L, httpd->cgilua_cpath);
  }

#if 0  /* debugging */
	path = getLuaPath(L);
	wtof("%s: path=\"%s\"", __func__, path);
	if (path) {
		free(path);
		path = NULL;
	}
#endif /* debugging */

  // wtof("%s: ready to process script=%s", __func__, script);

  /* Here we're formatting the dataset stirng with the script
   * member name to be found in the PDS allocated to the
   * LUALIB DD. This should prevent any unwanted execution
   * of Lua scripts from outside datasets.
   */
  rc = strlen(script);
  if (rc > 8)
    rc = 8;
  if (httpd->cgilua_dataset && httpd->cgilua_dataset[0]) {
    sprintf(dataset, "%s(%.*s)", httpd->cgilua_dataset, rc, script);
    if (readable(dataset))
      goto doit;
  }

  // wtof("%s: httpd->cgilua_path=\"%s\"", __func__, httpd->cgilua_path);
  if (httpd->cgilua_path && httpd->cgilua_path[0]) {
    char *pathnames = make_pathnames(httpd->cgilua_path, script);
    char *name;
    char *rest;

    if (!pathnames)
      goto dodd;
    // wtof("%s: pathnames=\"%s\"", __func__, pathnames);

    for (name = strtok(pathnames, ";"); name; name = strtok(rest, ";")) {
      rest = strtok(NULL, "");
      // wtof("%s: name=\"%s\" rest=\"%s\"", __func__, name, rest);
      if (readable(name)) {
        strcpy(dataset, name);
        free(pathnames);
        goto doit;
      }
    }
    free(pathnames);
  }

dodd:
  sprintf(dataset, "DD:CGILUA(%.*s)", rc, script);
  if (readable(dataset))
    goto doit;

  /* last chance, see if the request path exist */
  script = http_get_env(httpc, "REQUEST_PATH");
  if (script) {
    strcpy(dataset, script);
    if (readable(dataset))
      goto doit;
  }

  wtof("HTTPD417I CGI Lua script \"%s\" is not readable or does not exist",
       script);
  goto quit;

doit:
  // wtof("%s: calling luaL_dofile(L,\"%s\")", __func__, dataset);
  rc = luaL_dofile(L, dataset);
  if (rc) {
    const char *s = lua_tostring(L, 1);
    if (s && strstr(s, "cannot open") && strstr(s, "No Error")) {
      wtof("HTTPD417I CGI Lua script \"%s\" does not exist", dataset);
    } else {
      /* Something went wrong, dump stack to console */
      wtof("HTTPD417E Error in CGI Lua script \"%s\"", dataset);
      dumpstack(L, "HTTPD418E");
    }
  }

  // dumpstack(L, __func__ );

#if 0  /* debugging */
	luaL_dostring(L, "if http.print then http.oprint = print, print = http.print end");
	luaL_dostring(L, "for n in pairs(_G) do print(n) end");	
	luaL_dostring(L, "print('-------------------------------')");	
	luaL_dostring(L, "for n in pairs(http) do print(n) end");	
	luaL_dostring(L, "print('-------------------------------')");	
	luaL_dostring(L, "print('http.server_version ' .. http.server_version)");	
	luaL_dostring(L, "for n,v in pairs(http.vars) do print(n..'='..v) end");	
	luaL_dostring(L, "print('-------------------------------')");	
	luaL_dostring(L, "print(\"Hello\")");
#endif /* debugging */

  /* create response using STDOUT dataseet */
  // wtof("%s: calling process_stdout(httpd, httpc)", __func__);
  lines = process_stdout(httpd, httpc);

  /* process any errors found in STDERR dataset */
  // wtof("%s: calling process_stderr(httpd, httpc, script)", __func__);
  errors = process_stderr(httpd, httpc, script);

quit:
  if (path)
    free(path);

  // wtof("%s: calling lua_close(L)", __func__);
  if (L)
    lua_close(L);

  if (!httpc->resp) {
    if (errors) {
      http_resp(httpc, 503);
      http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
      http_printf(httpc, "\r\n");
      http_printf(httpc,
                  "One or more errors occurred processing your request\n");
    } else {
      http_resp(httpc, 200);
      http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
      http_printf(httpc, "\r\n");
      http_printf(httpc,
                  "Well this is weird, that Lua script \"%s\" didn't return "
                  "any output.\n",
                  script);
    }
  }

  httpc->state = CSTATE_DONE;
  // wtof("%s: exit rc=%d", __func__, rc);

  // wtof("%s: exit rc=%d", __func__, rc);
  return rc;
}

static int setLuaPath(lua_State *L, const char *path) {
  HTTPD *httpd = cgihttpd();

  lua_getglobal(L, "package");

  // wtof("%s: new path=\"%s\"", __func__, path);

  lua_pushstring(L, path); // push the new one
  lua_setfield(
      L, -2,
      "path"); // set the field "path" in table at -2 with value at top of stack
  lua_remove(L, -1); // get rid of package table from top of stack

  return 0; // all done!
}

static int setLuaCPath(lua_State *L, const char *path) {
  HTTPD *httpd = cgihttpd();

  lua_getglobal(L, "package");

  // wtof("%s: new path=\"%s\"", __func__, path);

  lua_pushstring(L, path);      // push the new one
  lua_setfield(L, -2, "cpath"); // set the field "path" in table at -2 with
                                // value at top of stack
  lua_remove(L, -1);            // get rid of package table from top of stack

  return 0; // all done!
}

static char *getLuaPath(lua_State *L) {
  HTTPD *httpd = cgihttpd();
  char *path;

  lua_getglobal(L, "package");
  lua_getfield(L, -1,
               "path"); // get field "path" from table at top of stack (-1)

  path = strdup(lua_tostring(L, -1)); // grab path string from top of stack
  // wtof("%s: path=\"%s\"", __func__, path);

  lua_pop(L, 1); // remove path string from stack
  lua_pop(L, 1); // remove package table from stack

  return path; // don't forget to free this when your done
}

static void dumpstack(lua_State *L, const char *funcname) {
  HTTPD *httpd = cgihttpd();
  int top = lua_gettop(L);
  int i;
  int j;
  char buf[256];

  wtof("%s Stack Dump (%d)", funcname, top);
  for (i = 1, j = -top; i <= top; i++, j++) {
    const char *typename = luaL_typename(L, i);
    sprintf(buf, "%3d (%d) %.12s", i, j, luaL_typename(L, i));
    switch (lua_type(L, i)) {
    case LUA_TNUMBER:
      wtof("%s %g", buf, lua_tonumber(L, i));
      break;
    case LUA_TSTRING:
      wtof("%s %s", buf, lua_tostring(L, i));
      break;
    case LUA_TBOOLEAN:
      wtof("%s %s", buf, (lua_toboolean(L, i) ? "true" : "false"));
      break;
    case LUA_TNIL:
      wtof("%s %s", buf, "nil");
      break;
    default:
      wtof("%s %p", buf, lua_topointer(L, i));
      break;
    }
  }
}

static int http_lua_print(lua_State *L) {
  HTTPD *httpd = cgihttpd();
  HTTPC *httpc = cgihttpc();
  int top = lua_gettop(L);
  int i;
  const char *p;
  char buf[80];

  // wtof("%s: enter top=%d", __func__, top);
  // wtof("%s: crt=%p httpd=%p httpc=%p", __func__, crt, httpd, httpc);

  for (i = 1; i <= top; i++) {
    if (i > 1)                           /* not the first element? */
      process_print(httpd, httpc, "\t"); /* add a tab before it */

    switch (lua_type(L, i)) {
    case LUA_TNUMBER:
      sprintf(buf, "%g", lua_tonumber(L, i));
      // wtof("%s: TNUMBER buf=\"%s\"", __func__, buf);
      process_print(httpd, httpc, buf);
      break;
    case LUA_TSTRING:
      p = lua_tostring(L, i);
      // wtof("%s: TSTRING p=\"%s\"", __func__, p);
      process_print(httpd, httpc, p);
      break;
    case LUA_TBOOLEAN:
      sprintf(buf, "%s", (lua_toboolean(L, i) ? "true" : "false"));
      // wtof("%s: TBOOLEAN buf=\"%s\"", __func__, buf);
      process_print(httpd, httpc, buf);
      break;
    case LUA_TNIL:
      p = "nil";
      // wtof("%s: TNIL p=\"%s\"", __func__, p);
      process_print(httpd, httpc, p);
      break;
    default:
      sprintf(buf, "%p", lua_topointer(L, i));
      // wtof("%s: default buf=\"%s\"", __func__, buf);
      process_print(httpd, httpc, buf);
      break;
    } /* switch */
    // lua_pop(L, 1);  /* pop result */
  } /* for */

  lua_settop(L, 0); /* clear stack */

#if 0
    CLIBCRT	*crt 		= __crtget();
	HTTPD	*httpd 		= crt->crtapp1;
	HTTPC	*httpc 		= crt->crtapp2;
	int 	top			= lua_gettop(L);  /* number of arguments */
	int i;

	for (i = 1; i <= top; i++) {  /* for each argument */
		const char *s = lua_tostring(L, i);  /* convert it to string */
		// wtof("%s: %3d \"%s\"", __func__, i, s);
		if (i > 1)  /* not the first element? */
			process_print(httpd, httpc, "\t");  /* add a tab before it */
		process_print(httpd, httpc, s);  /* print it */
		lua_pop(L, 1);  /* pop result */
	}
#endif

  process_print(httpd, httpc, "\n");
  // wtof("%s: exit", __func__);

  return 0;
}

static int create_lua_vars(lua_State *L) {
  HTTPD *httpd = cgihttpd();
  HTTPC *httpc = cgihttpc();
  unsigned count = array_count(&httpc->env);
  unsigned n;
  int i;
  char *p;
  char name[256];

  lua_createtable(L, 0, count);

  for (n = 0; n < count; n++) {
    HTTPV *env = httpc->env[n];

    if (!env)
      continue;

    for (i = 0; env->name[i]; i++) {
      p = &env->name[i];
      if (*p == '-') {
        name[i] = '_';
      } else {
        name[i] = *p;
        // name[i] = toupper(*p);
      }
    }
    name[i] = 0;

    lua_pushstring(L, env->value);
    lua_setfield(L, -2, name);
  }

  /* returning 1 table on top of stack */
  return 1;
}

static int open_http(lua_State *L) {
  HTTPD *httpd = cgihttpd();
  luaL_Reg reg[] = {
      {"print", http_lua_print},
      {"publish", http_lua_publish},
      {NULL, NULL},
  };

  // dumpstack(L, __func__ );

  luaL_newlib(L, reg);

  // dumpstack(L, __func__ );

  /* create version */
  lua_pushstring(L, httpd->version);
  lua_setfield(L, -2, "server_version");

  /* create vars table */
  create_lua_vars(L);
  lua_setfield(L, -2, "vars");

  // dumpstack(L, __func__ );
  return 1;
}

static int process_print(HTTPD *httpd, HTTPC *httpc, const char *buf) {
  int rc = 0;
  const char *p;

  if (!httpc->resp && __patmat(buf, "HTTP/?.? *")) {
    /* looks like a HTTP response header */
    char *tmp = strdup(buf);
    if (tmp) {
      char *p1 = strtok(tmp, " ");  /* HTTP/1.0 */
      char *p2 = strtok(NULL, " "); /* nnn */
      char *p3 = strtok(NULL, "");  /* OK or whatever */

      if (p2)
        httpc->resp = atoi(p2);
      free(tmp);
    }
  }

  if (!httpc->resp) {
    /* we don't have a HTTP response */
    http_resp(httpc, 200);
    for (p = buf; *p == ' '; p++)
      ;
    if (p[0] == '<') {
      /* looks like a HTML markup tag */
      http_printf(httpc, "Content-Type: %s\r\n", "text/html");
    } else {
      /* anything else */
      http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
    }
    http_printf(httpc, "\r\n");
  }

  rc = http_printf(httpc, "%s", buf);

  return rc;
}

static int process_stdout(HTTPD *httpd, HTTPC *httpc) {
  int lines = 0;
  FILE *fp = NULL;
  char *p = NULL;
  char buf[256];
  char ddstdout[12] = "DD:xxxxxxxx";

  if (!stdout)
    goto quit;

  strcpy(&ddstdout[3], stdout->ddname);
  fclose(stdout);
  stdout = NULL;

  /* create response */
  fp = fopen(ddstdout, "r");
  // wtof("%s: fopen(\"DD:STDOUT\",\"r\") fp=0x%08X", __func__, fp);
  if (!fp)
    goto quit;

  while (p = fgets(buf, sizeof(buf), fp)) {
    if (!lines) {
      if (!httpc->resp && __patmat(buf, "HTTP/?.? *")) {
        /* looks like a HTTP response header */
        char *tmp = strdup(buf);
        if (tmp) {
          char *p1 = strtok(tmp, " ");  /* HTTP/1.0 */
          char *p2 = strtok(NULL, " "); /* nnn */
          char *p3 = strtok(NULL, "");  /* OK or whatever */

          if (p2)
            httpc->resp = atoi(p2);
          free(tmp);
        }
      }

      if (!httpc->resp) {
        /* we don't have a HTTP response */
        http_resp(httpc, 200);
        while (*p == ' ')
          p++;
        if (p[0] == '<') {
          /* looks like a HTML markup tag */
          http_printf(httpc, "Content-Type: %s\r\n", "text/html");
        } else {
          /* anything else */
          http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
        }
        http_printf(httpc, "\r\n");
      }
    }
    http_printf(httpc, "%s", buf);
    lines++;
  }

quit:
  if (fp)
    fclose(fp);

  return lines;
}

static int process_stderr(HTTPD *httpd, HTTPC *httpc, const char *script) {
  int errors = 0;
  FILE *fp = NULL;
  char *p = NULL;
  char buf[256];
  char ddstderr[12] = "DD:xxxxxxxx";

  if (!stderr)
    goto quit;

  strcpy(&ddstderr[3], stderr->ddname);
  fclose(stderr);
  stderr = NULL;

  fp = fopen(ddstderr, "r");
  // wtof("%s: fopen(\"DD:STDERR\",\"r\") fp=0x%08X", __func__, fp);
  if (!fp)
    goto quit;

  while (p = fgets(buf, sizeof(buf), fp)) {
    if (!errors) {
      wtof("HTTPD500I CGI Program HTTPLUA script \"%s\"", script);
    }
    // wtodumpf(buf, strlen(buf), "%s: stderr", __func__);
    /* strip trailing newline */
    p = strrchr(buf, '\n');
    if (p)
      *p = 0;

    wtof("HTTPD501I %s", buf);
    errors++;
  }

quit:
  if (fp)
    fclose(fp);
  return errors;
}

static int free_alloc(const char *ddname) {
  int err = 1;
  unsigned count = 0;
  TXT99 **txt99 = NULL;
  RB99 rb99 = {0};

  // wtof("%s: enter ddname=\"%s\"", __func__, ddname);

  /* we want to unallocate the DDNAME */
  err = __txddn(&txt99, ddname);
  if (err)
    goto quit;

  count = arraycount(&txt99);
  if (!count)
    goto quit;

  /* Set high order bit to mark end of list */
  count--;
  txt99[count] = (TXT99 *)((unsigned)txt99[count] | 0x80000000);

  /* construct the request block for dynamic allocation */
  rb99.len = sizeof(RB99);
  rb99.request = S99VRBUN;
  rb99.flag1 = S99NOCNV;
  rb99.txtptr = txt99;

  /* SVC 99 */
  err = __svc99(&rb99);
  if (err) {
    // wtof("%s: err=%d error=0x%04X info=0x%04X", __func__, err, rb99.error,
    // rb99.info);
    goto quit;
  }

quit:
  if (txt99)
    FreeTXT99Array(&txt99);

  // wtof("%s: exit rc=%d", __func__, err);
  return err;
}

static int alloc_temp(char *ddname, const char *tmpname) {
  int err = 1;
  unsigned count = 0;
  TXT99 **txt99 = NULL;
  RB99 rb99 = {0};
  char tempname[40];

  // wtof("%s: enter ddname=\"%s\"", __func__, ddname);
  if (!tmpname)
    tmpname = "TEMP";
  snprintf(tempname, sizeof(tempname), "&%s", tmpname);
  tempname[sizeof(tempname) - 1] = 0;

  /* we want the DDNAME returned to us */
  err = __txrddn(&txt99, NULL);
  if (err)
    goto quit;

  /* allocate this dataset */
  err = __txdsn(&txt99, tempname);
  if (err)
    goto quit;

  /* DISP=NEW */
  err = __txnew(&txt99, NULL);
  if (err)
    goto quit;

  /* BLKSIZE=27998 */
  err = __txbksz(&txt99, "27998");
  if (err)
    goto quit;

  /* LRECL=255 */
  err = __txlrec(&txt99, "255");
  if (err)
    goto quit;

  /* SPACE=...(pri,sec) */
  err = __txspac(&txt99, "15,15");
  if (err)
    goto quit;

  /* SPACE=CYLS */
  err = __txcyl(&txt99, NULL);
  if (err)
    goto quit;

  /* RECFM=VB */
  err = __txrecf(&txt99, "VB");
  if (err)
    goto quit;

  /* DSORG=PS */
  err = __txorg(&txt99, "PS");
  if (err)
    goto quit;

  count = arraycount(&txt99);
  if (!count)
    goto quit;

  /* Set high order bit to mark end of list */
  count--;
  txt99[count] = (TXT99 *)((unsigned)txt99[count] | 0x80000000);

  /* construct the request block for dynamic allocation */
  rb99.len = sizeof(RB99);
  rb99.request = S99VRBAL;
  rb99.flag1 = S99NOCNV;
  rb99.txtptr = txt99;

  /* SVC 99 */
  err = __svc99(&rb99);
  if (err)
    goto quit;

  /* return DDNAME */
  memcpy(ddname, txt99[0]->text, 8);

quit:
  if (txt99)
    FreeTXT99Array(&txt99);

  // wtof("%s: exit rc=%d", __func__, err);
  return err;
}

static int alloc_dummy(char *ddname) {
  int err = 1;
  unsigned count = 0;
  TXT99 **txt99 = NULL;
  RB99 rb99 = {0};

  // wtof("%s: enter ddname=\"%s\"", __func__, ddname);

  /* we want the DDNAME returned to us */
  err = __txrddn(&txt99, NULL);
  if (err)
    goto quit;

  /* allocate dummy dataset */
  err = __txdmy(&txt99, NULL);
  if (err)
    goto quit;

  count = arraycount(&txt99);
  if (!count)
    goto quit;

  /* Set high order bit to mark end of list */
  count--;
  txt99[count] = (TXT99 *)((unsigned)txt99[count] | 0x80000000);

  /* construct the request block for dynamic allocation */
  rb99.len = sizeof(RB99);
  rb99.request = S99VRBAL;
  rb99.flag1 = S99NOCNV;
  rb99.txtptr = txt99;

  /* SVC 99 */
  err = __svc99(&rb99);
  if (err)
    goto quit;

  /* return DDNAME */
  memcpy(ddname, txt99[0]->text, 8);

quit:
  if (txt99)
    FreeTXT99Array(&txt99);

  // wtof("%s: exit rc=%d", __func__, err);
  return err;
}

static int http_lua_publish(lua_State *L) {
  HTTPD *httpd = cgihttpd();
  HTTPT *httpt;
  MQTC *mqtc;
  int rc;
  const char *topic = luaL_checkstring(L, 1);
  const char *data = luaL_optstring(L, 2, "");

  if (!httpd)
    goto quit;
  if (!httpd->httpt)
    goto quit;
  if (!httpd->httpt->mqtc)
    goto quit;

  if (!topic)
    goto quit;
  if (!data)
    goto quit;

  httpt = httpd->httpt;
  mqtc = httpt->mqtc;

  if (mqtc->flags & MQTC_FLAG_RECONNECT) {
    /* prevent attempts to publish while the MQTT client is in error state */
    goto quit;
  }

  rc = mqtc_pub(mqtc, 0, 1, topic, data);

  /* check for errors */
  if (rc) {
    lua_pushinteger(L, 8);
    lua_pushfstring(L, "publish: client error: %d", rc);
    return 2;
  }

  lua_pushinteger(L, 0);
  lua_pushstring(L, "publish: okay");
  return 2;

quit:
  lua_pushinteger(L, 4);
  lua_pushstring(L, "publish: not available");
  return 2;
}
