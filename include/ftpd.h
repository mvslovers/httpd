#ifndef FTPD_H
#define FTPD_H
/*
** FTP Daemon (server)
** Copyright (c) 2017 Mike Rayborn. All rights reserved.
*/
#include <stddef.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <time.h>
#include <errno.h>
#include "clibcrt.h"                /* C runtime area for each task */
#include "clibenv.h"                /* C runtime environment vars   */
#include "clibstae.h"               /* C runtime recovery routines  */
#include "socket.h"                 /* sockets via DYN75            */
#include "clibwto.h"                /* write to operator            */
#include "dbg.h"                    /* debugging goodies            */
#include "errors.h"                 /* the missing errno values     */
#include "clibthrd.h"               /* basic threads                */
#include "clibthdi.h"               /* thread management            */
#include "racf.h"                   /* security environment         */
#include "ufs.h"                    /* Unix File System             */

typedef struct ftpd     FTPD;       /* FTP Daemon (server)          */
typedef struct ftpc     FTPC;       /* FTP Client                   */
typedef enum   ftpstate FTPSTATE;   /* FTP Client state             */
typedef struct ftpx     FTPX;       /* FTP function vector          */

typedef unsigned char   UCHAR;
typedef unsigned short  USHRT;
typedef unsigned int    UINT;
typedef unsigned long   ULONG;
typedef unsigned long long U64;

extern UCHAR *ebc2asc;
extern UCHAR *asc2ebc;
extern FTPD  *ftpd;

struct ftpd {
    char        eye[8];             /* 00 eye catcher               */
#define FTPD_EYE    "*FTPD*"        /* ...                          */
    CTHDTASK    *ftpd_thread;       /* 08 ftpd_thread subtask       */
    FTPC        **ftpc;             /* 0C clients array             */

    unsigned    addr;               /* 10 our listener IP address   */
    int         port;               /* 14 our listener port         */
    int         listen;             /* 18 our listener socket       */
    unsigned    unused;             /* 1C available                 */

    FILE        *stats;             /* 20 statistics file (log)     */
    FILE        *dbg;               /* 24 debug/trace output        */
    volatile UCHAR flags;           /* 28 processing flags          */
#define FTPD_FLAG_INIT     0x01     /* ... initializing             */
#define FTPD_FLAG_LISTENER 0x02     /* ... listener thread created  */
#define FTPD_FLAG_READY    0x04     /* ... ready                    */
#define FTPD_FLAG_QUIESCE  0x40     /* ... don't accept new request */
#define FTPD_FLAG_SHUTDOWN 0x80     /* ... shutdown now             */
    UCHAR       unused1[3];         /* 29 available                 */
    UFSSYS      *sys;               /* 2C Unix File System          */
};                                  /* 30 (48 bytes)                */

enum ftpstate {
    FTPSTATE_INITIAL=0,
    FTPSTATE_LOGIN,
    FTPSTATE_IDLE,
    FTPSTATE_TRANSFER,
    FTPSTATE_TERMINATE
};

/* each client is 4088 bytes (4096-8) */
struct ftpc {
    char        eye[8];             /* 00 eye catcher               */
#define FTPC_EYE    "*FTPC*"        /* ...                          */
    FTPD        *ftpd;              /* 08 => FTPD (server)          */
    ACEE        *acee;              /* 0C security environment      */

    double      start;              /* 10 start time in seconds     */
    double      end;                /* 18 end time in seconds       */

    volatile UCHAR state;           /* 20 current state (FTPSTATE)  */
    UCHAR       flags;              /* 21 processing flags          */
#define FTPC_FLAG_PASSIVE   0x80    /* ... 1=passive mode           */
#define FTPC_FLAG_SEND      0x40    /* ... 1=sending data           */
#define FTPC_FLAG_RECV      0x20    /* ... 1=receiving data         */
#define FTPC_FLAG_FILE      0x10    /* ... 1=fp is FILE handle      */
#define FTPC_FLAG_UFS       0x08    /* ... 1=fp is UFS FILE handle  */
#define FTPC_FLAG_CWDPDS    0x04    /* ... 1=cwd is a PDS           */
#define FTPC_FLAG_CWDDS     0x02    /* ... 1=cwd is a dataset level */
#define FTPC_FLAG_RNFR      0x01    /* ... 1=fp is RNFR name string */

    char        mode;               /* 22 'S', 'B', 'C'             */
#define FTPC_MODE_STREAM        'S'
#define FTPC_MODE_BLOCK         'B'
#define FTPC_MODE_COMPRESSED    'C'

    char        structure;          /* 23 'F', 'R', 'P'             */
#define FTPC_STRUCTURE_FILE     'F'
#define FTPC_STRUCTURE_RECORD   'R'
#define FTPC_STRUCTURE_PAGE     'P'

    char        type;               /* 24 'A', 'E', 'I', 'L'        */
#define FTPC_TYPE_ASCII         'A'
#define FTPC_TYPE_EBCDIC        'E'
#define FTPC_TYPE_IMAGE         'I'
#define FTPC_TYPE_LOCAL         'L'

    UCHAR       fflags;             /* 25 file is flags             */
#define FTPC_FFLAG_MVS      0x80    /* ... MVS datasets             */
#define FTPC_FFLAG_UFS      0x40    /* ... Unix File System         */

    char        unused[2];          /* 26 available                 */

    int         client_socket;      /* 28 client socket             */
    unsigned    client_addr;        /* 2C client IP address         */
    int         client_port;        /* 30 client port               */

    int         data_socket;        /* 34 data socket               */
    unsigned    data_addr;          /* 38 data IP address           */
    int         data_port;          /* 3C data port                 */

    void        *fp;                /* 40 file handle               */
    unsigned    len;                /* 44 length of data in buf     */
    unsigned    pos;                /* 48 position in buf           */
    unsigned    count;              /* 4C bytes transfered          */

    UFS         *ufs;               /* 50 Unix File System handle   */
    char        cwd[252];           /* 54 cwd for datasets          */
    int         cmdlen;             /* 150 length of data in cmd    */
    char        cmd[252];           /* 154 command buffer           */
#define FTPBUF  (0x0FF8-0x0250)     /* ... 4088-592 = 3496          */
    UCHAR       buf[FTPBUF];        /* 250 data buffer              */
                                    /* FF8                          */
};                                  /* FF8 (4088 bytes)             */

extern int      ftpdenter(const char *fmt, ...);
extern int      ftpdexit(const char *fmt, ...);

extern FTPD *   ftpdinit(int argc, char **argv);
extern void     ftpdterm(FTPD **ftpd);
extern int      ftpdthrd(void *arg1, void *arg2);
extern double * ftpdsecs(double *secs);
extern void *   ftpfopen(FTPC *ftpc, const char *fn, const char *fm, int fqnrc);

/* client functions */
extern void     ftpcterm(FTPC **ftpc);
extern char *   ftpcgets(FTPC *ftpc);
extern int      ftpcmsg(FTPC *ftpc, char *msg, ...);
extern int      ftpcopen(FTPC *ftpc);
extern int      ftpcprtf(FTPC *ftpc, char *fmt, ...);

/* FTPD inside HTTPD helpers */
extern int ftpd_process_clients(void)                           asm("FTPDPCS");
extern int ftpd_process_client(FTPC *ftpc)                      asm("FTPDPC");

/* FTPD command functions */
extern int ftpdabor(FTPC *ftpc);   /* Abort                 */
extern int ftpdacct(FTPC *ftpc);   /* Account               */
extern int ftpdallo(FTPC *ftpc);   /* Allocate              */
extern int ftpdappe(FTPC *ftpc);   /* Append                */
extern int ftpdcdup(FTPC *ftpc);   /* Change Dir Up         */
extern int ftpdcwd (FTPC *ftpc);   /* Change Working Dir    */
extern int ftpddele(FTPC *ftpc);   /* Delete                */
extern int ftpdhelp(FTPC *ftpc);   /* Help            (211) */
extern int ftpdlist(FTPC *ftpc);   /* List (Directory)      */
extern int ftpdmkd (FTPC *ftpc);   /* Make Directory        */
extern int ftpdmode(FTPC *ftpc);   /* Mode                  */
extern int ftpdnlst(FTPC *ftpc);   /* Name List (Directory) */
extern int ftpdnoop(FTPC *ftpc);   /* No operation          */
extern int ftpdnoti(FTPC *ftpc);   /* Command not implemented */
extern int ftpdpass(FTPC *ftpc);   /* Password              */
extern int ftpdpasv(FTPC *ftpc);   /* Passive               */
extern int ftpdport(FTPC *ftpc);   /* Port                  */
extern int ftpdpwd (FTPC *ftpc);   /* Print Working Dir     */
extern int ftpdquit(FTPC *ftpc);   /* Quit                  */
extern int ftpdrein(FTPC *ftpc);   /* Reinitialize          */
extern int ftpdretr(FTPC *ftpc);   /* Retrieve              */
extern int ftpdrmd (FTPC *ftpc);   /* Remove Directory      */
extern int ftpdrnfr(FTPC *ftpc);   /* Rename From           */
extern int ftpdrnft(FTPC *ftpc);   /* Rename To             */
extern int ftpdsite(FTPC *ftpc);   /* Site                  */
extern int ftpdsmnt(FTPC *ftpc);   /* Structure Mount       */
extern int ftpdstat(FTPC *ftpc);   /* Status                */
extern int ftpdstor(FTPC *ftpc);   /* Store                 */
extern int ftpdstou(FTPC *ftpc);   /* Store Unique          */
extern int ftpdstru(FTPC *ftpc);   /* Structure             */
extern int ftpdsyst(FTPC *ftpc);   /* System                */
extern int ftpdtype(FTPC *ftpc);   /* Type (Data)           */
extern int ftpduser(FTPC *ftpc);   /* User                  */

extern int ftpslist(FTPC *ftpc, int brief);
extern unsigned ftpc_ds_count(const char *level, const char *option, const char *filter) asm("FTPCDSC");

/* ftpfqn() - returns FTPFQN_RC_type */
#define FTPFQN_RC_PARMS		-2		/* invalid parameters 				*/
#define FTPFQN_RC_ERROR		-1		/* Couldn't make sense of the input */
#define FTPFQN_RC_PATH		0		/* out is a path name 				*/
#define FTPFQN_RC_HLQ		1       /* out is a high level qualifier 	*/
#define FTPFQN_RC_DSN		2		/* out is a dataset name 			*/
#define FTPFQN_RC_DSNMEM	3		/* out is a dataset(member) name 	*/
extern int ftpfqn(FTPC *ftpc, const char *in, char *out);

#endif
