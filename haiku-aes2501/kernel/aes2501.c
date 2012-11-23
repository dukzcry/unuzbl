/* Dumb driver which does init */

#include "aes2501_common.h"

#include <string.h>

#include <device_manager.h>
#include <USB3.h>

//#define COMPACT_DRIVER 1

//#define TRACE_AES 1
#ifdef TRACE_AES
#       define TRACE(x...) dprintf("aes2501: " x)
#else
#       define TRACE(x...) ;
#endif

#define INPUT_AES_MODULE_NAME "drivers/input/aes2501/driver_v1"
#define INPUT_AES_DEVICE_MODULE_NAME "drivers/input/aes2501/device_v1"

/* Base Namespace devices are published to */
#define INPUT_AES_BASENAME "input/aes2501/%d"

// name of pnp generator of path ids
#define INPUT_AES_PATHID_GENERATOR "aes2501/path_id"


/* Missing things: serialisation on device obtain/release */


/* usb module isn't ready for the new driver API :( */
typedef struct {
	device_node *node;
	//usb_device_module_info
	driver_module_info *usb;
	usb_device usb_cookie;
} input_aes_device_info;

/* bypass device/driver separation */
typedef struct {
	struct {
		usb_pipe pipe_in, pipe_out;
	} device;
	struct {
		status_t status;
		size_t actual_length;
	} transfer; // ! instead of passing cookie
	sem_id lock;
} input_aes_type;

static struct {
	usb_module_info *usb;
	usb_device device;
	bool matched;
	int path_id;
} input_aes_static;
/* */

static device_manager_info *sDeviceManager;
static input_aes_type *input_aes;

static void aes_usb_transfer_callback(void *, status_t, void *, size_t);
static status_t aes_usb_read_regs(unsigned char *);
static status_t aes_setup_pipes(const usb_interface_info *);
static status_t aes_usb_read(unsigned char* const, size_t);

static status_t bulk_transfer(int, unsigned char *, size_t);
static status_t clear_stall(void);

static void input_aes_uninit_driver(void *);

extern status_t aes_usb_exec(status_t (*bulk_transfer)(), status_t (*clear_stall)(),
	bool strict, const pairs *cmd, unsigned int num);

//	#pragma mark -
// device module API

#define BULK_TIMEOUT 2000

enum aes2501_regs {
/* 1 = continuous scans, 0 = single scans */
#define AES2501_CTRL2_READ_REGS		0x02 /* dump registers */
	AES2501_REG_ENDROW = 0x94, /* end row */
	AES2501_REG_AUTOCALOFFSET = 0xa8,
};
enum aes2501_rates {
	/* rate of detection cycles: */
	AES2501_DETCTRL_DRATE_31_MS		= 0x02, /* every 31.24ms */
};

static const usb_support_descriptor kSupportedDevices[] = {
	{0,0,0,AES_VID,AES_PID}
};

static status_t
input_aes_device_added(usb_device dev, void **cookie)
{
	input_aes_static.matched = true;

	// this is not initialization func :-), fast exit
	input_aes_static.device = dev;
	return B_OK;
}

/* no hotplug, folks */
static usb_notify_hooks sNotifyHooks = {
	&input_aes_device_added,
	NULL /* hid_device_removed */
};

static status_t
input_aes_init_device(void *_cookie, void **cookie)
{
	device_node *node = (device_node *)_cookie;
	input_aes_device_info *device;
	device_node *parent;

	if (!(device = (input_aes_device_info *)calloc(1, sizeof(*device))))
		return B_NO_MEMORY;

	device->node = node;

	parent = sDeviceManager->get_parent_node(node);
	sDeviceManager->get_driver(parent, (driver_module_info **)&device->usb,
		(void **)&device->usb_cookie);
	sDeviceManager->put_node(parent);

	*cookie = device;
	return B_OK;
}


static void
input_aes_uninit_device(void *_cookie)
{
	input_aes_device_info *device = (input_aes_device_info *)_cookie;
	free(device);
}


static status_t
input_aes_open(void *_cookie, const char *path, int flags, void** cookie)
{
	input_aes_device_info *device = (input_aes_device_info *)_cookie;
	*cookie = device;
	return B_OK;
}


static status_t
input_aes_read(void* _cookie, off_t position, void *buf, size_t* num_bytes)
{
	return B_ERROR;
}


static status_t
input_aes_write(void* cookie, off_t position, const void* buffer, size_t* num_bytes)
{
	return B_ERROR;
}


static status_t
input_aes_control(void* _cookie, uint32 op, void* arg, size_t len)
{
	return B_ERROR;
}


static status_t
input_aes_close (void* cookie)
{
	return B_OK;
}


static status_t
input_aes_free (void* cookie)
{
	return B_OK;
}


//	#pragma mark -
// driver module API

static float
input_aes_support(device_node *parent)
{
	uint32 path_id;
	bigtime_t timeout = 6000000
	/* 9000000 // overrated much, measured on busy-wait */, start;

	// limit to single driver instance
	if ((path_id = sDeviceManager->create_id(INPUT_AES_PATHID_GENERATOR)) != 0) {
		if (path_id < 0) {
			TRACE("support(): couldn't create a path_id\n");
		}
		else
			sDeviceManager->free_id(INPUT_AES_PATHID_GENERATOR, path_id);

		return 0.0f;
	}

	input_aes_static.usb->register_driver("usb_aes2501", kSupportedDevices, 1,
		NULL);
	input_aes_static.usb->install_notify("usb_aes2501", &sNotifyHooks);

	// ugly: wait for install_notify()
	start = system_time();
	while(input_aes_static.matched != true) {
		if ((system_time() - start) > timeout) {
			input_aes_static.usb->uninstall_notify("usb_aes2501");

			return 0.0f;
		}
		snooze (timeout / 50);
	}
	dprintf("aes2501: Found supported device\n");

	input_aes_static.path_id = path_id;

	return 0.8f;
}


static status_t
input_aes_register_device(device_node *node)
{
	device_attr attrs[] = {
		{ B_DEVICE_PRETTY_NAME, B_STRING_TYPE, { string: "AuthenTec AES2501 USB driver" }},
		{ NULL }
	};

	return sDeviceManager->register_node(node, INPUT_AES_MODULE_NAME, attrs, NULL, NULL);
}


static status_t
input_aes_init_driver(device_node *node, void **_driverCookie)
{
	const usb_configuration_info *conf;
	int i;
	unsigned char *buf;

	const pairs cmd_1[] = {
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ 0, 0 },
	{ 0xb0, 0x27 }, /* Reserved? */
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ 0xff, 0x00 }, /* Reserved? */
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ AES2501_REG_DETCTRL,
		AES2501_DETCTRL_DRATE_CONTINUOUS | AES2501_DETCTRL_SDELAY_31_MS },
	{ AES2501_REG_COLSCAN, AES2501_COLSCAN_SRATE_128_US },
	{ AES2501_REG_MEASDRV,
		AES2501_MEASDRV_MDRIVE_0_325 | AES2501_MEASDRV_MEASURE_SQUARE },
	{ AES2501_REG_MEASFREQ, AES2501_MEASFREQ_2M },
	{ AES2501_REG_DEMODPHASE1, DEMODPHASE_NONE },
	{ AES2501_REG_DEMODPHASE2, DEMODPHASE_NONE },
	{ AES2501_REG_CHANGAIN,
		AES2501_CHANGAIN_STAGE2_4X | AES2501_CHANGAIN_STAGE1_16X },
	{ AES2501_REG_ADREFHI, 0x44 },
	{ AES2501_REG_ADREFLO, 0x34 },
	{ AES2501_REG_STRTCOL, 0x16 },
	{ AES2501_REG_ENDCOL, 0x16 },
	{ AES2501_REG_DATFMT, AES2501_DATFMT_BIN_IMG | 0x08 }, // {0xff,0x00},
	{ AES2501_REG_TREG1, 0x70 },
	{ 0xa2, 0x02 },
	{ 0xa7, 0x00 },
	{ AES2501_REG_TREGC, AES2501_TREGC_ENABLE },
	{ AES2501_REG_TREGD, 0x1a },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE },
	{ AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT },
	{ AES2501_REG_LPONT, AES2501_LPONT_MIN_VALUE },
	};
	const pairs cmd_2[] = {
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_AUTOCALOFFSET, 0x41 },
	{ AES2501_REG_EXCITCTRL, 0x42 },
	{ AES2501_REG_DETCTRL, 0x53 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE },
	};
	const pairs cmd_3[] = {
	{ 0xff, 0x00 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_AUTOCALOFFSET, 0x41 },
	{ AES2501_REG_EXCITCTRL, 0x42 },
	{ AES2501_REG_DETCTRL, 0x53 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE },
	};
	const pairs cmd_4[] = {
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ 0xb0, 0x27 },
	{ AES2501_REG_ENDROW, 0x0a },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE },
	{ AES2501_REG_DETCTRL, 0x45 },
	{ AES2501_REG_AUTOCALOFFSET, 0x41 },
	};

	*_driverCookie = node;
	TRACE("init_driver()\n");

	if (!(input_aes = (input_aes_type *) malloc(sizeof(input_aes_type)))) {
		input_aes_uninit_driver(NULL);
		return B_ERROR;
	}

	input_aes->lock = -1;

	if (!(conf = input_aes_static.usb->get_nth_configuration(input_aes_static.device, 0))) {
		TRACE("can't get default configuration\n");
		input_aes_uninit_driver(NULL);
		return B_ERROR;
	}

	if (input_aes_static.usb->set_configuration(input_aes_static.device, conf) != B_OK) {
		TRACE("can't set configuration\n");
		input_aes_uninit_driver(NULL);
		return B_ERROR;
	}

	if (aes_setup_pipes(conf->interface[0].active) != B_OK) {
		TRACE("can't setup pipes\n");
		input_aes_uninit_driver(NULL);
		return B_ERROR;
	}

	if ((input_aes->lock = create_sem(0, "lock")) < 0 ||
		aes_usb_exec(&bulk_transfer, &clear_stall, true, cmd_1, G_N_ELEMENTS(cmd_1)) != B_OK ||
		aes_usb_read(NULL, 20 /* 44 */) != B_OK ||
		aes_usb_exec(&bulk_transfer, &clear_stall, true, cmd_2, G_N_ELEMENTS(cmd_2)) != B_OK
	) {
		input_aes_uninit_driver(NULL);
		return B_ERROR;
	}

#ifndef COMPACT_DRIVER
	if (!(buf =
#if 0
	calloc(126, 1)
#else
	malloc(126) // !
#endif
	)) {
		input_aes_uninit_driver(NULL);
		return B_ERROR;
	}
	if (aes_usb_read_regs(buf) != B_OK) {
		free(buf);
		input_aes_uninit_driver(NULL);
		return B_ERROR;
	}

	TRACE("reg 0xaf = 0x%x\n", buf[0x5f]);
	i = 0;
	while (buf[0x5f] == 0x6b) {
		TRACE("reg 0xaf = 0x%x\n", buf[0x5f]);
		if (aes_usb_exec(&bulk_transfer, &clear_stall, true, cmd_3, G_N_ELEMENTS(cmd_3)) != B_OK ||
			aes_usb_read_regs(buf) != B_OK) {
			free(buf);
			input_aes_uninit_driver(NULL);
			return B_ERROR;
		}
		if (++i == 13)
			break;
	}
#if 0
	TRACE("reg vals:\n");
	for (i = 0; i < 126; i++)
		if (buf[i])
			dprintf("0x%x = 0x%x, ", i, buf[i]);
#endif
	free(buf);
#endif // COMPACT_DRIVER
	if (aes_usb_exec(&bulk_transfer, &clear_stall, true, cmd_4, G_N_ELEMENTS(cmd_4)) != B_OK) {
		input_aes_uninit_driver(NULL);
		return B_ERROR;
	}

	return B_OK;
}


static void
input_aes_uninit_driver(void *driverCookie)
{
	//TRACE("uninit_driver()\n");

	input_aes_static.usb->cancel_queued_transfers(input_aes->device.pipe_in);
	input_aes_static.usb->cancel_queued_transfers(input_aes->device.pipe_out);

	if (input_aes) {
		delete_sem(input_aes->lock);
		free(input_aes);
	}
	input_aes_static.usb->uninstall_notify("usb_aes2501");
}


static status_t
input_aes_register_child_devices(void *_cookie)
{
	device_node *node = _cookie;
	char name[128];

	TRACE("register_child_devices()\n");

	snprintf(name, sizeof(name), INPUT_AES_BASENAME, input_aes_static.path_id);
	sDeviceManager->publish_device(node, name, INPUT_AES_DEVICE_MODULE_NAME);

	dprintf("aes2501: input_aes_device_removed() is not implemented, no need to stay resident,"
		" unloading\n");
	return B_ERROR;
}


module_dependency module_dependencies[] = {
	{ B_DEVICE_MANAGER_MODULE_NAME, (module_info **)&sDeviceManager },
	{ B_USB_MODULE_NAME, (module_info **)&input_aes_static.usb },
	{}
};

driver_module_info input_aes_driver_module = {
	{
		INPUT_AES_MODULE_NAME,
		0,
		NULL
	},

	input_aes_support,
	input_aes_register_device,
	input_aes_init_driver,
	input_aes_uninit_driver,
	input_aes_register_child_devices,
	NULL,	// rescan
	NULL,	// removed
};


struct device_module_info input_aes_device_module = {
	{
		INPUT_AES_DEVICE_MODULE_NAME,
		0,
		NULL
	},

	input_aes_init_device,
	input_aes_uninit_device,
	NULL,

	input_aes_open,
	input_aes_close,
	input_aes_free,
	input_aes_read,
	input_aes_write,
	NULL,
	input_aes_control,

	NULL,
	NULL
};

module_info *modules[] = {
	(module_info *)&input_aes_driver_module,
	(module_info *)&input_aes_device_module,
	NULL
};

/*
void c------------------------------() {}
*/

static void aes_usb_transfer_callback(void *cookie, status_t status, void *data, size_t actlen)
{
	switch (status) {
		case B_DEV_UNEXPECTED_PID:
			input_aes->transfer.status = B_BUSY;
			break;
		case B_DEV_FIFO_UNDERRUN:
			//TRACE("data underrun\n");
		case B_DEV_FIFO_OVERRUN:
		default:
			input_aes->transfer.status = status;
	};
	input_aes->transfer.actual_length = actlen;

	release_sem_etc(input_aes->lock, 1, 0);
}

static status_t aes_usb_read_regs(unsigned char *buf)
{
	const pairs regwrite[] = { { AES2501_REG_CTRL2, AES2501_CTRL2_READ_REGS } };

	if (aes_usb_exec(&bulk_transfer, &clear_stall, true, regwrite, 1) != B_OK)
		return B_ERROR;

	return aes_usb_read(buf, 126);
}

static status_t aes_setup_pipes(const usb_interface_info *uii)
{
	size_t epts[] = { -1, -1 }, ep = 0;

	for (; ep < uii->endpoint_count; ep++) {
		usb_endpoint_descriptor *ed = uii->endpoint[ep].descr;
		if ((ed->attributes & USB_ENDPOINT_ATTR_MASK) == USB_ENDPOINT_ATTR_BULK)
			(ed->endpoint_address & USB_ENDPOINT_ADDR_DIR_IN)
				== USB_ENDPOINT_ADDR_DIR_IN ?
				(epts[0] = ep) : (epts[1] = ep);
	}

	input_aes->device.pipe_in = uii->endpoint[epts[0]].handle;
	input_aes->device.pipe_out = uii->endpoint[epts[1]].handle;

	TRACE("endpoint is: %lx %lx\n", input_aes->device.pipe_in, input_aes->device.pipe_out);

	return epts[0] >= 0 && epts[1] >= 0 ? B_OK : B_ENTRY_NOT_FOUND;
}

static status_t aes_usb_read(unsigned char* const buf, size_t size)
{
	unsigned char *data;
	size_t res;

	if (!(data = (buf ? buf : malloc(size)) ))
		return B_ERROR;
	res = bulk_transfer(AES2501_IN, data, size);
	if (!buf)
		free(data);

	switch (res) {
		case B_DEV_FIFO_UNDERRUN:
			if (!buf)
				return B_OK;
		break;
		case B_DEV_FIFO_OVERRUN:
			if (!buf)
				dprintf("aes2501: data overrun."
				/*please bump aes_usb_read(NULL, [value] by some*/ "\n");
		break;
		case B_OK:
			if (buf && input_aes->transfer.actual_length != size) {
				TRACE("request %lu bytes, but got %lu bytes.\n", size,
					input_aes->transfer.actual_length);
				return B_ERROR;
			}
	}
	return res;
}

/* Callbacks */
static status_t bulk_transfer(int direction, unsigned char *data, size_t size)
{
	usb_pipe *dir = (direction == AES2501_OUT ? &input_aes->device.pipe_out : &input_aes->device.pipe_in);
	status_t ret;

	if (input_aes_static.usb->queue_bulk(*dir, data, size, &aes_usb_transfer_callback,
		NULL) != B_OK)
		return B_ERROR;

	// block for consecutive transfers
	if ((ret = acquire_sem_etc(input_aes->lock, 1, B_RELATIVE_TIMEOUT, BULK_TIMEOUT)) < B_OK) {
		if (ret == B_TIMED_OUT)
			// on init consider critical, on operating just give up
			return B_TIMED_OUT;
		return B_ERROR;
	}

	return input_aes->transfer.status;
}
static status_t clear_stall(void)
{
	return input_aes_static.usb->clear_feature(input_aes->device.pipe_out, USB_FEATURE_ENDPOINT_HALT);
}
/* */
