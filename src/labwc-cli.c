// SPDX-License-Identifier: GPL-2.0-only
/*
 * labwc IPC CLI
 *
 * Usage: labwc-cli <command> [args]
 *
 * Commands:
 *   screenshot full              - Screenshot all outputs
 *   screenshot output <name>     - Screenshot specific output
 *   screenshot region <x> <y> <w> <h> - Screenshot region
 *   screenshot focused           - Screenshot focused window
 *   screenshot list              - List available outputs
 *
 *   wm list                      - List windows
 *   wm active                    - Get active window info
 *   wm focus <window_id>         - Focus window by ID
 *   wm close <window_id>         - Close window by ID
 *   wm maximize <window_id>      - Maximize window
 *   wm minimize <window_id>      - Minimize window
 *   wm restore <window_id>       - Restore window
 *   wm fullscreen <window_id>    - Toggle fullscreen
 *
 *   zoom get                     - Get current zoom scale
 *   zoom set <scale>             - Set zoom scale
 *   zoom increase                - Increase zoom
 *   zoom decrease                - Decrease zoom
 *   zoom reset                   - Reset zoom
 *   zoom enabled                 - Check if zoom is enabled
 *
 *   overview start               - Start overview
 *   overview stop                - Stop overview
 *   overview toggle              - Toggle overview
 *   overview status              - Check overview status
 *
 *   edge get                     - Get edge trigger settings
 *   edge set <l>,<r>,<t>,<b>     - Set edge triggers (0/1)
 *
 *   invert window                - Toggle window inversion
 *   invert monitor               - Toggle monitor inversion
 *   invert get                   - Get inversion status
 */

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#define LABWC_IPC_MAGIC 0x4C425743

enum labwc_ipc_command {
	LABWC_IPC_SCREENSHOT_FULL = 1,
	LABWC_IPC_SCREENSHOT_OUTPUT = 2,
	LABWC_IPC_SCREENSHOT_REGION = 3,
	LABWC_IPC_SCREENSHOT_FOCUSED = 4,
	LABWC_IPC_SCREENSHOT_LIST_OUTPUTS = 5,

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

	LABWC_IPC_ZOOM_GET_SCALE = 31,
	LABWC_IPC_ZOOM_SET_SCALE = 32,
	LABWC_IPC_ZOOM_INCREASE = 33,
	LABWC_IPC_ZOOM_DECREASE = 34,
	LABWC_IPC_ZOOM_RESET = 35,
	LABWC_IPC_ZOOM_GET_ENABLED = 36,
	LABWC_IPC_ZOOM_GET_OUTPUT = 37,

	LABWC_IPC_OVERVIEW_START = 41,
	LABWC_IPC_OVERVIEW_STOP = 42,
	LABWC_IPC_OVERVIEW_TOGGLE = 43,
	LABWC_IPC_OVERVIEW_STATUS = 44,

	LABWC_IPC_EDGE_GET_TRIGGERS = 51,
	LABWC_IPC_EDGE_SET_TRIGGERS = 52,

	LABWC_IPC_INVERT_WINDOW = 61,
	LABWC_IPC_INVERT_MONITOR = 62,
	LABWC_IPC_INVERT_GET = 63,

	LABWC_IPC_XWAYLAND_STATUS = 71,
	LABWC_IPC_XWAYLAND_RESTART = 72,
};

static void
usage(const char *argv0)
{
	fprintf(stderr, "Usage: %s <command> [args]\n\n", argv0);
	fprintf(stderr, "Screenshot commands:\n");
	fprintf(stderr, "  %s screenshot full\n", argv0);
	fprintf(stderr, "  %s screenshot output <name>\n", argv0);
	fprintf(stderr, "  %s screenshot region <x> <y> <w> <h>\n", argv0);
	fprintf(stderr, "  %s screenshot focused\n", argv0);
	fprintf(stderr, "  %s screenshot list\n\n", argv0);
	fprintf(stderr, "Window management:\n");
	fprintf(stderr, "  %s wm list\n", argv0);
	fprintf(stderr, "  %s wm active\n", argv0);
	fprintf(stderr, "  %s wm focus <id>\n", argv0);
	fprintf(stderr, "  %s wm close <id>\n", argv0);
	fprintf(stderr, "  %s wm maximize <id>\n", argv0);
	fprintf(stderr, "  %s wm minimize <id>\n", argv0);
	fprintf(stderr, "  %s wm restore <id>\n", argv0);
	fprintf(stderr, "  %s wm fullscreen <id>\n\n", argv0);
	fprintf(stderr, "Zoom commands:\n");
	fprintf(stderr, "  %s zoom get\n", argv0);
	fprintf(stderr, "  %s zoom set <scale>\n", argv0);
	fprintf(stderr, "  %s zoom increase\n", argv0);
	fprintf(stderr, "  %s zoom decrease\n", argv0);
	fprintf(stderr, "  %s zoom reset\n", argv0);
	fprintf(stderr, "  %s zoom enabled\n\n", argv0);
	fprintf(stderr, "Overview commands:\n");
	fprintf(stderr, "  %s overview start\n", argv0);
	fprintf(stderr, "  %s overview stop\n", argv0);
	fprintf(stderr, "  %s overview toggle\n", argv0);
	fprintf(stderr, "  %s overview status\n\n", argv0);
	fprintf(stderr, "Screen edge commands:\n");
	fprintf(stderr, "  %s edge get\n", argv0);
	fprintf(stderr, "  %s edge set <l>,<r>,<t>,<b>\n\n", argv0);
fprintf(stderr, "Invert commands:\n");
	fprintf(stderr, " %s invert window\n", argv0);
	fprintf(stderr, " %s invert monitor\n", argv0);
	fprintf(stderr, " %s invert get\n\n", argv0);
	fprintf(stderr, "XWayland commands:\n");
	fprintf(stderr, " %s xwayland status\n", argv0);
	fprintf(stderr, " %s xwayland restart\n", argv0);
}

static int
connect_socket(void)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		fprintf(stderr, "XDG_RUNTIME_DIR not set\n");
		return -1;
	}

	char socket_path[256];
	snprintf(socket_path, sizeof(socket_path), "%s/labwc.sock", runtime_dir);

	int fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (fd < 0) {
		perror("socket");
		return -1;
	}

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

	if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Cannot connect to %s\n", socket_path);
		close(fd);
		return -1;
	}

	return fd;
}

static ssize_t
send_command(int fd, uint32_t command, const char *payload, size_t payload_len)
{
	uint8_t header[12];
	header[0] = LABWC_IPC_MAGIC & 0xFF;
	header[1] = (LABWC_IPC_MAGIC >> 8) & 0xFF;
	header[2] = (LABWC_IPC_MAGIC >> 16) & 0xFF;
	header[3] = (LABWC_IPC_MAGIC >> 24) & 0xFF;
	header[4] = command & 0xFF;
	header[5] = (command >> 8) & 0xFF;
	header[6] = (command >> 16) & 0xFF;
	header[7] = (command >> 24) & 0xFF;
	header[8] = payload_len & 0xFF;
	header[9] = (payload_len >> 8) & 0xFF;
	header[10] = (payload_len >> 16) & 0xFF;
	header[11] = (payload_len >> 24) & 0xFF;

	if (write(fd, header, 12) != 12) {
		return -1;
	}

	if (payload && payload_len > 0) {
		if (write(fd, payload, payload_len) != (ssize_t)payload_len) {
			return -1;
		}
	}

	return payload_len;
}

static ssize_t
read_response(int fd, char *buf, size_t len)
{
	return read(fd, buf, len);
}

int
main(int argc, char *argv[])
{
	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	const char *cmd = argv[1];
	const char *subcmd = argc > 2 ? argv[2] : NULL;

	int fd = connect_socket();
	if (fd < 0) {
		return 1;
	}

	int ret = 0;
	char response[1024];
	ssize_t response_len;

	if (strcmp(cmd, "screenshot") == 0) {
		if (!subcmd) {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
		if (strcmp(subcmd, "full") == 0) {
			send_command(fd, LABWC_IPC_SCREENSHOT_FULL, NULL, 0);
		} else if (strcmp(subcmd, "list") == 0) {
			send_command(fd, LABWC_IPC_SCREENSHOT_LIST_OUTPUTS, NULL, 0);
		} else if (strcmp(subcmd, "focused") == 0) {
			send_command(fd, LABWC_IPC_SCREENSHOT_FOCUSED, NULL, 0);
		} else if (strcmp(subcmd, "output") == 0) {
			if (argc < 4) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			send_command(fd, LABWC_IPC_SCREENSHOT_OUTPUT, argv[3], strlen(argv[3]));
		} else if (strcmp(subcmd, "region") == 0) {
			if (argc < 7) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			char payload[64];
			snprintf(payload, sizeof(payload), "%s %s %s %s", argv[3], argv[4], argv[5], argv[6]);
			send_command(fd, LABWC_IPC_SCREENSHOT_REGION, payload, strlen(payload));
		} else {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
	} else if (strcmp(cmd, "wm") == 0) {
		if (!subcmd) {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
		if (strcmp(subcmd, "list") == 0) {
			send_command(fd, LABWC_IPC_WM_LIST_WINDOWS, NULL, 0);
		} else if (strcmp(subcmd, "active") == 0) {
			send_command(fd, LABWC_IPC_WM_GET_ACTIVE, NULL, 0);
		} else if (strcmp(subcmd, "focus") == 0) {
			if (argc < 4) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			send_command(fd, LABWC_IPC_WM_FOCUS, argv[3], strlen(argv[3]));
		} else if (strcmp(subcmd, "close") == 0) {
			if (argc < 4) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			send_command(fd, LABWC_IPC_WM_CLOSE, argv[3], strlen(argv[3]));
		} else if (strcmp(subcmd, "maximize") == 0) {
			if (argc < 4) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			send_command(fd, LABWC_IPC_WM_MAXIMIZE, argv[3], strlen(argv[3]));
		} else if (strcmp(subcmd, "minimize") == 0) {
			if (argc < 4) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			send_command(fd, LABWC_IPC_WM_MINIMIZE, argv[3], strlen(argv[3]));
		} else if (strcmp(subcmd, "restore") == 0) {
			if (argc < 4) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			send_command(fd, LABWC_IPC_WM_RESTORE, argv[3], strlen(argv[3]));
		} else if (strcmp(subcmd, "fullscreen") == 0) {
			if (argc < 4) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			send_command(fd, LABWC_IPC_WM_FULLSCREEN, argv[3], strlen(argv[3]));
		} else {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
	} else if (strcmp(cmd, "zoom") == 0) {
		if (!subcmd) {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
		if (strcmp(subcmd, "get") == 0) {
			send_command(fd, LABWC_IPC_ZOOM_GET_SCALE, NULL, 0);
		} else if (strcmp(subcmd, "enabled") == 0) {
			send_command(fd, LABWC_IPC_ZOOM_GET_ENABLED, NULL, 0);
		} else if (strcmp(subcmd, "increase") == 0) {
			send_command(fd, LABWC_IPC_ZOOM_INCREASE, NULL, 0);
		} else if (strcmp(subcmd, "decrease") == 0) {
			send_command(fd, LABWC_IPC_ZOOM_DECREASE, NULL, 0);
		} else if (strcmp(subcmd, "reset") == 0) {
			send_command(fd, LABWC_IPC_ZOOM_RESET, NULL, 0);
		} else if (strcmp(subcmd, "set") == 0) {
			if (argc < 4) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			send_command(fd, LABWC_IPC_ZOOM_SET_SCALE, argv[3], strlen(argv[3]));
		} else {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
	} else if (strcmp(cmd, "overview") == 0) {
		if (!subcmd) {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
		if (strcmp(subcmd, "start") == 0) {
			send_command(fd, LABWC_IPC_OVERVIEW_START, NULL, 0);
		} else if (strcmp(subcmd, "stop") == 0) {
			send_command(fd, LABWC_IPC_OVERVIEW_STOP, NULL, 0);
		} else if (strcmp(subcmd, "toggle") == 0) {
			send_command(fd, LABWC_IPC_OVERVIEW_TOGGLE, NULL, 0);
		} else if (strcmp(subcmd, "status") == 0) {
			send_command(fd, LABWC_IPC_OVERVIEW_STATUS, NULL, 0);
		} else {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
	} else if (strcmp(cmd, "edge") == 0) {
		if (!subcmd) {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
		if (strcmp(subcmd, "get") == 0) {
			send_command(fd, LABWC_IPC_EDGE_GET_TRIGGERS, NULL, 0);
		} else if (strcmp(subcmd, "set") == 0) {
			if (argc < 4) {
				usage(argv[0]);
				ret = 1;
				goto done;
			}
			send_command(fd, LABWC_IPC_EDGE_SET_TRIGGERS, argv[3], strlen(argv[3]));
		} else {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
	} else if (strcmp(cmd, "invert") == 0) {
		if (!subcmd) {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
		if (strcmp(subcmd, "window") == 0) {
			send_command(fd, LABWC_IPC_INVERT_WINDOW, NULL, 0);
		} else if (strcmp(subcmd, "monitor") == 0) {
			send_command(fd, LABWC_IPC_INVERT_MONITOR, NULL, 0);
		} else if (strcmp(subcmd, "get") == 0) {
			send_command(fd, LABWC_IPC_INVERT_GET, NULL, 0);
		} else {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
	} else if (strcmp(cmd, "xwayland") == 0) {
		if (!subcmd) {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
		if (strcmp(subcmd, "status") == 0) {
			send_command(fd, LABWC_IPC_XWAYLAND_STATUS, NULL, 0);
		} else if (strcmp(subcmd, "restart") == 0) {
			send_command(fd, LABWC_IPC_XWAYLAND_RESTART, NULL, 0);
		} else {
			usage(argv[0]);
			ret = 1;
			goto done;
		}
	} else {
		usage(argv[0]);
		ret = 1;
		goto done;
	}

	response_len = read_response(fd, response, sizeof(response) - 1);
	if (response_len > 0) {
		response[response_len] = '\0';
		printf("%s\n", response);
	} else {
		fprintf(stderr, "No response from labwc\n");
		ret = 1;
	}

done:
	close(fd);
	return ret;
}