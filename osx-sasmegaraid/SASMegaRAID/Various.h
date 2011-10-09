#ifndef _VARS_
#define _VARS_

#include "OSDepend.h"

/* Debug purpose stuff */
#define MRAID_D_CMD                             0x0001
#define MRAID_D_INTR                            0x0002
#define MRAID_D_MISC                            0x0004
#define MRAID_D_RW                              0x0020

/* Generic purpose constants */
#define MRAID_FRAME_SIZE                        64
#define MRAID_SENSE_SIZE                        128
#define MRAID_INVALID_CTX                       0xffffffff
#define MRAID_OSTS_INTR_VALID                   0x00000002
#define MRAID_OSTS_PPC_INTR_VALID               0x80000000
#define MRAID_OSTS_GEN2_INTR_VALID              (0x00000001 | 0x00000004)

/* Firmware states */
#define MRAID_STATE_MASK                        0xf0000000
#define MRAID_STATE_UNDEFINED                   0x00000000
#define MRAID_STATE_BB_INIT                     0x10000000 /* BBU? */
#define MRAID_STATE_WAIT_HANDSHAKE              0x60000000
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

/* Command opcodes */
#define MRAID_CMD_LD_SCSI_IO                    0x03

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

/* Mailbox bytes in direct command */
#define MRAID_MBOX_SIZE                         12

/* Driver definitions */
#define MRAID_MAX_LD                            64
#define MRAID_PREFER_MSI                        TRUE

/* Note of packed attr: The packed attr specifies that a var of structure field
 * should have the smallest possible alignment - one byte for a var, and one bit
 * for a field, unless you specify a larger value with the aligned attr. (GCC info) */

/* Sense buffer */
struct mraid_sense {
    UInt8                           mse_data[MRAID_SENSE_SIZE];
} __attribute__((packed));
/* Scatter gather access elements */
struct mraid_sg32 {
    UInt32                          addr;
    UInt32                          len;
} __attribute__((packed));
struct mraid_sg64 {
    UInt64                          addr;
    UInt32                          len;
} __attribute__((packed));
union mraid_sgl {
    struct mraid_sg32               sg32[1];
    struct mraid_sg64               sg64[1];
} __attribute__((packed));

/* Message frame */
struct mraid_iokitframe_header {
    UInt8                           mrh_cmd;
    UInt8                           mrh_cmd_status;
    UInt64                          mrh_data_len;
};
// TO-DO: TEMPRORARY!
struct mraid_frame_header {
    UInt8                           mrh_cmd;
    UInt8                           mrh_sense_len;
    UInt8                           mrh_cmd_status;
    UInt8                           mrh_scsi_status;
    UInt8                           mrh_target_id;
    UInt8                           mrh_lun_id;
    UInt8                           mrh_cdb_len;
    UInt8                           mrh_sg_count;
    UInt8                           mrh_context;
    UInt8                           mrh_pad0;
    UInt8                           mrh_flags;
    UInt8                           mrh_timeout;
    UInt8                           mrh_data_len;
} __attribute__((packed));
//
struct mraid_init_frame {
    struct mraid_frame_header       mif_header;
    UInt32                          mif_qinfo_new_addr_lo;
    UInt32                          mif_qinfo_new_addr_hi;
    UInt32                          mif_qinfo_old_addr_lo;
    UInt32                          mif_qinfo_old_addr_hi;
    UInt32                          mif_reserved[6];
} __attribute__((packed));
struct mraid_io_frame {
    struct mraid_frame_header       mif_header;
    UInt32                          mif_sense_addr_lo;
    UInt32                          mif_sense_addr_hi;
    UInt32                          mif_lba_lo;
    UInt32                          mif_lba_hi;
    union mraid_sgl                 mif_sgl;
} __attribute__((packed));
#define MRAID_PASS_FRAME_SIZE       48
struct mraid_pass_frame {
    struct mraid_frame_header       mpf_header;
    UInt32                          mpf_sense_addr_lo;
    UInt32                          mpf_sense_addr_hi;
    UInt8                           mpf_cdb[16];
    union mraid_sgl                 mpf_sgl;
} __attribute__((packed));
struct mraid_dcmd_frame {
    struct mraid_frame_header       mdf_header;
    UInt32                          mdf_opcode;
    UInt8                           mdf_mbox[MRAID_MBOX_SIZE];
    union mraid_sgl                 mdf_sgl;
} __attribute__((packed));
struct mraid_abort_frame {
    struct mraid_frame_header       maf_header;
    UInt32                          maf_abort_context;
    UInt32                          maf_pad;
    UInt32                          maf_abort_mraid_addr_lo;
    UInt32                          maf_abort_mraid_addr_hi;
    UInt32                          maf_reserved[6];
} __attribute__((packed));
struct mraid_smp_frame {
    struct mraid_frame_header       msf_header;
    UInt64                          msf_sas_addr;
    union {
        struct mraid_sg32           sg32[2];
        struct mraid_sg64           sg64[2];
    } msf_sgl;
} __attribute__((packed));
struct mraid_stp_frame {
    struct mraid_frame_header       msf_header;
    UInt16                          msf_fis[10];
    UInt32                          msf_stp_flags;
    union {
        struct mraid_sg32           sg32[2];
        struct mraid_sg64           sg64[2];
    } msf_sgl;
} __attribute__((packed));

union mraid_frame {
    struct mraid_frame_header       mrr_header;
    struct mraid_init_frame         mrr_init;
    struct mraid_io_frame           mrr_io;
    struct mraid_pass_frame         mrr_pass;
    struct mraid_dcmd_frame         mrr_dcmd;
    struct mraid_abort_frame        mrr_abort;
    struct mraid_smp_frame          mrr_smp;
    struct mraid_stp_frame          mrr_stp;
    UInt8                           mrr_bytes[MRAID_FRAME_SIZE];
};

struct mraid_mem {
    //TO-DO: Replace bus_dma* vars with IOBufferMemoryDescriptor
    //bus_dmamap_t                 am_map;
    //bus_dma_segment_t            am_seg;
    size_t                          am_size;
    caddr_t                         am_kva;
};

#define MRAIDMEM_KVA(_am)           ((mraid_prod_cons *)(_am)->am_kva)

struct mraid_prod_cons {
    UInt32                          mpc_producer;
    UInt32                          mpc_consumer;
    UInt32                          mpc_reply_q[1]; /* Compensate for 1 extra reply per spec */
};

enum mraid_iop {
    MRAID_IOP_XSCALE,
    MRAID_IOP_PPC,
    MRAID_IOP_GEN2
};
struct	mraid_pci_device {
	UInt16							mpd_vendor;
	UInt16							mpd_product;
	enum mraid_iop					mpd_iop;
	const struct mraid_pci_subtype	*mpd_subtype;
};
struct mraid_pci_subtype {
	UInt32		st_id;
	const char	*st_string;
};
namespace mraid_structs {
static const struct mraid_pci_subtype mraid_1078_subtypes[] = {
	{ 0x10061000,	"SAS 8888ELP" },
	{ 0x100a1000,	"SAS 8708ELP" },
	{ 0x100f1000,	"SAS 8708E" },
	{ 0x10121000,	"SAS 8704ELP" },
	{ 0x10131000,	"SAS 8708EM2" },
	{ 0x10161000,	"SAS 8880EM2" },
	{ 0x1f0a1028,	"Dell PERC 6/e" },
	{ 0x1f0b1028,	"Dell PERC 6/i" },
	{ 0x1f0d1028,	"Dell CERC 6/i" },
	{ 0x1f0c1028,	"Dell PERC 6/i integrated" },
	{ 0x1f111028,	"Dell CERC 6/i integrated" },
	{ 0x0,		"" }
};
static const struct mraid_pci_subtype mraid_perc5_subtypes[] = {
	{ 0x1f011028,	"Dell PERC 5/e" },
	{ 0x1f021028,	"Dell PERC 5/i" },
	{ 0x0,		"" }
};
static const struct mraid_pci_subtype mraid_gen2_subtypes[] = {
	{ 0x1f151028,	"Dell PERC H800 Adapter" },
	{ 0x1f161028,	"Dell PERC H700 Adapter" },
	{ 0x1f171028,	"Dell PERC H700 Integrated" },
	{ 0x1f181028,	"Dell PERC H700 Modular" },
	{ 0x1f191028,	"Dell PERC H700" },
	{ 0x1f1a1028,	"Dell PERC H800 Proto Adapter" },
	{ 0x1f1b1028,	"Dell PERC H800" },
	{ 0x0,		"" }
};
static const struct mraid_pci_device mraid_pci_devices[] = {
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_SAS,
	  MRAID_IOP_XSCALE,	NULL },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_MEGARAID_VERDE_ZCR,
	  MRAID_IOP_XSCALE,	NULL },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS1078,
	  MRAID_IOP_PPC,		mraid_1078_subtypes },
	{ PCI_VENDOR_SYMBIOS,   PCI_PRODUCT_SYMBIOS_SAS1078DE,
	  MRAID_IOP_PPC,		mraid_1078_subtypes },
	{ PCI_VENDOR_DELL,		PCI_PRODUCT_DELL_PERC5,
	  MRAID_IOP_XSCALE,		mraid_perc5_subtypes },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2108_1,
	  MRAID_IOP_GEN2,		mraid_gen2_subtypes },
	{ PCI_VENDOR_SYMBIOS,	PCI_PRODUCT_SYMBIOS_SAS2108_2,
	  MRAID_IOP_GEN2,		mraid_gen2_subtypes },
};
}

#endif /* ifndef */

/* ... struct mraid_ccb ... */
//SLIST_ENTRY(mraid_ccb)          ccb_link;
/* The first arg is a head name */
//SLIST_HEAD(mraid_ccb_list, mraid_ccb);
/*                          */
