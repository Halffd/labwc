/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_OVERVIEW_H
#define LABWC_OVERVIEW_H

struct output;

void overview_init(struct output *output);
void overview_finish(void);
bool overview_is_active(void);
void overview_on_cursor_release(int x, int y);
void overview_scroll_update(int delta);
void workspace_overview_init(struct output *output);

#endif /* LABWC_OVERVIEW_H */