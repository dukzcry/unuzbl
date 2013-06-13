#include <sys/module.h>
#include <sys/device.h>

#include <sys/lua.h>
#include <sys/ioctl.h>

#include <dev/pci/pcivar.h>

#include "ioconf.c"

#define NAME xgifb_cd.cd_name /* files.pci's symbol */
#define PATH "./xgifb.lua"

extern int luaioctl(dev_t, u_long, void *, int, struct lwp *);

struct xgifb_softc {
  device_t sc_dev;

  bus_space_tag_t sc_iot;
  bus_space_handle_t sc_ioh;
};

static int
xgifb_match(device_t parent, cfdata_t match, void *aux)
{
  struct pci_attach_args *pa = (struct pci_attach_args *) aux;

  return 0;
}
static void
xgifb_attach(device_t parent, device_t self, void *aux)
{
  struct xgifb_softc *sc = device_private(self);
  struct pci_attach_args *const pa = (struct pci_attach_args *) aux;
}
CFATTACH_DECL_NEW(xgifb, sizeof(struct xgifb_softc), xgifb_match,
		  xgifb_attach, NULL, NULL);

MODULE(MODULE_CLASS_DRIVER, xgifb, "pci,lua");
static int
xgifb_modcmd(modcmd_t cmd, void *opaque)
{
  int ret = 0;
  struct lua_create cr = {
    .name = "xgifb",
    .desc = "\0"
  };
  struct lua_load l = {
    .state = "xgifb",
    .path = PATH
  };

  switch (cmd) {
  case MODULE_CMD_INIT:
    if (luaioctl(0, LUACREATE, &cr, 0, NULL)) {
      aprint_error("%s: LUACREATE", NAME);
      return -1;
    }
    if (luaioctl(0, LUALOAD, &l, 0, NULL)) {
      aprint_error("%s: LUALOAD", NAME);
      goto fail;
    }
    ret = config_init_component(cfdriver_ioconf_xgifb,
				    cfattach_ioconf_xgifb, cfdata_ioconf_xgifb);
    break;
  case MODULE_CMD_FINI:
    luaioctl(0, LUADESTROY, &cr, 0, NULL);
    ret = config_fini_component(cfdriver_ioconf_xgifb,
				  cfattach_ioconf_xgifb, cfdata_ioconf_xgifb);
    break;
  default:
    ret = ENOTTY;
  }

  return ret;
 fail:
  return luaioctl(0, LUADESTROY, &cr, 0, NULL);
}
