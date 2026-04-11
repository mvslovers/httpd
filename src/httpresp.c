/* HTTPRESP.C
** HTTP response
** Transitions to next state as needed.
*/
#include "httpd.h"
#include "clibsmf.h"
#include "clibtiot.h"
#include "clibssib.h"

extern int
httpresp(HTTPC *httpc, int resp)
{
	UCHAR		*smfid  = (UCHAR*)__smfid();	/* 4 character SMF ID */
	UCHAR		*jobid  = (UCHAR*)__jobid();	/* 8 character JOB ID */
	UCHAR		*jobname = (UCHAR*)__jobname(); /* 8 character JOB NAME */
    int     	rc  = 0;
    UCHAR   	*p;
    UCHAR    	date[40];
    time64_t	now;

    http_enter("httpresp(%d)\n", resp);

    switch(resp) {
    case 200: p = "200 OK";                     break;
    case 201: p = "201 Created";                break;
    case 202: p = "202 Accepted";               break;
    case 204: p = "204 No Content";             break;
    case 301: p = "301 Moved Permanently";      break;
    case 302: p = "302 Moved Temporarily";      break;
	case 303: p = "303 See Other";				break;
    case 304: p = "304 Not Modified";           break;
    case 400: p = "400 Bad Request";            break;
    case 401: p = "401 Unauthorized";           break;
    case 403: p = "403 Forbidden";              break;
    case 404: p = "404 Not Found";              break;
    case 405: p = "405 Method Not Allowed";     break;
    case 409: p = "409 Conflict";               break;
    case 414: p = "414 URI Too Long";           break;
    case 500: p = "500 Internal Server Error";  break;
    case 507: p = "507 Insufficient Storage";   break;
    case 501: p = "501 Not Implemented";        break;
    case 502: p = "502 Bad Gateway";            break;
    case 503: p = "503 Service Unavailable";    break;
    default : p = "500 Internal Server Error";  break;
    }

    httpc->resp = resp;

    /* match response version to client request version */
    {
        UCHAR *ver = http_get_env(httpc, "REQUEST_VERSION");
        if (ver && http_cmp(ver, "HTTP/1.1") == 0) {
            rc = http_printf(httpc, "HTTP/1.1 %s\r\n", p);
        } else {
            rc = http_printf(httpc, "HTTP/1.0 %s\r\n", p);
        }
    }
    if (rc) goto quit;

	now = time64(NULL);
    http_date_rfc1123( now, date, sizeof(date) );
    rc = http_printf(httpc, "Date: %s\r\n", date );
    if (rc) goto quit;

    p = http_server_name(httpc->httpd);
    if (p) {
        rc = http_printf(httpc, "Server: %s\r\n", p );
        if (rc) goto quit;
    }
	if (jobname) {
		rc = http_printf(httpc, "Jobname: %-8.8s\r\n", jobname);
		if (rc) goto quit;
	}
	if (jobid) {
		rc = http_printf(httpc, "Jobid: %-8.8s\r\n", jobid);
		if (rc) goto quit;
	}
	if (smfid) {
		rc = http_printf(httpc, "Node: %-4.4s\r\n", smfid);
		if (rc) goto quit;
	}

    if (httpc->keepalive) {
        rc = http_printf(httpc, "Connection: keep-alive\r\n");
    } else {
        rc = http_printf(httpc, "Connection: close\r\n");
    }
    if (rc) goto quit;

quit:
    http_exit("httpresp(), rc=%d\n", rc);
    return rc;
}
