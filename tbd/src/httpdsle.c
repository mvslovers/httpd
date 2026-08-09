#include "httpd.h"
#include "httpds.h"

int httpds_error(HTTPDS *ds, const char *fmt, ...) 
{
    int     rc  = 0; 
    va_list args;
    char    buf[2048];

    va_start( args, fmt );
    rc = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end( args );
    
    buf[sizeof(buf)-1] = 0;

    if (!httpc->resp) {
        http_resp(httpc, 501);  /* not implemented response code */
        http_printf(httpc, "Content: text/plain\r\n");
        http_printf(httpc, "\r\n");
        http_printf(httpc, "Error: %s\n", buf);
    }
    else {
        http_printf(httpc, "<pre>\n");
        http_printf(httpc, "Error: %s\n", buf);
        http_printf(httpc, "</pre>\n");
    }

    return rc;
}
