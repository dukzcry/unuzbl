#include <sys/module.h>
#include <sys/device.h>

#include <dev/pci/pcivar.h>

#include "ioconf.c"

struct xgifb_softc {
  device_t sc_dev;

  bus_space_tag_t sc_iot;
  bus_space_handle_t sc_ioh;
};

static int
xgifb_match(device_t parent, cfdata_t match, void *aux)
{
  struct pci_attach_args *pa = (struct pci_attach_args *) aux;

  printf("\n%s\n", __FUNCTION__);

  return 0;
}
static void
xgifb_attach(device_t parent, device_t self, void *aux)
{
  struct xgifb_softc *sc = device_private(self);
  struct pci_attach_args *const pa = (struct pci_attach_args *) aux;
}
CFATTACH_DECL_NEW(xgifb, sizeof(struct xgifb_softc), xgifb_match, xgifb_attach, NULL, NULL);

MODULE(MODULE_CLASS_DRIVER, xgifb, NULL);
static int
xgifb_modcmd(modcmd_t cmd, void *opaque)
{
  int error = 0;
  
  switch (cmd) {
  case MODULE_CMD_INIT:
    error = config_init_component(cfdriver_ioconf_xgifb,
				    cfattach_ioconf_xgifb, cfdata_ioconf_xgifb);
    if (error)
      aprint_error("%s: unable to init component\n",
		     xgifb_cd.cd_name); /* files.pci's symbol */
    break;
  case MODULE_CMD_FINI:
    error = config_fini_component(cfdriver_ioconf_xgifb,
				  cfattach_ioconf_xgifb, cfdata_ioconf_xgifb);
    break;
  default:
    error = ENOTTY;
  }

  return error;
}
