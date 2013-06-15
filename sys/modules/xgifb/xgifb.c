#include <sys/module.h>
#include <sys/device.h>

#include <sys/lua.h>
#include <sys/mallocvar.h>
#include <sys/ioctl.h>

#include <dev/pci/pcidevs.h>
#include "luahw.h"

#include "ioconf.c"

#define NAME xgifb_cd.cd_name /* files.pci's symbol */
MALLOC_DECLARE(M_DEVBUF);

extern int luaioctl(dev_t, u_long, void *, int, struct lwp *);
static struct {
  klua_State *K;
  lua_State *L;
} xgifb_glbl;
const struct pci_matchid xgifb_devices[] = {
  { PCI_VENDOR_XGI, PCI_PRODUCT_XGI_VOLARI_Z7 },
  { PCI_VENDOR_XGI, PCI_PRODUCT_XGI_VOLARI_Z9M },
  { PCI_VENDOR_XGI, PCI_PRODUCT_XGI_VOLARI_Z11 }
};

struct xgifb_softc {
  device_t sc_dev;

  bus_space_tag_t sc_iot;
  bus_space_handle_t sc_ioh;
};

static int
xgifb_match(device_t parent, cfdata_t match, void *aux)
{
  lua_State *L = xgifb_glbl.L;
  struct pci_attach_args *pa = (struct pci_attach_args *) aux;

  lua_getglobal(L, "xgifbMatch");
  if (!lua_isfunction(L, -1))
    return 0;

  lua_pushlightuserdata(L, pa);
  lua_pushlightuserdata(L, xgifb_devices);
  lua_pushinteger(L, __arraycount(xgifb_devices));
  lua_pcall(L, 3, 1, 0);
  return lua_tointeger(L, -1);
}
static void
xgifb_attach(device_t parent, device_t self, void *aux)
{
  lua_State *L = xgifb_glbl.L;
  struct xgifb_softc *sc = device_private(self);
  struct pci_attach_args *const pa = (struct pci_attach_args *) aux;

  lua_getglobal(L, "xgifbAttach");
  if (!lua_isfunction(L, -1))
    return;

  lua_pushlightuserdata(L, &parent);
  lua_pushlightuserdata(L, sc);
  lua_pushlightuserdata(L, pa);
  lua_pcall(L, 3, 0, 0);
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
  klua_State *K;

  switch (cmd) {
  case MODULE_CMD_INIT:
    if (!(K = klua_newstate(lua_alloc, M_DEVBUF, NAME,
				     "XGI Volari frame buffer"))) {
      aprint_error("%s: LUACREATE\n", NAME);
      return -1;
    }
    xgifb_glbl.K = K; /* for nextcoming calls */
    xgifb_glbl.L = K->L;

    snprintf(l.path, MAXPATHLEN, "%s/xgifb/xgifb.lua", module_base);
    if (luaioctl(0, LUALOAD, &l, 0, NULL)) {
      aprint_error("%s: LUALOAD\n", NAME);
      goto fail;
    }
    K->ks_prot = true;

    ret = config_init_component(cfdriver_ioconf_xgifb,
				    cfattach_ioconf_xgifb, cfdata_ioconf_xgifb);
    break;
  case MODULE_CMD_FINI:
    if (!(ret = config_fini_component(cfdriver_ioconf_xgifb,
				      cfattach_ioconf_xgifb,
				      cfdata_ioconf_xgifb)))
      klua_close(xgifb_glbl.K);
    break;
  default:
    ret = ENOTTY;
  }

  return ret;
 fail:
  klua_close(K);
  return -1;
}
