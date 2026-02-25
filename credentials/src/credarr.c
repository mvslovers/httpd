#include "cred.h"

static CRED **wsakey = NULL;

CRED ***cred_array(void)
{
    CRED ***array = __wsaget(wsakey, sizeof(wsakey));
    
    return array;
}
