#include "OSDepend.h"

/* Generic purpose constants */
#define MRAID_FRAME_SIZE                        64
#define MRAID_SENSE_SIZE                        128
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
#define MRAID_FRAME_DONT_POST_IN_REPLY_QUEUE	0x0001
#define MRAID_FRAME_SGL32                       0x0000
#define MRAID_FRAME_SGL64                       0x0002
#define MRAID_FRAME_DIR_WRITE                   0x0008
#define MRAID_FRAME_DIR_READ                    0x0010

/* Command opcodes */
#define MRAID_CMD_INIT                          0x00
#define MRAID_CMD_DCMD                          0x05

/* Direct commands */
#define MRAID_DCMD_CTRL_GET_INFO                0x01010000

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

/* Mailbox bytes in direct command */
#define MRAID_MBOX_SIZE                         12

typedef enum {
	MRAID_STAT_OK =                 0x00,
} mraid_status_t;

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
struct mraid_init_frame {
    struct mraid_frame_header       mif_header;
    UInt64                          mif_qinfo_new_addr;
    UInt64                          mif_qinfo_old_addr;
    UInt32                          mif_reserved[6];
} __attribute__((packed));
/* Queue init */
struct mraid_init_qinfo {
    UInt32                          miq_flags;
    UInt32                          miq_rq_entries;
	UInt64                          miq_rq_addr;
	UInt64                          miq_pi_addr;
	UInt64                          miq_ci_addr;
} __attribute__((packed));
struct mraid_io_frame {
    struct mraid_frame_header       mif_header;
    UInt32                          mif_sense_addr_lo;
    UInt32                          mif_sense_addr_hi;
    UInt32                          mif_lba_lo;
    UInt32                          mif_lba_hi;
    union mraid_sgl                 mif_sgl;
} __attribute__((packed));
struct mraid_pass_frame {
    struct mraid_frame_header       mpf_header;
    UInt32                          mpf_sense_addr_lo;
    UInt32                          mpf_sense_addr_hi;
    UInt8                           mpf_cdb[16];
    union mraid_sgl                 mpf_sgl;
} __attribute__((packed));
#define MRAID_DCMD_FRAME_SIZE	40
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

struct mraid_prod_cons {
    UInt32                          mpc_producer;
    UInt32                          mpc_consumer;
    UInt32                          mpc_reply_q[1]; /* Compensate for 1 extra reply per spec */
};

/* mraid_ctrl_info */
struct mraid_ctrl_props {
	UInt16		mcp_seq_num;
	UInt16		mcp_pred_fail_poll_interval;
	UInt16		mcp_intr_throttle_cnt;
	UInt16		mcp_intr_throttle_timeout;
	UInt8			mcp_rebuild_rate;
	UInt8			mcp_patrol_read_rate;
	UInt8			mcp_bgi_rate;
	UInt8			mcp_cc_rate;
	UInt8			mcp_recon_rate;
	UInt8			mcp_cache_flush_interval;
	UInt8			mcp_spinup_drv_cnt;
	UInt8			mcp_spinup_delay;
	UInt8			mcp_cluster_enable;
	UInt8			mcp_coercion_mode;
	UInt8			mcp_alarm_enable;
	UInt8			mcp_disable_auto_rebuild;
	UInt8			mcp_disable_battery_warn;
	UInt8			mcp_ecc_bucket_size;
	UInt16		mcp_ecc_bucket_leak_rate;
	UInt8			mcp_restore_hotspare_on_insertion;
	UInt8			mcp_expose_encl_devices;
	UInt8			mcp_reserved[38];
} __attribute__((packed));
struct mraid_info_pci {
	UInt16		mip_vendor;
	UInt16		mip_device;
	UInt16		mip_subvendor;
	UInt16		mip_subdevice;
	UInt16		mip_reserved[24];
} __attribute__((packed));
/* Host interface info */
struct mraid_info_host {
	UInt8		mih_type;
	UInt8		mih_reserved[6];
	UInt8		mih_port_count;
	UInt64      mih_port_addr[8];
} __attribute__((packed));
/* Device interface info */
struct mraid_info_device {
	UInt8		mid_type;
	UInt8		mid_reserved[6];
	UInt8		mid_port_count;
	UInt8		mid_port_addr[8];
} __attribute__((packed));
/* Firmware component info */
struct mraid_info_component {
	char			mic_name[8];
	char			mic_version[32];
	char			mic_build_date[16];
	char			mic_build_time[16];
} __attribute__((packed));

/* MRAID_DCMD_CTRL_GETINFO */
struct mraid_ctrl_info {
	struct mraid_info_pci       mci_pci;
	struct mraid_info_host      mci_host;
	struct mraid_info_device	mci_device;
    
	/* Firmware components that are present and active. */
	UInt32		mci_image_check_word;
	UInt32		mci_image_component_count;
	struct mraid_info_component mci_image_component[8];
    
	/* Firmware components that have been flashed but are inactive */
	UInt32		mci_pending_image_component_count;
	struct mraid_info_component mci_pending_image_component[8];
    
	UInt8			mci_max_arms;
	UInt8			mci_max_spans;
	UInt8			mci_max_arrays;
	UInt8			mci_max_lds;
	char			mci_product_name[80];
	char			mci_serial_number[32];
	UInt32		mci_hw_present;
	UInt32		mci_current_fw_time;
	UInt16		mci_max_cmds;
	UInt16		mci_max_sg_elements;
	UInt32		mci_max_request_size;
	UInt16		mci_lds_present;
	UInt16		mci_lds_degraded;
	UInt16		mci_lds_offline;
	UInt16		mci_pd_present;
	UInt16		mci_pd_disks_present;
	UInt16		mci_pd_disks_pred_failure;
	UInt16		mci_pd_disks_failed;
	UInt16		mci_nvram_size;
	UInt16		mci_memory_size;
	UInt16		mci_flash_size;
	UInt16		mci_ram_correctable_errors;
	UInt16		mci_ram_uncorrectable_errors;
	UInt8			mci_cluster_allowed;
	UInt8			mci_cluster_active;
	UInt16		mci_max_strips_per_io;
    
	UInt32		mci_raid_levels;
    
	UInt32		mci_adapter_ops;
    
	UInt32		mci_ld_ops;
    
	struct {
		UInt8		min;
		UInt8		max;
		UInt8		reserved[2];
	} __attribute__((packed))		mci_stripe_sz_ops;
    
	UInt32		mci_pd_ops;
    
	UInt32		mci_pd_mix_support;
    
	UInt8			mci_ecc_bucket_count;
	UInt8			mci_reserved2[11];
	struct mraid_ctrl_props	mci_properties;
	char			mci_package_version[0x60];
	UInt8			mci_pad[0x800 - 0x6a0];
} __attribute__((packed));

/* MR_DCMD_LD_GET_LIST */
struct mraid_ld {
	UInt8			mld_target;
	UInt8			mld_res;
	UInt16		mld_seq;
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