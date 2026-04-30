// SPDX-License-Identifier: GPL-2.0-only
#define _POSIX_C_SOURCE 200809L
#include "config/rcxml.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <wlr/util/box.h>
#include <wlr/util/log.h>
#include "action.h"
#include "common/dir.h"
#include "common/mem.h"
#include "common/parse-bool.h"
#include "common/string-helpers.h"
#include "common/toml.h"
#include "config/keybind.h"
#include "config/mousebind.h"
#include "config/default-bindings.h"
#include "labwc.h"
#include "view.h"

static toml_table_t *toml_root;

static void
read_core_toml(toml_table_t *table)
{
	toml_table_t *core = toml_table_in(table, "core");
	if (!core) {
		return;
	}

	toml_datum_t decoration = toml_string_in(core, "decoration");
	if (decoration.ok) {
		if (!strcmp(decoration.u.s, "client")) {
			rc.xdg_shell_server_side_deco = false;
		} else {
			rc.xdg_shell_server_side_deco = true;
		}
		free(decoration.u.s);
	}

	toml_datum_t gap = toml_int_in(core, "gap");
	if (gap.ok) {
		rc.gap = (int)gap.u.i;
	}

	toml_datum_t adaptive_sync = toml_string_in(core, "adaptiveSync");
	if (adaptive_sync.ok) {
		if (!strcasecmp(adaptive_sync.u.s, "fullscreen")) {
			rc.adaptive_sync = LAB_ADAPTIVE_SYNC_FULLSCREEN;
		} else {
			int ret = parse_bool(adaptive_sync.u.s, -1);
			if (ret == 1) {
				rc.adaptive_sync = LAB_ADAPTIVE_SYNC_ENABLED;
			} else {
				rc.adaptive_sync = LAB_ADAPTIVE_SYNC_DISABLED;
			}
		}
		free(adaptive_sync.u.s);
	}

	toml_datum_t allow_tearing = toml_string_in(core, "allowTearing");
	if (allow_tearing.ok) {
		if (!strcasecmp(allow_tearing.u.s, "fullscreen")) {
			rc.allow_tearing = LAB_TEARING_FULLSCREEN;
		} else if (!strcasecmp(allow_tearing.u.s, "always")) {
			rc.allow_tearing = LAB_TEARING_ENABLED;
		} else {
			rc.allow_tearing = LAB_TEARING_DISABLED;
		}
		free(allow_tearing.u.s);
	}

	toml_datum_t auto_enable = toml_bool_in(core, "autoEnableOutputs");
	if (auto_enable.ok) {
		rc.auto_enable_outputs = auto_enable.u.b;
	}

	toml_datum_t reuse_mode = toml_bool_in(core, "reuseOutputMode");
	if (reuse_mode.ok) {
		rc.reuse_output_mode = reuse_mode.u.b;
	}

	toml_datum_t xwayland = toml_bool_in(core, "xwaylandPersistence");
	if (xwayland.ok) {
		rc.xwayland_persistence = xwayland.u.b;
	}

	toml_datum_t primary_sel = toml_bool_in(core, "primarySelection");
	if (primary_sel.ok) {
		rc.primary_selection = primary_sel.u.b;
	}

	toml_datum_t hide_max = toml_bool_in(core, "hideMaximizeButton");
	if (hide_max.ok) {
		rc.hide_maximized_window_titlebar = hide_max.u.b;
	}

	toml_datum_t prompt_cmd = toml_string_in(core, "promptCommand");
	if (prompt_cmd.ok) {
		xstrdup_replace(rc.prompt_command, prompt_cmd.u.s);
		free(prompt_cmd.u.s);
	}
}

static void
read_placement_toml(toml_table_t *table)
{
	toml_table_t *placement = toml_table_in(table, "placement");
	if (!placement) {
		return;
	}

	toml_datum_t policy = toml_string_in(placement, "policy");
	if (policy.ok) {
		enum lab_placement_policy p = view_placement_parse(policy.u.s);
		if (p != LAB_PLACE_INVALID) {
			rc.placement_policy = p;
		}
		free(policy.u.s);
	}

	toml_datum_t cascade_x = toml_int_in(placement, "cascadeOffsetX");
	if (cascade_x.ok) {
		rc.placement_cascade_offset_x = (int)cascade_x.u.i;
	}

	toml_datum_t cascade_y = toml_int_in(placement, "cascadeOffsetY");
	if (cascade_y.ok) {
		rc.placement_cascade_offset_y = (int)cascade_y.u.i;
	}
}

static void
read_focus_toml(toml_table_t *table)
{
	toml_table_t *focus = toml_table_in(table, "focus");
	if (!focus) {
		return;
	}

	toml_datum_t follow = toml_bool_in(focus, "followMouse");
	if (follow.ok) {
		rc.focus_follow_mouse = follow.u.b;
	}

	toml_datum_t follow_req = toml_bool_in(focus, "followMouseRequiresMovement");
	if (follow_req.ok) {
		rc.focus_follow_mouse_requires_movement = follow_req.u.b;
	}

	toml_datum_t raise = toml_bool_in(focus, "raiseOnFocus");
	if (raise.ok) {
		rc.raise_on_focus = raise.u.b;
	}
}

static void
read_resistance_toml(toml_table_t *table)
{
	toml_table_t *resistance = toml_table_in(table, "resistance");
	if (!resistance) {
		return;
	}

	toml_datum_t screen_edge = toml_int_in(resistance, "screenEdgeStrength");
	if (screen_edge.ok) {
		rc.screen_edge_strength = (int)screen_edge.u.i;
	}

	toml_datum_t window_edge = toml_int_in(resistance, "windowEdgeStrength");
	if (window_edge.ok) {
		rc.window_edge_strength = (int)window_edge.u.i;
	}

	toml_datum_t unsnap = toml_int_in(resistance, "unSnapThreshold");
	if (unsnap.ok) {
		rc.unsnap_threshold = (int)unsnap.u.i;
	}

	toml_datum_t unmaximize = toml_int_in(resistance, "unMaximizeThreshold");
	if (unmaximize.ok) {
		rc.unmaximize_threshold = (int)unmaximize.u.i;
	}
}

static void
read_theme_toml(toml_table_t *table)
{
	toml_table_t *theme = toml_table_in(table, "theme");
	if (!theme) {
		return;
	}

	toml_datum_t name = toml_string_in(theme, "name");
	if (name.ok) {
		xstrdup_replace(rc.theme_name, name.u.s);
		free(name.u.s);
	}

	toml_datum_t icon = toml_string_in(theme, "iconTheme");
	if (icon.ok) {
		xstrdup_replace(rc.icon_theme_name, icon.u.s);
		free(icon.u.s);
	}

	toml_datum_t fallback_icon = toml_string_in(theme, "fallbackIconTheme");
	if (fallback_icon.ok) {
		xstrdup_replace(rc.fallback_app_icon_name, fallback_icon.u.s);
		free(fallback_icon.u.s);
	}

	toml_datum_t corner = toml_int_in(theme, "cornerRadius");
	if (corner.ok) {
		rc.corner_radius = (int)corner.u.i;
	}

	toml_datum_t show_title = toml_bool_in(theme, "showTitle");
	if (show_title.ok) {
		rc.show_title = show_title.u.b;
	}

	toml_datum_t keep_border = toml_bool_in(theme, "keepBorder");
	if (keep_border.ok) {
		rc.ssd_keep_border = keep_border.u.b;
	}
}

static void
read_window_switcher_toml(toml_table_t *table)
{
	toml_table_t *ws = toml_table_in(table, "windowSwitcher");
	if (!ws) {
		return;
	}

	toml_datum_t preview = toml_bool_in(ws, "preview");
	if (preview.ok) {
		rc.window_switcher.preview = preview.u.b;
	}

	toml_datum_t outlines = toml_bool_in(ws, "outlines");
	if (outlines.ok) {
		rc.window_switcher.outlines = outlines.u.b;
	}

	toml_datum_t unshade = toml_bool_in(ws, "unshade");
	if (unshade.ok) {
		rc.window_switcher.unshade = unshade.u.b;
	}

	toml_datum_t thumbnail_format = toml_string_in(ws, "thumbnailLabelFormat");
	if (thumbnail_format.ok) {
		xstrdup_replace(rc.window_switcher.osd.thumbnail_label_format, thumbnail_format.u.s);
		free(thumbnail_format.u.s);
	}
}

static void
read_keyboard_toml(toml_table_t *table)
{
	toml_table_t *keyboard = toml_table_in(table, "keyboard");
	if (!keyboard) {
		return;
	}

	toml_array_t *keybinds = toml_array_in(keyboard, "keybind");
	if (!keybinds) {
		return;
	}

	int nelem = toml_array_nelem(keybinds);
	for (int i = 0; i < nelem; i++) {
		toml_table_t *bind = toml_table_at(keybinds, i);
		if (!bind) {
			continue;
		}

		toml_datum_t key = toml_string_in(bind, "key");
		if (!key.ok) {
			continue;
		}

		fill_keybind_toml(key.u.s, bind);
		free(key.u.s);
	}
}

static void
read_mouse_toml(toml_table_t *table)
{
	toml_table_t *mouse = toml_table_in(table, "mouse");
	if (!mouse) {
		return;
	}

	int nkval = toml_table_nkval(mouse);
	for (int i = 0; i < nkval; i++) {
		const char *name = toml_key_in(mouse, i);
		if (!name) {
			continue;
		}

		toml_table_t *context = toml_table_in(mouse, name);
		if (!context) {
			continue;
		}

		fill_mouse_context_toml(name, context);
	}
}

void
toml_read_config(const char *filename)
{
	FILE *fp = fopen(filename, "r");
	if (!fp) {
		wlr_log(WLR_ERROR, "failed to open config file '%s'", filename);
		return;
	}

	char errbuf[256];
	toml_root = toml_parse_file(fp, errbuf, sizeof(errbuf));
	fclose(fp);

	if (!toml_root) {
		wlr_log(WLR_ERROR, "TOML config error: %s", errbuf);
		return;
	}

	read_keyboard_toml(toml_root);
	read_mouse_toml(toml_root);

	read_core_toml(toml_root);
	read_placement_toml(toml_root);
	read_focus_toml(toml_root);
	read_resistance_toml(toml_root);
	read_theme_toml(toml_root);
	read_window_switcher_toml(toml_root);

	wlr_log(WLR_INFO, "TOML config parsing completed");
}