#include <sys/param.h>

#include "lautoc.h"
#include "lautoc_helpers.h"

static int get_instance_ptr_idx(lua_State* L, int index) {
  /* Stupid! */
  while (!(lua_isuserdata(L, index) && lua_isnumber(L, index-1))) index--;
  return index;
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
void boilerplate_func(lua_State* L, const char *type, void *obj) {
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
