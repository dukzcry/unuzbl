#include <sys/module.h>

#include <sys/lua.h>

#include <dev/pci/pcireg.h>

MODULE(MODULE_CLASS_LUA_BINDING, luahw, "lua");

struct hw_reg {
  const char *n;
  int (*f)(lua_State *);
};

static int
pci_vendor(lua_State *L)
{
  int arg, res;

  arg = lua_tointeger(L, 1);
  res = PCI_VENDOR(arg);
  lua_pushinteger(L, res);
  return 1;
}
static int
pci_product(lua_State *L)
{
  int arg, res;

  arg = lua_tointeger(L, 1);
  res = PCI_PRODUCT(arg);
  lua_pushinteger(L, res);
  return 1;
}

int
luaopen_hw(void *ls)
{
  lua_State *L = (lua_State *)ls;
  int n, nfunc;
  struct hw_reg hw[] = {
    { "PCI_VENDOR", pci_vendor },
    { "PCI_PRODUCT", pci_product },
  };

  nfunc = __arraycount(hw);
  lua_createtable(L, nfunc, 0);
  for (n = 0; n < nfunc; n++) {
    lua_pushcfunction(L, hw[n].f);
    lua_setfield(L, -2, hw[n].n);
  }

  lua_setglobal(L, "hw");
  return 1;
}
static int
luahw_modcmd(modcmd_t cmd, void *opaque)
{
  int error;
  switch (cmd) {
  case MODULE_CMD_INIT:
    error = lua_mod_register("hw", luaopen_hw);
    break;
  case MODULE_CMD_FINI:
    error = lua_mod_unregister("hw");
    break;
  default:
    error = ENOTTY;
  }
  return error;
}
