#include <sys/lua.h>

struct table_methods {
  const char *n;
  int (*f)(lua_State *);
};
void *get_instance_ptr(lua_State*);
