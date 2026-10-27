/* HTTPDECO.C
** Decode an escaped buffer (a query or form value: "+" is a space)
*/
#include "httpd.h"
#include "httpdecx.h"

extern UCHAR *
httpdeco(UCHAR *str)
{
    /* The table is selected once per call, not once per escape.  It used to
    ** be the asc2ebc global, which was module storage and therefore unwritable
    ** on an APF/LNKLST fetch (#197). */
    return httpdecx(str, http_codepage()->atoe, 1);
}
