// SPDX-License-Identifier: GPL-2.0-only

#include "labwc-ipc.h"
#include "common/macros.h"
#include "common/string-helpers.h"
#include "labwc.h"
#include "output.h"
#include "overview.h"
#include "screen-edges.h"
#include "view.h"
#include "zoom.h"
#include <assert.h>
#include <cairo/cairo.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <png.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/log.h>
#include "config/rcxml.h"

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
save_png_from_buffer(struct wlr_buffer *buffer, int width, int height,
	const char *filename)
{
	void *data;
	uint32_t format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ,
			&data, &format, &stride)) {
		wlr_log(WLR_ERROR, "failed to access buffer data");
		return;
	}

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		data, CAIRO_FORMAT_ARGB32, width, height, stride);

	if (cairo_surface_status(surface)) {
		wlr_log(WLR_ERROR, "cairo surface error");
		wlr_buffer_end_data_ptr_access(buffer);
		cairo_surface_destroy(surface);
		return;
	}

	cairo_surface_write_to_png(surface, filename);
	cairo_surface_destroy(surface);
	wlr_buffer_end_data_ptr_access(buffer);
}

static void
render_node(struct wlr_render_pass *pass, struct wlr_scene_node *node, int x, int y)
{
	switch (node->type) {
	case WLR_SCENE_NODE_TREE: {
		struct wlr_scene_tree *tree = wlr_scene_tree_from_node(node);
		struct wlr_scene_node *child;
		wl_list_for_each(child, &tree->children, link) {
			render_node(pass, child, x + node->x, y + node->y);
		}
		break;
	}
	case WLR_SCENE_NODE_BUFFER: {
		struct wlr_scene_buffer *scene_buffer =
			wlr_scene_buffer_from_node(node);
		if (!scene_buffer->buffer) {
			break;
		}
		struct wlr_texture *texture = NULL;
		struct wlr_client_buffer *client_buffer =
			wlr_client_buffer_get(scene_buffer->buffer);
		if (client_buffer) {
			texture = client_buffer->texture;
		}
		if (!texture) {
			break;
		}
		wlr_render_pass_add_texture(pass, &(struct wlr_render_texture_options){
			.texture = texture,
			.src_box = scene_buffer->src_box,
			.dst_box = {
				.x = x,
				.y = y,
				.width = scene_buffer->dst_width,
				.height = scene_buffer->dst_height,
			},
			.transform = scene_buffer->transform,
		});
		break;
	}
	case WLR_SCENE_NODE_RECT:
		break;
	}
}

static bool
capture_output(struct output *output, struct wlr_buffer **buffer_out,
	int *width_out, int *height_out)
{
	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);

	struct wlr_buffer *buffer = wlr_allocator_create_buffer(
		server.allocator, width, height,
		&output->wlr_output->swapchain->format);
	if (!buffer) {
		return false;
	}

	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		server.renderer, buffer, NULL);

	struct wlr_box output_box;
	wlr_output_layout_get_box(server.output_layout, output->wlr_output, &output_box);

	render_node(pass, &server.scene->tree.node, -output_box.x, -output_box.y);

	if (!wlr_render_pass_submit(pass)) {
		wlr_buffer_drop(buffer);
		return false;
	}

	*buffer_out = buffer;
	*width_out = width;
	*height_out = height;
	return true;
}

static bool
capture_focused(struct wlr_buffer **buffer_out, int *width_out, int *height_out)
{
	struct view *view = server.active_view;
	if (!view || !view->content_tree) {
		return false;
	}

	int width = view->current.width;
	int height = view->current.height;

	struct wlr_buffer *buffer = wlr_allocator_create_buffer(
		server.allocator, width, height,
		&view->output->wlr_output->swapchain->format);
	if (!buffer) {
		return false;
	}

	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		server.renderer, buffer, NULL);

	render_node(pass, &view->content_tree->node, 0, 0);

	if (!wlr_render_pass_submit(pass)) {
		wlr_buffer_drop(buffer);
		return false;
	}

	*buffer_out = buffer;
	*width_out = width;
	*height_out = height;
	return true;
}

static void
ipc_handle_client(int client_fd)
{
	struct labwc_ipc_message msg;
	ssize_t bytes = read(client_fd, &msg, sizeof(msg));
	if (bytes != sizeof(msg)) {
		close(client_fd);
		return;
	}

	if (msg.magic != LABWC_IPC_MAGIC) {
		close(client_fd);
		return;
	}

	char *payload = NULL;
	uint32_t payload_size = msg.size;
	if (payload_size > 0) {
		payload = malloc(payload_size + 1);
		if (!payload) {
			close(client_fd);
			return;
		}
		if (read(client_fd, payload, payload_size) != payload_size) {
			free(payload);
			close(client_fd);
			return;
		}
		payload[payload_size] = '\0';
	}

	char response[512];
	ssize_t response_len = 0;
	char path[512];
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		snprintf(response, sizeof(response), "ERROR: XDG_RUNTIME_DIR not set");
		response_len = strlen(response);
		goto send_response;
	}

	struct output *output = NULL;

	switch (msg.command) {
	/* Screenshot commands */
	case LABWC_IPC_SCREENSHOT_FULL: {
		struct output *o;
		int total_width = 0, total_height = 0;
		wl_list_for_each(o, &server.outputs, link) {
			int w, h;
			wlr_output_effective_resolution(o->wlr_output, &w, &h);
			total_width += w;
			total_height = MAX(total_height, h);
		}
		if (total_width == 0 || total_height == 0) {
			total_width = 1920;
			total_height = 1080;
		}
		struct wlr_buffer *buffer = wlr_allocator_create_buffer(
			server.allocator, total_width, total_height, WL_SHM_FORMAT_ARGB8888);
		if (!buffer) {
			snprintf(response, sizeof(response), "ERROR: allocation failed");
			break;
		}
		struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
			server.renderer, buffer, NULL);
		wl_list_for_each(o, &server.outputs, link) {
			struct wlr_box box;
			wlr_output_layout_get_box(server.output_layout, o->wlr_output, &box);
			render_node(pass, &server.scene->tree.node, -box.x, -box.y);
		}
		if (!wlr_render_pass_submit(pass)) {
			wlr_buffer_drop(buffer);
			snprintf(response, sizeof(response), "ERROR: render failed");
			break;
		}
		snprintf(path, sizeof(path), "%s/labwc-screenshot-full-%ld.png",
			runtime_dir, (long)time(NULL));
		save_png_from_buffer(buffer, total_width, total_height, path);
		wlr_buffer_drop(buffer);
		snprintf(response, sizeof(response), "OK:%s", path);
		break;
	}
	case LABWC_IPC_SCREENSHOT_OUTPUT: {
		if (payload) {
			struct output *o;
			wl_list_for_each(o, &server.outputs, link) {
				if (strcmp(o->wlr_output->name, payload) == 0) {
					output = o;
					break;
				}
			}
		}
		if (!output) {
			output = output_nearest_to_cursor();
		}
		if (!output) {
			snprintf(response, sizeof(response), "ERROR: no output");
			break;
		}
		struct wlr_buffer *buffer;
		int width, height;
		if (!capture_output(output, &buffer, &width, &height)) {
			snprintf(response, sizeof(response), "ERROR: capture failed");
			break;
		}
		snprintf(path, sizeof(path), "%s/labwc-screenshot-%s-%ld.png",
			runtime_dir, output->wlr_output->name, (long)time(NULL));
		save_png_from_buffer(buffer, width, height, path);
		wlr_buffer_drop(buffer);
		snprintf(response, sizeof(response), "OK:%s", path);
		break;
	}
	case LABWC_IPC_SCREENSHOT_REGION: {
		int x, y, w, h;
		if (!payload || sscanf(payload, "%d,%d,%d,%d", &x, &y, &w, &h) != 4) {
			snprintf(response, sizeof(response), "ERROR: invalid region (x,y,w,h)");
			break;
		}
		struct wlr_buffer *buffer = wlr_allocator_create_buffer(
			server.allocator, w, h, WL_SHM_FORMAT_ARGB8888);
		if (!buffer) {
			snprintf(response, sizeof(response), "ERROR: allocation failed");
			break;
		}
		struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
			server.renderer, buffer, NULL);
		render_node(pass, &server.scene->tree.node, -x, -y);
		if (!wlr_render_pass_submit(pass)) {
			wlr_buffer_drop(buffer);
			snprintf(response, sizeof(response), "ERROR: render failed");
			break;
		}
		snprintf(path, sizeof(path), "%s/labwc-screenshot-region-%ld.png",
			runtime_dir, (long)time(NULL));
		save_png_from_buffer(buffer, w, h, path);
		wlr_buffer_drop(buffer);
		snprintf(response, sizeof(response), "OK:%s", path);
		break;
	}
	case LABWC_IPC_SCREENSHOT_FOCUSED: {
		struct wlr_buffer *buffer;
		int width, height;
		if (!capture_focused(&buffer, &width, &height)) {
			snprintf(response, sizeof(response), "ERROR: no focused window");
			break;
		}
		snprintf(path, sizeof(path), "%s/labwc-screenshot-focused-%ld.png",
			runtime_dir, (long)time(NULL));
		save_png_from_buffer(buffer, width, height, path);
		wlr_buffer_drop(buffer);
		snprintf(response, sizeof(response), "OK:%s", path);
		break;
	}
	case LABWC_IPC_SCREENSHOT_LIST_OUTPUTS: {
		char *p = response;
		size_t left = sizeof(response);
		struct output *o;
		wl_list_for_each(o, &server.outputs, link) {
			int len = snprintf(p, left, "%s\n", o->wlr_output->name);
			if (len > 0 && (size_t)len < left) {
				p += len;
				left -= len;
			}
		}
		response_len = p - response;
		break;
	}

	/* Window management commands */
	case LABWC_IPC_WM_LIST_WINDOWS: {
		char *p = response;
		size_t left = sizeof(response);
		struct view *view;
		wl_list_for_each(view, &server.views, link) {
			int len = snprintf(p, left, "%s: %s\n",
				view->app_id ? view->app_id : "unknown",
				view->title ? view->title : "untitled");
			if (len > 0 && (size_t)len < left) {
				p += len;
				left -= len;
			}
		}
		response_len = p - response;
		break;
	}
	case LABWC_IPC_WM_GET_ACTIVE: {
		struct view *view = server.active_view;
		if (view) {
			snprintf(response, sizeof(response), "OK:%s",
				view->title ? view->title : "untitled");
		} else {
			snprintf(response, sizeof(response), "OK:");
		}
		break;
	}
	case LABWC_IPC_WM_CLOSE: {
		struct view *view = server.active_view;
		if (view) {
			view_close(view);
			snprintf(response, sizeof(response), "OK");
		} else {
			snprintf(response, sizeof(response), "ERROR: no active window");
		}
		break;
	}
	case LABWC_IPC_WM_MAXIMIZE: {
		struct view *view = server.active_view;
		if (view) {
			view_maximize(view, VIEW_AXIS_BOTH);
			snprintf(response, sizeof(response), "OK");
		} else {
			snprintf(response, sizeof(response), "ERROR: no active window");
		}
		break;
	}
	case LABWC_IPC_WM_MINIMIZE: {
		struct view *view = server.active_view;
		if (view) {
			view_minimize(view, NULL);
			snprintf(response, sizeof(response), "OK");
		} else {
			snprintf(response, sizeof(response), "ERROR: no active window");
		}
		break;
	}
	case LABWC_IPC_WM_RESTORE: {
		struct view *view = server.active_view;
		if (view) {
			if (view->minimized) {
				view_minimize(view, NULL);
			}
			view_set_maximized(view, false);
			snprintf(response, sizeof(response), "OK");
		} else {
			snprintf(response, sizeof(response), "ERROR: no active window");
		}
		break;
	}
	case LABWC_IPC_WM_FULLSCREEN: {
		struct view *view = server.active_view;
		if (view) {
			view_toggle_fullscreen(view);
			snprintf(response, sizeof(response), "OK");
		} else {
			snprintf(response, sizeof(response), "ERROR: no active window");
		}
		break;
	}

	/* Zoom commands */
	case LABWC_IPC_ZOOM_GET_SCALE: {
		output = output_nearest_to_cursor();
		if (output) {
			snprintf(response, sizeof(response), "%.2f", zoom_get_scale(output));
		} else {
			snprintf(response, sizeof(response), "ERROR: no output");
		}
		break;
	}
	case LABWC_IPC_ZOOM_SET_SCALE: {
		if (payload) {
			double scale = atof(payload);
			if (scale >= 1.0 && scale <= 10.0) {
				output = output_nearest_to_cursor();
				if (output) {
					zoom_set_scale(output, scale);
					snprintf(response, sizeof(response), "OK");
				} else {
					snprintf(response, sizeof(response), "ERROR: no output");
				}
			} else {
				snprintf(response, sizeof(response), "ERROR: scale must be 1.0-10.0");
			}
		} else {
			snprintf(response, sizeof(response), "ERROR: no scale specified");
		}
		break;
	}
	case LABWC_IPC_ZOOM_INCREASE: {
		output = output_nearest_to_cursor();
		if (output) {
			zoom_adjust_scale(output, rc.mag_increment);
			snprintf(response, sizeof(response), "%.2f", zoom_get_scale(output));
		} else {
			snprintf(response, sizeof(response), "ERROR: no output");
		}
		break;
	}
	case LABWC_IPC_ZOOM_DECREASE: {
		output = output_nearest_to_cursor();
		if (output) {
			zoom_adjust_scale(output, -rc.mag_increment);
			snprintf(response, sizeof(response), "%.2f", zoom_get_scale(output));
		} else {
			snprintf(response, sizeof(response), "ERROR: no output");
		}
		break;
	}
	case LABWC_IPC_ZOOM_RESET: {
		output = output_nearest_to_cursor();
		if (output) {
			zoom_disable(output);
			snprintf(response, sizeof(response), "1.00");
		} else {
			snprintf(response, sizeof(response), "ERROR: no output");
		}
		break;
	}
	case LABWC_IPC_ZOOM_GET_ENABLED: {
		output = output_nearest_to_cursor();
		if (output) {
			snprintf(response, sizeof(response), "%d", output->zoom_enabled ? 1 : 0);
		} else {
			snprintf(response, sizeof(response), "ERROR: no output");
		}
		break;
	}
	case LABWC_IPC_ZOOM_GET_OUTPUT: {
		output = output_nearest_to_cursor();
		if (output) {
			snprintf(response, sizeof(response), "%s", output->wlr_output->name);
		} else {
			snprintf(response, sizeof(response), "ERROR: no output");
		}
		break;
	}

	/* Overview commands */
	case LABWC_IPC_OVERVIEW_START: {
		output = output_nearest_to_cursor();
		if (output) {
			if (!overview_is_active()) {
				overview_init(output);
			}
			snprintf(response, sizeof(response), "OK");
		} else {
			snprintf(response, sizeof(response), "ERROR: no output");
		}
		break;
	}
	case LABWC_IPC_OVERVIEW_STOP: {
		if (overview_is_active()) {
			overview_finish();
		}
		snprintf(response, sizeof(response), "OK");
		break;
	}
	case LABWC_IPC_OVERVIEW_TOGGLE: {
		output = output_nearest_to_cursor();
		if (output) {
			if (overview_is_active()) {
				overview_finish();
			} else {
				overview_init(output);
			}
			snprintf(response, sizeof(response), "OK");
		} else {
			snprintf(response, sizeof(response), "ERROR: no output");
		}
		break;
	}
	case LABWC_IPC_OVERVIEW_STATUS: {
		snprintf(response, sizeof(response), "%d", overview_is_active() ? 1 : 0);
		break;
	}

	/* Screen edge commands */
	case LABWC_IPC_EDGE_GET_TRIGGERS: {
		bool left, right, top, bottom;
		screen_edges_get_triggers(&left, &right, &top, &bottom);
		snprintf(response, sizeof(response), "%d,%d,%d,%d",
			left ? 1 : 0, right ? 1 : 0, top ? 1 : 0, bottom ? 1 : 0);
		break;
	}
	case LABWC_IPC_EDGE_SET_TRIGGERS: {
		if (payload) {
			int left = 0, right = 0, top = 0, bottom = 0;
			if (sscanf(payload, "%d,%d,%d,%d", &left, &right, &top, &bottom) == 4) {
				screen_edges_set_triggers(left != 0, right != 0, top != 0, bottom != 0);
				snprintf(response, sizeof(response), "OK");
			} else {
				snprintf(response, sizeof(response), "ERROR: invalid format (l,r,t,b)");
			}
		} else {
			snprintf(response, sizeof(response), "ERROR: no payload");
		}
		break;
	}

	default:
		snprintf(response, sizeof(response), "ERROR: unknown command %u", msg.command);
		break;
	}

	response_len = response_len > 0 ? response_len : strlen(response);

send_response:
	if (payload) {
		free(payload);
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
labwc_ipc_init(void)
{
	const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
	if (!runtime_dir) {
		wlr_log(WLR_ERROR, "XDG_RUNTIME_DIR not set, cannot create IPC socket");
		return;
	}

	snprintf(socket_path, sizeof(socket_path), "%s/labwc.sock", runtime_dir);

	ipc_fd = socket(AF_UNIX, SOCK_STREAM, 0);
	if (ipc_fd < 0) {
		wlr_log(WLR_ERROR, "Failed to create IPC socket");
		return;
	}

	unlink(socket_path);

	struct sockaddr_un addr;
	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

	if (bind(ipc_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		wlr_log(WLR_ERROR, "Failed to bind IPC socket to %s", socket_path);
		close(ipc_fd);
		ipc_fd = -1;
		return;
	}

	if (listen(ipc_fd, 5) < 0) {
		wlr_log(WLR_ERROR, "Failed to listen on IPC socket");
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
		wlr_log(WLR_ERROR, "Failed to add IPC event source");
		close(ipc_fd);
		ipc_fd = -1;
		unlink(socket_path);
		return;
	}

	wlr_log(WLR_INFO, "LabWC IPC listening on %s", socket_path);
}

void
labwc_ipc_finish(void)
{
	ipc_close();
}