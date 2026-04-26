// SPDX-License-Identifier: GPL-2.0-only

#include "screen-edges.h"
#include "common/macros.h"
#include "labwc.h"
#include "output.h"
#include "overview.h"

#define EDGE_THRESHOLD 5

static struct {
	bool left;
	bool right;
	bool top;
	bool bottom;
} edge_triggers = {0};

static int last_x = -1;
static int last_y = -1;

static uint64_t last_edge_check_time = 0;
#define EDGE_CHECK_INTERVAL_MS 150

static uint64_t
get_time_ms(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

static void
trigger_overview(void)
{
	struct output *output = output_nearest_to_cursor();
	if (output && !overview_is_active()) {
		overview_init(output);
	}
}

static void
check_edges(int x, int y)
{
	uint64_t now = get_time_ms();
	if (now - last_edge_check_time < EDGE_CHECK_INTERVAL_MS) {
		return;
	}
	last_edge_check_time = now;

	struct output *output = output_nearest_to_cursor();
	if (!output) {
		return;
	}

	int width, height;
	wlr_output_effective_resolution(output->wlr_output, &width, &height);
	if (width == 0 || height == 0) {
		return;
	}

	bool at_left_edge = (x < EDGE_THRESHOLD);
	bool at_right_edge = (x > width - EDGE_THRESHOLD);
	bool at_top_edge = (y < EDGE_THRESHOLD);
	bool at_bottom_edge = (y > height - EDGE_THRESHOLD);

	if (last_x >= 0 && last_y >= 0) {
		bool was_at_left = (last_x < EDGE_THRESHOLD);
		bool was_at_right = (last_x > width - EDGE_THRESHOLD);
		bool was_at_top = (last_y < EDGE_THRESHOLD);
		bool was_at_bottom = (last_y > height - EDGE_THRESHOLD);

		if (was_at_left && at_left_edge && edge_triggers.left) {
			trigger_overview();
			return;
		}
		if (was_at_right && at_right_edge && edge_triggers.right) {
			trigger_overview();
			return;
		}
		if (was_at_top && at_top_edge && edge_triggers.top) {
			trigger_overview();
			return;
		}
		if (was_at_bottom && at_bottom_edge && edge_triggers.bottom) {
			trigger_overview();
			return;
		}
	}

	last_x = x;
	last_y = y;
}

void
screen_edges_check(int x, int y)
{
	check_edges(x, y);
}

void
screen_edges_init(void)
{
	edge_triggers.left = true;
	edge_triggers.right = true;
	edge_triggers.top = false;
	edge_triggers.bottom = false;

	last_x = -1;
	last_y = -1;
	last_edge_check_time = 0;

	wlr_log(WLR_INFO, "screen-edges: initialized (left=%d, right=%d, top=%d, bottom=%d)",
		edge_triggers.left, edge_triggers.right,
		edge_triggers.top, edge_triggers.bottom);
}

void
screen_edges_finish(void)
{
	wlr_log(WLR_INFO, "screen-edges: finished");
}