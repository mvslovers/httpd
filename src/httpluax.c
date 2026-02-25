/* 
 * HTTPLUAX.C - Loads all Lua functions into struct of Lua function
 * pointers that can then be called via pointer to function pointers.
 */
#define HTTPLUAX_PRIVATE	/* we don't want the defines */
#include "httpluax.h"

LUAX httpluax = {
	lua_ident,				lua_newstate,			lua_close,
	lua_newthread,			lua_closethread,		lua_resetthread,
	lua_atpanic,			lua_version,			lua_absindex,
	lua_gettop,				lua_settop,				lua_pushvalue,
	lua_rotate,				lua_copy,				lua_checkstack,
	lua_xmove,				lua_isnumber,			lua_isstring,
	lua_iscfunction,		lua_isinteger,			lua_isuserdata,
	lua_type,				lua_typename,			lua_tonumberx,
	lua_tointegerx,			lua_toboolean,			lua_tolstring,
	lua_rawlen,				lua_tocfunction,		lua_touserdata,
	lua_tothread,			lua_topointer,			lua_rawequal,
	lua_compare,			lua_pushnil,			lua_pushnumber,
	lua_pushinteger,		lua_pushlstring,		lua_pushstring,
	lua_pushvfstring,		lua_pushfstring,		lua_pushcclosure,
	lua_pushboolean,		lua_pushlightuserdata,	lua_pushthread,
	lua_getglobal,			lua_gettable,			lua_getfield,
	lua_geti,				lua_rawget,				lua_rawgeti,
	lua_rawgetp,			lua_createtable,		lua_newuserdatauv,
	lua_getmetatable,		lua_getiuservalue,		lua_setglobal,
	lua_settable,			lua_setfield,			lua_seti,
	lua_rawset,				lua_rawseti,			lua_rawsetp,
	lua_setmetatable,		lua_setiuservalue,		lua_callk,
	lua_pcallk,				lua_load,				lua_dump,
	lua_yieldk,				lua_resume,				lua_status,
	lua_isyieldable,		lua_setwarnf,			lua_warning,
	lua_gc,					lua_error,				lua_next,
	lua_concat,				lua_len,				lua_stringtonumber,
	lua_getallocf,			lua_setallocf,			lua_toclose,
	lua_closeslot,			lua_getstack,			lua_getinfo,
	lua_getlocal,			lua_setlocal,			lua_getupvalue,
	lua_setupvalue,			lua_upvalueid,			lua_upvaluejoin,
	lua_sethook,			lua_gethook,			lua_gethookmask,
	lua_gethookcount,		lua_setcstacklimit,		lua_arith,
	(void*)0,				(void*)0,				(void*)0,
	/* 0x198 (408 bytes) */
	/* LAUXLIB */
	luaL_checkversion_,		luaL_getmetafield,		luaL_callmeta,
	luaL_tolstring,			luaL_argerror,			luaL_typeerror,
	luaL_checklstring,		luaL_optlstring,		luaL_checknumber,
	luaL_optnumber,			luaL_checkinteger,		luaL_optinteger,
	luaL_checkstack,		luaL_checktype,			luaL_checkany,
	luaL_newmetatable,		luaL_setmetatable,		luaL_testudata,
	luaL_checkudata,		luaL_where,				luaL_error,
	luaL_checkoption,		luaL_fileresult,		luaL_execresult,
	luaL_ref,				luaL_unref,				luaL_loadfilex,
	luaL_loadbufferx,		luaL_loadstring,		luaL_newstate,
	luaL_len,				luaL_addgsub,			luaL_gsub,
	luaL_setfuncs,			luaL_getsubtable,		luaL_traceback,
	luaL_requiref,			luaL_buffinit,			luaL_prepbuffsize,
	luaL_addlstring,		luaL_addstring,			luaL_addvalue,
	luaL_pushresult,		luaL_pushresultsize,	luaL_buffinitsize,
	luaL_openlibs,			(void*)0,				(void*)0 
	/* 0x258 (600 bytes */
};
