/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_COLOR_INVERT_H
#define LABWC_COLOR_INVERT_H

#include <stdbool.h>
#include <wayland-server-core.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/util/box.h>

struct output;
struct view;

void invert_toggle_window(struct view *view);
void invert_toggle_output(struct output *output);
void invert_toggle_monitor(void);
void invert_toggle_window_monitor(void);

bool window_is_inverted(struct view *view);
bool output_is_inverted(struct output *output);
bool monitor_is_inverted(void);
bool window_monitor_is_inverted(void);

void output_invert(struct output *output, struct wlr_buffer *buffer,
		struct wlr_box *damage);
void view_invert(struct view *view, struct wlr_buffer *buffer,
		struct wlr_box *damage);

#endif /* LABWC_COLOR_INVERT_H */