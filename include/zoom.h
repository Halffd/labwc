/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_ZOOM_H
#define LABWC_ZOOM_H

#include <stdbool.h>

struct output;
struct wlr_buffer;
struct wlr_box;

void zoom_enable(struct output *output);
void zoom_disable(struct output *output);
void zoom_toggle(void);
void zoom_set_scale(struct output *output, double scale);
double zoom_get_scale(struct output *output);
void zoom_adjust_scale(struct output *output, double delta);
bool output_wants_zoom(struct output *output);
void zoom_draw(struct output *output, struct wlr_buffer *output_buffer,
	struct wlr_box *damage);
bool zoom_is_enabled(void);
void zoom_reset(void);

#endif /* LABWC_ZOOM_H */