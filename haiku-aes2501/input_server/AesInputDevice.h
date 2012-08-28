#ifndef AES_INPUT_DEVICE_H
#define AES_INPUT_DEVICE_H

#include "aes2501_common.h"
#include "aes2501_app_common.h"

#include <add-ons/input_server/InputServerDevice.h>
#include <USBKit.h>

#include <Entry.h>
#include <driver_settings.h>

#include <Buffer.h>
#include <BufferGroup.h>
#include <Messenger.h>

#define COMPACT_DRIVER 1

#define MAX_FRAMES 150 // limit number of buffers for strip data
#define GET_THRESHOLD (AES2501_REG_DATFMT - AES2501_REG_CTRL1) * 2 + 1

#define PRINT_AES 1
#ifdef PRINT_AES
	inline void _iprint(const char* fmt, ...) {
		FILE* log = fopen("/var/log/aes2501.log", "a");
		va_list ap;
		va_start(ap, fmt);
		vfprintf(log, fmt, ap);
		va_end(ap);
		fflush(log);
		fclose(log);
       }
#	define PRINT(x...)	_iprint (x)
#else
#	define PRINT(x...) ;
#endif

extern "C" _EXPORT BInputServerDevice* instantiate_input_device();
extern "C" status_t aes_usb_exec(status_t (*bulk_transfer)(int, unsigned char *, size_t),
	status_t (*clear_stall)(), bool strict, const pairs *cmd, unsigned int num);

enum state {
	AES_DETECT_FINGER,
	AES_RUN_CAPTURE,
	AES_STRIP_SCAN,
	AES_HANDLE_STRIPS,
	AES_HANDLE_SCROLL,
	AES_GET_CAPS,
	AES_MOUSE_DOWN,
	AES_MOUSE_UP,
	AES_BREAK_LOOP
};

enum aes2501_regs {
	AES2501_REG_IMAGCTRL = 0x98, /* image data */
/* don't send image or authentication messages when imaging */
#define AES2501_IMAGCTRL_IMG_DATA_DISABLE	0x01
/* send histogram when imaging */
#define AES2501_IMAGCTRL_HISTO_DATA_ENABLE	0x02
/* return test registers with register dump */
#define AES2501_IMAGCTRL_TST_REG_ENABLE		0x20
	AES2501_REG_CHWORD1 = 0x9b, /* challenge word 1 */
	AES2501_REG_CHWORD2 = 0x9c,
	AES2501_REG_CHWORD3 = 0x9d,
	AES2501_REG_CHWORD4 = 0x9e,
	AES2501_REG_CHWORD5 = 0x9f,
};
enum aes2501_sensor_gain1 {
	AES2501_CHANGAIN_STAGE1_2X	= 0x00,
};
enum aes2501_sensor_gain2 {
	AES2501_CHANGAIN_STAGE2_2X	= 0x00,
};

typedef struct {
	int8 which_button;
	bool handle_click, handle_scroll, do_scan;
} AesSettings;

class AesInputDevice;
static AesInputDevice *InputDevice;
class AesInputDevice: public BInputServerDevice {
	class AesUSBRoster *URoster;

	BUSBDevice *device;
	struct {
		BUSBEndpoint *pipe_in, *pipe_out;
	} dev_data;
	AesSettings *settings;
	thread_id DeviceWatcherId;
public:
	AesInputDevice();
	virtual ~AesInputDevice();
private:
	void SetSettings();
	status_t InitThread();
	BMessage *PrepareMessage();
	status_t DeviceWatcher();
	static status_t InitThreadProxy(void *_this)
	{
		AesInputDevice *dev = (AesInputDevice *) _this;
		return dev->InitThread();
	}
	static status_t DeviceWatcherProxy(void *_this)
	{
		AesInputDevice *dev = (AesInputDevice *) _this;
		return dev->DeviceWatcher();
	}
	status_t AddBuffersTo(BBufferGroup *, BMessage *);
	status_t aes_setup_pipes(const BUSBInterface *);
	status_t aes_usb_read(unsigned char* const, size_t);

	status_t bulk_transfer(int, unsigned char *, size_t);
	status_t clear_stall();
	static status_t g_bulk_transfer(int dir, unsigned char *dat, size_t s)
	{
		return InputDevice->bulk_transfer(dir, dat, s);
	}
	static status_t g_clear_stall(void)
	{
		return InputDevice->clear_stall();
	}
public:
	/* BInputServerDevice */
	virtual status_t InitCheck();
	virtual status_t Start(const char*, void*);
	virtual status_t Stop(const char*, void*);
	/* */

	friend class AesUSBRoster;
};

class AesUSBRoster: public BUSBRoster {
	virtual void DeviceRemoved(BUSBDevice *dev) {}; // no hotplug
public:
	virtual status_t DeviceAdded(BUSBDevice *dev);
};

#endif // AES_INPUT_DEVICE_H
