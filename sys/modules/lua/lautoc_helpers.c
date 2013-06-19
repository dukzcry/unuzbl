#include <sys/param.h>

#include "lautoc.h"
#include "lautoc_helpers.h"

int get_instance_ptr_idx(lua_State* L, int index) {
  /* Stupid! */
  while (!lua_isnil(L, index)) index--;
  return index-1;
}
int index_func(lua_State* L) {
  const char* membername = lua_tostring(L, -1);
  int instance_idx = get_instance_ptr_idx(L, -2);
  return luaA_struct_push_member_name_typeid(L, lua_tointeger(L, instance_idx-1),
					     lua_touserdata(L, instance_idx), membername);
}
int newindex_func(lua_State* L) {
  const char* membername = lua_tostring(L, -2);
  int instance_idx = get_instance_ptr_idx(L, -3);
  luaA_struct_to_member_name_typeid(L, lua_tointeger(L, instance_idx-1), 
				    lua_touserdata(L, instance_idx), membername, -1);
  return 0;
}

struct table_methods struct_methods[] = {
  { "__index", index_func },
  { "__newindex", newindex_func }
};
int num_struct_methods = __arraycount(struct_methods);
