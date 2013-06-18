#include <dev/pci/pcivar.h>

struct table_methods {
  const char *n;
  int (*f)(lua_State *);
};
struct pci_matchid {
  u_int16_t pm_vid;
  u_int16_t pm_pid;
};
