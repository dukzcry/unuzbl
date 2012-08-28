/* Written by Artem Falcon <lomka@gero.in> */

#include "AesInputDevice.h"

const static uint32 kPollThreadPriority = B_FIRST_REAL_TIME_PRIORITY + 4,
	kPollInterval = 1000000 * 0.055;
const static char *kAesInputDirectory = "/dev/input/aes2501";


/* Missing things: serialisation on communication between threads */


BInputServerDevice*
instantiate_input_device()
{
	return (InputDevice = new AesInputDevice());
}
AesInputDevice::AesInputDevice()
: device(NULL),
  URoster(NULL), settings(NULL)
{
}
AesInputDevice::~AesInputDevice()
{
	if (URoster) {
		URoster->Stop();
		delete URoster;
	}
	if (settings)
		free(settings);
}

status_t
AesInputDevice::InitCheck()
{
	BEntry entry(kAesInputDirectory);
	const BUSBConfiguration *conf;
	bigtime_t timeout = 1000, start;
	input_device_ref dev = { "AuthenTec AES2501 USB", B_POINTING_DEVICE,
		(void *) this };
	input_device_ref *deviceList[] = { &dev, NULL };
	thread_id InitThread;
	char buf[3];

	// (f)open would be poor choice
	if (!entry.Exists()) {
		PRINT("Kernel-side driver didn't make device init for us\n");
		return B_ERROR;
	}

	if (!(URoster = new AesUSBRoster()))
		return B_ERROR;

	/* dupe: We're doing a duplicate of what kernel driver did in it's init phase,
	   because of two reasons: 1) someone may wish to adopt this project for
	   hot swapping support, 2) i don't want to mess with creating of shared memory
	   area region and passing parms from kernel driver to userspace one */
	URoster->Start();

	// as always :-!
	start = system_time();
	while(device == NULL) {
		if ((system_time() - start) > timeout) {
			debug_printf("aes2501: Supported device is missing. Unplugged?\n");

			return B_ERROR;
		}
		snooze(timeout);
	}

	if (device->SetConfiguration(conf = device->ConfigurationAt(0)) < B_OK) {
		PRINT("can't set configuration\n");
		return B_ERROR;
	}
	if (aes_setup_pipes(conf->InterfaceAt(0)) != B_OK) {
		PRINT("can't setup pipes\n");
		return B_ERROR;
	}
	// end of dupe

	// simulate 'block with timeout'
	if ((InitThread = spawn_thread(InitThreadProxy, "AES2501 Init", 50, this)) < B_OK ||
		(DeviceWatcherId = spawn_thread(DeviceWatcherProxy, "AES2501 Poll",
		kPollThreadPriority, this)) < B_OK ||
		send_data(InitThread, 0, NULL, 0) != B_OK) // pass main's thread id
		return B_ERROR;
	resume_thread(InitThread);
	timeout = 5000; // bump up, if added code into InitThread() or subcalls
	start = system_time();
	while(!has_data(InitThread)) {
		if ((system_time() - start) > timeout) {
			kill_thread(InitThread);
			return B_ERROR;
		}
		snooze(timeout);
	}
	receive_data(&InitThread, (char *) &buf, 3);
	if (strcmp(buf, "OK"))
		return B_ERROR;

	if (!(settings = (AesSettings *) malloc(sizeof(AesSettings))))
		return B_ERROR;

	RegisterDevices(deviceList);
	return B_OK;
}
status_t
AesInputDevice::Start(const char *name, void *cookie)
{
	/* Would be nice to do:
		our_device->set_mouse_type(1); _/ */

	SetSettings();

	/* \_ and:
		if (settings->handle_click == true) {
			map.button[0] = (settings->which_button == AES_SECOND_BUTTON) ?
				B_TERTIARY_MOUSE_BUTTON : to_mouseview_name(settings->which_button);
			our_device->set_mouse_map(&map);
		}
		but Haiku allows only global mouse settings */

	resume_thread(DeviceWatcherId);

	return 0;
}
status_t
AesInputDevice::Stop(const char *name, void *cookie)
{
	kill_thread(DeviceWatcherId);
}

status_t
AesUSBRoster::DeviceAdded(BUSBDevice *dev)
{
	if (dev->VendorID() == AES_VID && dev->ProductID() == AES_PID) {
		InputDevice->device = dev;
		return B_OK;
	}

	return B_ERROR;
}

/*
void c------------------------------() {}
*/

void AesInputDevice::SetSettings()
{
	void *handle;

	settings->which_button = 0x04;

	// TO-DO: share common settings with Touchpad preflet
	// Driver Settings API isn't right to be used here, but whatever...
	if ((handle = load_driver_settings("aes2501"))) {
		const char *str = get_driver_parameter(handle, "bind_to_which_button", NULL, NULL);
		if (str)
			if (str[0] == '1')
				settings->which_button = 0x01;
			else if (str[0] == '3')
				settings->which_button = 0x02;
	}
	settings->handle_click = get_driver_boolean_parameter(handle, "handle_click", true, true);
	settings->handle_scroll = get_driver_boolean_parameter(handle, "handle_scroll", true, true);
	settings->do_scan = get_driver_boolean_parameter(handle, "do_scan", true, true);
	unload_driver_settings(handle);
}

status_t AesInputDevice::InitThread()
{
	thread_id sender;

	const pairs start_scan_cmd[] = {
	{ 0xb0, 0x27 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ 0xff, 0x00 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_SCAN_RESET },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_SCAN_RESET },
	};

	receive_data(&sender, NULL, 0);

	if (aes_usb_exec(&g_bulk_transfer, &g_clear_stall, true, start_scan_cmd,
		G_N_ELEMENTS(start_scan_cmd)) != B_OK) {
		send_data(sender, 0, "ER", 3);
		return 0;
	}

	send_data(sender, 0, "OK", 3);
	return 0;
}

BMessage *AesInputDevice::PrepareMessage()
{
	BMessage *message = new BMessage();
	if (!message)
		return NULL;

	message->AddInt32("x", 0);
	message->AddInt32("y", 0);
	message->AddInt32("buttons", settings->which_button);

	return message;
}

status_t AesInputDevice::DeviceWatcher()
{
	bool instant = false;
	state s = AES_DETECT_FINGER;


	int substate = false, sum, i;
	unsigned char *buf;

	int threshold;

	BMessage *message;

	BBufferGroup *BBuffers = NULL;


	bool click_only = !settings->handle_scroll && !settings->do_scan;

	unsigned char *pthres;

	unsigned char *data;
	uint16 *histogram;
	buffer_clone_info info;

	BMessenger *messenger = NULL;
	status_t status;

	const pairs det_fp_cmd[] = {
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ AES2501_REG_DETCTRL,
		AES2501_DETCTRL_DRATE_CONTINUOUS | AES2501_DETCTRL_SDELAY_31_MS },
	{ AES2501_REG_COLSCAN, AES2501_COLSCAN_SRATE_128_US },
	{ AES2501_REG_MEASDRV, AES2501_MEASDRV_MDRIVE_0_325 | AES2501_MEASDRV_MEASURE_SQUARE },
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
	{ 0, 0 },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE },
	{ AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT },
	{ AES2501_REG_LPONT, AES2501_LPONT_MIN_VALUE },
	};

	const pairs capture_cmd_1[] = {
	{ AES2501_REG_CTRL1, AES2501_CTRL1_MASTER_RESET },
	{ 0, 0 },
	{ AES2501_REG_EXCITCTRL, 0x40 },
	{ AES2501_REG_DETCTRL,
		AES2501_DETCTRL_SDELAY_31_MS | AES2501_DETCTRL_DRATE_CONTINUOUS },
	{ AES2501_REG_COLSCAN, AES2501_COLSCAN_SRATE_128_US },
	{ AES2501_REG_DEMODPHASE2, 0x7c },
	{ AES2501_REG_MEASDRV,
		AES2501_MEASDRV_MEASURE_SQUARE | AES2501_MEASDRV_MDRIVE_0_325 },
	{ AES2501_REG_DEMODPHASE1, 0x24 },
	{ AES2501_REG_CHWORD1, 0x00 },
	{ AES2501_REG_CHWORD2, 0x6c },
	{ AES2501_REG_CHWORD3, 0x09 },
	{ AES2501_REG_CHWORD4, 0x54 },
	{ AES2501_REG_CHWORD5, 0x78 },
	{ 0xa2, 0x02 },
	{ 0xa7, 0x00 },
	{ 0xb6, 0x26 },
	{ 0xb7, 0x1a },
	{ AES2501_REG_CTRL1, AES2501_CTRL1_REG_UPDATE },
	{ AES2501_REG_IMAGCTRL,
		AES2501_IMAGCTRL_TST_REG_ENABLE | AES2501_IMAGCTRL_HISTO_DATA_ENABLE |
		AES2501_IMAGCTRL_IMG_DATA_DISABLE },
	{ AES2501_REG_STRTCOL, 0x10 },
	{ AES2501_REG_ENDCOL, 0x1f },
	{ AES2501_REG_CHANGAIN,
		AES2501_CHANGAIN_STAGE1_2X | AES2501_CHANGAIN_STAGE2_2X },
	{ AES2501_REG_ADREFHI, 0x70 },
	{ AES2501_REG_ADREFLO, 0x20 },
	{ AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT },
	{ AES2501_REG_LPONT, AES2501_LPONT_MIN_VALUE },
	};
	const pairs capture_cmd_2[] = {
	{ AES2501_REG_IMAGCTRL,
		AES2501_IMAGCTRL_TST_REG_ENABLE | AES2501_IMAGCTRL_HISTO_DATA_ENABLE |
		AES2501_IMAGCTRL_IMG_DATA_DISABLE },
	{ AES2501_REG_STRTCOL, 0x10 },
	{ AES2501_REG_ENDCOL, 0x1f },
	{ AES2501_REG_CHANGAIN, AES2501_CHANGAIN_STAGE1_16X },
	{ AES2501_REG_ADREFHI, 0x70 },
	{ AES2501_REG_ADREFLO, 0x20 },
	{ AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT },
	};
	const pairs strip_scan_cmd[] = {
	{ AES2501_REG_IMAGCTRL,
#ifndef COMPACT_DRIVER
		AES2501_IMAGCTRL_TST_REG_ENABLE |
#endif
		AES2501_IMAGCTRL_HISTO_DATA_ENABLE /* | AES2501_IMAGCTRL_IMG_DATA_DISABLE */ },
	{ AES2501_REG_STRTCOL, 0x00 },
	{ AES2501_REG_ENDCOL, 0x2f },
	{ AES2501_REG_CHANGAIN, AES2501_CHANGAIN_STAGE1_16X },
	{ AES2501_REG_ADREFHI, 0x5b },
	{ AES2501_REG_ADREFLO, 0x20 },
	{ AES2501_REG_CTRL2, AES2501_CTRL2_SET_ONE_SHOT },
	};


	if (!click_only) {
		if (!(buf = (unsigned char *) malloc(
#ifndef COMPACT_DRIVER
			1705 /* 159 */)))
			return 0;
			threshold = 0;
#else
			1643 /* 97 */)))
			return 0;
			threshold = 8;
#endif
		if ((info.area = area_for(buf + 1)) == B_ERROR)
			return 0;
		info.size = 192*8;
		info.offset = info.buffer = info.flags = 0;

		data = buf + 1 + info.size /* = buf */;
		histogram = (uint16 *)(data + 1);
		pthres = data + 1 + 16*2 + 1 + 8; /* buf + 1 + 16*2 */
	}
	else {
		unsigned char histgr[20 /* 44 */];
		buf = &histgr[0];
	}

	while (1)
	{
		switch (s) {
		case AES_DETECT_FINGER:
			if (aes_usb_exec(&g_bulk_transfer, &g_clear_stall, false, det_fp_cmd,
				G_N_ELEMENTS(det_fp_cmd)) != B_OK || aes_usb_read(buf, 20 /* 44 */) != B_OK) {
				s = AES_BREAK_LOOP;
				break;
			}

			sum = 0;
			for (i = 1; i < 9; i++)
				sum += (buf[i] & 0xf) + (buf[i] >> 4);
			if (sum > 20) {
				// fast handle of touch
				if (click_only) {
					if (!substate) {
						s = AES_MOUSE_DOWN;
						instant = true;
					}
				} else {
					s = AES_RUN_CAPTURE;
					instant = true;
				}
			}
			else if (substate) {
				s = AES_MOUSE_UP;
				substate = 0;
				instant = true;
			}

			if (s == AES_MOUSE_DOWN || s == AES_MOUSE_UP)
				if (!(message = PrepareMessage())) {
					s = AES_BREAK_LOOP;
					break;
				}
		break;
		case AES_RUN_CAPTURE:
			if (aes_usb_exec(&g_bulk_transfer, &g_clear_stall, false, capture_cmd_1,
				G_N_ELEMENTS(capture_cmd_1)) != B_OK || aes_usb_read(NULL, 159) != B_OK ||
				aes_usb_exec(&g_bulk_transfer, &g_clear_stall, false, capture_cmd_2,
				G_N_ELEMENTS(capture_cmd_2)) != B_OK || aes_usb_read(NULL, 159) != B_OK) {
					s = AES_BREAK_LOOP;
					break;
				}

			BBuffers = new BBufferGroup();
			if (BBuffers->InitCheck() != B_OK) {
				s = AES_BREAK_LOOP;
				break;
			}

			s = AES_STRIP_SCAN;
		break;
		case AES_STRIP_SCAN:
			if (aes_usb_exec(&g_bulk_transfer, &g_clear_stall, true, strip_scan_cmd,
				G_N_ELEMENTS(strip_scan_cmd)) != B_OK || aes_usb_read(buf,
#ifndef COMPACT_DRIVER
				1705 /* 159 */) != B_OK) {
				s = AES_BREAK_LOOP;
				break;
			}

			if (!threshold) {
				s = AES_GET_CAPS;
				break;
			}
#else
				1643 /* 97 */) != B_OK) {
				s = AES_BREAK_LOOP;
				break;
			}
#endif
			if (*data != 0xde) {
				PRINT("didn't receive proper histogram\n");

				s = AES_BREAK_LOOP;
				break;
			}

			sum = 0;
			// binded to little-endian machine
			for (i = threshold; i < 16; i++)
				sum += histogram[i];
			if (sum > 0 && substate < MAX_FRAMES) {
				substate++;
				if (BBuffers->AddBuffer(info) != B_OK)
					s = AES_BREAK_LOOP;
				break;
			}
			if (sum < 0 /* non fatal */ || substate < 1) {
				delete BBuffers; BBuffers = NULL;

				s = AES_DETECT_FINGER;
				substate = instant = false;
			}
			else
				s = AES_HANDLE_STRIPS;
		break;
		case AES_HANDLE_STRIPS:
			if (!settings->do_scan) {
				s = AES_HANDLE_SCROLL;
				break;
			} else {
				s = settings->handle_scroll ? AES_HANDLE_SCROLL : AES_DETECT_FINGER;

				messenger = new BMessenger(kAesSignature, -1, &status);
				if (status != B_OK) {
					delete messenger; messenger = NULL;
					if (s == AES_DETECT_FINGER) {
						delete BBuffers; BBuffers = NULL;
						substate = instant = false;
					}
					break;
				}
				if (!(message = new BMessage(kAesBufferGroupMessage))) {
					s = AES_BREAK_LOOP;
					break;
				}
				if (this->AddBuffersTo(BBuffers, message) != B_OK) {
					delete message;

					s = AES_BREAK_LOOP;
					break;
				}
				if (messenger->SendMessage(message, (BHandler *) NULL, 1000) != B_OK) {
					delete message;
					delete messenger; messenger = NULL;
					if (s == AES_DETECT_FINGER) {
						delete BBuffers; BBuffers = NULL;
					}
				}
			}

			if (s == AES_DETECT_FINGER)
				substate = instant = false;
		break;
		case AES_HANDLE_SCROLL:
		break;
#ifndef COMPACT_DRIVER
		case AES_GET_CAPS:
			if (*pthres != AES2501_REG_CTRL1 /* = FIRST_AES2501_REG */) {
				PRINT("not a register dump\n");

				s = AES_BREAK_LOOP;
				break;
			}
			if ((threshold = pthres[GET_THRESHOLD]) < 0) {
				s = AES_BREAK_LOOP;
				break;
			}
			if ((threshold &= 0x0f) > 0x0f)
				threshold = 8;

			s = AES_STRIP_SCAN;
		break;
#endif
		case AES_MOUSE_DOWN:
			message->what = B_MOUSE_DOWN;
			EnqueueMessage(message);

			s = AES_DETECT_FINGER;
			substate = 1;
			instant = false;
		break;
		case AES_MOUSE_UP:
			message->what = B_MOUSE_UP;
			EnqueueMessage(message);

			s = AES_DETECT_FINGER;
			instant = false;
		break;
		}

		if (s == AES_BREAK_LOOP)
			break;
		if (!instant)
			snooze(kPollInterval);
	}

	if (!click_only)
		free(buf);
	if (BBuffers)
		delete BBuffers;
	if (messenger)
		delete messenger;
}

status_t AesInputDevice::AddBuffersTo(BBufferGroup *group, BMessage *message)
{
	int32 count, i;
	BBuffer **buffers;

	group->CountBuffers(&count);
	buffers = new BBuffer * [count];
	if (!group->GetBufferList(count, buffers)) {
		delete [] buffers;
		return B_ERROR;
	}

	message->AddInt32("count", count);
	for (i = 0; i < count; i++)
		message->AddInt32("buffer", buffers[i]->ID());

	delete [] buffers;
	return B_OK;
}

status_t AesInputDevice::aes_setup_pipes(const BUSBInterface *uii)
{
	size_t ep = 0;
	BUSBEndpoint *epts[] = { NULL, NULL };

	for (; ep < uii->CountEndpoints(); ep++) {
		BUSBEndpoint *ed = (BUSBEndpoint *) uii->EndpointAt(ep);
		if (ed->IsBulk())
			(ed->IsInput()) ? (epts[0] = ed) : (epts[1] = ed);
	}

	dev_data.pipe_in = epts[0];
	dev_data.pipe_out = epts[1];

	PRINT("endpoint is: %d %d\n", dev_data.pipe_in->Index(), dev_data.pipe_out->Index());

	return epts[0] && epts[1] ? B_OK : B_ENTRY_NOT_FOUND;
}

status_t AesInputDevice::aes_usb_read(unsigned char* const buf, size_t size)
{
	unsigned char *data;
	status_t res;

	if (!(data = (buf ? buf : (unsigned char *) malloc(size)) ))
		return B_ERROR;
	res = bulk_transfer(AES2501_IN, data, size);
	if (!buf)
		free(data);

	switch (res) {
		case B_DEV_FIFO_UNDERRUN:
				return B_OK;
		case B_DEV_FIFO_OVERRUN:
			debug_printf("aes2501: data overrun."
			/*please bump aes_usb_read(buf, [value] by some*/ "\n");
		default:
			return res;
	};
}

/* Callbacks */
status_t AesInputDevice::bulk_transfer(int direction, unsigned char *data, size_t size)
{
	BUSBEndpoint *dir = (direction == AES2501_OUT ? dev_data.pipe_out : dev_data.pipe_in);
	ssize_t res;

	if ((res = dir->BulkTransfer(data, size)) != size)
		if (res < size) {
			if (direction == AES2501_IN)
				PRINT("request %lu bytes, but got %lu bytes.\n", size, res);
			return B_DEV_FIFO_UNDERRUN;
		}
		else return B_DEV_FIFO_OVERRUN;

	if (dir->IsStalled())
		return B_DEV_STALLED;

	return B_OK;
}
status_t AesInputDevice::clear_stall()
{
	return dev_data.pipe_out->ClearStall();
}
/* */
