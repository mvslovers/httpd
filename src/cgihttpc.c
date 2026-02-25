#include "httpd.h"

HTTPC *cgihttpc(void)
{
	HTTPC 		*httpc  = NULL;
    CLIBCRT		*crt 	= __crtget();
    
    if (crt) {
		if (crt->crtapp2 && strcmp(crt->crtapp2, HTTPC_EYE)==0) {
			httpc = crt->crtapp2;
			goto quit;
		}
	}
	
	if (!httpc) {
		CLIBGRT *grt = __grtget();
		if (grt) {
			if (grt->grtapp2 && strcmp(grt->grtapp2, HTTPC_EYE)==0) {
				httpc = grt->grtapp2;
				goto quit;
			}
		}
	}

quit:	
#if 0
	if (!httpc) {
		wtof("%s: HTTPC not found", __func__);
		wto_traceback(NULL);
	}
#endif
	return httpc;
}
