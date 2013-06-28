#include <sys/param.h>

#include "lautoc_helpers.h"

static int get_instance_ptr_idx(lua_State* L, int index) {
  /* Stupid! */
  while (!(lua_isuserdata(L, index) && lua_touserdata(L, index) == &boiler_func)
	 && -index < LUAI_MAXCSTACK) index--;
  return index-1;
}
static int index_func(lua_State* L) {
  const char* membername = lua_tostring(L, -1);
  int instance_idx = get_instance_ptr_idx(L, -2);
  return luaA_struct_push_member_name_typeid(L, lua_tointeger(L, instance_idx-1),
					     lua_touserdata(L, instance_idx), membername);
}
static int newindex_func(lua_State* L) {
  const char* membername = lua_tostring(L, -2);
  int instance_idx = get_instance_ptr_idx(L, -3);
  luaA_struct_to_member_name_typeid(L, lua_tointeger(L, instance_idx-1), 
				    lua_touserdata(L, instance_idx), membername, -1);
  return 0;
}
static struct table_methods struct_methods[] = {
  { "__index", index_func },
  { "__newindex", newindex_func }
};
static int num_struct_methods = __arraycount(struct_methods);
void boiler_func(lua_State* L, const char *type, void *obj) {
  int n;

  /* Metatable for access to the C struct */
  lua_createtable(L, 1, 0);
  /* Locked in a top table */
  lua_createtable(L, num_struct_methods, 0);
  for (n = 0; n < num_struct_methods; n++) {
    lua_pushcfunction(L, struct_methods[n].f);
    lua_setfield(L, -2, struct_methods[n].n);
  }
  lua_setmetatable(L, -2);
  /* */
}

int luaA_push_addr_void_ptr(lua_State* L, luaA_Type type_id,const void* c_in) {
  lua_pushlightuserdata(L, *(void_addr_ptr**)&c_in);                                                                           
  return 1;
}
int luaA_push_boiler(lua_State* L, luaA_Type t, const void* c_in) {
  boiler_binds* p = (boiler_binds*)c_in;
  lua_pushinteger(L, p->type);
  lua_pushlightuserdata(L, p->data);
  lua_pushlightuserdata(L, &boiler_func);
  return 3;
}
