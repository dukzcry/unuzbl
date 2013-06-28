#include <sys/lua.h>
#include "lautoc.h"

typedef void* void_addr_ptr;
struct table_methods {
  const char *n;
  int (*f)(lua_State *);
};
typedef struct boiler_binds {
  int type;
  void *data;
} boiler_binds;
void boiler_func(lua_State*, const char *, void *);
int luaA_push_addr_void_ptr(lua_State* L, luaA_Type, const void* c_in);
int luaA_push_boiler(lua_State* L, luaA_Type, const void* c_in);
