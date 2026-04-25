// SPDX-License-Identifier: GPL-2.0-only

#include "zoom.h"
#include <assert.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/transform.h>
#include "common/box.h"
#include "config/rcxml.h"
#include "labwc.h"
#include "output.h"
#include "theme.h"

static struct wlr_buffer *tmp_buffer = NULL;
static struct wlr_texture *tmp_texture = NULL;

void
zoom_enable(struct output *output)
{
	output->zoom_enabled = true;
	if (output->zoom_scale <= 1.0) {
		output->zoom_scale = 1.0 + rc.mag_increment;
	}
	wlr_output_schedule_frame(output->wlr_output);
}

void
zoom_disable(struct output *output)
{
	output->zoom_enabled = false;
	wlr_output_schedule_frame(output->wlr_output);
}

void
zoom_toggle(void)
{
	struct output *output = output_nearest_to_cursor();
	if (!output) {
		return;
	}

	output->zoom_enabled = !output->zoom_enabled;
	if (output->zoom_enabled && output->zoom_scale <= 1.0) {
		output->zoom_scale = 1.0 + rc.mag_increment;
	}
	wlr_output_schedule_frame(output->wlr_output);
}

void
zoom_set_scale(struct output *output, double scale)
{
	output->zoom_scale = scale;
	wlr_output_schedule_frame(output->wlr_output);
}

double
zoom_get_scale(struct output *output)
{
	return output->zoom_scale;
}

void
zoom_adjust_scale(struct output *output, double delta)
{
	double new_scale = output->zoom_scale + delta;
	if (new_scale < 1.0) {
		new_scale = 1.0;
	}
	if (new_scale > 10.0) {
		new_scale = 10.0;
	}
	output->zoom_scale = new_scale;
	wlr_output_schedule_frame(output->wlr_output);
}

bool
output_wants_zoom(struct output *output)
{
	if (!output->zoom_enabled) {
		return false;
	}
	return output->zoom_scale > 1.0;
}

void
zoom_draw(struct output *output, struct wlr_buffer *output_buffer,
		struct wlr_box *damage)
{
	if (!output->zoom_enabled || output->zoom_scale <= 1.0) {
		return;
	}

	struct wlr_box output_box = {
		.width = output_buffer->width,
		.height = output_buffer->height,
	};

	double scale = output->zoom_scale;
	int scaled_width = (int)(output_box.width * scale);
	int scaled_height = (int)(output_box.height * scale);

	if (tmp_buffer && (tmp_buffer->width != scaled_width ||
			tmp_buffer->height != scaled_height)) {
		if (tmp_texture) {
			wlr_texture_destroy(tmp_texture);
			tmp_texture = NULL;
		}
		wlr_buffer_drop(tmp_buffer);
		tmp_buffer = NULL;
	}

	if (!tmp_buffer) {
		tmp_buffer = wlr_allocator_create_buffer(
			server.allocator, scaled_width, scaled_height,
			&output->wlr_output->swapchain->format);
	}
	if (!tmp_buffer) {
		wlr_log(WLR_ERROR, "Failed to allocate temporary zoom buffer");
		return;
	}

	if (!tmp_texture) {
		tmp_texture = wlr_texture_from_buffer(server.renderer, tmp_buffer);
	}
	if (!tmp_texture) {
		wlr_log(WLR_ERROR, "Failed to allocate temporary zoom texture");
		wlr_buffer_drop(tmp_buffer);
		tmp_buffer = NULL;
		return;
	}

	struct wlr_render_pass *render_pass = wlr_renderer_begin_buffer_pass(
		server.renderer, tmp_buffer, NULL);
	if (!render_pass) {
		wlr_log(WLR_ERROR, "Failed to begin zoom render pass");
		return;
	}

	wlr_buffer_lock(output_buffer);
	struct wlr_texture *output_texture = wlr_texture_from_buffer(
		server.renderer, output_buffer);
	if (!output_texture) {
		goto cleanup;
	}

	struct wlr_fbox src_box = {
		.x = 0,
		.y = 0,
		.width = output_box.width,
		.height = output_box.height,
	};

	struct wlr_render_texture_options opts = {
		.texture = output_texture,
		.src_box = src_box,
		.dst_box = {0, 0, scaled_width, scaled_height},
		.filter_mode = WLR_SCALE_FILTER_BILINEAR,
	};
	wlr_render_pass_add_texture(render_pass, &opts);
	if (!wlr_render_pass_submit(render_pass)) {
		wlr_log(WLR_ERROR, "Failed to render zoom source");
		wlr_texture_destroy(output_texture);
		goto cleanup;
	}
	wlr_texture_destroy(output_texture);

	render_pass = wlr_renderer_begin_buffer_pass(
		server.renderer, output_buffer, NULL);
	if (!render_pass) {
		wlr_log(WLR_ERROR, "Failed to begin second zoom render pass");
		goto cleanup;
	}

	src_box = (struct wlr_fbox){
		.x = 0,
		.y = 0,
		.width = scaled_width,
		.height = scaled_height,
	};

	opts = (struct wlr_render_texture_options){
		.texture = tmp_texture,
		.src_box = src_box,
		.dst_box = {0, 0, output_box.width, output_box.height},
		.filter_mode = WLR_SCALE_FILTER_BILINEAR,
	};
	wlr_render_pass_add_texture(render_pass, &opts);
	if (!wlr_render_pass_submit(render_pass)) {
		wlr_log(WLR_ERROR, "Failed to submit zoom render pass");
		goto cleanup;
	}

	*damage = output_box;

cleanup:
	wlr_buffer_unlock(output_buffer);
}

bool
zoom_is_enabled(void)
{
	struct output *output = output_nearest_to_cursor();
	if (!output) {
		return false;
	}
	return output->zoom_enabled;
}

void
zoom_reset(void)
{
	if (tmp_texture && tmp_buffer) {
		wlr_texture_destroy(tmp_texture);
		wlr_buffer_drop(tmp_buffer);
		tmp_buffer = NULL;
		tmp_texture = NULL;
	}

	struct output *output;
	wl_list_for_each(output, &server.outputs, link) {
		output->zoom_enabled = false;
		output->zoom_scale = 1.0;
	}
}