#include <sys/module.h>

#include <dev/pci/pcireg.h>

#include "luahw.h"

MODULE(MODULE_CLASS_LUA_BINDING, luahw, "lua");

static int
pci_matchbyid(lua_State *L)
{
  struct pci_attach_args *pa;
  const struct pci_matchid *ids;
  int nent;

  const struct pci_matchid *pm;
  int i;

  pa = lua_touserdata(L, -3);
  ids = lua_touserdata(L, -2);
  nent = lua_tointeger(L, -1);
  lua_pop(L, 3);

  for (i = 0, pm = ids; i < nent; i++, pm++)
    if (PCI_VENDOR(pa->pa_id) == pm->pm_vid &&
	PCI_PRODUCT(pa->pa_id) == pm->pm_pid) {
      lua_pushinteger(L, 1);
      return 1;
    }
  lua_pushinteger(L, 0);
  return 1;
}
static void
aprint_devinfo(lua_State *L)
{
  struct pci_attach_args *pa = lua_touserdata(L, -1);
  pci_aprint_devinfo(pa, NULL);
  lua_pop(L, -1);
}

int
luaopen_hw(void *ls)
{
  lua_State *L = (lua_State *)ls;
  int n, nfunc;
  struct table_methods methods[] = {
    { "pci_matchbyid", pci_matchbyid },
    { "pci_aprint_devinfo", aprint_devinfo }
  };

  nfunc = __arraycount(methods);
  lua_createtable(L, nfunc, 0);
  for (n = 0; n < nfunc; n++) {
    lua_pushcfunction(L, methods[n].f);
    lua_setfield(L, -2, methods[n].n);
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
