/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef LABWC_SCREEN_EDGES_H
#define LABWC_SCREEN_EDGES_H

#include <stdbool.h>

void screen_edges_init(void);
void screen_edges_finish(void);
void screen_edges_check(int x, int y);
bool screen_edges_get_triggers(bool *left, bool *right, bool *top, bool *bottom);
void screen_edges_set_triggers(bool left, bool right, bool top, bool bottom);

#endif /* LABWC_SCREEN_EDGES_H */