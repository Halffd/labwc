// SPDX-License-Identifier: GPL-2.0-only
#include "color-invert.h"
#include <cairo/cairo.h>
#include <wlr/render/allocator.h>
#include <wlr/util/log.h>
#include "labwc.h"
#include "output.h"
#include "view.h"

static struct wlr_buffer *tmp_buffer;
static struct wlr_texture *tmp_texture;
static int tmp_width;
static int tmp_height;

void
invert_toggle_window(struct view *view)
{
	if (!view) {
		return;
	}
	view->inverted = !view->inverted;
	wlr_log(WLR_DEBUG, "window inverted: %s",
		view->inverted ? "on" : "off");
}

void
invert_toggle_output(struct output *output)
{
	if (!output) {
		return;
	}
	output->inverted = !output->inverted;
	wlr_log(WLR_DEBUG, "output %s inverted: %s", output->wlr_output->name,
		output->inverted ? "on" : "off");
}

void
invert_toggle_monitor(void)
{
	struct output *output = output_nearest_to_cursor();
	invert_toggle_output(output);
}

void
invert_toggle_window_monitor(void)
{
	struct view *view = server.active_view;
	if (view) {
		invert_toggle_window(view);
	} else {
		invert_toggle_monitor();
	}
}

bool
window_is_inverted(struct view *view)
{
	return view ? view->inverted : false;
}

bool
output_is_inverted(struct output *output)
{
	return output ? output->inverted : false;
}

bool
monitor_is_inverted(void)
{
	struct output *output = output_nearest_to_cursor();
	return output_is_inverted(output);
}

bool
window_monitor_is_inverted(void)
{
	struct view *view = server.active_view;
	return view ? view->inverted : monitor_is_inverted();
}

static void
invert_cairo_surface(cairo_surface_t *surface)
{
	unsigned char *data = cairo_image_surface_get_data(surface);
	int width = cairo_image_surface_get_width(surface);
	int height = cairo_image_surface_get_height(surface);
	int stride = cairo_image_surface_get_stride(surface);

	for (int y = 0; y < height; y++) {
		uint32_t *row = (uint32_t *)(data + y * stride);
		for (int x = 0; x < width; x++) {
			uint32_t p = row[x];
			uint8_t a = (p >> 24) & 0xff;
			uint8_t r = (p >> 16) & 0xff;
			uint8_t g = (p >> 8) & 0xff;
			uint8_t b = p & 0xff;
			row[x] = (a << 24) | ((255 - r) << 16) | ((255 - g) << 8) | (255 - b);
		}
	}
	cairo_surface_mark_dirty(surface);
}

void
output_invert(struct output *output, struct wlr_buffer *buffer,
		struct wlr_box *damage)
{
	if (!output->inverted) {
		return;
	}

	struct wlr_box output_box = {
		.width = buffer->width,
		.height = buffer->height,
	};

	if (!output->inverted) {
		return;
	}

	if (tmp_buffer && (tmp_width != output_box.width ||
			tmp_height != output_box.height)) {
		if (tmp_texture) {
			wlr_texture_destroy(tmp_texture);
			tmp_texture = NULL;
		}
		wlr_buffer_drop(tmp_buffer);
		tmp_buffer = NULL;
	}

	if (!tmp_buffer) {
		tmp_width = output_box.width;
		tmp_height = output_box.height;
		tmp_buffer = wlr_allocator_create_buffer(server.allocator,
			tmp_width, tmp_height, WL_SHM_FORMAT_ARGB8888);
		if (!tmp_buffer) {
			wlr_log(WLR_ERROR, "Failed to create temp buffer for invert");
			return;
		}
	}

	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		server.renderer, tmp_buffer, NULL);
	if (!pass) {
		wlr_log(WLR_ERROR, "Failed to begin invert render pass");
		return;
	}

	wlr_buffer_lock(buffer);
	struct wlr_texture *tex = wlr_texture_from_buffer(server.renderer, buffer);
	if (!tex) {
		wlr_buffer_unlock(buffer);
		wlr_render_pass_submit(pass);
		wlr_log(WLR_ERROR, "Failed to create texture from buffer");
		return;
	}

	struct wlr_fbox src_box = {
		.x = 0, .y = 0,
		.width = output_box.width,
		.height = output_box.height,
	};
	struct wlr_render_texture_options opts = {
		.texture = tex,
		.src_box = src_box,
		.dst_box = {0, 0, tmp_width, tmp_height},
		.filter_mode = WLR_SCALE_FILTER_NEAREST,
	};
	wlr_render_pass_add_texture(pass, &opts);
	wlr_texture_destroy(tex);

	if (!wlr_render_pass_submit(pass)) {
		wlr_buffer_unlock(buffer);
		wlr_log(WLR_ERROR, "Failed to submit first invert render pass");
		return;
	}

	wlr_buffer_unlock(buffer);

	void *data;
	enum wl_shm_format format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(tmp_buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ | WLR_BUFFER_DATA_PTR_ACCESS_WRITE,
			&data, &format, &stride)) {
		wlr_log(WLR_ERROR, "Failed to access temp buffer data");
		return;
	}

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		data, CAIRO_FORMAT_ARGB32, tmp_width, tmp_height, stride);
	invert_cairo_surface(surface);
	cairo_surface_destroy(surface);

	wlr_buffer_end_data_ptr_access(tmp_buffer);

	if (!tmp_texture) {
		tmp_texture = wlr_texture_from_buffer(server.renderer, tmp_buffer);
	}
	if (!tmp_texture) {
		wlr_log(WLR_ERROR, "Failed to create texture from inverted buffer");
		return;
	}

	pass = wlr_renderer_begin_buffer_pass(server.renderer, buffer, NULL);
	if (!pass) {
		wlr_log(WLR_ERROR, "Failed to begin second invert render pass");
		return;
	}

	src_box = (struct wlr_fbox){
		.x = 0, .y = 0,
		.width = tmp_width,
		.height = tmp_height,
	};
	opts = (struct wlr_render_texture_options){
		.texture = tmp_texture,
		.src_box = src_box,
		.dst_box = {0, 0, output_box.width, output_box.height},
		.filter_mode = WLR_SCALE_FILTER_NEAREST,
	};
	wlr_render_pass_add_texture(pass, &opts);
	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "Failed to submit invert render pass");
		return;
	}

	*damage = output_box;
}

void
view_invert(struct view *view, struct wlr_buffer *buffer,
		struct wlr_box *damage)
{
	if (!view || !view->inverted) {
		return;
	}
	output_invert(view->output, buffer, damage);
}