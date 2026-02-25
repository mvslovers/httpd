/* HTTPDM.C - CGI Program, Display Memory */
#include "httpd.h"

#define httpx   (httpd->httpx)

typedef struct options  OPTIONS;
struct options {
    char        *title;
	void		*mem;
	unsigned    len;
    int         size;
    int         data;
};

#if 0
static int getself(char *jobname, char *jobid);
#endif

static int display_memory(HTTPD *httpd, HTTPC *httpc, OPTIONS *options);
static int try_memory(HTTPD *httpd, HTTPC *httpc, OPTIONS *options);

int main(int argc, char **argv)
{
    int         rc      = 0;
    CLIBPPA     *ppa    = __ppaget();
    CLIBGRT     *grt    = __grtget();
    HTTPD       *httpd  = grt->grtapp1;
    HTTPC       *httpc  = grt->grtapp2;
    OPTIONS     options = {NULL,NULL,0,0,0};
    char        *title  = NULL;
    char        *memory = NULL;
    char        *length = NULL;
    char        *chunk  = NULL;
    char        *data   = NULL;

    if (!httpd) {
        wtof("This program %s must be called by the HTTPD web server%s", argv[0], "");
        /* TSO callers might not see a WTO message, so we send a STDOUT message too */
        printf("This program %s must be called by the HTTPD web server%s", argv[0], "\n");
        return 12;
    }

    /* Get the query variables from the URL */
    if (!title) title = http_get_env(httpc, "QUERY_TITLE");
    if (!title) title = http_get_env(httpc, "QUERY_T");
    if (!title) title = "";
    options.title = title;

    if (!memory) memory = http_get_env(httpc, "QUERY_MEMORY");
    if (!memory) memory = http_get_env(httpc, "QUERY_MEM");
    if (!memory) memory = http_get_env(httpc, "QUERY_M");
    if (!memory) memory = "0";
	options.mem = (void*) strtoul(memory, NULL, 16);
    
    /* sanity check */
    options.mem = (void*)((unsigned) options.mem & 0x00FFFFFF);

    if (!length) length = http_get_env(httpc, "QUERY_LENGTH");
    if (!length) length = http_get_env(httpc, "QUERY_LEN");
    if (!length) length = http_get_env(httpc, "QUERY_L");
    if (!length) length = "256";
	options.len  = strtoul(length, NULL, 0);

    /* sanity check */
	if (options.len <= 0) options.len = 256;
	if (options.len > 4096) options.len = 4096;

    if (!chunk) chunk = http_get_env(httpc, "QUERY_CHUNK");
    if (!chunk) chunk = http_get_env(httpc, "QUERY_SIZE");
    if (!chunk) chunk = http_get_env(httpc, "QUERY_C");
    if (!chunk) chunk = http_get_env(httpc, "QUERY_S");
    if (!chunk) chunk = "16";
    options.size = (int) strtoul(chunk, NULL, 0);

    /* sanity check */
    options.size = (options.size+3) & 0xFC; /* round to full word */
    if (options.size <= 0)  options.size = 16;
    if (options.size > 64)  options.size = 64;

    if (!data) data = http_get_env(httpc, "QUERY_DATA");
    if (!data) data = http_get_env(httpc, "QUERY_D");
    if (!data) data = "0";
    options.data = (int) strtoul(data, NULL, 0);

    display_memory(httpd, httpc, &options);

quit:
    return 0;
}

static int
display_memory(HTTPD *httpd, HTTPC *httpc, OPTIONS *options)
{
    int         rc      = 0;
	char		*mem    = options->mem;
    int         data    = options->data;
    int         len     = options->len;
    int         size    = options->size;

    if (rc = http_resp(httpc,200) < 0) goto quit;
    if (rc = http_printf(httpc, "Cache-Control: no-store\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "Content-Type: %s\r\n", data ? "text/plain" : "text/html") < 0) goto quit;
    if (rc = http_printf(httpc, "Access-Control-Allow-Origin: *\r\n") < 0) goto quit;
    if (rc = http_printf(httpc, "\r\n") < 0) goto quit;

    if (!data) {
        if (rc = http_printf(httpc, "<!DOCTYPE html>\n") < 0) goto quit;
        if (rc = http_printf(httpc, "<html>\n") < 0) goto quit;

        if (rc = http_printf(httpc, "<head>\n") < 0) goto quit;

        if (rc = http_printf(httpc, "<title>HTTPDM</title>\n") < 0) goto quit;

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

        http_printf(httpc, "<form>\n");
        if (options->title && options->title[0]) {
            http_printf(httpc, "<input type=\"hidden\" id=\"title\" name=\"title\" value=\"%s\">\n", options->title);
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
    }

	/* call try_memory() from ESTAE protected try() */
	try(try_memory, httpd, httpc, options);

    if (!data) {
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
    }
    
done:
    if (!data) {
        http_printf(httpc, "</pre>\n");
        http_printf(httpc, "</div>\n");
        http_printf(httpc, "</body>\n");
        http_printf(httpc, "</html>\n");
    }
	
quit:
	return 0;
}

static int 
try_memory(HTTPD *httpd, HTTPC *httpc, OPTIONS *options)
{
    int             size    = options->len;  /* memory size */
    int             chunk   = options->size; /* chunk size */
    int             pos     = 0;
    int             i, j;
    int             ie      = 0;
    int             x       = (chunk * 2) + ((chunk / 4) - 1);
    unsigned        addr    = 0;
    unsigned char   *area   = (unsigned char*)options->mem;
    unsigned char   e;
    char            eChar[256];

    if (options->title && options->title[0]) {
        http_printf(httpc, "Dump of %s %08X (%d byte chunks) (%d bytes total)\n",
            options->title, area, options->size, options->len);
    }
    else {
        http_printf(httpc, "Dump of %08X (%d byte chunks) (%d bytes total)\n",
            area, options->size, options->len);
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
        
        if (!options->data) {
            if ((i&3)==0) {
                addr = *(unsigned*)area;
                http_printf(httpc, "<a href=\"?m=%p&l=%d&c=%d&d=%d\">", 
                    addr, options->len, options->size, options->data);
            }
        }
        http_printf(httpc, "%02X", *area);
        pos += 2;

        if ((i & 3) == 3) {
            if (options->data) {
                http_printf(httpc, " ");
            }
            else {
                http_printf(httpc, "</a> ");
            }
            pos += 1;
        }
        
        e = isgraph(*area) ? *area : *area==' ' ? ' ' : '.';

        if (options->data) {
            ie += sprintf(&eChar[ie],"%c", e);
        }
        else {
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
        }

        area++;
        size--;

        if (size==0 && ((i&3)!=3)) {
            if (options->data) {
                http_printf(httpc, " ");
            }
            else {
                http_printf(httpc, "</a> ");
            }
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

#if 0
static int
getself(char *jobname, char *jobid)
{
    unsigned    *psa        = (unsigned*)0;
    unsigned    *tcb        = (unsigned*)psa[540/4];    /* A(current TCB) */
    unsigned    *jscb       = (unsigned*)tcb[180/4];    /* A(JSCB) */
    unsigned    *ssib       = (unsigned*)jscb[316/4];   /* A(SSIB) */

    const char  *name       = (const char*)tcb[12/4];   /* A(TIOT), but the job name is first 8 chars of TIOT so we cheat just a bit */
    const char  *id         = ((const char*)ssib) + 12; /* jobid is in SSIB at offset 12 */
    int         i;

    for(i=0; i < 8 && name[i] > ' '; i++) {
        jobname[i] = name[i];
    }
    jobname[i] = 0;

    for(i=0; i < 8 && id[i] >= ' '; i++) {
        jobid[i] = id[i];
        /* the job id may have space(s) which should be '0' */
        if (jobid[i]==' ') jobid[i] = '0';
    }
    jobid[i] = 0;

    return 0;
}
#endif
