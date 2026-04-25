/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_ZOOM_IPC_H
#define LABWC_ZOOM_IPC_H

#include <stdbool.h>
#include <stdint.h>

struct workspace;

enum zoom_ipc_command {
	ZOOM_IPC_GET_SCALE = 1,
	ZOOM_IPC_SET_SCALE = 2,
	ZOOM_IPC_INCREASE = 3,
	ZOOM_IPC_DECREASE = 4,
	ZOOM_IPC_RESET = 5,
	ZOOM_IPC_GET_ENABLED = 6,
	ZOOM_IPC_GET_OUTPUT = 7,
};

struct zoom_ipc_message {
	uint32_t magic;
	uint32_t command;
	uint32_t size;
	char payload[];
};

#define ZOOM_IPC_MAGIC 0x5A6F6F6D  /* "Zoom" in hex */

void zoom_ipc_init(void);
void zoom_ipc_finish(void);

#endif /* LABWC_ZOOM_IPC_H */