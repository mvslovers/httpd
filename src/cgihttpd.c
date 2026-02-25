#include "httpd.h"

HTTPD *cgihttpd(void)
{
	HTTPD 		*httpd  = NULL;
    CLIBCRT		*crt 	= __crtget();
    
    if (crt) {
		if (crt->crtapp1 && strcmp(crt->crtapp1, HTTPD_EYE)==0) {
			httpd = crt->crtapp1;
			goto quit;
		}
	}

	if (!httpd) {
		CLIBGRT *grt = __grtget();
		
		if (grt) {
			if (grt->grtapp1 && strcmp(grt->grtapp1, HTTPD_EYE)==0) {
				httpd = grt->grtapp1;
				goto quit;
			}
		}
	}

quit:	
	if (!httpd) {
		wtof("%s: HTTPD not found", __func__);
		wto_traceback(NULL);
	}
	return httpd;
}
