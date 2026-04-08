#include "httpd.h"

int
httpd048(HTTPD *httpd)
{
	int			rc 		= 0;
	char		buf[40] = "";

	if (!(httpd->login & HTTPD_LOGIN_ALL)) {
		wtof("HTTPD048I Login not required for any request");
		goto quit;
	}

	if ((httpd->login & HTTPD_LOGIN_ALL) == HTTPD_LOGIN_ALL) {
		strcpy(buf, "ALL");
	}
	else {
		if (httpd->login & HTTPD_LOGIN_CGI) {
			strcat(buf, buf[0] ? ",CGI" : "CGI");
		}
		if (httpd->login & HTTPD_LOGIN_GET) {
			strcat(buf, buf[0] ? ",GET" : "GET");
		}
		if (httpd->login & HTTPD_LOGIN_HEAD) {
			strcat(buf, buf[0] ? ",HEAD" : "HEAD");
		}
		if (httpd->login & HTTPD_LOGIN_POST) {
			strcat(buf, buf[0] ? ",POST" : "POST");
		}
	}
	wtof("HTTPD048I Login required for (%s) request", buf);

quit:
	return rc;
}
