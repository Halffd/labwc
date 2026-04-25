/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCREENSHOT_IPC_H
#define LABWC_SCREENSHOT_IPC_H

#include <stdint.h>

enum screenshot_ipc_command {
	SCREENSHOT_IPC_FULL = 1,
	SCREENSHOT_IPC_OUTPUT = 2,
	SCREENSHOT_IPC_REGION = 3,
	SCREENSHOT_IPC_FOCUSED = 4,
	SCREENSHOT_IPC_LIST_OUTPUTS = 5,
};

struct screenshot_ipc_message {
	uint32_t magic;
	uint32_t command;
	uint32_t size;
	char payload[];
};

#define SCREENSHOT_IPC_MAGIC 0x53637265 /* "Scre" in hex */

void screenshot_ipc_init(void);
void screenshot_ipc_finish(void);

#endif /* LABWC_SCREENSHOT_IPC_H */