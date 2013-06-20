#include <sys/module.h>
#include <sys/device.h>

#include <sys/mallocvar.h>
#include <sys/ioctl.h>

#include <dev/pci/pcidevs.h>

#include "luahw.h"
#include "lautoc.h"

#include "ioconf.c"

#define NAME xgifb_cd.cd_name /* files.pci's symbol */
MALLOC_DECLARE(M_DEVBUF);
extern int luaioctl(dev_t, u_long, void *, int, struct lwp *);
extern struct table_methods *struct_methods;
extern int num_struct_methods;

const struct pci_matchid xgifb_devices[] = {
  { PCI_VENDOR_XGI, PCI_PRODUCT_XGI_VOLARI_Z7 },
  { PCI_VENDOR_XGI, PCI_PRODUCT_XGI_VOLARI_Z9M },
  { PCI_VENDOR_XGI, PCI_PRODUCT_XGI_VOLARI_Z11 }
};
typedef struct xgifb_softc {
  lua_State *L;
  bus_space_tag_t sc_iot;
  bus_space_handle_t sc_ioh;
} xgifb_softc;

static struct xgifb_softc xgifbcn;

static int
xgifb_match(device_t parent, cfdata_t match, void *aux)
{
  lua_State *L = xgifbcn.L;
  int res;

  struct pci_attach_args *pa = (struct pci_attach_args *) aux;

  lua_getglobal(L, "xgifbMatch");
  if (!lua_isfunction(L, -1))
    return 0;

  lua_pushlightuserdata(L, pa);
  lua_pushlightuserdata(L, xgifb_devices);
  lua_pushinteger(L, __arraycount(xgifb_devices));
  lua_pcall(L, 3, 1, 0);
  res = lua_tointeger(L, -1); lua_pop(L, 1);
  return res;
}
static void
xgifb_attach(device_t parent, device_t self, void *aux)
{
  lua_State *L = xgifbcn.L;
  int n;

  struct xgifb_softc *sc = device_private(self);
  struct pci_attach_args *const pa = (struct pci_attach_args *) aux;

  lua_getglobal(L, "xgifbAttach");
  if (!lua_isfunction(L, -1))
    return;

  luaA_struct(L, xgifb_softc);
  luaA_struct_member(L, xgifb_softc, sc_iot, void*);
  luaA_struct_member(L, xgifb_softc, sc_ioh, void*);

  /* Metatable for access to the C struct */
  lua_createtable(L, 1, 0);
  /* Locked in a top table */
  lua_createtable(L, num_struct_methods, 0);
  for (n = 0; n < num_struct_methods; n++) {
    lua_pushcfunction(L, struct_methods[n].f);
    lua_setfield(L, -2, struct_methods[n].n);
  }
  lua_setmetatable(L, -2);
  /* Accesors bindings */
  lua_pushinteger(L, luaA_type_find("xgifb_softc"));
  lua_pushlightuserdata(L, sc);
  lua_pushnil(L);
  /* */
  lua_pushlightuserdata(L, pa);
  lua_pcall(L, 2, 0, 0);
}
CFATTACH_DECL_NEW(xgifb, sizeof(struct xgifb_softc), xgifb_match,
		  xgifb_attach, NULL, NULL);

MODULE(MODULE_CLASS_DRIVER, xgifb, "pci,lua");
static int
xgifb_modcmd(modcmd_t cmd, void *opaque)
{
  int ret = 0;
  struct lua_load l = {
    .state = "xgifb",
  };
  static klua_State *K;

  switch (cmd) {
  case MODULE_CMD_INIT:
    if (!(K = klua_newstate(lua_alloc, M_DEVBUF, NAME,
				     "XGI Volari frame buffer"))) {
      aprint_error("%s: LUACREATE\n", NAME);
      return -1;
    }
    xgifbcn.L = K->L;

    snprintf(l.path, MAXPATHLEN, "%s/xgifb/xgifb.lbc", module_base);
    if (luaioctl(0, LUALOAD, &l, 0, NULL)) {
      snprintf(l.path, MAXPATHLEN, "%s/xgifb/xgifb.lua", module_base);
      if (luaioctl(0, LUALOAD, &l, 0, NULL)) {
	aprint_error("%s: LUALOAD\n", NAME);
	goto fail;
      }
    }
    K->ks_prot = true;

    ret = config_init_component(cfdriver_ioconf_xgifb,
				    cfattach_ioconf_xgifb, cfdata_ioconf_xgifb);
    break;
  case MODULE_CMD_FINI:
    if (!(ret = config_fini_component(cfdriver_ioconf_xgifb,
				      cfattach_ioconf_xgifb,
				      cfdata_ioconf_xgifb)))
      klua_close(K);
    break;
  default:
    ret = ENOTTY;
  }

  return ret;
 fail:
  klua_close(K);
  return -1;
}
