/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_WM_IPC_H
#define LABWC_WM_IPC_H

#include <stdbool.h>
#include <stdint.h>

#define WM_IPC_MAGIC 0x576d0000  /* "Wm\0" */

enum wm_ipc_command {
	WM_IPC_LIST_WINDOWS = 1,
	WM_IPC_GET_ACTIVE_WINDOW = 2,
	WM_IPC_FOCUS_WINDOW = 3,
	WM_IPC_CLOSE_WINDOW = 4,
	WM_IPC_MOVE_WINDOW = 5,
	WM_IPC_RESIZE_WINDOW = 6,
	WM_IPC_MAXIMIZE_WINDOW = 7,
	WM_IPC_MINIMIZE_WINDOW = 8,
	WM_IPC_RESTORE_WINDOW = 9,
	WM_IPC_FULLSCREEN_WINDOW = 10,
	WM_IPC_SET_WINDOW_GEOMETRY = 11,
	WM_IPC_GET_WINDOW_GEOMETRY = 12,
	WM_IPC_MOVE_TO_OUTPUT = 13,
	WM_IPC_SNAP_WINDOW = 14,
};

struct wm_ipc_message {
	uint32_t magic;
	uint32_t command;
	uint32_t size;
	char payload[];
};

struct wm_window_info {
	char app_id[128];
	char title[256];
	int x, y;
	int width, height;
	int output_id;
	int desktop_id;
	uint32_t states;  /* bitmask of window states */
};

#define WM_WINDOW_STATE_MINIMIZED (1 << 0)
#define WM_WINDOW_STATE_MAXIMIZED (1 << 1)
#define WM_WINDOW_STATE_FULLSCREEN (1 << 2)
#define WM_WINDOW_STATE_FOCUSED (1 << 3)
#define WM_WINDOW_STATE_ACTIVE (1 << 4)

void wm_ipc_init(void);
void wm_ipc_finish(void);

#endif /* LABWC_WM_IPC_H */