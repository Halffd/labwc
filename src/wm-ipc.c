// SPDX-License-Identifier: GPL-2.0-only

#include "wm-ipc.h"
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
#include "action.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "output.h"
#include "view.h"

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

static struct view *
find_view_by_app_id(const char *app_id)
{
	struct view *view;
	wl_list_for_each(view, &server.views, link) {
		if (view->app_id && strcmp(view->app_id, app_id) == 0) {
			return view;
		}
	}
	return NULL;
}

static struct view *
find_view_by_title(const char *title)
{
	struct view *view;
	wl_list_for_each(view, &server.views, link) {
		if (view->title && strstr(view->title, title)) {
			return view;
		}
	}
	return NULL;
}

static void
ipc_handle_client(int client_fd)
{
	struct wm_ipc_message msg;
	ssize_t bytes = read(client_fd, &msg, sizeof(msg));
	if (bytes != sizeof(msg)) {
		close(client_fd);
		return;
	}

	if (msg.magic != WM_IPC_MAGIC) {
		close(client_fd);
		return;
	}

	char response[4096];
	ssize_t response_len = 0;

	switch (msg.command) {
	case WM_IPC_LIST_WINDOWS: {
		int count = 0;
		struct view *view;
		wl_list_for_each(view, &server.views, link) {
			count++;
		}
		snprintf(response, sizeof(response), "{\"count\":%d,\"windows\":[", count);
		response_len = strlen(response);

		int i = 0;
		wl_list_for_each(view, &server.views, link) {
			char win_info[512];
			int len = snprintf(win_info, sizeof(win_info),
				"{\"id\":%lu,\"app_id\":\"%s\",\"title\":\"%s\","
				"\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d,"
				"\"minimized\":%s,\"maximized\":%s,\"fullscreen\":%s}",
				(unsigned long)view->creation_id,
				view->app_id ? view->app_id : "",
				view->title ? view->title : "",
				view->current.x, view->current.y,
				view->current.width, view->current.height,
				view->minimized ? "true" : "false",
				(view->maximized != VIEW_AXIS_NONE) ? "true" : "false",
				view->fullscreen ? "true" : "false");

			if (i > 0) {
				write(client_fd, ",", 1);
			}
			write(client_fd, win_info, len);
			i++;
		}
		write(client_fd, "]}", 2);
		close(client_fd);
		return;
	}

	case WM_IPC_GET_ACTIVE_WINDOW: {
		struct view *view = server.active_view;
		if (view) {
			snprintf(response, sizeof(response),
				"{\"id\":%lu,\"app_id\":\"%s\",\"title\":\"%s\"}",
				(unsigned long)view->creation_id,
				view->app_id ? view->app_id : "",
				view->title ? view->title : "");
		} else {
			sprintf(response, "{}");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_FOCUS_WINDOW: {
		char identifier[256] = {0};
		if (msg.size > 0 && msg.size < sizeof(identifier)) {
			read(client_fd, identifier, msg.size);
		}

		struct view *view = NULL;
		if (identifier[0] >= '0' && identifier[0] <= '9') {
			uint64_t id = strtoull(identifier, NULL, 10);
			struct view *v;
			wl_list_for_each(v, &server.views, link) {
				if (v->creation_id == id) {
					view = v;
					break;
				}
			}
		} else if (identifier[0] != '\0') {
			view = find_view_by_app_id(identifier);
			if (!view) {
				view = find_view_by_title(identifier);
			}
		}

		if (view) {
			desktop_focus_view(view, true);
			sprintf(response, "OK");
		} else {
			sprintf(response, "NOT_FOUND");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_CLOSE_WINDOW: {
		char identifier[256] = {0};
		if (msg.size > 0 && msg.size < sizeof(identifier)) {
			read(client_fd, identifier, msg.size);
		}

		struct view *view = NULL;
		if (identifier[0] >= '0' && identifier[0] <= '9') {
			uint64_t id = strtoull(identifier, NULL, 10);
			struct view *v;
			wl_list_for_each(v, &server.views, link) {
				if (v->creation_id == id) {
					view = v;
					break;
				}
			}
		} else if (identifier[0] != '\0') {
			view = find_view_by_app_id(identifier);
			if (!view) {
				view = find_view_by_title(identifier);
			}
		}

		if (view) {
			view_close(view);
			sprintf(response, "OK");
		} else {
			sprintf(response, "NOT_FOUND");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_MAXIMIZE_WINDOW: {
		char identifier[256] = {0};
		if (msg.size > 0 && msg.size < sizeof(identifier)) {
			read(client_fd, identifier, msg.size);
		}

		struct view *view = server.active_view;
		if (identifier[0] >= '0' && identifier[0] <= '9') {
			uint64_t id = strtoull(identifier, NULL, 10);
			struct view *v;
			wl_list_for_each(v, &server.views, link) {
				if (v->creation_id == id) {
					view = v;
					break;
				}
			}
		} else if (identifier[0] != '\0') {
			view = find_view_by_app_id(identifier);
			if (!view) {
				view = find_view_by_title(identifier);
			}
		}

		if (view) {
			view_maximize(view, /*store_natural_geometry*/ true);
			sprintf(response, "OK");
		} else {
			sprintf(response, "NOT_FOUND");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_MINIMIZE_WINDOW: {
		char identifier[256] = {0};
		if (msg.size > 0 && msg.size < sizeof(identifier)) {
			read(client_fd, identifier, msg.size);
		}

		struct view *view = server.active_view;
		if (identifier[0] >= '0' && identifier[0] <= '9') {
			uint64_t id = strtoull(identifier, NULL, 10);
			struct view *v;
			wl_list_for_each(v, &server.views, link) {
				if (v->creation_id == id) {
					view = v;
					break;
				}
			}
		} else if (identifier[0] != '\0') {
			view = find_view_by_app_id(identifier);
			if (!view) {
				view = find_view_by_title(identifier);
			}
		}

		if (view) {
			view_minimize(view, server.active_view == view);
			sprintf(response, "OK");
		} else {
			sprintf(response, "NOT_FOUND");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_RESTORE_WINDOW: {
		char identifier[256] = {0};
		if (msg.size > 0 && msg.size < sizeof(identifier)) {
			read(client_fd, identifier, msg.size);
		}

		struct view *view = server.active_view;
		if (identifier[0] >= '0' && identifier[0] <= '9') {
			uint64_t id = strtoull(identifier, NULL, 10);
			struct view *v;
			wl_list_for_each(v, &server.views, link) {
				if (v->creation_id == id) {
					view = v;
					break;
				}
			}
		} else if (identifier[0] != '\0') {
			view = find_view_by_app_id(identifier);
			if (!view) {
				view = find_view_by_title(identifier);
			}
		}

if (view) {
		view_set_maximized(view, VIEW_AXIS_NONE);
		if (view->minimized) {
			view_minimize(view, false);
		}
		sprintf(response, "OK");
		} else {
			sprintf(response, "NOT_FOUND");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_FULLSCREEN_WINDOW: {
		char identifier[256] = {0};
		if (msg.size > 0 && msg.size < sizeof(identifier)) {
			read(client_fd, identifier, msg.size);
		}

		struct view *view = server.active_view;
		if (identifier[0] >= '0' && identifier[0] <= '9') {
			uint64_t id = strtoull(identifier, NULL, 10);
			struct view *v;
			wl_list_for_each(v, &server.views, link) {
				if (v->creation_id == id) {
					view = v;
					break;
				}
			}
		} else if (identifier[0] != '\0') {
			view = find_view_by_app_id(identifier);
			if (!view) {
				view = find_view_by_title(identifier);
			}
		}

if (view) {
		view_toggle_fullscreen(view);
		sprintf(response, "OK");
		} else {
			sprintf(response, "NOT_FOUND");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_MOVE_WINDOW: {
		char args[512] = {0};
		if (msg.size > 0 && msg.size < sizeof(args)) {
			read(client_fd, args, msg.size);
		}

		struct view *view = server.active_view;
		int x, y;
		if (sscanf(args, "%d,%d", &x, &y) == 2) {
			if (args[0] >= '0' && args[0] <= '9') {
				uint64_t id = strtoull(args, NULL, 10);
				struct view *v;
				wl_list_for_each(v, &server.views, link) {
					if (v->creation_id == id) {
						view = v;
						break;
					}
				}
			}
			view_move(view, x, y);
			sprintf(response, "OK");
		} else {
			sprintf(response, "INVALID_ARGS");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_RESIZE_WINDOW: {
		char args[512] = {0};
		if (msg.size > 0 && msg.size < sizeof(args)) {
			read(client_fd, args, msg.size);
		}

		struct view *view = server.active_view;
		int width, height;
		if (sscanf(args, "%dx%d", &width, &height) == 2) {
			if (args[0] >= '0' && args[0] <= '9') {
				uint64_t id = strtoull(args, NULL, 10);
				struct view *v;
				wl_list_for_each(v, &server.views, link) {
					if (v->creation_id == id) {
						view = v;
						break;
					}
				}
			}
			view_move_resize(view, (struct wlr_box){0, 0, width, height});
			sprintf(response, "OK");
		} else {
			sprintf(response, "INVALID_ARGS");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_SET_WINDOW_GEOMETRY: {
		char args[512] = {0};
		if (msg.size > 0 && msg.size < sizeof(args)) {
			read(client_fd, args, msg.size);
		}

		struct view *view = server.active_view;
		int x, y, width, height;
		if (sscanf(args, "%d,%d,%d,%d", &x, &y, &width, &height) == 4) {
			if (args[0] >= '0' && args[0] <= '9') {
				uint64_t id = strtoull(args, NULL, 10);
				struct view *v;
				wl_list_for_each(v, &server.views, link) {
					if (v->creation_id == id) {
						view = v;
						break;
					}
				}
			}
			view_move_resize(view, (struct wlr_box){x, y, width, height});
			sprintf(response, "OK");
		} else {
			sprintf(response, "INVALID_ARGS");
		}
		response_len = strlen(response);
		break;
	}

	case WM_IPC_GET_WINDOW_GEOMETRY: {
		struct view *view = server.active_view;
		if (view) {
			snprintf(response, sizeof(response),
				"{\"x\":%d,\"y\":%d,\"width\":%d,\"height\":%d}",
				view->current.x, view->current.y,
				view->current.width, view->current.height);
		} else {
			sprintf(response, "{}");
		}
		response_len = strlen(response);
		break;
	}

	default:
		sprintf(response, "UNKNOWN_COMMAND");
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
wm_ipc_init(void)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR not set, cannot create WM IPC socket");
		return;
	}

	snprintf(socket_path, sizeof(socket_path), "%s/labwc-wm.sock", runtime_dir);

	ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc_fd < 0) {
		wlr_log(WLR_ERROR, "Failed to create WM IPC socket");
		return;
	}

	unlink(socket_path);

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

	if (bind(ipc_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		wlr_log(WLR_ERROR, "Failed to bind WM IPC socket to %s", socket_path);
		close(ipc_fd);
		ipc_fd = -1;
		return;
	}

	if (listen(ipc_fd, 5) < 0) {
		wlr_log(WLR_ERROR, "Failed to listen on WM IPC socket");
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
		wlr_log(WLR_ERROR, "Failed to add WM IPC event source");
		close(ipc_fd);
		ipc_fd = -1;
		unlink(socket_path);
		return;
	}

	wlr_log(WLR_INFO, "WM IPC listening on %s", socket_path);
}

void
wm_ipc_finish(void)
{
	ipc_close();
}