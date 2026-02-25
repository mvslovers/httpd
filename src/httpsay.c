#include "httpd.h"
#include "clibppa.h"
#include "clibgrt.h"

/* MVS style parameter when called from BREXX for
 * ADDRESS LINKMVS "HTTPVARS var1 .. var15" 
 */
typedef struct parm {
	short		len;
	char		buf[1];
} PARM;

/* BREXX structs */
typedef struct efpl {
    void          *efplcom;                     /*  reserved                                */
    void          *efplbarg;                    /*  reserved                                */
    void          *efplearg;                    /*  reserved                                */
    void          *efplfb;                      /*  reserved                                */
    void          *efplarg;                     /*  Pointer to arguments table              */
    void          *efpleval;                    /*  Pointer to address of the EVALBLOCK     */
} EFPL;

typedef struct argtable_entry {
    void          *argtable_argstring_ptr;    /* Address of the argument string */
    int            argtable_argstring_length; /* Length of the argument string  */
} ATE;

typedef struct evalblock {
    int            evalblock_evpad1;            /*  reserved - set to binary zero           */
    int            evalblock_evsize;            /*  Size of EVALBLOCK in double words       */
    int            evalblock_evlen;             /*  Length of data                          */
    int            evalblock_evpad2;            /*  reserved - set to binary zero           */
    unsigned char  evalblock_evdata[1];         /*  Result                                  */
} EVAL;

#define MAXPARMS 15 /* maximum number of arguments we can handle */

extern int main(int argc, char **argv);
extern void __exita(int status);

static int process_say(HTTPD *httpd, HTTPC *httpc, const char *buf);

/* initialize to 0 to prevent linkage editor from trying to resolve httpx */
HTTPX *httpx = 0;

/* we want to use the httpx pointer in the httpd struct for the various
   http_xxx functions, so we define httpx to do just that
*/
#define httpx   (httpd->httpx)

/* NOTE: This code expects to be called from @@CRTM which is 
 * a minimal @@CRT0 implementation that does not create a
 * PPA area in the stack. This allows the @@PPAGET code to
 * locate the PPA of the parent so that the GRT area can be 
 * located and used for the HTTPD and HTTPC pointers.
 */
int
__start(void *r0, char *pgmname, int tsojbid, void **r1)
{
	int			rc			= 0;
    // CLIBPPA	*ppa    	= __ppaget();
    CLIBGRT     *grt    	= __grtget();
    // CLIBCRT 	*crt		= __crtget();
    HTTPD       *httpd  	= grt->grtapp1;
    HTTPC       *httpc  	= grt->grtapp2;
    PARM		*parm;
	EFPL		*efpl		= NULL;
	ATE			*ate		= NULL;	
	// EVAL		*eval		= NULL;
    char        *buf		= NULL;
    int			i;

#if 0
	wtof("%s: r0=0x%08X", __func__, r0);
	wtodumpf(r0, 32, "r0");
	wtof("%s: r1=0x%08X", __func__, r1);
	wtodumpf(r1, 32, "r1");
#endif

#if 1
	/* This code for ADDRESS LINKMVS "HTTPSAY var1 var2 ... var15" */
	if (r1) {
		for(parm = r1[0], i=0; i < MAXPARMS; parm = r1[++i]) {
			if (!parm) break;
			// wtodumpf(parm, 32, "parm[%d]", i);
			
			buf = calloc(1, parm->len + 1);
			if (buf) {
				memcpy(buf, parm->buf, parm->len);
				rc = process_say(httpd, httpc, buf);
				free(buf);
				buf = NULL;
			}

			if ((unsigned)parm & 0x80000000) break;
		}
	}

#else	
	/* this code for BREXX call HTTPSAY(parms,...) */
    if (r1) {
		efpl = (EFPL *)r1;

		ate = (ATE *)efpl->efplarg;
		for(i=0; i < MAXPARMS; i++, ate++) {
			if (!ate || ate->argtable_argstring_ptr == (void*)0xFFFFFFFF) break;
			
			buf = calloc(1, ate->argtable_argstring_length + 1);
			if (buf) {
				memcpy(buf, ate->argtable_argstring_ptr, ate->argtable_argstring_length);
				rc = process_say(httpd, httpc, buf);
				free(buf);
				buf = NULL;
			}
		}
	}
#endif

quit:
	if (buf) free(buf);
    __exit(rc);
    return (rc);
}

static int 
process_say(HTTPD *httpd, HTTPC *httpc, const char *buf)
{
	int			rc	= 0;
	const char	*p;
	
	if (!httpc->resp && __patmat(buf, "HTTP/?.? *")) {
		/* looks like a HTTP response header */
		char *tmp = strdup(buf);
		if (tmp) {
			char *p1 = strtok(tmp, " ");	/* HTTP/1.0 */
			char *p2 = strtok(NULL, " ");	/* nnn */
			char *p3 = strtok(NULL, "");	/* OK or whatever */
					
			if (p2) httpc->resp = atoi(p2);
			free(tmp);
		}
	}

	if (!httpc->resp) {
		/* we don't have a HTTP response */
		http_resp(httpc,200);
		for(p = buf; *p==' '; p++);
		if (p[0]=='<') {
			/* looks like a HTML markup tag */
			http_printf(httpc, "Content-Type: %s\r\n", "text/html");
		}
		else {
			/* anything else */
			http_printf(httpc, "Content-Type: %s\r\n", "text/plain");
		}
		http_printf(httpc, "\r\n");
	}

	http_printf(httpc, "%s\n", buf);
	
	return rc;
}
