/* FTPDLIST.C */
#include "httpd.h"
#include "cliblist.h"
#include "clibary.h"

int
ftpdlist(FTPC *ftpc)
{
    /* use ftpslist() with brief set to false */
    return ftpslist(ftpc, 0);
}

