#include <sys/lua.h>

struct table_methods {
  const char *n;
  int (*f)(lua_State *);
};
void boilerplate_func(lua_State*, const char *, void *);
