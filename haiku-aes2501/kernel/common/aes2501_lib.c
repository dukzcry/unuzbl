/* Written by Artem Falcon <lomka@gero.in> */

/* Early stage of libfpaes */

#include "aes2501_lib.h"

#include <sys/param.h>
#include <SupportDefs.h>

#define MAX_REGWRITES_PER_REQ 16
#define MAX_RETRIES 1

/* These are universal amongst AuthenTec devices */
status_t aes_usb_exec(status_t (*usb_write)(), status_t (*clear_stall)(),
	bool, const pairs *, unsigned int);

static status_t usb_write(status_t (*bulk_transfer)(),
	const pairs *cmd, unsigned int num)
{
	size_t size = num * 2, offset = 0;
	unsigned char data[size];
	unsigned int i;

	for (i = 0; i < num; i++) {
		data[offset++] = cmd[i].reg;
		data[offset++] = cmd[i].val;
	}

	return bulk_transfer(AES2501_OUT, data, size);
}

status_t aes_usb_exec(status_t (*bulk_transfer)(), status_t (*clear_stall)(),
	bool strict, const pairs *cmd, unsigned int num)
{
	unsigned int i;
	int skip = 0, add_offset = 0;

	for (i = 0; i < num; i += add_offset + skip) {
		int limit = MIN(num, i + MAX_REGWRITES_PER_REQ), j;
		status_t res;

		skip = 0;
		/* handle 0 reg, i.e. request separator */
		if (!cmd[i].reg) {
			skip = 1;
			add_offset = 0;
			continue;
		}
		for (j = i; j < limit; j++) // up to: limit || new separator
			if (!cmd[j].reg) {
				skip = 1;
				break;
			}
		/* */

		add_offset = j - i;
		limit = strict ? 0 : MAX_RETRIES;
		for (j = 0; j <= limit; j++) {
			if ((res = usb_write(bulk_transfer, &cmd[i], add_offset)) == B_OK)
				break;
			else if (res == B_TIMED_OUT) {
				if (strict)
					return B_ERROR;
				break;
			}
			else if (res == B_BUSY || B_DEV_FIFO_UNDERRUN || B_DEV_FIFO_OVERRUN) {
				if (strict)
					return B_ERROR;
				continue;
			}
			else if (res == B_DEV_STALLED) {
				if (strict)
					return B_ERROR;
				if(clear_stall() != B_OK)
					break;
			}
			else return B_ERROR;
		}
	}

	return B_OK;
}
/* */
