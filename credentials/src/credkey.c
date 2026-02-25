#include "cred.h"

/* We use the credkey() address as the WSA "key" with the high byte
 * set to 0xFF to prevent the allocated storage from being initialized 
 * with what the "key" points to.
 * 
 * Or to say it another way, we want __wsaget() to NOT copy the
 * key to the newly allocated storage area.
 */
#define OURKEY 	((void*)(0xFF000000 | (unsigned)credkey))

CREDKEY *credkey(void)
{
    CREDKEY *key = __wsaget(OURKEY, sizeof(CREDKEY));

    return key;
}
