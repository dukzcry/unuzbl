#include <sys/lua.h>

typedef void* void_addr_ptr;
struct table_methods {
  const char *n;
  int (*f)(lua_State *);
};
void boilerplate_func(lua_State*, const char *, void *);
