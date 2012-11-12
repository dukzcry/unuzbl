#include "OSDepend.h"

/* Debug purpose stuff */
#define MRAID_D_CMD                             0x0001
#define MRAID_D_INTR                            0x0002
#define MRAID_D_MISC                            0x0004
#define MRAID_D_RW                              0x0020
#define MRAID_D_MEM                             0x0040

/* Generic purpose constants */
#define MRAID_FRAME_SIZE                        64
#define MRAID_SENSE_SIZE                        128
#define MRAID_INVALID_CTX                       0xffffffff
#define MRAID_OSTS_INTR_VALID                   0x00000002
#define MRAID_OSTS_PPC_INTR_VALID               0x80000000
#define MRAID_OSTS_GEN2_INTR_VALID              (0x00000001 | 0x00000004)
#define MRAID_OSTS_SKINNY_INTR_VALID            0x00000001

/* Firmware states */
#define MRAID_STATE_MASK                        0xf0000000
#define MRAID_STATE_UNDEFINED                   0x00000000
#define MRAID_STATE_BB_INIT                     0x10000000
#define MRAID_STATE_FW_INIT                     0x40000000
#define MRAID_STATE_WAIT_HANDSHAKE              0x60000000
#define MRAID_STATE_DEVICE_SCAN                 0x80000000
#define MRAID_STATE_FLUSH_CACHE                 0xa0000000
#define MRAID_STATE_READY                       0xb0000000
#define MRAID_STATE_OPERATIONAL                 0xc0000000
#define MRAID_STATE_FAULT                       0xf0000000
#define MRAID_STATE_MAXSGL_MASK                 0x00ff0000
#define MRAID_STATE_MAXCMD_MASK                 0x0000ffff

/* Command resets */
#define MRAID_INIT_READY                        0x00000002
#define MRAID_INIT_CLEAR_HANDSHAKE              0x00000008

/* Frame flags */
#define MRAID_FRAME_SGL32                       0x0000
#define MRAID_FRAME_SGL64                       0x0002

#define MRAID_PCI_MEMSIZE                       0x2000      /* 8k */

/* Vendor list */
#define	PCI_VENDOR_SYMBIOS						0x1000		/* Symbios Logic */
#define	PCI_VENDOR_DELL							0x1028		/* Dell */
/* Product list */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_SAS		0x0411		/* MegaRAID SAS 1064R */
#define	PCI_PRODUCT_SYMBIOS_MEGARAID_VERDE_ZCR	0x0413		/* MegaRAID Verde ZCR */
#define	PCI_PRODUCT_SYMBIOS_SAS1078				0x0060		/* SAS1078 */
#define PCI_PRODUCT_SYMBIOS_SAS1078DE           0x007c      /* SAS1078DE */
#define	PCI_PRODUCT_DELL_PERC5					0x0015		/* PERC 5 */
#define	PCI_PRODUCT_SYMBIOS_SAS2108_1			0x0078		/* MegaRAID SAS2108 CRYPTO GEN2 */
#define	PCI_PRODUCT_SYMBIOS_SAS2108_2			0x0079		/* MegaRAID SAS2108 GEN2 */
#define PCI_PRODUCT_SYMBIOS_SAS2008_1           0x0073      /* MegaRAID SAS2008 */

/* Scatter gather access elements */
struct mraid_sg32 {
    UInt32                          addr;
    UInt32                          len;
} __attribute__((packed));
struct mraid_sg64 {
    UInt64                          addr;
    UInt32                          len;
} __attribute__((packed));

enum mraid_iop {
    MRAID_IOP_XSCALE,
    MRAID_IOP_PPC,
    MRAID_IOP_GEN2,
    MRAID_IOP_SKINNY
};
struct	mraid_pci_device {
	UInt16							mpd_vendor;
	UInt16							mpd_product;
	enum mraid_iop					mpd_iop;
	const struct mraid_pci_subtype	*mpd_subtype;
};
namespace mraid_structs {
    static const struct mraid_pci_device mraid_pci_devices[] = {
        { PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_SAS,
            MRAID_IOP_XSCALE	},
        { PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_VERDE_ZCR,
            MRAID_IOP_XSCALE },
        { PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1078,
            MRAID_IOP_PPC },
        { PCI_VENDOR_SYMBIOS,   PCI_PRODUCT_SYMBIOS_SAS1078DE,
            MRAID_IOP_PPC },
        { PCI_VENDOR_DELL,		PCI_PRODUCT_DELL_PERC5,
            MRAID_IOP_XSCALE },
        { PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2108_1,
            MRAID_IOP_GEN2 },
        { PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2108_2,
            MRAID_IOP_GEN2 },
        { PCI_VENDOR_SYMBIOS,   PCI_PRODUCT_SYMBIOS_SAS2008_1,
            MRAID_IOP_SKINNY }
    };
}