/* credidin.c - initialize credential id structure with default values */
#include "cred.h"

CREDID
credid_init(CREDID *id)
{
	CREDID	tmp;
	
	if (!id) id = &tmp;
	
	memset(id, 0x00, sizeof(CREDID));
	
	return *id;
}

#if 0
int main(int argc, char **argv)
{
	CREDID	id;
	
	credid_init(&id);
	
	wtodumpf(&id, sizeof(id), "CREDID");
	
	return 0;
}
#endif
