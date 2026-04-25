// SPDX-License-Identifier: GPL-2.0-only

#include "zoom-ipc.h"
#include "zoom.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wlr/util/log.h>
#include "labwc.h"
#include "config/rcxml.h"
#include "output.h"

static int ipc_fd = -1;
static struct wl_event_source *ipc_event_source = NULL;
static char socket_path[256];

static void
ipc_close(void)
{
	if (ipc_event_source) {
		wl_event_source_remove(ipc_event_source);
		ipc_event_source = NULL;
	}
	if (ipc_fd >= 0) {
		close(ipc_fd);
		ipc_fd = -1;
	}
	unlink(socket_path);
}

static void
ipc_handle_client(int client_fd)
{
	struct zoom_ipc_message msg;
	ssize_t bytes = read(client_fd, &msg, sizeof(msg));
	if (bytes != sizeof(msg)) {
		close(client_fd);
		return;
	}

	if (msg.magic != ZOOM_IPC_MAGIC) {
		close(client_fd);
		return;
	}

	struct output *output = output_nearest_to_cursor();
	if (!output) {
		close(client_fd);
		return;
	}

	char response[64];
	ssize_t response_len = 0;

	switch (msg.command) {
	case ZOOM_IPC_GET_SCALE: {
		double scale = zoom_get_scale(output);
		snprintf(response, sizeof(response), "%.2f", scale);
		response_len = strlen(response);
		break;
	}
	case ZOOM_IPC_SET_SCALE: {
		if (msg.size > 0) {
			char scale_str[32] = {0};
			ssize_t scale_len = msg.size < sizeof(scale_str) - 1 ? msg.size : sizeof(scale_str) - 1;
			if (read(client_fd, scale_str, scale_len) == scale_len) {
				double scale = atof(scale_str);
				if (scale >= 1.0 && scale <= 10.0) {
					zoom_set_scale(output, scale);
				}
			}
		}
		strcpy(response, "OK");
		response_len = 2;
		break;
	}
	case ZOOM_IPC_INCREASE:
		zoom_adjust_scale(output, rc.mag_increment);
		snprintf(response, sizeof(response), "%.2f", zoom_get_scale(output));
		response_len = strlen(response);
		break;
	case ZOOM_IPC_DECREASE:
		if (output->zoom_scale > 1.0) {
			zoom_adjust_scale(output, -rc.mag_increment);
		}
		snprintf(response, sizeof(response), "%.2f", zoom_get_scale(output));
		response_len = strlen(response);
		break;
	case ZOOM_IPC_RESET:
		zoom_disable(output);
		strcpy(response, "1.00");
		response_len = 4;
		break;
	case ZOOM_IPC_GET_ENABLED:
		snprintf(response, sizeof(response), "%d", output->zoom_enabled ? 1 : 0);
		response_len = strlen(response);
		break;
	case ZOOM_IPC_GET_OUTPUT:
		snprintf(response, sizeof(response), "%s", output->wlr_output->name);
		response_len = strlen(response);
		break;
	default:
		strcpy(response, "UNKNOWN_COMMAND");
		response_len = strlen(response);
		break;
	}

	write(client_fd, response, response_len);
	close(client_fd);
}

static int
ipc_on_activity(int fd, uint32_t mask, void *data)
{
	int client_fd = accept(fd, NULL, NULL);
	if (client_fd < 0) {
		return 0;
	}
	ipc_handle_client(client_fd);
	return 0;
}

void
zoom_ipc_init(void)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR not set, cannot create zoom IPC socket");
		return;
	}

	snprintf(socket_path, sizeof(socket_path), "%s/labwc-zoom.sock", runtime_dir);

	ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc_fd < 0) {
		wlr_log(WLR_ERROR, "Failed to create zoom IPC socket");
		return;
	}

	unlink(socket_path);

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

	if (bind(ipc_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		wlr_log(WLR_ERROR, "Failed to bind zoom IPC socket to %s", socket_path);
		close(ipc_fd);
		ipc_fd = -1;
		return;
	}

	if (listen(ipc_fd, 5) < 0) {
		wlr_log(WLR_ERROR, "Failed to listen on zoom IPC socket");
		close(ipc_fd);
		ipc_fd = -1;
		unlink(socket_path);
		return;
	}

	int flags = fcntl(ipc_fd, F_GETFL, 0);
	fcntl(ipc_fd, F_SETFL, flags | O_NONBLOCK);

	ipc_event_source = wl_event_loop_add_fd(server.wl_event_loop, ipc_fd,
		WL_EVENT_READABLE, ipc_on_activity, NULL);
	if (!ipc_event_source) {
		wlr_log(WLR_ERROR, "Failed to add zoom IPC event source");
		close(ipc_fd);
		ipc_fd = -1;
		unlink(socket_path);
		return;
	}

	wlr_log(WLR_INFO, "Zoom IPC listening on %s", socket_path);
}

void
zoom_ipc_finish(void)
{
	ipc_close();
}