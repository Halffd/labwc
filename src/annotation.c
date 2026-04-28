// SPDX-License-Identifier: GPL-2.0-only
#include "annotation.h"
#include <cairo/cairo.h>
#include <wlr/render/allocator.h>
#include <wlr/util/log.h>
#include "labwc.h"
#include "output.h"

#define MAX_POINTS 10000

static struct {
	bool active;
	int count;
	struct {
		int x;
		int y;
	} points[MAX_POINTS];
	double line_width;
	double color[4];
} annotation = {
	.active = false,
	.count = 0,
	.line_width = 3.0,
	.color = { 1.0, 0.2, 0.2, 1.0 }, /* red */
};

void
annotation_init(void)
{
	wlr_log(WLR_DEBUG, "annotation init");
}

void
annotation_finish(void)
{
	annotation.active = false;
	annotation.count = 0;
	wlr_log(WLR_DEBUG, "annotation finish");
}

void
annotation_toggle(void)
{
	annotation.active = !annotation.active;
	wlr_log(WLR_DEBUG, "annotation %s",
		annotation.active ? "enabled" : "disabled");
}

bool
annotation_is_active(void)
{
	return annotation.active;
}

void
annotation_clear(void)
{
	annotation.count = 0;
	wlr_log(WLR_DEBUG, "annotation cleared");
}

void
annotation_add_point(int x, int y)
{
	if (!annotation.active) {
		return;
	}
	if (annotation.count >= MAX_POINTS) {
		wlr_log(WLR_DEBUG, "annotation point limit reached");
		return;
	}
	annotation.points[annotation.count].x = x;
	annotation.points[annotation.count].y = y;
	annotation.count++;
}

void
annotation_draw(struct output *output, struct wlr_buffer *buffer,
		struct wlr_box *damage)
{
	if (!annotation.active || annotation.count < 2) {
		return;
	}

	int width = buffer->width;
	int height = buffer->height;

	void *data;
	enum wl_shm_format format;
	size_t stride;
	if (!wlr_buffer_begin_data_ptr_access(buffer,
			WLR_BUFFER_DATA_PTR_ACCESS_READ | WLR_BUFFER_DATA_PTR_ACCESS_WRITE,
			&data, &format, &stride)) {
		wlr_log(WLR_ERROR, "Failed to access buffer for annotation");
		return;
	}

	cairo_surface_t *surface = cairo_image_surface_create_for_data(
		data, CAIRO_FORMAT_ARGB32, width, height, stride);

	cairo_t *cairo = cairo_create(surface);
	if (cairo_status(cairo)) {
		wlr_log(WLR_ERROR, "Failed to create cairo for annotation");
		cairo_surface_destroy(surface);
		wlr_buffer_end_data_ptr_access(buffer);
		return;
	}

	cairo_set_source_rgba(cairo, annotation.color[0],
		annotation.color[1], annotation.color[2], annotation.color[3]);
	cairo_set_line_width(cairo, annotation.line_width);
	cairo_set_line_cap(cairo, CAIRO_LINE_CAP_ROUND);
	cairo_set_line_join(cairo, CAIRO_LINE_JOIN_ROUND);

	int prev_x = annotation.points[0].x;
	int prev_y = annotation.points[0].y;

	for (int i = 1; i < annotation.count; i++) {
		int cur_x = annotation.points[i].x;
		int cur_y = annotation.points[i].y;

		/* Skip if points are the same */
		if (cur_x == prev_x && cur_y == prev_y) {
			continue;
		}

		/* Reduce jitter - skip small movements */
		int dx = cur_x - prev_x;
		int dy = cur_y - prev_y;
		if (dx * dx + dy * dy < 4) {
			continue;
		}

		cairo_move_to(cairo, prev_x, prev_y);
		cairo_line_to(cairo, cur_x, cur_y);
		cairo_stroke(cairo);

		prev_x = cur_x;
		prev_y = cur_y;
	}

	cairo_destroy(cairo);
	cairo_surface_destroy(surface);
	wlr_buffer_end_data_ptr_access(buffer);

	damage->x = 0;
	damage->y = 0;
	damage->width = width;
	damage->height = height;
}