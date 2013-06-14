#include <sys/module.h>

#include <sys/lua.h>

#include <dev/pci/pcireg.h>
#include "luahw.h"

MODULE(MODULE_CLASS_LUA_BINDING, luahw, "lua");

struct hw_reg {
  const char *n;
  int (*f)(lua_State *);
};

static int
pci_matchbyid(lua_State *L)
{
  struct pci_attach_args *pa;
  const struct pci_matchid *ids;
  int nent;

  const struct pci_matchid *pm;
  int i;

  pa = lua_touserdata(L, 1);
  ids = lua_touserdata(L, 2);
  nent = lua_tointeger(L, 3);

  for (i = 0, pm = ids; i < nent; i++, pm++)
    if (PCI_VENDOR(pa->pa_id) == pm->pm_vid &&
	PCI_PRODUCT(pa->pa_id) == pm->pm_pid) {
      lua_pushinteger(L, 1);
      return 1;
    }
  lua_pushinteger(L, 0);
  return 1;
}

int
luaopen_hw(void *ls)
{
  lua_State *L = (lua_State *)ls;
  int n, nfunc;
  struct hw_reg hw[] = {
    { "pci_matchbyid", pci_matchbyid },
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
