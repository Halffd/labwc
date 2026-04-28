/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_ANNOTATION_H
#define LABWC_ANNOTATION_H

#include <stdbool.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/util/box.h>

struct output;

void annotation_init(void);
void annotation_finish(void);
void annotation_toggle(void);
bool annotation_is_active(void);
void annotation_clear(void);
void annotation_add_point(int x, int y);
void annotation_draw(struct output *output, struct wlr_buffer *buffer,
		struct wlr_box *damage);

#endif /* LABWC_ANNOTATION_H */