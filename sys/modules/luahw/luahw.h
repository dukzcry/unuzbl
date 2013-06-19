#include <dev/pci/pcivar.h>

#include "lautoc_helpers.h"

struct pci_matchid {
  u_int16_t pm_vid;
  u_int16_t pm_pid;
};
