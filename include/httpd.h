#ifndef HTTPD_H
#define HTTPD_H
/*
** HTTP Daemon (server)
** Copyright (c) 2016, 2024 Mike Rayborn. All rights reserved.
*/

/* crent370 headers */
#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <time64.h>					/* 64 bit time prototypes		*/
#include <errno.h>
#include "clibos.h"                 /* __setsp()/__getmsp() subpool */
#include "clibppa.h"                /* C runtime program properties */
#include "clibcrt.h"                /* C runtime area for each task */
#include "clibenv.h"                /* C runtime environment vars   */
#include "clibstae.h"               /* C runtime recovery routines  */
#include "clibwto.h"                /* write to operator            */
#include "clibcib.h"                /* console information block    */
#include "clibthrd.h"               /* basic threads                */
#include "clibthdi.h"               /* thread management            */
#include "cliblink.h"               /* link to external program     */
#include "clibary.h"                /* dynamic arrays               */
#include "sha256.h"					/* SHA 256 function				*/
#include "clibb64.h"				/* base64 encode/decode			*/
#include "clibssib.h"				/* SSIB, __ssib(), __jobid()	*/
#include "clibtiot.h"				/* TIOT, __tiot(), __jobname()	*/
#include "clibsmf.h"                /* __smfid()                    */
#include "clibtry.h"                /* try(), tryrc()               */

/* ufs headers */
#include "libufs.h"                 /* UFS client stubs via UFSD STC */

/* our headers */
#include "socket.h"                 /* sockets via DYN75            */
#include "dbg.h"                    /* debugging goodies            */
#include "errors.h"                 /* the missing errno values     */
#include "racf.h"                   /* security environment         */
#include "types.h"                  /* UCHAR, USHRT, UINT, ULONG    */
#include "cred.h"					/* Credentials					*/
#include "httpxlat.h"               /* ASCII/EBCDIC translation     */
#include "httpbody.h"               /* request body classification  */
#include "httpstat.h"               /* status code -> status line   */
#include "httprlm.h"                /* HTTP_REALM_CFG_MAX (#193)    */

/* httpluax.h removed — HTTPLUA is now a separate project (mvslovers/httplua) */

typedef struct httpd    HTTPD;      /* HTTP Daemon (server)         */
typedef struct httpc    HTTPC;      /* HTTP Client                  */
typedef struct httpm    HTTPM;      /* HTTP Mime                    */
typedef struct httpx    HTTPX;      /* HTTP function vector         */
typedef struct httpv    HTTPV;      /* HTTP Variables               */
typedef struct httproute HTTPROUTE; /* One route (MOD= or LOC=)     */
typedef struct smf_httpd_request SMF_HTTPD_REQ;     /* SMF request  */
typedef struct smf_httpd_session SMF_HTTPD_SESS;    /* SMF session  */
typedef enum   cstate   CSTATE;     /* HTTP Client state            */
typedef enum   rdw      RDW;        /* RDW option                   */

/* The OR / BOR trigraph macros (??!??! and ??!) that used to live here are
** gone: they existed because '|' is not typable on a 3270 keyboard, but the
** sources are edited on the host now and '||' was already used on 64 lines
** across 21 files.  Keeping them cost 257 -Wall warnings -- every translation
** unit including this header -- for two remaining call sites. */

/* The ebc2asc / asc2ebc globals are gone (issue #197).  They were module
** storage that http_xlate_init() stored into, which abends S0C4 when the load
** module is fetched from an APF-authorized or LNKLST library.  Ask for the
** table instead:  const UCHAR *atoe = http_codepage()->atoe;  hoisted out of
** the loop that indexes it. */

struct httpd {
    char        eye[8];             /* 00 eye catcher               */
#define HTTPD_EYE   "*HTTPD*"       /* ...                          */
    HTTPX       *httpx;             /* 08 HTTP function vector      */
    HTTPC       **httpc;            /* 0C clients array             */

    unsigned    addr;               /* 10 our listener IP address   */
    int         port;               /* 14 our listener port         */
    int         listen;             /* 18 our listener socket       */
    const char  *cfg_realm;         /* 1C realm/server name (#193)  */
                                    /* ... always set after config  */

    FILE        *dbg;               /* 20 debug/trace output        */
    int         tzoffset;           /* 24 time zone offset in secs  */
    HTTPC       **busy;             /* 28 busy clients              */
    /* Frozen: httpcgi.h publishes offset 0x2C plus the QUIESCE and SHUTDOWN
    ** masks to CGI modules as an ABI commitment (issue #125).  Do not move
    ** the field or renumber those two bits.  The offset is asserted at
    ** compile time in src/httpx.c; the mask values cannot be cross-checked
    ** -- the two headers can never be included together -- so they are only
    ** kept in step by this note. */
    volatile UCHAR flag;            /* 2C processing flags          */
#define HTTPD_FLAG_INIT     0x01    /* ... initializing             */
#define HTTPD_FLAG_LISTENER 0x02    /* ... listener thread created  */
#define HTTPD_FLAG_READY    0x04    /* ... ready                    */
#define HTTPD_FLAG_CFGERR   0x08    /* ... fatal Parmlib error      */
#define HTTPD_FLAG_QUIESCE  0x40    /* ... don't accept new request */
#define HTTPD_FLAG_SHUTDOWN 0x80    /* ... shutdown now             */

	UCHAR		unused_2d;			/* 2D (was: the global LOGIN
									   bitmask, retired in #105 --
									   AUTH= per route is the only
									   authentication policy now) */

	volatile UCHAR client;			/* 2E client options			*/
#define HTTPD_CLIENT_INMSG	0x80	/* ... client timeout messagep 	*/
#define HTTPD_CLIENT_INDUMP	0x40	/* ... client timeout buf dump 	*/
#define HTTPD_CLIENT_STATS	0x20	/* ... client statistics desired*/

    UCHAR       unused;   			/* 2F unused                    */

    CTHDTASK    *socket_thread;     /* 30 socket_thread subtask     */
    CTHDMGR     *mgr;               /* 34 worker_thread manager     */
    char        rname[12];          /* 38 resource name             */
    HTTPROUTE   **route;            /* 44 route table               */
	time64_t 	uptime;				/* 48 Server startup time		*/
    void        *unused_50;         /* 50 (was: FTPD *ftpd)         */
    UFSSYS      *ufssys;            /* 54 Unix like file system     */
    void        *unused_58;         /* 58 (was: LUAX *luax)         */
    const char  *version;			/* 5C HTTPD Version				*/
    void        *unused_60;         /* 60 (was: lua_State *config)  */
    UCHAR		cfg_maxtask;		/* 64 config max task			*/
    UCHAR		cfg_mintask;		/* 65 config min task			*/
    UCHAR		cfg_client_timeout;	/* 66 client timeout seconds	*/
    UCHAR		smf_level;			/* 67 SMF recording level		*/
#define SMF_LEVEL_NONE   0			/* ... no SMF recording			*/
#define SMF_LEVEL_ERROR  1			/* ... only resp >= 400			*/
#define SMF_LEVEL_AUTH   2			/* ... auth events + errors		*/
#define SMF_LEVEL_ALL    3			/* ... every request + sessions	*/
    UCHAR		smf_type;			/* 68 SMF record type (def 243)	*/
    UCHAR		unused_69[2];		/* 69-6A available				*/
    UCHAR       cfg_cgictx;         /* 6B # CGI context pointers    */
    UCHAR       ufs_enabled;        /* 6C UFS filesystem enabled    */
    UCHAR       dbg_enabled;        /* 6D debug output enabled      */
    UCHAR       bind_tries;         /* 6E socket bind retry count   */
    UCHAR       bind_sleep;         /* 6F socket bind retry delay   */
	unsigned	total_requests;		/* 70 total HTTP requests		*/
    unsigned	total_errors;		/* 74 total error responses		*/
    unsigned	total_bytes_sent;	/* 78 total bytes sent			*/
    unsigned	active_connections;	/* 7C active client connections	*/
    const HTTPCP *xlate;            /* 80 codepage pair in effect   */
                                    /* ... (was: st_dataset) (#197) */
    void        *unused_84;         /* 84 (was: cgilua_dataset)     */
    ACEE        *stc_prev_acee;     /* 88 STC ACEE to put back at   */
                                    /* ... shutdown, NULL until the */
                                    /* ... switch took (#197)       */
    void        *unused_8C;         /* 8C (was: cgilua_cpath)       */
    UFS			*ufs;				/* 90 Unix "like" File System   */
    USHRT       cfg_session_maxage; /* 94 credential hard max-age   */
                                    /* ... (min), 0=off (#118)      */
    UCHAR       unused_96[2];       /* 96 (was: HTTPT *httpt)       */
    CTHDTASK    *self;              /* 98 HTTPD main thread         */
    void        **cgictx;           /* 9C array of CGI context ptrs */
#define HTTPD_CGICTX_MIN    0       /* ... minimum number of cgictx */
#define HTTPD_CGICTX_MAX    255     /* ... maximum number of cgictx */
    char        docroot[128];       /* A0 UFS document root prefix  */
    UCHAR       listen_queue;       /* 120 listen backlog           */
    UCHAR       unused_121[3];      /* 121 alignment padding        */
    char        codepage[16];       /* 124 codepage name            */
    UCHAR       cfg_keepalive_timeout; /* 134 keepalive idle secs   */
    UCHAR       cfg_keepalive_max;  /* 135 max reqs per connection  */
    USHRT       cfg_session_timeout; /* 136 credential idle TTL (min), 0=off */
    CREDKEY     *credkey;           /* 138 blowfish key ptr (issue #111) */
    CRED        ***credarr;         /* 13C credential array ptr (issue #113) */
    /* The settled Basic realm / server name (#193).  cfg_realm above points
    ** here once http_config() has settled it.  In the control block rather
    ** than in a file-scope buffer because the module's own storage is key 0
    ** when it is fetched from an APF-authorized or LNKLST library, and a
    ** key-8 store into it abends S0C4 (#197).  Inline rather than malloc'd so
    ** no allocation can fail on this path. */
    char        cfg_realm_val[HTTP_REALM_CFG_MAX + 1];  /* 140          */
};                                  /* 181, padded to 188 (392)     */

/* HTTP variables */
struct httpv {
    char    eye[8];                 /* 00 eye catcher               */
#define HTTPV_EYE   "*HTTPV*"       /* ...                          */
    char    *value;                 /* 08 => variable value         */
    char    name[2];                /* 0C "name, 0, value, 0"       */
};

enum cstate {
    CSTATE_IN=0,                    /* read request from client     */
    CSTATE_PARSE,                   /* parse request headers        */
    CSTATE_GET,                     /* process GET request          */
    CSTATE_HEAD,                    /* process HEAD request         */
    CSTATE_PUT,                     /* process PUT request          */
    CSTATE_POST,                    /* process POST request         */
    CSTATE_DELETE,                  /* process DELETE request       */
    CSTATE_DONE=10,                 /* processing complete          */
    CSTATE_REPORT,                  /* report results (log)         */
    CSTATE_RESET,                   /* reset client info            */
    CSTATE_CLOSE                    /* release client               */
};

/* no-progress send policy (issues #199, #201): a send reporting zero
   progress means the client's receive window is closed, not that the
   socket is dead -- pause and retry instead of busy-spinning a worker
   at 100% CPU, and give up once the budget below is exhausted */
#define SEND_STALL_MAX      100     /* consecutive no-progress sends*/
#define SEND_STALL_PAUSE    100000  /* microseconds between retries */

enum rdw {
    RDW_NONE=0,                     /* no RDW (default)             */
    RDW_B2,                         /* Big Endian 2 byte            */
    RDW_L2,                         /* Little Endian 2 byte         */
    RDW_B4,                         /* Big Endian 4 byte            */
    RDW_L4                          /* Little Endian 4 byte         */
};

/* each client is 4096 bytes */
struct httpc {
    char        eye[8];             /* 00 eye catcher               */
#define HTTPC_EYE   "*HTTPC*"       /* ...                          */
    HTTPD       *httpd;             /* 08 => HTTPD (server)         */
    HTTPV       **env;              /* 0C variables array           */

    short       state;              /* 10 current state (CSTATE)    */
    UCHAR       subtype;            /* 11 sub type (if any)         */
    UCHAR       substate;           /* 12 sub state (if any)        */
    int         socket;             /* 14 socket                    */
    unsigned    addr;               /* 18 client IP address         */
    int         port;               /* 1C client port               */

    FILE        *fp;                /* 20 file handle               */
    volatile unsigned len;          /* 24 length of data in buf     */
    volatile unsigned pos;          /* 28 position in buf           */
    unsigned    sent;               /* 2C bytes sent                */

    double      start;              /* 30 start time in seconds 	*/
    double      end;                /* 38 end time in seconds 		*/

    UCHAR       rdw;                /* 40 RDW option                */
    UCHAR       chunked;            /* 41 chunked transfer encoding */
    short       resp;               /* 42 response code             */
    CRED    	*cred;              /* 44 client credential      	*/
    UFS         *ufs;               /* 48 UFS handle                */
    UFSFILE     *ufp;               /* 4C UFS file pointer          */
    
    UCHAR		ssi;				/* 50 Server Side Include enable */
    UCHAR		ssilevel;			/* 51 SSI processing level		*/
#define SSI_LEVEL_MAX	10			/* ... max SSI processing levele*/
	UCHAR		content_length_set;	/* 52 Content-Length was sent	*/
	UCHAR		keepalive;			/* 53 keep-alive active			*/
	unsigned	connect_time;		/* 54 SMF time at connect 1/100s*/
	unsigned	total_bytes_sent;	/* 58 accum bytes all requests	*/
	unsigned	request_count;		/* 5C requests on this conn		*/

#define CBUFSIZE (0x1000-0x0060)    /* ... 4096-96 = 4000           */
    UCHAR       buf[CBUFSIZE];      /* 60 data buffer               */
                                    /* 1000                         */
};                                  /* 1000 (4096 bytes)            */

/* HTTP Mime */
struct httpm {
   UCHAR        *ext;               /* 00 File Extension            */
   UCHAR        *type;              /* 04 Mime type                 */
   int          binary;             /* 08 Binary flag               */
};                                  /* 0C (12 bytes)                */

/* One route: a MOD= entry with a program, or a LOC= static prefix without one
** (pgm == NULL).  Both carry the same auth policy, which is what stopped
** "CGI" being an honest name for this (#105). */
struct httproute {
    UCHAR       eye[8];             /* 00 Eye catcher for dumps     */
/* 7 characters, and it has to stay that way: httpacgi() sets this with
** strcpy(), so the NUL is the 8th byte of eye[8].  A longer name would run
** into wild at +08 and silently switch wildcard matching off.  Asserted at
** compile time in httpx.c. */
#define HTTPROUTE_EYE  "*ROUTE*"    /* ...                          */
    UCHAR       wild;               /* 08 '*' or '?' in path name   */
	UCHAR		unused_09;			/* 09 (was: login required --
									   retired with the global LOGIN
									   bitmask in #105; auth at +14
									   is the policy.  The slot stays
									   so auth does not move.)		*/
    USHRT       len;                /* 0A Path length               */
    char  		*path;              /* 0C Path name to match        */
    char  		*pgm;               /* 10 program (NULL = LOC route) */
    /* per-route auth policy -- append-only.  HTTPROUTE is heap-allocated and
       opaque to CGI modules (httpcgi.h), so appending fields is safe. */
    UCHAR       auth;               /* 14 AUTH mode (HTTP_AUTH_*)   */
    UCHAR       resattr;            /* 15 RACF attr (RACF_ATTR_*)   */
    UCHAR       unused_16;          /* 16 (was: reclaim -- #174 made
                                          the reclaim unconditional) */
    UCHAR       unused_17;          /* 17 padding (was implicit)    */
    char        *resclass;          /* 18 RACF class (NULL = none)  */
    char        *resname;           /* 1C RACF resource name        */
};									/* 20 (32 bytes)				*/

/* HTTPROUTE.auth -- per-route authentication mode, and since #105 the ONLY
   authentication policy: the global LOGIN bitmask that a route without AUTH=
   used to inherit is gone, and with it HTTP_AUTH_DEFAULT.

   NONE is deliberately 0, so the three ways a route can come into being all
   mean the same thing.  A Parmlib line without AUTH=, a calloc()'d route in
   httpacgi(), and a route registered by a module through the httpx vector
   (which has no auth argument at all) are then identically public.  A route
   is protected because it says so, never because the server was configured
   that way somewhere else -- that indirection is what #105 removed. */
#define HTTP_AUTH_NONE    0         /* no AUTH= / AUTH=NONE -> public       */
#define HTTP_AUTH_FORM    1         /* AUTH=FORM  -> HTML login form        */
#define HTTP_AUTH_BASIC   2         /* AUTH=BASIC -> 401 WWW-Authenticate   */
#define HTTP_AUTH_TOKEN   3         /* AUTH=TOKEN -> bare 401, no challenge */

/* AUTH= selects TWO things, and neither of them is the credential source:
   whether the route needs authentication at all, and how an unauthenticated
   request is challenged.  The source is not per-route -- resolve_credential()
   runs in httppc() BEFORE the route is even looked up, so every route already
   accepts every source (Sec-Token, LtpaToken2, Bearer, Basic).

   That is why the fourth mode is a challenge and not a source (#121).  TOKEN
   means "authentication required, but never advertise a challenge": the 401 is
   bare.  A WWW-Authenticate makes a browser pop its native credential dialog,
   and the Basic credentials it then caches outlive the token session and defeat
   token logout (#119) -- an API route must never trigger that, regardless of
   what the client sent.  #120's X-CSRF-ZOSMF-HEADER suppression does the same
   job by guessing from a request header; TOKEN is the server-declared form and
   does not consult that header at all. */

/* HTTP_CGI_SUBPOOL -- the heap subpool every CGI allocates from, and which
** httppcgi() releases in one FREEMAIN when the CGI abends, so the cost of an
** abend is bounded by the request instead of by the life of the address space
** (#154).  Since #174 this is unconditional -- the retired RECLAIM= keyword
** briefly made it per-route while the module fleet was audited.
**
** The storage contract for server modules is one sentence: malloc() is
** request storage; storage that must outlive the request is obtained with
** __getmsp(size, 0).  The subpool is per-task, so MVS also releases it when
** the worker TCB ends -- unpinned "persistent" storage does not merely risk
** the abend path, it cannot survive at all.
**
** The subpool is set by cgistart INSIDE the module, not around the LINK: a
** worker is a cthread and CTHREAD builds no CLIBPPA, so __setsp() there is a
** no-op (measured -- see test/mvs/tstsp.c).  The module's own @@CRT0 does own
** a PPA, and @@EXITA pops it on return, which is the bracket this needs.
**
** Range 1-127 is what problem state can obtain; 0 is the shared default and
** means "no subpool set" in the cgistart handshake, so it must never be the
** value here. */
#define HTTP_CGI_SUBPOOL  5         /* keep in sync with httpcgi.h          */

/* http_link() failure codes (#131).
**
** http_link() returns the linked module's own return code, or a negative
** value when the module did not run to completion.  Historically EVERY
** negative value was read as an abend code, so a module that could not be
** loaded at all -- __link() returns -1 on its LINK error-return path, no
** abend involved -- was reported to the client as "U0001 ABEND", sending the
** reader looking for a dump that does not exist.
**
** An abend code from ___try() is formatted 0x00sssuuu (clibtry.h), so it can
** never exceed 0x00FFFFFF.  Anything of greater magnitude is therefore free
** as a sentinel and cannot collide with a real abend, however the module
** failed.  httppcgi() maps these to their own text.
**
** Residual ambiguity, by design: a module that deliberately returns -1 is
** indistinguishable from one that could not be loaded, because __link()
** collapses both into *prc = -1.  Server modules return 0. */
#define HTTP_LINK_ENOLOAD (-0x01000001) /* module not found, LINK failed    */
#define HTTP_LINK_ENOPGM  (-0x01000002) /* route carries no program name    */
#define HTTP_LINK_EESTAE  (-0x01000003) /* __linkds could not set the ESTAE */

/* A module that ran to completion and returned a NEGATIVE rc of its own.
** It cannot be passed through raw: -5 from the module and the negated abend
** code of a U0005 are the same integer, and reporting the one as the other is
** the whole of #131.  The rc is folded into a reserved range instead, so
** httppcgi() can report what happened without claiming an abend.
**
** Only a module with its own __start can get here -- cgistart clamps a
** negative main() rc to 0, because a CGI reports failures through the HTTP
** response and R15 is not that channel.  The fold is bounded to the abend
** range on the way in, so it cannot overflow. */
#define HTTP_LINK_EPGMRC_BASE (-0x02000000)
#define HTTP_LINK_EPGMRC(rc)  (HTTP_LINK_EPGMRC_BASE + (rc))
#define HTTP_LINK_IS_PGMRC(v) ((v) <= HTTP_LINK_EPGMRC_BASE)
#define HTTP_LINK_PGMRC(v)    ((v) - HTTP_LINK_EPGMRC_BASE)

/* The configuration DD.  The STC PROC allocates it as &D(&M), so the member is
   a startup choice (S HTTPD,M=HTTPPRM1) -- see parmlib_name() in httpprm.c. */
#define HTTPD_PARMLIB_DD "HTTPPRM"

/* The identity the STC logs on to at startup, replacing the STC/STCGROUP one
   RAKF hands out -- which holds ALTER on every data set (issue #177).  These
   are defaults; S HTTPD,STCUSER=x,STCGROUP=y overrides them.  They come from
   the JCL PARM and not the Parmlib because the logon happens before the
   Parmlib member is opened; see stc_identity() in httpd.c. */
#define HTTPD_STC_USER   "HTTPD"
#define HTTPD_STC_GROUP  "USER"

/* SMF — HTTP records */
#define SMF_TYPE_HTTPD_DEFAULT 243
#define SMF_HTTPD_SUBTYPE_REQ  1	/* Request completed			*/
#define SMF_HTTPD_SUBTYPE_SESS 2	/* Session closed				*/

struct smf_httpd_request {
    SMF_HEADER      hdr;            /* 00 Standard SMF Header (18B) */
    char            subsys[8];      /* 12 Subsystem ID              */
    short           subtype;        /* 1A 1 = Request completed     */
    char            userid[8];      /* 1C RACF user (blank=none)    */
    unsigned        client_addr;    /* 24 Client IPv4 address       */
    unsigned        resp_code;      /* 28 HTTP status code          */
    unsigned        bytes_sent;     /* 2C Response bytes            */
    unsigned        duration_us;    /* 30 Request duration (us)     */
    char            method[8];      /* 34 GET/POST/PUT/DELETE       */
    char            uri[64];        /* 3C Request URI (truncated)   */
};                                  /* 7C (124 bytes)               */

struct smf_httpd_session {
    SMF_HEADER      hdr;            /* 00 Standard SMF Header (18B) */
    char            subsys[8];      /* 12 Subsystem ID              */
    short           subtype;        /* 1A 2 = Session closed        */
    char            userid[8];      /* 1C last RACF user            */
    unsigned        client_addr;    /* 24 Client IPv4 address       */
    unsigned        connect_time;   /* 28 Connect time (1/100s)     */
    unsigned        duration_us;    /* 2C Total session duration us */
    unsigned        request_count;  /* 30 Requests on connection    */
    unsigned        total_bytes;    /* 34 Total bytes sent          */
};                                  /* 38 (56 bytes)                */

/* HTTP function execution vector */
extern HTTPX    *httpx;             /* Global pointer to HTTPX      */
struct httpx {
    char        eye[8];             /* 00 eye catcher               */
#define HTTPX_EYE   "*HTTPX*"       /* ...                          */
    HTTPD       *httpd;             /* 08 => HTTPD (server)         */
    UCHAR       (*http_cgi_subpool)(HTTPC *);
                                    /* 0C heap subpool for this CGI */

    int         (*http_in)(HTTPC*); /* 10 read input from client    */
    int         (*http_parse)(HTTPC*);
                                    /* 14 parse input from client   */
    int         (*http_get)(HTTPC*);
                                    /* 18 process GET request       */
    int         (*http_head)(HTTPC*);
                                    /* 1C process GET request       */
    int         (*http_put)(HTTPC*);
                                    /* 20 process PUT request       */
    int         (*http_post)(HTTPC*);
                                    /* 24 process POST request     */
    int         (*http_done)(HTTPC*);
                                    /* 28 done with request         */
    int         (*http_report)(HTTPC*);
                                    /* 2C report results            */
    int         (*http_reset)(HTTPC*);
                                    /* 30 reset for next request    */
    int         (*http_close)(HTTPC*);
                                    /* 34 all done, close client    */
    int         (*http_send)(HTTPC*,const UCHAR *,int);
                                    /* 38 send data asis to client  */
    UCHAR *     (*http_decode)(UCHAR*);
                                    /* 3C decode escaped string     */
    int         (*http_del_env)(HTTPC *, const UCHAR *name);
                                    /* 40 delete variable           */
    unsigned    (*http_find_env)(HTTPC *, const UCHAR *name);
                                    /* 44 find variable             */
    int         (*http_set_env)(HTTPC *, const UCHAR *, const UCHAR *);
                                    /* 48 set variable              */
    HTTPV *     (*http_new_env)(const UCHAR *, const UCHAR *);
                                    /* 4C allocate new variable     */
    int         (*http_set_http_env)(HTTPC *, const UCHAR *, const UCHAR *);
                                    /* 50 set HTTP variable         */
    int         (*http_set_query_env)(HTTPC *, const UCHAR *, const UCHAR *);
                                    /* 54 set QUERY variable        */
    UCHAR *     (*http_get_env)(HTTPC *, const UCHAR *);
                                    /* 58 get variable value        */
    UCHAR *     (*http_date_rfc1123)( time64_t, UCHAR *, size_t);
                                    /* 5C format a RFC1123 date     */
    int         (*http_printv)(HTTPC *, const char *, va_list);
                                    /* 60 printv                    */
    int         (*http_printf)(HTTPC *,  const char *, ... );
                                    /* 64 printf                    */
    int         (*http_resp)(HTTPC *, int);
                                    /* 68 send response             */
    UCHAR *     (*http_server_name)(HTTPD *);
                                    /* 6C get server name           */
    int         (*http_resp_not_implemented)(HTTPC *);
                                    /* 70 501 not implemented       */
    UCHAR *     (*http_etoa)(UCHAR *, int);
                                    /* 74 convert EBCDIC to ASCII   */
    UCHAR *     (*http_atoe)(UCHAR *, int);
                                    /* 78 convert ASCII to EBCDIC   */
    void *      (*array_new)(unsigned);
                                    /* 7C allocate dynamic array    */
    int         (*array_add)(void *, void *);
                                    /* 80 add item to array         */
    int         (*array_addf)(void *, const char *, ... );
                                    /* 84 add formatted string      */
    unsigned    (*array_count)(void *);
                                    /* 88 count items in array      */
    int         (*array_free)(void *);
                                    /* 8C free dynamic array        */
    void *      (*array_get)(void *, unsigned);
                                    /* 90 get item by index         */
    unsigned    (*array_size)(void *);
                                    /* 94 size of array in items    */
    void *      (*array_del)(void *, unsigned);
                                    /* 98 delete item from array    */
    int         (*http_cmp)(const UCHAR *, const UCHAR *);
                                    /* 9C caseless string compare   */
    int         (*http_cmpn)(const UCHAR *, const UCHAR *, int);
                                    /* A0 caseless string compare n */
    int         (*http_dbgw)(const char *, int);
                                    /* A4 debug write               */
    int         (*http_dbgs)(const char *);
                                    /* A8 debug puts                */
    int         (*http_dbgf)(const char *fmt, ...);
                                    /* AC debug printf              */
    int         (*http_dump)(void *, int, const char *, ...);
                                    /* B0 debug hex dump            */
    int         (*http_enter)(const char *, ...);
                                    /* B4 debug enter function      */
    int         (*http_exit)(const char *, ...);
                                    /* B8 debug exit function       */
    double *    (*http_secs)(double *);
                                    /* BC current time as double    */
    const HTTPM *(*http_mime)(const UCHAR *);
                                    /* C0 get mime for document     */
    int         (*http_resp_internal_error)(HTTPC *);
                                    /* C4 resp internal server error*/
    int         (*http_resp_not_found)(HTTPC *, const UCHAR *);
                                    /* C8 resp not found            */
    FILE *      (*http_open)(HTTPC *, const UCHAR *, const HTTPM *);
                                    /* CC open path name            */
    int         (*http_send_binary)(HTTPC *);
                                    /* D0 send file binary          */
    int         (*http_read)(FILE *, UCHAR *, int, int);
                                    /* D4 read file/dataset         */
    int         (*http_send_file)(HTTPC *, int);
                                    /* D8 send file                 */
    int         (*http_send_text)(HTTPC *);
                                    /* DC send file text            */
    int         (*http_is_busy)(HTTPC *);
                                    /* E0 is client busy            */
    int         (*http_set_busy)(HTTPC *);
                                    /* E4 add client to busy array  */
    int         (*http_reset_busy)(HTTPC *);
                                    /* E8 remove client from busy   */
    int         (*http_process_clients)(void);
                                    /* EC process clients           */
    char *      (*http_ntoa)(struct in_addr in);
                                    /* F0 format network address    */
    int         (*http_console)(CIB *cib);
                                    /* F4 process console command   */
    int         (*http_process_client)(HTTPC *);
                                    /* F8 process a client          */
    int         (*http_link)(HTTPC *, const char *);
                                    /* FC link to external program  */
    HTTPROUTE *   (*http_find_route)(HTTPD *httpd, const char *path);
                                    /* 100 find route for path name   */
    HTTPROUTE *   (*http_add_route)(HTTPD *httpd, const char *pgm, const char *path, int login);
                                    /* 104 add route for pgm and path */
    int         (*http_process_route)(HTTPC *httpc, HTTPROUTE *route);
                                    /* 108 process CGI request      */
    void        *unused_10C;        /* 10C (was: mqtc_pub)          */
    unsigned char *(*http_xlate)(unsigned char *, int, const unsigned char *);
                                    /* 110 translate with explicit table */
    HTTPCP      *xlate_cp037;       /* 114 CP037 codepage pair          */
    HTTPCP      *xlate_1047;        /* 118 IBM-1047 codepage pair       */
    HTTPCP      *xlate_legacy;      /* 11C legacy hybrid codepage pair  */
    UFS *       (*http_get_ufs)(HTTPC *);
                                    /* 120 get/create UFS handle    */
    void *      (*http_cgictx_get)(HTTPD *, const char *, unsigned);
                                    /* 124 find/create CGI context  */
    UCHAR *     (*http_get_userid)(HTTPC *, UCHAR *out, unsigned outlen);
                                    /* 128 client userid into buf   */
    ACEE *      (*http_get_acee)(HTTPC *);
                                    /* 12C client RACF ACEE / NULL  */
    int         (*http_get_token)(HTTPC *, UCHAR *out, unsigned outlen);
                                    /* 130 copy session token, ret len */
    int         (*http_check_auth)(HTTPC *, const char *classname,
                                   const char *resource, int attr);
                                    /* 134 RACF resource check      */
    int         (*http_logout)(HTTPC *);
                                    /* 138 end client session       */
    UCHAR *     (*http_get_password)(HTTPC *, UCHAR *out, unsigned outlen);
                                    /* 13C client password into buf */
    const char *(*http_realm)(HTTPD *);
                                    /* 140 settled Basic realm (#193) */
};

extern int http_in(HTTPC*)                                              asm("HTTPIN");
extern int http_parse(HTTPC*)                                           asm("HTTPPARS");
extern int http_delete(HTTPC*)                                          asm("HTTPDEL");
extern int http_get(HTTPC*)                                             asm("HTTPGET");
extern int http_head(HTTPC*)                                            asm("HTTPHEAD");
extern int http_put(HTTPC*)                                             asm("HTTPPUT");
extern int http_post(HTTPC*)                                            asm("HTTPPOST");
extern int http_done(HTTPC*)                                            asm("HTTPDONE");
extern int http_report(HTTPC*)                                          asm("HTTPREPO");
extern int http_reset(HTTPC*)                                           asm("HTTPRESE");
extern int http_close(HTTPC*)                                           asm("HTTPCLOS");
extern int http_send(HTTPC*,const UCHAR *,int)                          asm("HTTPSEND");
extern UCHAR *http_decode(UCHAR*)                                       asm("HTTPDECO");
extern int http_del_env(HTTPC *, const UCHAR *name)                     asm("HTTPDENV");
extern unsigned http_find_env(HTTPC *, const UCHAR *name)               asm("HTTPFENV");
extern int http_set_env(HTTPC *, const UCHAR *, const UCHAR *)          asm("HTTPSENV");
extern HTTPV *http_new_env(const UCHAR *, const UCHAR *)                asm("HTTPNENV");
extern int http_set_http_env(HTTPC *, const UCHAR *, const UCHAR *)     asm("HTTPSHEN");
extern int http_set_query_env(HTTPC *, const UCHAR *, const UCHAR *)    asm("HTTPSQEN");
extern UCHAR *http_get_env(HTTPC *, const UCHAR *)                      asm("HTTPGENV");
extern UCHAR *http_date_rfc1123( time64_t, UCHAR *, size_t)             asm("HTTP1123");
extern int http_printv(HTTPC *, const char *, va_list)                  asm("HTTPPRTV");
extern int http_printf(HTTPC *,  const char *, ... )                    asm("HTTPPRTF");
extern int http_resp(HTTPC *, int)                                      asm("HTTPRESP");
extern UCHAR *http_server_name(HTTPD *)                                 asm("HTTPGSNA");
extern int http_resp_not_implemented(HTTPC *)                           asm("HTTPRNIM");
extern UCHAR *http_etoa(UCHAR *, int)                                   asm("HTTPETOA");
extern UCHAR *http_atoe(UCHAR *, int)                                   asm("HTTPATOE");
extern int http_cmp(const UCHAR *, const UCHAR *)                       asm("HTTPCMP");
extern int http_cmpn(const UCHAR *, const UCHAR *, int)                 asm("HTTPCMPN");
/* internal string-safety helpers (not CGI-facing, so no HTTPX vector) */
extern UCHAR *http_html_escape(UCHAR *, size_t, const UCHAR *)          asm("HTTPESC");
extern int http_safe_redirect(const UCHAR *)                           asm("HTTPSRDR");
/* upper case into a caller buffer, for the runtime-lower-case strings that
** reach a WTO (build stamp, libc370 version) -- see include/httpdmsg.h */
extern const char *http_upcase(char *, unsigned, const char *)          asm("HTTPUPCS");
/* what DD:HTTPPRM resolves to, e.g. "SYS2.PARMLIB(HTTPPRM0)" (httpprm.c);
** written once at startup and again for every F HTTPD,D CONFIG */
extern const char *parmlib_name(char *, size_t);
extern int http_dbgw(const char *, int)                                 asm("DBGW");
extern int http_dbgs(const char *)                                      asm("DBGS");
extern int http_dbgf(const char *fmt, ...)                              asm("DBGF");
extern int http_dump(void *, int, const char *, ...)                    asm("DBGDUMP");
extern int http_enter(const char *, ...)                                asm("DBGENTER");
extern int http_exit(const char *, ...)                                 asm("DBGEXIT");
extern double *http_secs(double *)                                      asm("HTTPSECS");
extern const HTTPM *http_mime(const UCHAR *)                            asm("HTTPMIME");
extern int http_resp_internal_error(HTTPC *)                            asm("HTTPRISE");
extern int http_resp_not_found(HTTPC *, const UCHAR *)                  asm("HTTPRNF");
extern FILE *http_open(HTTPC *, const UCHAR *, const HTTPM *)           asm("HTTPOPEN");
extern int http_send_binary(HTTPC *)                                    asm("HTTPSNDB");
extern int http_read(FILE *, UCHAR *, int, int)                         asm("HTTPREAD");
extern int http_send_file(HTTPC *, int)                         		asm("HTTPFILE");
extern int http_send_text(HTTPC *)                                      asm("HTTPSNDT");
extern int http_is_busy(HTTPC *)                                        asm("HTTPISB");
extern int http_set_busy(HTTPC *)                                       asm("HTTPSBZ");
extern int http_reset_busy(HTTPC *)                                     asm("HTTPRBZ");
extern int http_process_clients(void)                                   asm("HTTPPCS");
extern char *http_ntoa(struct in_addr in)                               asm("HTTPNTOA");
extern int http_console(CIB *cib)                                       asm("HTTPCONS");
extern int http_process_client(HTTPC *)                                 asm("HTTPPC");
extern int http_link(HTTPC *, const char *)                             asm("HTTPLINK");
extern HTTPROUTE *http_find_route(HTTPD *httpd, const char *path)       asm("HTTPFCGI");
extern UCHAR http_cgi_subpool(HTTPC *)                                  asm("HTTPCGSP");
extern HTTPROUTE *http_add_route(HTTPD *httpd, const char *pgm, const char *path, int login) asm("HTTPACGI");
extern int http_process_route(HTTPC *httpc, HTTPROUTE *route)           asm("HTTPPCGI");
extern unsigned char *http_xlate(unsigned char *, int, const unsigned char *) asm("HTTPXLAT");
extern UFS *http_get_ufs(HTTPC *)                                          asm("HTTPGUFS");
extern void *http_cgictx_get(HTTPD *, const char *, unsigned)              asm("HTTPGCTX");
extern UCHAR *http_get_userid(HTTPC *, UCHAR *out, unsigned outlen)         asm("HTTPGUID");
extern ACEE *http_get_acee(HTTPC *)                                        asm("HTTPGACE");
extern int http_get_token(HTTPC *, UCHAR *out, unsigned outlen)            asm("HTTPGTOK");
/* 0 == permitted (SAF rc 4, "resource not protected", is normalized to 0),
   8 and up == refused, -1 == the request is not authenticated.  Never test
   rc <= 4: that reads the -1 as allowed.  See src/httpxauth.c. */
extern int http_check_auth(HTTPC *, const char *classname,
                           const char *resource, int attr)                asm("HTTPCKAU");
extern int http_logout(HTTPC *)                                           asm("HTTPLOUT");
extern UCHAR *http_get_password(HTTPC *, UCHAR *out, unsigned outlen)      asm("HTTPGPWD");
extern const char *http_realm(HTTPD *)                                     asm("HTTPGRLM");
extern double httpsecs(double *psecs)									asm("HTTPSECS");
extern int httpcred(HTTPC *httpc)										asm("HTTPCRED");
extern int http_debug(HTTPC *httpc, const char *options)				asm("HTTPDBUG");
extern int http_config(HTTPD *httpd, const char *member)				asm("HTTPCONF");
extern void httpsmf(HTTPC *httpc)										asm("HTTPSMF");
extern void httpsmf_session(HTTPD *httpd, HTTPC *httpc)				asm("HTTPSMFS");
extern HTTPD *cgihttpd(void)											asm("CGIHTTPD");
extern HTTPC *cgihttpc(void)											asm("CGIHTTPC");
extern int http_getc(HTTPC *httpc)                                      asm("HTTPGETC");
extern int http_gets(HTTPC *httpc, UCHAR *buf, unsigned max)            asm("HTTPGETS");


#ifndef HTTP_PRIVATE
/* public calls via httpx function vector */
#define http_in(httpc) \
    ((httpx->http_in)((httpc)))

#define http_parse(httpc) \
    ((httpx->http_parse)((httpc)))

#define http_get(httpc) \
    ((httpx->http_get)((httpc)))

#define http_head(httpc) \
    ((httpx->http_head)((httpc)))

#define http_put(httpc) \
    ((httpx->http_put)((httpc)))

#define http_post(httpc) \
    ((httpx->http_post)((httpc)))

#define http_done(httpc) \
    ((httpx->http_done)((httpc)))

#define http_report(httpc) \
    ((httpx->http_report)((httpc)))

#define http_reset(httpc) \
    ((httpx->http_reset)((httpc)))

#define http_close(httpc) \
    ((httpx->http_close)((httpc)))

#define http_send(httpc,buf,len) \
    ((httpx->http_send)((httpc),(buf),(len)))

#define http_decode(str) \
    ((httpx->http_decode)((str)))

#define http_del_env(httpc,name) \
    ((httpx->http_del_env)((httpc),(name)))

#define http_find_env(httpc,name) \
    ((httpx->http_find_env)((httpc),(name)))

#define http_set_env(httpc,name,value) \
    ((httpx->http_set_env)((httpc),(name),(value)))

#define http_new_env(name,value) \
    ((httpx->http_new_env)((name),(value)))

#define http_set_http_env(httpc,name,value) \
    ((httpx->http_set_http_env)((httpc),(name),(value)))

#define http_set_query_env(httpc,name,value) \
    ((httpx->http_set_query_env)((httpc),(name),(value)))

#define http_get_env(httpc,name) \
    ((httpx->http_get_env)((httpc),(name)))

#define http_date_rfc1123(t,buf,size) \
    ((httpx->http_date_rfc1123)((t),(buf),(size)))

#define http_printv(httpc,fmt,args) \
    ((httpx->http_printv)((httpc),(fmt),(args)))

#define http_printf(httpc,fmt,...) \
    ((httpx->http_printf)((httpc),(fmt),## __VA_ARGS__))

#define http_resp(httpc,resp) \
    ((httpx->http_resp)((httpc),(resp)))

#define http_server_name(httpd) \
    ((httpx->http_server_name)((httpd)))

#define http_resp_not_implemented(httpc) \
    ((httpx->http_resp_not_implemented)((httpc)))

#define http_etoa(buf,len) \
    ((httpx->http_etoa)((buf),(len)))

#define http_atoe(buf,len) \
    ((httpx->http_atoe)((buf),(len)))

    /* allocate a new array of size elements */
#undef array_new
#define array_new(size) \
    ((httpx->array_new)((size)))

    /* add item to array */
#undef array_add
#define array_add(varray,vitem) \
    ((httpx->array_add)((varray),(vitem)))

    /* add formatted string to array */
#undef array_addf
#define array_addf(varray,fmt,...) \
    ((httpx->array_addf)((varray),(fmt),## __VA_ARGS__))

    /* return number of items in array */
#undef array_count
#define array_count(varray) \
    ((httpx->array_count)((varray)))

    /* free array storage, items are not freed (you need to free them first) */
#undef array_free
#define array_free(varray) \
    ((httpx->array_free)((varray)))

    /* return item (index) from array, index range 1 through count */
#undef array_get
#define array_get(varray,indx) \
    ((httpx->array_get)((varray),(indx)))

    /* return size of array in items */
#undef array_size
#define array_size(varray) \
    ((httpx->array_size)((varray)))

    /* delete item from array */
#undef array_del
#define array_del(varray,indx) \
    ((httpx->array_del)((varray),(indx)))

#define http_cmp(s1,s2) \
    ((httpx->http_cmp)((s1),(s2)))

#define http_cmpn(s1,s2,n) \
    ((httpx->http_cmpn)((s1),(s2),(n)))

#define http_dbgw(buf,len) \
    ((httpx->http_dbgw)((buf),(len)))

#define http_dbgs(str) \
    ((httpx->http_dbgs)((str)))

#define http_dbgf(fmt,...) \
    ((httpx->http_dbgf((fmt),## __VA_ARGS__)))

#define http_dump(buf,len,fmt,...) \
    ((httpx->http_dump)((buf),(len),(fmt),## __VA_ARGS__))

#define http_enter(fmt,...) \
    ((httpx->http_enter)((fmt),## __VA_ARGS__))

#define http_exit(fmt,...) \
    ((httpx->http_exit)((fmt),## __VA_ARGS__))

#define http_secs(secs) \
    ((httpx->http_secs)((secs)))

#define http_mime(path) \
    ((httpx->http_mime)((path)))

#define http_resp_internal_error(httpc) \
    ((httpx->http_resp_internal_error)((httpc)))

#define http_resp_not_found(httpc,path) \
    ((httpx->http_resp_not_found)((httpc),(path)))

#define http_open(httpc,path,mime) \
    ((httpx->http_open)((httpc),(path),(mime)))

#define http_send_binary(httpc) \
    ((httpx->http_send_binary)((httpc)))

#define http_read(fp,buf,size,rdw) \
    ((httpx->http_read)((fp),(buf),(size),(rdw)))

#define http_send_file(httpc,binary) \
    ((httpx->http_send_file)((httpc),(binary)))

#define http_send_text(httpc) \
    ((httpx->http_send_text)((httpc)))

#define http_is_busy(httpc) \
    ((httpx->http_is_busy)((httpc)))

#define http_set_busy(httpc) \
    ((httpx->http_set_busy)((httpc)))

#define http_reset_busy(httpc) \
    ((httpx->http_reset_busy)((httpc)))

#define http_process_clients() \
    ((httpx->http_process_clients)())

#define http_ntoa(addr) \
    ((httpx->http_ntoa)((addr)))

#define http_console(cib) \
    ((httpx->http_console)((cib)))

#define http_process_client(httpc) \
    ((httpx->http_process_client)((httpc)))

#define http_link(httpc,pgm) \
    ((httpx->http_link)((httpc),(pgm)))

#define http_find_route(httpd,path) \
    ((httpx->http_find_route)((httpd),(path)))

#define http_cgi_subpool(httpc) \
    ((httpx->http_cgi_subpool)((httpc)))

#define http_add_route(httpd,pgm,path,login) \
    ((httpx->http_add_route)((httpd),(pgm),(path),(login)))

#define http_process_route(httpc,route) \
    ((httpx->http_process_route)((httpc),(route)))

#define http_xlate(buf,len,tbl) \
    ((httpx->http_xlate)((buf),(len),(tbl)))

#define http_get_ufs(httpc) \
    ((httpx->http_get_ufs)((httpc)))

#define http_cgictx_get(httpd,eye,size) \
    ((httpx->http_cgictx_get)((httpd),(eye),(size)))

#define http_get_userid(httpc,out,outlen) \
    ((httpx->http_get_userid)((httpc),(out),(outlen)))

#define http_get_acee(httpc) \
    ((httpx->http_get_acee)((httpc)))

#define http_get_token(httpc,out,outlen) \
    ((httpx->http_get_token)((httpc),(out),(outlen)))

#define http_check_auth(httpc,classname,resource,attr) \
    ((httpx->http_check_auth)((httpc),(classname),(resource),(attr)))

#define http_logout(httpc) \
    ((httpx->http_logout)((httpc)))

#define http_get_password(httpc,out,outlen) \
    ((httpx->http_get_password)((httpc),(out),(outlen)))

#define http_realm(httpd) \
    ((httpx->http_realm)((httpd)))

#endif  /* ifndef HTTPX_H */

#endif
