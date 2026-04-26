/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_IPC_H
#define LABWC_IPC_H

#include <stdint.h>

enum labwc_ipc_command {
	/* Screenshot commands (1-10) */
	LABWC_IPC_SCREENSHOT_FULL = 1,
	LABWC_IPC_SCREENSHOT_OUTPUT = 2,
	LABWC_IPC_SCREENSHOT_REGION = 3,
	LABWC_IPC_SCREENSHOT_FOCUSED = 4,
	LABWC_IPC_SCREENSHOT_LIST_OUTPUTS = 5,

	/* Window management commands (11-30) */
	LABWC_IPC_WM_LIST_WINDOWS = 11,
	LABWC_IPC_WM_GET_ACTIVE = 12,
	LABWC_IPC_WM_FOCUS = 13,
	LABWC_IPC_WM_CLOSE = 14,
	LABWC_IPC_WM_MAXIMIZE = 15,
	LABWC_IPC_WM_MINIMIZE = 16,
	LABWC_IPC_WM_RESTORE = 17,
	LABWC_IPC_WM_FULLSCREEN = 18,
	LABWC_IPC_WM_MOVE = 19,
	LABWC_IPC_WM_RESIZE = 20,

	/* Zoom commands (31-40) */
	LABWC_IPC_ZOOM_GET_SCALE = 31,
	LABWC_IPC_ZOOM_SET_SCALE = 32,
	LABWC_IPC_ZOOM_INCREASE = 33,
	LABWC_IPC_ZOOM_DECREASE = 34,
	LABWC_IPC_ZOOM_RESET = 35,
	LABWC_IPC_ZOOM_GET_ENABLED = 36,
	LABWC_IPC_ZOOM_GET_OUTPUT = 37,

	/* Overview/Alt-tab commands (41-50) */
	LABWC_IPC_OVERVIEW_START = 41,
	LABWC_IPC_OVERVIEW_STOP = 42,
	LABWC_IPC_OVERVIEW_TOGGLE = 43,
	LABWC_IPC_OVERVIEW_STATUS = 44,

	/* Screen edge commands (51-60) */
	LABWC_IPC_EDGE_GET_TRIGGERS = 51,
	LABWC_IPC_EDGE_SET_TRIGGERS = 52,
};

struct labwc_ipc_message {
	uint32_t magic;
	uint32_t command;
	uint32_t size;
	char payload[];
};

#define LABWC_IPC_MAGIC 0x4C425743 /* "LBC" in hex - LabWC */

void labwc_ipc_init(void);
void labwc_ipc_finish(void);

#endif /* LABWC_IPC_H */