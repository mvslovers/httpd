/* HTTPX.C
** HTTP execution vector
*/
#define HTTP_PRIVATE
#include "httpd.h"

/* ABI guard (issue #125).  httpcgi.h publishes two struct httpd offsets to
** external CGI modules as macros -- httpx at 0x08 and flag at 0x2C -- so a
** CGI reads those bytes without ever seeing the struct definition.  Moving
** either field breaks every CGI silently: the wrong byte is still readable,
** so there is no abend and no message, only a quiesce check that fires when
** it should not or never fires at all.  The declaration below has a negative
** array size when an offset moves, which fails the build here instead.  It
** declares no storage (extern, never defined) and costs nothing at runtime.
** _Static_assert is C11 and unavailable to cc370.
**
** Skipped under clang, which only ever sees this file through clangd: the
** offsets hold for the 32-bit MVS target, but clangd parses with the host's
** 64-bit pointers, where every field after eye[8] sits somewhere else and the
** assert would fail on a perfectly good struct.  cc370 is the compiler that
** matters here, and it is what CI builds with.  (cc370's sysroot offsetof is
** (size_t)&(((x *)0)->y) -- gcc folds that to a constant in an array bound,
** clang rejects it as a VLA, and gcc 3.4.6 predates __builtin_offsetof, so
** there is no one spelling both accept anyway.) */
#ifndef __clang__
extern char httpd_abi_offsets[(offsetof(struct httpd, httpx) == 0x08 &&
                               offsetof(struct httpd, flag)  == 0x2C) ? 1 : -1];
#endif

/* static execution vector */
static HTTPX vect = {
    HTTPX_EYE,                  /* 00 eye catcher                   */
    0,                          /* 08 available                     */
    http_cgi_subpool,           /* 0C http_cgi_subpool()            */
    http_in,                    /* 10 http_in()                     */
    http_parse,                 /* 14 http_parse()                  */
    http_get,                   /* 18 http_get()                    */
    http_head,                  /* 1C http_head()                   */
    http_put,                   /* 20 http_put()                    */
    http_post,                  /* 24 http_post()                   */
    http_done,                  /* 28 http_done()                   */
    http_report,                /* 2C http_report()                 */
    http_reset,                 /* 30 http_reset()                  */
    http_close,                 /* 34 http_close()                  */
    http_send,                  /* 38 http_send()                   */
    http_decode,                /* 3C http_decode()                 */
    http_del_env,               /* 40 http_del_env()                */
    http_find_env,              /* 44 http_find_env()               */
    http_set_env,               /* 48 http_set_env()                */
    http_new_env,               /* 4C http_new_env()                */
    http_set_http_env,          /* 50 http_set_http_env()           */
    http_set_query_env,         /* 54 http_set_query_env()          */
    http_get_env,               /* 58 http_get_env()                */
    http_date_rfc1123,          /* 5C http_date_rfc1123()           */
    http_printv,                /* 60 http_printv()                 */
    http_printf,                /* 64 http_printf()                 */
    http_resp,                  /* 68 http_resp()                   */
    http_server_name,           /* 6C http_server_name()            */
    http_resp_not_implemented,  /* 70 http_resp_not_implemented()   */
    http_etoa,                  /* 74 http_etoa()                   */
    http_atoe,                  /* 78 http_atoe()                   */
    arraynew,                   /* 7C arraynew()                    */
    arrayadd,                   /* 80 arrayadd()                    */
    arrayaddf,                  /* 84 arrayaddf()                   */
    arraycount,                 /* 88 arraycount()                  */
    arrayfree,                  /* 8C arrayfree()                   */
    arrayget,                   /* 90 arrayget()                    */
    arraysize,                  /* 94 arraysize()                   */
    arraydel,                   /* 98 arraydel()                    */
    http_cmp,                   /* 9C http_cmp()                    */
    http_cmpn,                  /* A0 http_cmpn()                   */
    http_dbgw,                  /* A4 http_dbgw()                   */
    http_dbgs,                  /* A8 http_dbgs()                   */
    http_dbgf,                  /* AC http_dbgf()                   */
    http_dump,                  /* B0 http_dump()                   */
    http_enter,                 /* B4 http_enter()                  */
    http_exit,                  /* B8 http_exit()                   */
    http_secs,                  /* BC http_secs()                   */
    http_mime,                  /* C0 http_mime()                   */
    http_resp_internal_error,   /* C4 http_resp_internal_error()    */
    http_resp_not_found,        /* C8 http_resp_not_found()         */
    http_open,                  /* CC http_open()                   */
    http_send_binary,           /* D0 http_send_binary()            */
    0,                          /* D4 http_read() -- not exported   */
    http_send_file,             /* D8 http_send_file()              */
    http_send_text,             /* DC http_send_text()              */
    http_is_busy,               /* E0 http_is_busy()                */
    http_set_busy,              /* E4 http_set_busy()               */
    http_reset_busy,            /* E8 http_reset_busy()             */
    http_process_clients,       /* EC http_process_clients()        */
    http_ntoa,                  /* F0 http_ntoa()                   */
    http_console,               /* F4 http_console()                */
    http_process_client,        /* F8 http_process_client()         */
    http_link,                  /* FC http_link()                   */
    http_find_cgi,              /* 100 http_find_cgi()              */
    http_add_cgi,               /* 104 http_add_cgi()               */
    http_process_cgi,           /* 108 http_process_cgi()           */
    NULL,                       /* 10C (was: mqtc_pub)              */
    http_xlate,                 /* 110 http_xlate()                 */
    &http_cp037,                /* 114 CP037 codepage pair          */
    &http_cp1047,               /* 118 IBM-1047 codepage pair       */
    &http_legacy,               /* 11C legacy hybrid codepage pair  */
    http_get_ufs,               /* 120 http_get_ufs()               */
    http_cgictx_get,            /* 124 http_cgictx_get()            */
    http_get_userid,            /* 128 http_get_userid()            */
    http_get_acee,              /* 12C http_get_acee()              */
    http_get_token,             /* 130 http_get_token()             */
    http_check_auth,            /* 134 http_check_auth()            */
    http_logout,                /* 138 http_logout()                */
    http_get_password,          /* 13C http_get_password()          */
    http_realm,                 /* 140 http_realm()                 */
};

HTTPX *httpx = &vect;
