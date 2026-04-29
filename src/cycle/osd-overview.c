// SPDX-License-Identifier: GPL-2.0-only
#include <assert.h>
#include <wlr/render/allocator.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_buffer.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>
#include "config/rcxml.h"
#include "common/box.h"
#include "common/buf.h"
#include "common/lab-scene-rect.h"
#include "common/list.h"
#include "common/mem.h"
#include "common/scene-helpers.h"
#include "cycle.h"
#include "labwc.h"
#include "node.h"
#include "output.h"
#include "scaled-buffer/scaled-font-buffer.h"
#include "scaled-buffer/scaled-icon-buffer.h"
#include "theme.h"
#include "view.h"
#include "workspaces.h"

#define OVERVIEW_ITEM_WIDTH 500
#define OVERVIEW_ITEM_HEIGHT 320

struct overview_item {
	union {
		struct view *view;
		struct workspace *workspace;
	};
	enum {
		OVERVIEW_ITEM_TYPE_VIEW = 0,
		OVERVIEW_ITEM_TYPE_WORKSPACE,
	} type;
	struct wlr_scene_tree *tree;
	struct lab_scene_rect *active_bg;
	struct scaled_font_buffer *normal_label;
	struct scaled_font_buffer *active_label;
	struct wlr_scene_buffer *thumb_buffer;
	struct wlr_box thumb_bounds;
	struct wl_list link;
};

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
		wlr_log(WLR_ERROR, "ignoring rect");
		break;
	}
}

static struct wlr_buffer *
render_workspace_thumb(struct output *output, struct workspace *workspace)
{
	struct wlr_box box = {0, 0, OVERVIEW_ITEM_WIDTH - 40, OVERVIEW_ITEM_HEIGHT - 80};

	struct view *view;
	int count = 0;
	struct view *views[16];
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_CURRENT_WORKSPACE) {
		if (view->workspace == workspace && count < 16) {
			views[count++] = view;
		}
	}
	if (count == 0) {
		return NULL;
	}

	struct wlr_buffer *buffer = wlr_allocator_create_buffer(server.allocator,
		box.width, box.height, &output->wlr_output->swapchain->format);
	if (!buffer) {
		wlr_log(WLR_ERROR, "failed to allocate buffer for workspace thumb");
		return NULL;
	}

	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		server.renderer, buffer, NULL);
	if (!pass) {
		wlr_buffer_drop(buffer);
		return NULL;
	}

	int tile_w = box.width / 2;
	int tile_h = box.height / 2;
	int idx = 0;
	for (int i = 0; i < 2 && idx < count; i++) {
		for (int j = 0; j < 2 && idx < count; j++) {
			struct view *v = views[idx++];
			if (!v->content_tree) {
				continue;
			}
			int x = i * tile_w;
			int y = j * tile_h;
			int w = tile_w - 4;
			int h = tile_h - 4;
			render_node(pass, &v->content_tree->node, x + 2, y + 2);
		}
	}

	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "failed to submit workspace render pass");
		wlr_buffer_drop(buffer);
		return NULL;
	}
	return buffer;
}

static struct overview_item *
create_workspace_item(struct wlr_scene_tree *parent, struct workspace *workspace,
	struct output *output, struct window_switcher_thumbnail_theme *switcher_theme)
{
	struct theme *theme = rc.theme;
	int padding = theme->border_width + switcher_theme->item_padding;
	int title_height = switcher_theme->title_height;
	int title_y = OVERVIEW_ITEM_HEIGHT - padding - title_height;
	struct wlr_box thumb_bounds = {
		.x = padding,
		.y = padding,
		.width = OVERVIEW_ITEM_WIDTH - 2 * padding,
		.height = title_y - 2 * padding,
	};

	struct overview_item *item = znew(*item);
	wl_list_init(&item->link);
	item->type = OVERVIEW_ITEM_TYPE_WORKSPACE;
	item->workspace = workspace;

	struct wlr_scene_tree *tree = lab_wlr_scene_tree_create(parent);
	node_descriptor_create(&tree->node, LAB_NODE_CYCLE_OSD_ITEM, NULL, item);
	item->tree = tree;

	struct lab_scene_rect_options opts = {
		.border_colors = (float *[1]) { switcher_theme->item_active_border_color },
		.nr_borders = 1,
		.border_width = switcher_theme->item_active_border_width,
		.bg_color = switcher_theme->item_active_bg_color,
		.width = OVERVIEW_ITEM_WIDTH,
		.height = OVERVIEW_ITEM_HEIGHT,
	};
	item->active_bg = lab_scene_rect_create(tree, &opts);

	lab_wlr_scene_rect_create(tree, OVERVIEW_ITEM_WIDTH,
		OVERVIEW_ITEM_HEIGHT, (float[4]){0});

	struct wlr_buffer *thumb_buffer = render_workspace_thumb(output, workspace);
	if (thumb_buffer) {
		item->thumb_buffer = lab_wlr_scene_buffer_create(tree, thumb_buffer);
		wlr_buffer_drop(thumb_buffer);
		struct wlr_box thumb_box = box_fit_within(
			thumb_buffer->width, thumb_buffer->height, &thumb_bounds);
		wlr_scene_buffer_set_dest_size(item->thumb_buffer,
			thumb_box.width, thumb_box.height);
		wlr_scene_node_set_position(&item->thumb_buffer->node,
			thumb_box.x, thumb_box.y);
	}

	struct buf buf = BUF_INIT;
	buf_add(&buf, workspace->name);

	item->normal_label = scaled_font_buffer_create(tree);
	scaled_font_buffer_update(item->normal_label, buf.data,
		OVERVIEW_ITEM_WIDTH - 2 * padding,
		&rc.font_osd, theme->osd_label_text_color, theme->osd_bg_color);
	wlr_scene_node_set_position(&item->normal_label->scene_buffer->node,
		(OVERVIEW_ITEM_WIDTH - item->normal_label->width) / 2, title_y);

	item->active_label = scaled_font_buffer_create(tree);
	scaled_font_buffer_update(item->active_label, buf.data,
		OVERVIEW_ITEM_WIDTH - 2 * padding,
		&rc.font_osd, theme->osd_label_text_color,
		switcher_theme->item_active_bg_color);
	wlr_scene_node_set_position(&item->active_label->scene_buffer->node,
		(OVERVIEW_ITEM_WIDTH - item->active_label->width) / 2, title_y);

	buf_reset(&buf);
	return item;
}

static struct wlr_buffer *
render_thumb(struct output *output, struct view *view)
{
	if (!view->content_tree) {
		return NULL;
	}
	struct wlr_buffer *buffer = wlr_allocator_create_buffer(server.allocator,
		view->current.width, view->current.height,
		&output->wlr_output->swapchain->format);
	if (!buffer) {
		wlr_log(WLR_ERROR, "failed to allocate buffer for thumbnail");
		return NULL;
	}
	struct wlr_render_pass *pass = wlr_renderer_begin_buffer_pass(
		server.renderer, buffer, NULL);
	render_node(pass, &view->content_tree->node, 0, 0);
	if (!wlr_render_pass_submit(pass)) {
		wlr_log(WLR_ERROR, "failed to submit render pass");
		wlr_buffer_drop(buffer);
		return NULL;
	}
	return buffer;
}

static struct overview_item *
create_overview_item(struct wlr_scene_tree *parent, struct view *view,
	struct output *output, struct window_switcher_thumbnail_theme *switcher_theme)
{
	struct theme *theme = rc.theme;
	int padding = theme->border_width + switcher_theme->item_padding;
	int title_height = switcher_theme->title_height;
	int title_y = OVERVIEW_ITEM_HEIGHT - padding - title_height;
	struct wlr_box thumb_bounds = {
		.x = padding,
		.y = padding,
		.width = OVERVIEW_ITEM_WIDTH - 2 * padding,
		.height = title_y - 2 * padding,
	};
	if (thumb_bounds.width <= 0 || thumb_bounds.height <= 0) {
		wlr_log(WLR_ERROR, "too small thumbnail area");
		return NULL;
	}

	struct overview_item *item = znew(*item);
	wl_list_init(&item->link);
	item->type = OVERVIEW_ITEM_TYPE_VIEW;
	item->view = view;
	item->thumb_bounds = thumb_bounds;

	struct wlr_scene_tree *tree = lab_wlr_scene_tree_create(parent);
	node_descriptor_create(&tree->node, LAB_NODE_CYCLE_OSD_ITEM, NULL, item);
	item->tree = tree;

	/* background for selected item */
	struct lab_scene_rect_options opts = {
		.border_colors = (float *[1]) { switcher_theme->item_active_border_color },
		.nr_borders = 1,
		.border_width = switcher_theme->item_active_border_width,
		.bg_color = switcher_theme->item_active_bg_color,
		.width = OVERVIEW_ITEM_WIDTH,
		.height = OVERVIEW_ITEM_HEIGHT,
	};
	item->active_bg = lab_scene_rect_create(tree, &opts);

	/* hitbox for mouse clicks */
	lab_wlr_scene_rect_create(tree, OVERVIEW_ITEM_WIDTH,
		OVERVIEW_ITEM_HEIGHT, (float[4]){0});

	/* thumbnail */
	struct wlr_buffer *thumb_buffer = render_thumb(output, view);
	if (thumb_buffer) {
		item->thumb_buffer = lab_wlr_scene_buffer_create(tree, thumb_buffer);
		wlr_buffer_drop(thumb_buffer);
		struct wlr_box thumb_box = box_fit_within(
			thumb_buffer->width, thumb_buffer->height, &thumb_bounds);
		wlr_scene_buffer_set_dest_size(item->thumb_buffer,
			thumb_box.width, thumb_box.height);
		wlr_scene_node_set_position(&item->thumb_buffer->node,
			thumb_box.x, thumb_box.y);
	}

	/* title labels */
	struct buf buf = BUF_INIT;
	cycle_osd_field_set_custom(&buf, view,
		rc.window_switcher.osd.thumbnail_label_format);

	item->normal_label = scaled_font_buffer_create(tree);
	scaled_font_buffer_update(item->normal_label, buf.data,
		OVERVIEW_ITEM_WIDTH - 2 * padding,
		&rc.font_osd, theme->osd_label_text_color, theme->osd_bg_color);
	wlr_scene_node_set_position(&item->normal_label->scene_buffer->node,
		(OVERVIEW_ITEM_WIDTH - item->normal_label->width) / 2, title_y);

	item->active_label = scaled_font_buffer_create(tree);
	scaled_font_buffer_update(item->active_label, buf.data,
		OVERVIEW_ITEM_WIDTH - 2 * padding,
		&rc.font_osd, theme->osd_label_text_color,
		switcher_theme->item_active_bg_color);
	wlr_scene_node_set_position(&item->active_label->scene_buffer->node,
		(OVERVIEW_ITEM_WIDTH - item->active_label->width) / 2, title_y);

	buf_reset(&buf);

	/* icon */
	int icon_size = switcher_theme->item_icon_size;
	struct scaled_icon_buffer *icon_buffer = scaled_icon_buffer_create(tree, icon_size, icon_size);
	scaled_icon_buffer_set_view(icon_buffer, view);
	int x = (OVERVIEW_ITEM_WIDTH - icon_size) / 2;
	int y = title_y - padding - icon_size + 10;
	wlr_scene_node_set_position(&icon_buffer->scene_buffer->node, x, y);

	return item;
}

static void
get_overview_geometry(struct output *output, int nr_items, int *nr_cols,
	int *nr_rows, int *nr_visible_rows)
{
	struct theme *theme = rc.theme;
	struct window_switcher_thumbnail_theme *switcher_theme =
		&theme->osd_window_switcher_thumbnail;
	int output_width, output_height;
	wlr_output_effective_resolution(output->wlr_output,
		&output_width, &output_height);
	int padding = theme->osd_border_width + switcher_theme->padding;

	int max_bg_width = output_width - 2 * padding;

	*nr_rows = 1;
	*nr_cols = nr_items;
	while (1) {
		assert(*nr_rows <= nr_items);
		int bg_width = *nr_cols * OVERVIEW_ITEM_WIDTH + 2 * padding;
		if (bg_width < max_bg_width) {
			break;
		}
		if (*nr_rows >= nr_items) {
			break;
		}
		(*nr_rows)++;
		*nr_cols = ceilf((float)nr_items / *nr_rows);
	}

	*nr_visible_rows = MIN(*nr_rows,
		(output_height - 2 * padding) / OVERVIEW_ITEM_HEIGHT);
}

void
overview_finish(void);

static void
overview_tree_destroy(struct wl_listener *listener, void *data)
{
	overview_finish();
}

struct overview_scroll_ctx {
	int top_row_idx;
	int nr_rows, nr_cols, nr_visible_rows;
	int delta_y;
	struct wlr_box bar_area;
	struct wlr_scene_tree *bar_tree;
	struct lab_scene_rect *bar;
};

struct overview_osd {
	struct output *output;
	struct wlr_scene_tree *tree;
	struct wlr_scene_tree *items_tree;
	struct wl_list items;
	struct wl_listener tree_destroy;
	int nr_cols;
	int nr_rows;
	struct overview_scroll_ctx scroll;
};

static struct overview_osd *overview_osd;

static void
overview_item_select(struct overview_item *item)
{
	if (!item) {
		return;
	}

	if (item->type == OVERVIEW_ITEM_TYPE_WORKSPACE) {
		struct workspace *workspace = item->workspace;
		overview_finish();
		if (workspace) {
			workspaces_switch_to(workspace, /*update_focus*/ true);
		}
		return;
	}

	if (!item->view) {
		return;
	}
	struct view *view = item->view;

	/* Hide OSD */
	overview_finish();

	/* Focus the selected view */
	desktop_focus_view(view, /*raise*/ true);
}

void
overview_init(struct output *output)
{
	if (server.input_mode == LAB_INPUT_STATE_CYCLE) {
		/* Already in cycle mode, just reinitialize */
		cycle_finish(/*switch_focus*/ false);
	}

	struct theme *theme = rc.theme;
	struct window_switcher_thumbnail_theme *switcher_theme =
		&theme->osd_window_switcher_thumbnail;
	int padding = theme->osd_border_width + switcher_theme->padding;

	/* Count views */
	struct view *view;
	int nr_views = 0;
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_NO_SKIP_WINDOW_SWITCHER) {
		nr_views++;
	}
	if (nr_views == 0) {
		wlr_log(WLR_DEBUG, "no views for overview");
		return;
	}

	/* Create overview state */
	overview_osd = znew(*overview_osd);
	overview_osd->output = output;
	wl_list_init(&overview_osd->items);

	/* Create scene tree */
	overview_osd->tree = lab_wlr_scene_tree_create(output->cycle_osd_tree);
	overview_osd->items_tree = lab_wlr_scene_tree_create(overview_osd->tree);

	overview_osd->tree_destroy.notify = overview_tree_destroy;
	wl_signal_add(&overview_osd->tree->node.events.destroy,
		&overview_osd->tree_destroy);

	/* Calculate grid layout */
	int nr_cols, nr_rows, nr_visible_rows;
	get_overview_geometry(output, nr_views, &nr_cols, &nr_rows, &nr_visible_rows);
	overview_osd->nr_cols = nr_cols;
	overview_osd->nr_rows = nr_rows;

	/* Create items */
	int index = 0;
	for_each_view(view, &server.views, LAB_VIEW_CRITERIA_NO_SKIP_WINDOW_SWITCHER) {
		struct overview_item *item = create_overview_item(
			overview_osd->items_tree, view, output, switcher_theme);
		if (!item) {
			continue;
		}
wl_list_append(&overview_osd->items, &item->link);

int x = (index % nr_cols) * OVERVIEW_ITEM_WIDTH + padding;
	int y = (index / nr_cols) * OVERVIEW_ITEM_HEIGHT + padding;
	wlr_scene_node_set_position(&item->tree->node, x, y);
	index++;
}

/* background */
int items_width = OVERVIEW_ITEM_WIDTH * nr_cols;
int items_height = OVERVIEW_ITEM_HEIGHT * nr_visible_rows;

	struct lab_scene_rect_options bg_opts = {
		.border_colors = (float *[1]) { theme->osd_border_color },
		.nr_borders = 1,
		.border_width = theme->osd_border_width,
		.bg_color = theme->osd_bg_color,
		.width = items_width + 2 * padding,
		.height = items_height + 2 * padding,
	};
	struct lab_scene_rect *bg = lab_scene_rect_create(overview_osd->tree, &bg_opts);
	wlr_scene_node_lower_to_bottom(&bg->tree->node);

	/* center on output */
	struct wlr_box output_box;
	wlr_output_layout_get_box(server.output_layout, output->wlr_output, &output_box);
	int lx = output_box.x + (output_box.width - bg_opts.width) / 2;
	int ly = output_box.y + (output_box.height - bg_opts.height) / 2;
	wlr_scene_node_set_position(&overview_osd->tree->node, lx, ly);

	/* Enter cycle input mode */
	seat_focus_override_begin(&server.seat, LAB_INPUT_STATE_CYCLE, LAB_CURSOR_DEFAULT);
	server.cycle.selected_view = NULL;

	cursor_update_focus();
}

void
workspace_overview_init(struct output *output)
{
	if (server.input_mode == LAB_INPUT_STATE_CYCLE) {
		cycle_finish(/*switch_focus*/ false);
	}

	struct theme *theme = rc.theme;
	struct window_switcher_thumbnail_theme *switcher_theme =
		&theme->osd_window_switcher_thumbnail;
	int padding = theme->border_width + switcher_theme->padding;

	int nr_workspaces = 0;
	struct workspace *workspace;
	wl_list_for_each(workspace, &server.workspaces.all, link) {
		nr_workspaces++;
	}
	if (nr_workspaces == 0) {
		wlr_log(WLR_DEBUG, "no workspaces for workspace overview");
		return;
	}

	overview_osd = znew(*overview_osd);
	overview_osd->output = output;
	wl_list_init(&overview_osd->items);

	overview_osd->tree = lab_wlr_scene_tree_create(output->cycle_osd_tree);
	overview_osd->items_tree = lab_wlr_scene_tree_create(overview_osd->tree);

	overview_osd->tree_destroy.notify = overview_tree_destroy;
	wl_signal_add(&overview_osd->tree->node.events.destroy,
		&overview_osd->tree_destroy);

	int nr_cols, nr_rows, nr_visible_rows;
	get_overview_geometry(output, nr_workspaces, &nr_cols, &nr_rows, &nr_visible_rows);
	overview_osd->nr_cols = nr_cols;
	overview_osd->nr_rows = nr_rows;

	int index = 0;
	struct workspace *ws;
	wl_list_for_each(ws, &server.workspaces.all, link) {
		struct overview_item *item = create_workspace_item(
			overview_osd->items_tree, ws, output, switcher_theme);
		if (!item) {
			continue;
		}
		wl_list_append(&overview_osd->items, &item->link);

		int x = (index % nr_cols) * OVERVIEW_ITEM_WIDTH + padding;
		int y = (index / nr_cols) * OVERVIEW_ITEM_HEIGHT + padding;
		wlr_scene_node_set_position(&item->tree->node, x, y);
		index++;
	}

	int items_width = OVERVIEW_ITEM_WIDTH * nr_cols;
	int items_height = OVERVIEW_ITEM_HEIGHT * nr_visible_rows;

	struct lab_scene_rect_options bg_opts = {
		.border_colors = (float *[1]) { theme->osd_border_color },
		.nr_borders = 1,
		.border_width = theme->osd_border_width,
		.bg_color = theme->osd_bg_color,
		.width = items_width + 2 * padding,
		.height = items_height + 2 * padding,
	};
	struct lab_scene_rect *bg = lab_scene_rect_create(overview_osd->tree, &bg_opts);
	wlr_scene_node_lower_to_bottom(&bg->tree->node);

	struct wlr_box output_box;
	wlr_output_layout_get_box(server.output_layout, output->wlr_output, &output_box);
	int lx = output_box.x + (output_box.width - bg_opts.width) / 2;
	int ly = output_box.y + (output_box.height - bg_opts.height) / 2;
	wlr_scene_node_set_position(&overview_osd->tree->node, lx, ly);

	seat_focus_override_begin(&server.seat, LAB_INPUT_STATE_CYCLE, LAB_CURSOR_DEFAULT);
	server.cycle.selected_view = NULL;

	cursor_update_focus();
}

void
overview_finish(void)
{
	if (!overview_osd) {
		return;
	}

	/* Destroy items */
	struct overview_item *item, *tmp;
	wl_list_for_each_safe(item, tmp, &overview_osd->items, link) {
		wl_list_remove(&item->link);
		free(item);
	}

	/* Destroy tree (triggers overview_tree_destroy) */
	wlr_scene_node_destroy(&overview_osd->tree->node);

	overview_osd = NULL;
	server.cycle.selected_view = NULL;

	seat_focus_override_end(&server.seat, /*restore_focus*/ false);
	cursor_update_focus();
}

static struct overview_item *
overview_item_at(int x, int y)
{
	if (!overview_osd) {
		return NULL;
	}

	struct overview_item *item;
	wl_list_for_each(item, &overview_osd->items, link) {
		struct wlr_box box = {
			.x = item->tree->node.x,
			.y = item->tree->node.y,
			.width = OVERVIEW_ITEM_WIDTH,
			.height = OVERVIEW_ITEM_HEIGHT,
		};
		if (wlr_box_contains_point(&box, x, y)) {
			return item;
		}
	}
	return NULL;
}

void
overview_on_cursor_release(int x, int y)
{
	struct overview_item *item = overview_item_at(x, y);
	if (item) {
		overview_item_select(item);
	} else {
		overview_finish();
	}
}

void
overview_scroll_update(int delta)
{
	if (!overview_osd) {
		return;
	}
	struct theme *theme = rc.theme;
	struct window_switcher_thumbnail_theme *switcher_theme =
		&theme->osd_window_switcher_thumbnail;
	int item_height = switcher_theme->item_height;

	overview_osd->scroll.delta_y += delta;
	int rows = overview_osd->scroll.delta_y / item_height;
	if (rows != 0) {
		overview_osd->scroll.top_row_idx += rows;
		overview_osd->scroll.delta_y -= rows * item_height;

		/* Clamp to valid range */
		int max_top = overview_osd->nr_rows - overview_osd->scroll.nr_visible_rows;
		overview_osd->scroll.top_row_idx = MAX(0, MIN(max_top, overview_osd->scroll.top_row_idx));

		int y_offset = -overview_osd->scroll.top_row_idx * item_height;
		wlr_scene_node_set_position(&overview_osd->items_tree->node, 0, y_offset);
	}
}

bool
overview_is_active(void)
{
	return overview_osd != NULL;
}