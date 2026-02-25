#ifndef HTTPLUAX_H
#define HTTPLUAX_H
#include "stdio.h"
#include "string.h"
#include "stdarg.h"
#include "clib.h"
#include "clibio.h"
#include "clibos.h"
#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"

/* To load the HTTPLUAX module, use the following:
 * 
 * #define HTTPLUAX luax
 * #include "httpluax.h"
 * LUAX *luax = (LUAX *) __load(0, "HTTPLUAX", 0, 0);
 * 
 * - - - O R - - - 
 * 
 * #define HTTPLUAX httpd->luax
 * #include "httpluax.h"
 * httpd->luax = (LUAX *) __load(0, "HTTPLUAX", 0, 0);
 * 
 * - - - O R - - - 
 * 
 * When httpluax is LKED with main program:
 * #define HTTPLUAX luax
 * #include "httpluax.h"
 * LAUX *luax = (LUAX*) &httpluax;
 * 
 * If you don't want the macro names for the LUAX functions
 * you can use #define HTTPLUAX_PRIVATE before the 
 * #include "httpluax.h" to hide the lua... macro names (defines)
 */

typedef struct luax		LUAX;

struct luax {
	const char 	   *lua_ident;
	lua_State 	   *(*lua_newstate) (lua_Alloc f, void *ud);
	void       		(*lua_close) (lua_State *L);
	lua_State 	   *(*lua_newthread) (lua_State *L);
	int        		(*lua_closethread) (lua_State *L, lua_State *from);
	int        		(*lua_resetthread) (lua_State *L);
	lua_CFunction 	(*lua_atpanic) (lua_State *L, lua_CFunction panicf);
	lua_Number 		(*lua_version) (lua_State *L);
	int   			(*lua_absindex) (lua_State *L, int idx);
	int   			(*lua_gettop) (lua_State *L);
	void  			(*lua_settop) (lua_State *L, int idx);
	void  			(*lua_pushvalue) (lua_State *L, int idx);
	void  			(*lua_rotate) (lua_State *L, int idx, int n);
	void  			(*lua_copy) (lua_State *L, int fromidx, int toidx);
	int   			(*lua_checkstack) (lua_State *L, int n);
	void  			(*lua_xmove) (lua_State *from, lua_State *to, int n);
	int             (*lua_isnumber) (lua_State *L, int idx);
	int             (*lua_isstring) (lua_State *L, int idx);
	int             (*lua_iscfunction) (lua_State *L, int idx);
	int             (*lua_isinteger) (lua_State *L, int idx);
	int             (*lua_isuserdata) (lua_State *L, int idx);
	int             (*lua_type) (lua_State *L, int idx);
	const char     *(*lua_typename) (lua_State *L, int tp);
	lua_Number      (*lua_tonumberx) (lua_State *L, int idx, int *isnum);
	lua_Integer     (*lua_tointegerx) (lua_State *L, int idx, int *isnum);
	int             (*lua_toboolean) (lua_State *L, int idx);
	const char     *(*lua_tolstring) (lua_State *L, int idx, size_t *len);
	lua_Unsigned    (*lua_rawlen) (lua_State *L, int idx);
	lua_CFunction   (*lua_tocfunction) (lua_State *L, int idx);
	void	       *(*lua_touserdata) (lua_State *L, int idx);
	lua_State      *(*lua_tothread) (lua_State *L, int idx);
	const void     *(*lua_topointer) (lua_State *L, int idx);
	int   			(*lua_rawequal) (lua_State *L, int idx1, int idx2);
	int   			(*lua_compare) (lua_State *L, int idx1, int idx2, int op);
	void        	(*lua_pushnil) (lua_State *L);
	void        	(*lua_pushnumber) (lua_State *L, lua_Number n);
	void        	(*lua_pushinteger) (lua_State *L, lua_Integer n);
	const char	   *(*lua_pushlstring) (lua_State *L, const char *s, size_t len);
	const char	   *(*lua_pushstring) (lua_State *L, const char *s);
	const char	   *(*lua_pushvfstring) (lua_State *L, const char *fmt, va_list argp);
	const char	   *(*lua_pushfstring) (lua_State *L, const char *fmt, ...);
	void  			(*lua_pushcclosure) (lua_State *L, lua_CFunction fn, int n);
	void  			(*lua_pushboolean) (lua_State *L, int b);
	void  			(*lua_pushlightuserdata) (lua_State *L, void *p);
	int   			(*lua_pushthread) (lua_State *L);
	int 			(*lua_getglobal) (lua_State *L, const char *name);
	int 			(*lua_gettable) (lua_State *L, int idx);
	int 			(*lua_getfield) (lua_State *L, int idx, const char *k);
	int 			(*lua_geti) (lua_State *L, int idx, lua_Integer n);
	int 			(*lua_rawget) (lua_State *L, int idx);
	int 			(*lua_rawgeti) (lua_State *L, int idx, lua_Integer n);
	int 			(*lua_rawgetp) (lua_State *L, int idx, const void *p);
	void  			(*lua_createtable) (lua_State *L, int narr, int nrec);
	void		   *(*lua_newuserdatauv) (lua_State *L, size_t sz, int nuvalue);
	int   			(*lua_getmetatable) (lua_State *L, int objindex);
	int  			(*lua_getiuservalue) (lua_State *L, int idx, int n);
	void  			(*lua_setglobal) (lua_State *L, const char *name);
	void  			(*lua_settable) (lua_State *L, int idx);
	void  			(*lua_setfield) (lua_State *L, int idx, const char *k);
	void  			(*lua_seti) (lua_State *L, int idx, lua_Integer n);
	void  			(*lua_rawset) (lua_State *L, int idx);
	void  			(*lua_rawseti) (lua_State *L, int idx, lua_Integer n);
	void  			(*lua_rawsetp) (lua_State *L, int idx, const void *p);
	int   			(*lua_setmetatable) (lua_State *L, int objindex);
	int   			(*lua_setiuservalue) (lua_State *L, int idx, int n);
	void  			(*lua_callk) (lua_State *L, int nargs, int nresults, lua_KContext ctx, lua_KFunction k);
	int   			(*lua_pcallk) (lua_State *L, int nargs, int nresults, int errfunc, lua_KContext ctx, lua_KFunction k);
	int   			(*lua_load) (lua_State *L, lua_Reader reader, void *dt, const char *chunkname, const char *mode);
	int 			(*lua_dump) (lua_State *L, lua_Writer writer, void *data, int strip);
	int  			(*lua_yieldk) (lua_State *L, int nresults, lua_KContext ctx, lua_KFunction k);
	int  			(*lua_resume) (lua_State *L, lua_State *from, int narg, int *nres);
	int  			(*lua_status) (lua_State *L);
	int 			(*lua_isyieldable) (lua_State *L);
	void 			(*lua_setwarnf) (lua_State *L, lua_WarnFunction f, void *ud);
	void 			(*lua_warning) (lua_State *L, const char *msg, int tocont);
	int 			(*lua_gc) (lua_State *L, int what, ...);
	int   			(*lua_error) (lua_State *L);
	int   			(*lua_next) (lua_State *L, int idx);
	void  			(*lua_concat) (lua_State *L, int n);
	void  			(*lua_len)    (lua_State *L, int idx);
	size_t   		(*lua_stringtonumber) (lua_State *L, const char *s);
	lua_Alloc 		(*lua_getallocf) (lua_State *L, void **ud);
	void      		(*lua_setallocf) (lua_State *L, lua_Alloc f, void *ud);
	void 			(*lua_toclose) (lua_State *L, int idx);
	void 			(*lua_closeslot) (lua_State *L, int idx);
	int 			(*lua_getstack) (lua_State *L, int level, lua_Debug *ar);
	int 			(*lua_getinfo) (lua_State *L, const char *what, lua_Debug *ar);
	const char	   *(*lua_getlocal) (lua_State *L, const lua_Debug *ar, int n);
	const char	   *(*lua_setlocal) (lua_State *L, const lua_Debug *ar, int n);
	const char	   *(*lua_getupvalue) (lua_State *L, int funcindex, int n);
	const char	   *(*lua_setupvalue) (lua_State *L, int funcindex, int n);
	void		   *(*lua_upvalueid) (lua_State *L, int fidx, int n);
	void  			(*lua_upvaluejoin) (lua_State *L, int fidx1, int n1, int fidx2, int n2);
	void 			(*lua_sethook) (lua_State *L, lua_Hook func, int mask, int count);
	lua_Hook 		(*lua_gethook) (lua_State *L);
	int 			(*lua_gethookmask) (lua_State *L);
	int 			(*lua_gethookcount) (lua_State *L);
	int 			(*lua_setcstacklimit) (lua_State *L, unsigned int limit);
	void  			(*lua_arith) (lua_State *L, int op);
	void 			*unused1;
	void 			*unused2;
	void			*unused3;
	/* 0x198 (408 bytes) */
	/* LAUXLIB */
	void 			(*luaL_checkversion_) (lua_State *L, lua_Number ver, size_t sz);
	int 			(*luaL_getmetafield) (lua_State *L, int obj, const char *e);
	int 			(*luaL_callmeta) (lua_State *L, int obj, const char *e);
	const char     *(*luaL_tolstring) (lua_State *L, int idx, size_t *len);
	int 			(*luaL_argerror) (lua_State *L, int arg, const char *extramsg);
	int 			(*luaL_typeerror) (lua_State *L, int arg, const char *tname);
	const char     *(*luaL_checklstring) (lua_State *L, int arg, size_t *l);
	const char     *(*luaL_optlstring) (lua_State *L, int arg, const char *def, size_t *l);
	lua_Number 		(*luaL_checknumber) (lua_State *L, int arg);
	lua_Number 		(*luaL_optnumber) (lua_State *L, int arg, lua_Number def);
	lua_Integer 	(*luaL_checkinteger) (lua_State *L, int arg);
	lua_Integer 	(*luaL_optinteger) (lua_State *L, int arg, lua_Integer def);
	void 			(*luaL_checkstack) (lua_State *L, int sz, const char *msg);
	void 			(*luaL_checktype) (lua_State *L, int arg, int t);
	void 			(*luaL_checkany) (lua_State *L, int arg);
	int   			(*luaL_newmetatable) (lua_State *L, const char *tname);
	void  			(*luaL_setmetatable) (lua_State *L, const char *tname);
	void           *(*luaL_testudata) (lua_State *L, int ud, const char *tname);
	void           *(*luaL_checkudata) (lua_State *L, int ud, const char *tname);
	void 			(*luaL_where) (lua_State *L, int lvl);
	int 			(*luaL_error) (lua_State *L, const char *fmt, ...);
	int 			(*luaL_checkoption) (lua_State *L, int arg, const char *def, const char *const lst[]);
	int 			(*luaL_fileresult) (lua_State *L, int stat, const char *fname);
	int 			(*luaL_execresult) (lua_State *L, int stat);
	int 			(*luaL_ref) (lua_State *L, int t);
	void 			(*luaL_unref) (lua_State *L, int t, int ref);
	int 			(*luaL_loadfilex) (lua_State *L, const char *filename, const char *mode);
	int 			(*luaL_loadbufferx) (lua_State *L, const char *buff, size_t sz, const char *name, const char *mode);
	int 			(*luaL_loadstring) (lua_State *L, const char *s);
	lua_State      *(*luaL_newstate) (void);
	lua_Integer 	(*luaL_len) (lua_State *L, int idx);
	void 			(*luaL_addgsub) (luaL_Buffer *b, const char *s, const char *p, const char *r);
	const char     *(*luaL_gsub) (lua_State *L, const char *s, const char *p, const char *r);
	void 			(*luaL_setfuncs) (lua_State *L, const luaL_Reg *l, int nup);
	int 			(*luaL_getsubtable) (lua_State *L, int idx, const char *fname);
	void 			(*luaL_traceback) (lua_State *L, lua_State *L1, const char *msg, int level);
	void 			(*luaL_requiref) (lua_State *L, const char *modname, lua_CFunction openf, int glb);
	void 			(*luaL_buffinit) (lua_State *L, luaL_Buffer *B);
	char           *(*luaL_prepbuffsize) (luaL_Buffer *B, size_t sz);
	void 			(*luaL_addlstring) (luaL_Buffer *B, const char *s, size_t l);
	void 			(*luaL_addstring) (luaL_Buffer *B, const char *s);
	void 			(*luaL_addvalue) (luaL_Buffer *B);
	void 			(*luaL_pushresult) (luaL_Buffer *B);
	void 			(*luaL_pushresultsize) (luaL_Buffer *B, size_t sz);
	char           *(*luaL_buffinitsize) (lua_State *L, luaL_Buffer *B, size_t sz);
	void 			(*luaL_openlibs) (lua_State *L);

	void	*lunused1;
	void	*lunused2;

};

extern LUAX httpluax;

#ifndef HTTPLUAX_PRIVATE

#ifndef HTTPLUAX
#error "You must define HTTPLUAX with your LUAX * pointer name"
#endif

#define lua_ident \
	(HTTPLUAX)->lua_ident
#define lua_newstate(f,ud) \
	(HTTPLUAX)->lua_newstate(f,ud)
#define lua_close(L) \
	(HTTPLUAX)->lua_close(L)
#define lua_newthread(L) \
	(HTTPLUAX)->lua_newthread(L)
#define lua_closethread(L,from) \
	(HTTPLUAX)->lua_closethread(L,from)
#define lua_resetthread(L) \
	(HTTPLUAX)->lua_resetthread(L)
#define lua_atpanic(L,panicf) \
	(HTTPLUAX)->lua_atpanic(L,panicf)
#define lua_version(L) \
	(HTTPLUAX)->lua_version(L)
#define lua_absindex(L,idx) \
	(HTTPLUAX)->lua_absindex(L,idx)
#define lua_gettop(L) \
	(HTTPLUAX)->lua_gettop(L)
#define lua_settop(L,idx) \
	(HTTPLUAX)->lua_settop(L,idx)
#define lua_pushvalue(L,idx) \
	(HTTPLUAX)->lua_pushvalue(L,idx)
#define lua_rotate(L,idx,n) \
	(HTTPLUAX)->lua_rotate(L,idx,n)
#define lua_copy(L,fromidx,toidx) \
	(HTTPLUAX)->lua_copy(L,fromidx,toidx)
#define lua_checkstack(L,n) \
	(HTTPLUAX)->lua_checkstack(L,n)
#define lua_xmove(from,to,n) \
	(HTTPLUAX)->lua_xmove(from,to,n)
#define lua_isnumber(L,idx) \
	(HTTPLUAX)->lua_isnumber(L,idx)
#define lua_isstring(L,idx) \
	(HTTPLUAX)->lua_isstring(L,idx)
#define lua_iscfunction(L,idx) \
	(HTTPLUAX)->lua_iscfunction(L,idx)
#define lua_isinteger(L,idx) \
	(HTTPLUAX)->lua_isinteger(L,idx)
#define lua_isuserdata(L,idx) \
	(HTTPLUAX)->lua_isuserdata(L,idx)
#define lua_type(L,idx) \
	(HTTPLUAX)->lua_type(L,idx)
#define lua_typename(L,tp) \
	(HTTPLUAX)->lua_typename(L,tp)
#define lua_tonumberx(L,idx,isnum) \
	(HTTPLUAX)->lua_tonumberx(L,idx,isnum)
#define lua_tointegerx(L,idx,isnum) \
	(HTTPLUAX)->lua_tointegerx(L,idx,isnum)
#define lua_toboolean(L,idx) \
	(HTTPLUAX)->lua_toboolean(L,idx)
#define lua_tolstring(L,idx,len) \
	(HTTPLUAX)->lua_tolstring(L,idx,len)
#define lua_rawlen(L,idx) \
	(HTTPLUAX)->lua_rawlen(L,idx)
#define lua_tocfunction(L,idx) \
	(HTTPLUAX)->lua_tocfunction(L,idx)
#define lua_touserdata(L,idx) \
	(HTTPLUAX)->lua_touserdata(L,idx)
#define lua_tothread(L,idx) \
	(HTTPLUAX)->lua_tothread(L,idx)
#define lua_topointer(L,idx) \
	(HTTPLUAX)->lua_topointer(L,idx)
#define lua_rawequal(L,idx1,idx2) \
	(HTTPLUAX)->lua_rawequal(L,idx1,idx2)
#define lua_compare(L,idx1,idx2,op) \
	(HTTPLUAX)->lua_compare(L,idx1,idx2,op)
#define lua_pushnil(L) \
	(HTTPLUAX)->lua_pushnil(L)
#define lua_pushnumber(L,n) \
	(HTTPLUAX)->lua_pushnumber(L,n)
#define lua_pushinteger(L,n) \
	(HTTPLUAX)->lua_pushinteger(L,n)
#define lua_pushlstring(L,s,len) \
	(HTTPLUAX)->lua_pushlstring(L,s,len)
#define lua_pushstring(L,s) \
	(HTTPLUAX)->lua_pushstring(L,s)
#define lua_pushvfstring(L,fmt,argp) \
	(HTTPLUAX)->lua_pushvfstring(L,fmt,argp)
#define lua_pushfstring(L,fmt,...) \
	(HTTPLUAX)->lua_pushfstring(L,fmt,## __VA_ARGS__)
#define lua_pushcclosure(L,fn,n) \
	(HTTPLUAX)->lua_pushcclosure(L,fn,n)
#define lua_pushboolean(L,b) \
	(HTTPLUAX)->lua_pushboolean(L,b)
#define lua_pushlightuserdata(L,p) \
	(HTTPLUAX)->lua_pushlightuserdata(L,p)
#define lua_pushthread(L) \
	(HTTPLUAX)->lua_pushthread(L)
#define lua_getglobal(L,name) \
	(HTTPLUAX)->lua_getglobal(L,name)
#define lua_gettable(L,idx) \
	(HTTPLUAX)->lua_gettable(L,idx)
#define lua_getfield(L,idx,k) \
	(HTTPLUAX)->lua_getfield(L,idx,k)
#define lua_geti(L,idx,n) \
	(HTTPLUAX)->lua_geti(L,idx,n)
#define lua_rawget(L,idx) \
	(HTTPLUAX)->lua_rawget(L,idx)
#define lua_rawgeti(L,idx,n) \
	(HTTPLUAX)->lua_rawgeti(L,idx,n)
#define lua_rawgetp(L,idx,p) \
	(HTTPLUAX)->lua_rawgetp(L,idx,p)
#define lua_createtable(L,narr,nrec) \
	(HTTPLUAX)->lua_createtable(L,narr,nrec)
#define lua_newuserdatauv(L,sz,nuvalue) \
	(HTTPLUAX)->lua_newuserdatauv(L,sz,nuvalue)
#define lua_getmetatable(L,objindex) \
	(HTTPLUAX)->lua_getmetatable(L,objindex)
#define lua_getiuservalue(L,idx,n) \
	(HTTPLUAX)->lua_getiuservalue(L,idx,n)
#define lua_setglobal(L,name) \
	(HTTPLUAX)->lua_setglobal(L,name)
#define lua_settable(L,idx) \
	(HTTPLUAX)->lua_settable(L,idx)
#define lua_setfield(L,idx,k) \
	(HTTPLUAX)->lua_setfield(L,idx,k)
#define lua_seti(L,idx,n) \
	(HTTPLUAX)->lua_seti(L,idx,n)
#define lua_rawset(L,idx) \
	(HTTPLUAX)->lua_rawset(L,idx)
#define lua_rawseti(L,idx,n) \
	(HTTPLUAX)->lua_rawseti(L,idx,n)
#define lua_rawsetp(L,idx,p) \
	(HTTPLUAX)->lua_rawsetp(L,idx,p)
#define lua_setmetatable(L,objindex) \
	(HTTPLUAX)->lua_setmetatable(L,objindex)
#define lua_setiuservalue(L,idx,n) \
	(HTTPLUAX)->lua_setiuservalue(L,idx,n)
#define lua_callk(L,nargs,nresults,ctx,k) \
	(HTTPLUAX)->lua_callk(L,nargs,nresults,ctx,k)
#undef lua_call
#define lua_call(L,n,r)	lua_callk(L, (n), (r), 0, NULL)
#define lua_pcallk(L,nargs,nresults,errfunc,ctx,k) \
	(HTTPLUAX)->lua_pcallk(L,nargs,nresults,errfunc,ctx,k)
#undef lua_pcall
#define lua_pcall(L,n,r,f) lua_pcallk(L, (n), (r), (f), 0, NULL)
#define lua_load(L,reader,dt,chunkname,mode) \
	(HTTPLUAX)->lua_load(L,reader,dt,chunkname,mode)
#define lua_dump(L,writer,data,strip) \
	(HTTPLUAX)->lua_dump(L,writer,data,strip)
#define lua_yieldk(L,nresults,ctx,k) \
	(HTTPLUAX)->lua_yieldk(L,nresults,ctx,k)
#define lua_resume(L,from,narg,nres) \
	(HTTPLUAX)->lua_resume(L,from,narg,nres)
#define lua_status(L) \
	(HTTPLUAX)->lua_status(L)
#define lua_isyieldable(L) \
	(HTTPLUAX)->lua_isyieldable(L)
#define lua_setwarnf(L,f,ud) \
	(HTTPLUAX)->lua_setwarnf(L,f,ud)
#define lua_warning(L,msg,tocont) \
	(HTTPLUAX)->lua_warning(L,msg,tocont)
#define lua_gc(L,what,...) \
	(HTTPLUAX)->lua_gc(L,what,## __VA_ARGS__)
#define lua_error(L) \
	(HTTPLUAX)->lua_error(L)
#define lua_next(L,idx) \
	(HTTPLUAX)->lua_next(L,idx)
#define lua_concat(L,n) \
	(HTTPLUAX)->lua_concat(L,n)
#define lua_len(L,idx) \
	(HTTPLUAX)->lua_len(L,idx)
#define lua_stringtonumber(L,s) \
	(HTTPLUAX)->lua_stringtonumber(L,s)
#define lua_getallocf(L,ud) \
	(HTTPLUAX)->lua_getallocf(L,ud)
#define lua_setallocf(L,f,ud) \
	(HTTPLUAX)->lua_setallocf(L,f,ud)
#define lua_toclose(L,idx) \
	(HTTPLUAX)->lua_toclose(L,idx)
#define lua_closeslot(L,idx) \
	(HTTPLUAX)->lua_closeslot(L,idx)
#define lua_getstack(L,level,ar) \
	(HTTPLUAX)->lua_getstack(L,level,ar)
#define lua_getinfo(L,what,ar) \
	(HTTPLUAX)->lua_getinfo(L,what,ar)
#define lua_getlocal(L,ar,n) \
	(HTTPLUAX)->lua_getlocal(L,ar,n)
#define lua_setlocal(L,ar,n) \
	(HTTPLUAX)->lua_setlocal(L,ar,n)
#define lua_getupvalue(L,funcindex,n) \
	(HTTPLUAX)->lua_getupvalue(L,funcindex,n)
#define lua_setupvalue(L,funcindex,n) \
	(HTTPLUAX)->lua_setupvalue(L,funcindex,n)
#define lua_upvalueid(L,fidx,n) \
	(HTTPLUAX)->lua_upvalueid(L,fidx,n)
#define lua_upvaluejoin(L,fidx1,n1,fidx2,n2) \
	(HTTPLUAX)->lua_upvaluejoin(L,fidx1,n1,fidx2,n2)
#define lua_sethook(L,func,mask,count) \
	(HTTPLUAX)->lua_sethook(L,func,mask,count)
#define lua_gethook(L) \
	(HTTPLUAX)->lua_gethook(L)
#define lua_gethookmask(L) \
	(HTTPLUAX)->lua_gethookmask(L)
#define lua_gethookcount(L) \
	(HTTPLUAX)->lua_gethookcount(L)
#define lua_setcstacklimit(L,limit) \
	(HTTPLUAX)->lua_setcstacklimit(L,limit)
#define lua_arith(L,op) \
	(HTTPLUAX)->lua_arith(L,op)
/* LAUXLIB */
#define luaL_checkversion_(L,ver,sz) \
	(HTTPLUAX)->luaL_checkversion_(L,ver,sz)
#define luaL_getmetafield(L,obj,e) \
	(HTTPLUAX)->luaL_getmetafield(L,obj,e)
#define luaL_callmeta(L,obj,e) \
	(HTTPLUAX)->luaL_callmeta(L,obj,e)
#define luaL_tolstring(L,idx,len) \
	(HTTPLUAX)->luaL_tolstring(L,idx,len)
#define luaL_argerror(L,arg,extramsg) \
	(HTTPLUAX)->luaL_argerror(L,arg,extramsg)
#define luaL_typeerror(L,arg,tname) \
	(HTTPLUAX)->luaL_typeerror(L,arg,tname)
#define luaL_checklstring(L,arg,len) \
	(HTTPLUAX)->luaL_checklstring(L,arg,len)
#define luaL_optlstring(L,arg,def,len) \
	(HTTPLUAX)->luaL_optlstring(L,arg,def,len)
#define luaL_checknumber(L,arg) \
	(HTTPLUAX)->luaL_checknumber(L,arg)
#define luaL_optnumber(L,arg,def) \
	(HTTPLUAX)->luaL_optnumber(L,arg,def)
#define luaL_checkinteger(L,arg) \
	(HTTPLUAX)->luaL_checkinteger(L,arg)
#define luaL_optinteger(L,arg,def) \
	(HTTPLUAX)->luaL_optinteger(L,arg,def)
#define luaL_checkstack(L,sz,msg) \
	(HTTPLUAX)->luaL_checkstack(L,sz,msg)
#define luaL_checktype(L,arg,t) \
	(HTTPLUAX)->luaL_checktype(L,arg,t)
#define luaL_checkany(L,arg) \
	(HTTPLUAX)->luaL_checkany(L,arg)
#define luaL_newmetatable(L,tname) \
	(HTTPLUAX)->luaL_newmetatable(L,tname)
#define luaL_setmetatable(L,tname) \
	(HTTPLUAX)->luaL_setmetatable(L,tname)
#define luaL_testudata(L,ud,tname) \
	(HTTPLUAX)->luaL_testudata(L,ud,tname)
#define luaL_checkudata(L,ud,tname) \
	(HTTPLUAX)->luaL_checkudata(L,ud,tname)
#define luaL_where(L,level) \
	(HTTPLUAX)->luaL_where(L,level)
#define luaL_error(L,fmt,...) \
	(HTTPLUAX)->luaL_error(L,fmt,## __VA_ARGS__)
#define luaL_checkoption(L,arg,def,lst) \
	(HTTPLUAX)->luaL_checkoption(L,arg,def,lst)
#define luaL_fileresult(L,stat,fname) \
	(HTTPLUAX)->luaL_fileresult(L,stat,fname)
#define luaL_execresult(L,stat) \
	(HTTPLUAX)->luaL_execresult(L,stat)
#define luaL_ref(L,t) \
	(HTTPLUAX)->luaL_ref(L,t)
#define luaL_unref(L,t,ref) \
	(HTTPLUAX)->luaL_unref(L,t,ref)
#define luaL_loadfilex(L,filename,mode) \
	(HTTPLUAX)->luaL_loadfilex(L,filename,mode)
#define luaL_loadbufferx(L,buff,sz,name,mode) \
	(HTTPLUAX)->luaL_loadbufferx(L,buff,sz,name,mode)
#define luaL_loadstring(L,s) \
	(HTTPLUAX)->luaL_loadstring(L,s)
#define luaL_newstate() \
	(HTTPLUAX)->luaL_newstate()
#define luaL_len(L,idx) \
	(HTTPLUAX)->luaL_len(L,idx)
#define luaL_addgsub(b,s,p,r) \
	(HTTPLUAX)->luaL_addgsub(b,s,p,r)
#define luaL_gsub(L,s,p,r) \
	(HTTPLUAX)->luaL_gsub(L,s,p,r)
#define luaL_setfuncs(L,l,nup) \
	(HTTPLUAX)->luaL_setfuncs(L,l,nup)
#define luaL_getsubtable(L,idx,fname) \
	(HTTPLUAX)->luaL_getsubtable(L,idx,fname)
#define luaL_traceback(L,L1,msg,level) \
	(HTTPLUAX)->luaL_traceback(L,L1,msg,level)
#define luaL_requiref(L,modname,openf,glb) \
	(HTTPLUAX)->luaL_requiref(L,modname,openf,glb)
#define luaL_buffinit(L,B) \
	(HTTPLUAX)->luaL_buffinit(L,B)
#define luaL_prepbuffsize(B,sz) \
	(HTTPLUAX)->luaL_prepbuffsize(B,sz)
#define luaL_addlstring(B,s,l) \
	(HTTPLUAX)->luaL_addlstring(B,s,l)
#define luaL_addstring(B,s) \
	(HTTPLUAX)->luaL_addstring(B,s)
#define luaL_addvalue(B) \
	(HTTPLUAX)->luaL_addvalue(B)
#define luaL_pushresult(B) \
	(HTTPLUAX)->luaL_pushresult(B)
#define luaL_pushresultsize(B,sz) \
	(HTTPLUAX)->luaL_pushresultsize(B,sz)
#define luaL_buffinitsize(L,B,sz) \
	(HTTPLUAX)->luaL_buffinitsize(L,B,sz)
#define luaL_openlibs(L) \
	(HTTPLUAX)->luaL_openlibs(L)



#undef lua_tonumber
#define lua_tonumber(L,i) \
	lua_tonumberx(L,(i),NULL)

#undef lua_tointeger
#define lua_tointeger(L,i) \
	lua_tointegerx(L,(i),NULL)

#undef lua_pop
#define lua_pop(L,n) \
	lua_settop(L, -(n)-1)

#undef lua_newtable
#define lua_newtable(L)	\
	lua_createtable(L, 0, 0)

#undef lua_pushcfunction
#define lua_pushcfunction(L,f) \
	lua_pushcclosure(L, (f), 0)

#undef lua_register
#define lua_register(L,n,f) \
	(lua_pushcfunction(L, (f)), lua_setglobal(L, (n)))

#undef lua_isfunction
#define lua_isfunction(L,n) \
	(lua_type(L, (n)) == LUA_TFUNCTION)

#undef lua_istable
#define lua_istable(L,n) \
	(lua_type(L, (n)) == LUA_TTABLE)

#undef lua_islightuserdata
#define lua_islightuserdata(L,n) \
	(lua_type(L, (n)) == LUA_TLIGHTUSERDATA)

#undef lua_isnil
#define lua_isnil(L,n) \
	(lua_type(L, (n)) == LUA_TNIL)

#undef lua_isboolean
#define lua_isboolean(L,n) \
	(lua_type(L, (n)) == LUA_TBOOLEAN)

#undef lua_isthread
#define lua_isthread(L,n) \
	(lua_type(L, (n)) == LUA_TTHREAD)

#undef lua_isnone
#define lua_isnone(L,n) \
	(lua_type(L, (n)) == LUA_TNONE)

#undef lua_isnoneornil
#define lua_isnoneornil(L, n) \
	(lua_type(L, (n)) <= 0)

#undef lua_pushliteral
#define lua_pushliteral(L, s) \
	lua_pushstring(L, "" s)

#undef lua_pushglobaltable
#define lua_pushglobaltable(L)  \
	((void)lua_rawgeti(L, LUA_REGISTRYINDEX, LUA_RIDX_GLOBALS))

#undef lua_tostring
#define lua_tostring(L,i)  \
	lua_tolstring(L, (i), NULL)

#undef lua_insert
#define lua_insert(L,idx) \
	lua_rotate(L, (idx), 1)

#undef lua_remove
#define lua_remove(L,idx) \
	(lua_rotate(L, (idx), -1), lua_pop(L, 1))

#undef lua_replace
#define lua_replace(L,idx) \
	(lua_copy(L, -1, (idx)), lua_pop(L, 1))

#undef lua_call
#define lua_call(L,n,r)	\
	lua_callk(L, (n), (r), 0, NULL)

#undef lua_pcall
#define lua_pcall(L,n,r,f) \
	lua_pcallk(L, (n), (r), (f), 0, NULL)

#undef lua_yield
#define lua_yield(L,n) \
	lua_yieldk(L, (n), 0, NULL)


/*
** {==============================================================
** compatibility macros
** ===============================================================
*/
#if defined(LUA_COMPAT_APIINTCASTS)
#undef lua_pushunsigned
#define lua_pushunsigned(L,n)	lua_pushinteger(L, (lua_Integer)(n))
#undef lua_tounsignedx
#define lua_tounsignedx(L,i,is)	((lua_Unsigned)lua_tointegerx(L,i,is))
#undef lua_tounsigned
#define lua_tounsigned(L,i)	lua_tounsignedx(L,(i),NULL)
#endif

#undef lua_newuserdata
#define lua_newuserdata(L,s)	lua_newuserdatauv(L,s,1)
#undef lua_getuservalue
#define lua_getuservalue(L,idx)	lua_getiuservalue(L,idx,1)
#undef lua_setuservalue
#define lua_setuservalue(L,idx)	lua_setiuservalue(L,idx,1)

#undef luaL_newlibtable
#define luaL_newlibtable(L,l) \
	lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#undef luaL_argcheck
#define luaL_argcheck(L, cond,arg,extramsg)	\
	((void)(luai_likely(cond) || luaL_argerror(L, (arg), (extramsg))))

#undef luaL_argexpected
#define luaL_argexpected(L,cond,arg,tname)	\
	((void)(luai_likely(cond) || luaL_typeerror(L, (arg), (tname))))

#undef luaL_checkstring
#define luaL_checkstring(L,n) \
	(luaL_checklstring(L, (n), NULL))

#undef luaL_optstring
#define luaL_optstring(L,n,d) \
	(luaL_optlstring(L, (n), (d), NULL))

#undef luaL_typename
#define luaL_typename(L,i) \
	lua_typename(L, lua_type(L,(i)))

#undef luaL_dostring
#define luaL_dostring(L, s) \
	(luaL_loadstring(L, s) || lua_pcall(L, 0, LUA_MULTRET, 0))

#undef luaL_getmetatable
#define luaL_getmetatable(L,n) \
	(lua_getfield(L, LUA_REGISTRYINDEX, (n)))

#undef luaL_opt
#define luaL_opt(L,f,n,d) \
	(lua_isnoneornil(L,(n)) ? (d) : f(L,(n)))

#undef luaL_loadbuffer
#define luaL_loadbuffer(L,s,sz,n) \
	luaL_loadbufferx(L,s,sz,n,NULL)

/* push the value used to represent failure/error */
#undef luaL_pushfail
#define luaL_pushfail(L) \
	lua_pushnil(L)


#undef luaL_addchar
#define luaL_addchar(B,c) \
  ((void)((B)->n < (B)->size || luaL_prepbuffsize((B), 1)), \
   ((B)->b[(B)->n++] = (c)))

#undef luaL_prepbuffer
#define luaL_prepbuffer(B) \
	luaL_prepbuffsize(B, LUAL_BUFFERSIZE)

#undef luaL_checkversion
#define luaL_checkversion(L) \
	luaL_checkversion_(L, LUA_VERSION_NUM, LUAL_NUMSIZES)

#undef luaL_newlib
#define luaL_newlib(L,l)  \
	(luaL_checkversion(L), luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

#undef luaL_loadfile
#define luaL_loadfile(L,f) \
	luaL_loadfilex(L,f,NULL)

#undef luaL_dofile
#define luaL_dofile(L, fn) \
	(luaL_loadfile(L, fn) || lua_pcall(L, 0, LUA_MULTRET, 0))


/*
** {============================================================
** Compatibility with deprecated conversions
** =============================================================
*/
#if defined(LUA_COMPAT_APIINTCASTS)

#undef luaL_checkunsigned
#define luaL_checkunsigned(L,a) \
	((lua_Unsigned)luaL_checkinteger(L,a))

#undef luaL_optunsigned
#define luaL_optunsigned(L,a,d)	\
	((lua_Unsigned)luaL_optinteger(L,a,(lua_Integer)(d)))

#undef luaL_checkint
#define luaL_checkint(L,n) \
	((int)luaL_checkinteger(L, (n)))

#undef luaL_optint
#define luaL_optint(L,n,d) \
	((int)luaL_optinteger(L, (n), (d)))

#undef luaL_checklong
#define luaL_checklong(L,n) \
	((long)luaL_checkinteger(L, (n)))

#undef luaL_optlong
#define luaL_optlong(L,n,d) \
	((long)luaL_optinteger(L, (n), (d)))

#endif /* #if defined(LUA_COMPAT_APIINTCASTS) */

#endif /* #ifndef HTTPLUAX_PRIVATE */
 
#endif /* HTTPLUAX_H */
