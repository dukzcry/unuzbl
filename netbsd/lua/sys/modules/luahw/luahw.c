#include <sys/module.h>

#include <dev/pci/pcireg.h>

#include "luahw.h"
#include "lautoc.h"

MODULE(MODULE_CLASS_MISC, luahw, "lua");

static int c_call(lua_State* L) {
  return luaA_call_name(L, lua_tostring(L, 1));
}

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
static int
aprint_devinfo(lua_State *L)
{
  struct pci_attach_args *pa = lua_touserdata(L, -1);
  pci_aprint_devinfo(pa, NULL);
  lua_pop(L, 1);
  return 0;
}
static int
mapreg_map(lua_State *L)
{
  int res;
  bus_size_t size;

  res = pci_mapreg_map(lua_touserdata(L, -6),
		       PCI_MAPREG_START + lua_tointeger(L, -5),
		       lua_tointeger(L, -4), lua_tointeger(L, -3),
		       lua_touserdata(L, -2), lua_touserdata(L, -1),
		       NULL, &size);
  lua_pop(L, 6);
  lua_pushinteger(L, res);
  lua_pushinteger(L, size);
  return 2;
}
static int
space_unmap(lua_State *L)
{
  bus_space_unmap(*(bus_space_tag_t *) lua_touserdata(L, -3),
		  *(bus_space_handle_t *) lua_touserdata(L, -2), 
		  lua_tointeger(L, -1));
  lua_pop(L, 3);
  return 0;
}

int
luaopen_hw(void *ls)
{
  lua_State *L = (lua_State *)ls;
  int n, nfunc;
  struct table_methods methods[] = {
    { "c_call", c_call },
    { "pci_matchbyid", pci_matchbyid },
    { "pci_aprint_devinfo", aprint_devinfo },
    { "pci_mapreg_map", mapreg_map },
    { "bus_space_unmap", space_unmap }
  };

  nfunc = __arraycount(methods);
  lua_createtable(L, nfunc, 0);
  for (n = 0; n < nfunc; n++) {
    lua_pushcfunction(L, methods[n].f);
    lua_setfield(L, -2, methods[n].n);
  }

  lua_pushinteger(L, PCI_MAPREG_TYPE_MEM);
  lua_setfield(L, -2, "PCI_MAPREG_TYPE_MEM");
  lua_pushinteger(L, PCI_MAPREG_TYPE_IO);
  lua_setfield(L, -2, "PCI_MAPREG_TYPE_IO");
  lua_pushinteger(L, BUS_SPACE_MAP_LINEAR);
  lua_setfield(L, -2, "BUS_SPACE_MAP_LINEAR");
  lua_pushinteger(L, BUS_SPACE_MAP_PREFETCHABLE);
  lua_setfield(L, -2, "BUS_SPACE_MAP_PREFETCHABLE");

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
