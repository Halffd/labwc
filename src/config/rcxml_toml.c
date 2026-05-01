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
#include "common/list.h"
#include "common/mem.h"
#include "common/parse-bool.h"
#include "common/string-helpers.h"
#include "common/toml.h"
#include "config/keybind.h"
#include "config/libinput.h"
#include "config/mousebind.h"
#include "config/default-bindings.h"
#include "config/touch.h"
#include "config/tablet.h"
#include "config/tablet-tool.h"
#include "regions.h"
#include "window-rules.h"
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
fill_window_rule_toml(toml_table_t *table)
{
	struct window_rule *rule = znew(*rule);
	rule->window_type = LAB_WINDOW_TYPE_INVALID;
	wl_list_append(&rc.window_rules, &rule->link);
	wl_list_init(&rule->actions);

	toml_datum_t identifier = toml_string_in(table, "identifier");
	if (identifier.ok) {
		xstrdup_replace(rule->identifier, identifier.u.s);
		free(identifier.u.s);
	}

	toml_datum_t title = toml_string_in(table, "title");
	if (title.ok) {
		xstrdup_replace(rule->title, title.u.s);
		free(title.u.s);
	}

	toml_datum_t match_once = toml_bool_in(table, "matchOnce");
	if (match_once.ok) {
		rule->match_once = match_once.u.b;
	}

	toml_datum_t sandbox_engine = toml_string_in(table, "sandboxEngine");
	if (sandbox_engine.ok) {
		xstrdup_replace(rule->sandbox_engine, sandbox_engine.u.s);
		free(sandbox_engine.u.s);
	}

	toml_datum_t sandbox_app_id = toml_string_in(table, "sandboxAppId");
	if (sandbox_app_id.ok) {
		xstrdup_replace(rule->sandbox_app_id, sandbox_app_id.u.s);
		free(sandbox_app_id.u.s);
	}

	toml_datum_t server_dec = toml_string_in(table, "serverDecoration");
	if (server_dec.ok) {
		if (!strcasecmp(server_dec.u.s, "yes") || !strcasecmp(server_dec.u.s, "force")) {
			rule->server_decoration = LAB_PROP_TRUE;
		} else if (!strcasecmp(server_dec.u.s, "no")) {
			rule->server_decoration = LAB_PROP_FALSE;
		} else {
			rule->server_decoration = LAB_PROP_UNSET;
		}
		free(server_dec.u.s);
	}

	toml_datum_t skip_taskbar = toml_string_in(table, "skipTaskbar");
	if (skip_taskbar.ok) {
		if (!strcasecmp(skip_taskbar.u.s, "yes")) {
			rule->skip_taskbar = LAB_PROP_TRUE;
		} else if (!strcasecmp(skip_taskbar.u.s, "no")) {
			rule->skip_taskbar = LAB_PROP_FALSE;
		} else {
			rule->skip_taskbar = LAB_PROP_UNSET;
		}
		free(skip_taskbar.u.s);
	}

	toml_datum_t skip_switcher = toml_string_in(table, "skipWindowSwitcher");
	if (skip_switcher.ok) {
		if (!strcasecmp(skip_switcher.u.s, "yes")) {
			rule->skip_window_switcher = LAB_PROP_TRUE;
		} else if (!strcasecmp(skip_switcher.u.s, "no")) {
			rule->skip_window_switcher = LAB_PROP_FALSE;
		} else {
			rule->skip_window_switcher = LAB_PROP_UNSET;
		}
		free(skip_switcher.u.s);
	}

	toml_array_t *actions = toml_array_in(table, "actions");
	if (actions) {
		append_parsed_actions_toml(actions, &rule->actions);
	}
}

static void
read_window_rules_toml(toml_table_t *table)
{
	toml_array_t *window_rules = toml_array_in(table, "windowRules");
	if (!window_rules) {
		return;
	}

	int nelem = toml_array_nelem(window_rules);
	for (int i = 0; i < nelem; i++) {
		toml_table_t *rule = toml_table_at(window_rules, i);
		if (!rule) {
			continue;
		}
		fill_window_rule_toml(rule);
	}
}

static void
read_libinput_toml(toml_table_t *table)
{
	toml_table_t *libinput = toml_table_in(table, "libinput");
	if (!libinput) {
		return;
	}

	int nkval = toml_table_nkval(libinput);
	for (int i = 0; i < nkval; i++) {
		const char *key = toml_key_in(libinput, i);
		if (!key) {
			continue;
		}

		toml_table_t *device = toml_table_in(libinput, key);
		if (!device) {
			continue;
		}

		struct libinput_category *category = libinput_category_create();

		enum lab_libinput_device_type type = get_device_type(key);
		if (type != LAB_LIBINPUT_DEVICE_NONE) {
			category->type = type;
		} else {
			xstrdup_replace(category->name, key);
		}

		toml_datum_t natural_scroll = toml_bool_in(device, "naturalScroll");
		if (natural_scroll.ok) {
			category->natural_scroll = natural_scroll.u.b ? 1 : 0;
		}

		toml_datum_t left_handed = toml_bool_in(device, "leftHanded");
		if (left_handed.ok) {
			category->left_handed = left_handed.u.b ? 1 : 0;
		}

		toml_datum_t pointer_speed = toml_double_in(device, "pointerSpeed");
		if (pointer_speed.ok) {
			category->pointer_speed = (float)pointer_speed.u.d;
			if (category->pointer_speed < -1) {
				category->pointer_speed = -1;
			} else if (category->pointer_speed > 1) {
				category->pointer_speed = 1;
			}
		}

		toml_datum_t tap = toml_bool_in(device, "tap");
		if (tap.ok) {
			category->tap = tap.u.b
				? LIBINPUT_CONFIG_TAP_ENABLED
				: LIBINPUT_CONFIG_TAP_DISABLED;
		}

		toml_datum_t tap_button_map = toml_string_in(device, "tapButtonMap");
		if (tap_button_map.ok) {
			if (!strcmp(tap_button_map.u.s, "lrm")) {
				category->tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
			} else if (!strcmp(tap_button_map.u.s, "lmr")) {
				category->tap_button_map = LIBINPUT_CONFIG_TAP_MAP_LMR;
			}
			free(tap_button_map.u.s);
		}

		toml_datum_t tap_and_drag = toml_bool_in(device, "tapAndDrag");
		if (tap_and_drag.ok) {
			category->tap_and_drag = tap_and_drag.u.b
				? LIBINPUT_CONFIG_DRAG_ENABLED
				: LIBINPUT_CONFIG_DRAG_DISABLED;
		}

		toml_datum_t drag_lock = toml_string_in(device, "dragLock");
		if (drag_lock.ok) {
			if (!strcasecmp(drag_lock.u.s, "timeout")) {
				category->drag_lock = LIBINPUT_CONFIG_DRAG_LOCK_ENABLED;
			} else {
				int ret = parse_bool(drag_lock.u.s, -1);
				if (ret < 0) {
					category->drag_lock = -1;
				} else {
					category->drag_lock = ret
						? LIBINPUT_CONFIG_DRAG_LOCK_ENABLED
						: LIBINPUT_CONFIG_DRAG_LOCK_DISABLED;
				}
			}
			free(drag_lock.u.s);
		}

		toml_datum_t accel_profile = toml_string_in(device, "accelProfile");
		if (accel_profile.ok) {
			if (!strcasecmp(accel_profile.u.s, "flat")) {
				category->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
			} else if (!strcasecmp(accel_profile.u.s, "adaptive")) {
				category->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
			} else {
				category->accel_profile = -1;
			}
			free(accel_profile.u.s);
		}

		toml_datum_t middle_emu = toml_bool_in(device, "middleEmulation");
		if (middle_emu.ok) {
			category->middle_emu = middle_emu.u.b
				? LIBINPUT_CONFIG_MIDDLE_EMULATION_ENABLED
				: LIBINPUT_CONFIG_MIDDLE_EMULATION_DISABLED;
		}

		toml_datum_t dwt = toml_bool_in(device, "disableWhileTyping");
		if (dwt.ok) {
			category->dwt = dwt.u.b
				? LIBINPUT_CONFIG_DWT_ENABLED
				: LIBINPUT_CONFIG_DWT_DISABLED;
		}

		toml_datum_t click_method = toml_string_in(device, "clickMethod");
		if (click_method.ok) {
			if (!strcasecmp(click_method.u.s, "none")) {
				category->click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
			} else if (!strcasecmp(click_method.u.s, "buttonareas")) {
				category->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
			} else if (!strcasecmp(click_method.u.s, "clickfinger")) {
				category->click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
			} else {
				category->click_method = -1;
			}
			free(click_method.u.s);
		}

		toml_datum_t scroll_method = toml_string_in(device, "scrollMethod");
		if (scroll_method.ok) {
			if (!strcasecmp(scroll_method.u.s, "none")) {
				category->scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
			} else if (!strcasecmp(scroll_method.u.s, "button")) {
				category->scroll_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
			} else if (!strcasecmp(scroll_method.u.s, "edge")) {
				category->scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
			} else if (!strcasecmp(scroll_method.u.s, "twofinger")) {
				category->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
			} else {
				category->scroll_method = -1;
			}
			free(scroll_method.u.s);
		}

		toml_datum_t send_events = toml_string_in(device, "sendEventsMode");
		if (send_events.ok) {
			if (!strcasecmp(send_events.u.s, "enabled")) {
				category->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
			} else if (!strcasecmp(send_events.u.s, "disabled")) {
				category->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
			} else if (!strcasecmp(send_events.u.s, "disabled-on-external-mouse")) {
				category->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
			} else {
				category->send_events_mode = -1;
			}
			free(send_events.u.s);
		}
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

static void
read_snapping_toml(toml_table_t *table)
{
	toml_table_t *snapping = toml_table_in(table, "snapping");
	if (!snapping) {
		return;
	}

	toml_datum_t inner_range = toml_int_in(snapping, "innerRange");
	if (inner_range.ok) {
		rc.snap_edge_range_inner = (int)inner_range.u.i;
	}

	toml_datum_t outer_range = toml_int_in(snapping, "outerRange");
	if (outer_range.ok) {
		rc.snap_edge_range_outer = (int)outer_range.u.i;
	}

	toml_datum_t corner_range = toml_int_in(snapping, "cornerRange");
	if (corner_range.ok) {
		rc.snap_edge_corner_range = (int)corner_range.u.i;
	}

	toml_datum_t overlay = toml_bool_in(snapping, "overlay");
	if (overlay.ok) {
		rc.snap_overlay_enabled = overlay.u.b;
	}

	toml_datum_t inner_delay = toml_int_in(snapping, "innerDelay");
	if (inner_delay.ok) {
		rc.snap_overlay_delay_inner = (int)inner_delay.u.i;
	}

	toml_datum_t outer_delay = toml_int_in(snapping, "outerDelay");
	if (outer_delay.ok) {
		rc.snap_overlay_delay_outer = (int)outer_delay.u.i;
	}

	toml_datum_t top_maximize = toml_bool_in(snapping, "topMaximize");
	if (top_maximize.ok) {
		rc.snap_top_maximize = top_maximize.u.b;
	}

	toml_datum_t tiling_events = toml_string_in(snapping, "tilingEvents");
	if (tiling_events.ok) {
		if (!strcasecmp(tiling_events.u.s, "always")) {
			rc.snap_tiling_events_mode = LAB_TILING_EVENTS_ALWAYS;
		} else if (!strcasecmp(tiling_events.u.s, "region")) {
			rc.snap_tiling_events_mode = LAB_TILING_EVENTS_REGION;
		} else if (!strcasecmp(tiling_events.u.s, "edge")) {
			rc.snap_tiling_events_mode = LAB_TILING_EVENTS_EDGE;
		} else {
			rc.snap_tiling_events_mode = LAB_TILING_EVENTS_NEVER;
		}
		free(tiling_events.u.s);
	}
}

static void
read_resize_toml(toml_table_t *table)
{
	toml_table_t *resize = toml_table_in(table, "resize");
	if (!resize) {
		return;
	}

	toml_datum_t popup_show = toml_string_in(resize, "popupShow");
	if (popup_show.ok) {
		if (!strcasecmp(popup_show.u.s, "always")) {
			rc.resize_indicator = LAB_RESIZE_INDICATOR_ALWAYS;
		} else if (!strcasecmp(popup_show.u.s, "non-pixel")) {
			rc.resize_indicator = LAB_RESIZE_INDICATOR_NON_PIXEL;
		} else {
			rc.resize_indicator = LAB_RESIZE_INDICATOR_NEVER;
		}
		free(popup_show.u.s);
	}

	toml_datum_t draw_contents = toml_bool_in(resize, "drawContents");
	if (draw_contents.ok) {
		rc.resize_draw_contents = draw_contents.u.b;
	}

	toml_datum_t corner_range = toml_int_in(resize, "cornerRange");
	if (corner_range.ok) {
		rc.resize_corner_range = (int)corner_range.u.i;
	}

	toml_datum_t minimum_area = toml_int_in(resize, "minimumArea");
	if (minimum_area.ok) {
		rc.resize_minimum_area = (int)minimum_area.u.i;
	}
}

static void
read_margin_toml(toml_table_t *table)
{
	toml_table_t *margin = toml_table_in(table, "margin");
	if (!margin) {
		return;
	}

	toml_datum_t top = toml_int_in(margin, "top");
	if (top.ok) {
		/* Find first usable_area_override and update its top margin */
		struct usable_area_override *override;
		wl_list_for_each(override, &rc.usable_area_overrides, link) {
			if (!override->output) {
				override->margin.top = (int)top.u.i;
				break;
			}
		}
	}

	toml_datum_t bottom = toml_int_in(margin, "bottom");
	if (bottom.ok) {
		struct usable_area_override *override;
		wl_list_for_each(override, &rc.usable_area_overrides, link) {
			if (!override->output) {
				override->margin.bottom = (int)bottom.u.i;
				break;
			}
		}
	}

	toml_datum_t left = toml_int_in(margin, "left");
	if (left.ok) {
		struct usable_area_override *override;
		wl_list_for_each(override, &rc.usable_area_overrides, link) {
			if (!override->output) {
				override->margin.left = (int)left.u.i;
				break;
			}
		}
	}

	toml_datum_t right = toml_int_in(margin, "right");
	if (right.ok) {
		struct usable_area_override *override;
		wl_list_for_each(override, &rc.usable_area_overrides, link) {
			if (!override->output) {
				override->margin.right = (int)right.u.i;
				break;
			}
		}
	}

	toml_datum_t output = toml_string_in(margin, "output");
	if (output.ok) {
		struct usable_area_override *override = znew(*override);
		override->output = xstrdup(output.u.s);
		wl_list_append(&rc.usable_area_overrides, &override->link);
		free(output.u.s);
	}
}

static void
read_desktops_toml(toml_table_t *table)
{
	toml_table_t *desktops = toml_table_in(table, "desktops");
	if (!desktops) {
		return;
	}

	toml_datum_t popup_time = toml_int_in(desktops, "popupTime");
	if (popup_time.ok) {
		rc.workspace_config.popuptime = (int)popup_time.u.i;
	}

	toml_datum_t prefix = toml_string_in(desktops, "prefix");
	if (prefix.ok) {
		xstrdup_replace(rc.workspace_config.prefix, prefix.u.s);
		free(prefix.u.s);
	}

	toml_datum_t initial = toml_string_in(desktops, "initial");
	if (initial.ok) {
		xstrdup_replace(rc.workspace_config.initial_workspace_name, initial.u.s);
		free(initial.u.s);
	}

	toml_array_t *workspace_list = toml_array_in(desktops, "workspace");
	if (workspace_list) {
		int nelem = toml_array_nelem(workspace_list);
		for (int i = 0; i < nelem; i++) {
			toml_datum_t name = toml_string_at(workspace_list, i);
			if (name.ok) {
				struct workspace_config *ws = znew(*ws);
				ws->name = xstrdup(name.u.s);
				wl_list_append(&rc.workspace_config.workspaces, &ws->link);
				free(name.u.s);
			}
		}
	}
}

static void
read_menu_toml(toml_table_t *table)
{
	toml_table_t *menu = toml_table_in(table, "menu");
	if (!menu) {
		return;
	}

	toml_datum_t ignore_period = toml_int_in(menu, "ignoreButtonReleasePeriod");
	if (ignore_period.ok) {
		rc.menu_ignore_button_release_period = (unsigned int)ignore_period.u.i;
	}

	toml_datum_t show_icons = toml_bool_in(menu, "showIcons");
	if (show_icons.ok) {
		rc.menu_show_icons = show_icons.u.b;
	}
}

static void
read_magnifier_toml(toml_table_t *table)
{
	toml_table_t *magnifier = toml_table_in(table, "magnifier");
	if (!magnifier) {
		return;
	}

	toml_datum_t width = toml_int_in(magnifier, "width");
	if (width.ok) {
		rc.mag_width = (int)width.u.i;
	}

	toml_datum_t height = toml_int_in(magnifier, "height");
	if (height.ok) {
		rc.mag_height = (int)height.u.i;
	}

	toml_datum_t init_scale = toml_double_in(magnifier, "initScale");
	if (init_scale.ok) {
		rc.mag_scale = (float)init_scale.u.d;
	}

	toml_datum_t increment = toml_double_in(magnifier, "increment");
	if (increment.ok) {
		rc.mag_increment = (float)increment.u.d;
	}

	toml_datum_t use_filter = toml_bool_in(magnifier, "useFilter");
	if (use_filter.ok) {
		rc.mag_filter = use_filter.u.b;
	}
}

static void
read_regions_toml(toml_table_t *table)
{
	toml_array_t *regions = toml_array_in(table, "regions");
	if (!regions) {
		return;
	}

	int nelem = toml_array_nelem(regions);
	for (int i = 0; i < nelem; i++) {
		toml_table_t *region = toml_table_at(regions, i);
		if (!region) {
			continue;
		}

		struct region *r = znew(*r);
		wl_list_append(&rc.regions, &r->link);

		toml_datum_t name = toml_string_in(region, "name");
		if (name.ok) {
			xstrdup_replace(r->name, name.u.s);
			free(name.u.s);
		}

		toml_datum_t x = toml_int_in(region, "x");
		if (x.ok) {
			r->percentage.x = (int)x.u.i;
		}

		toml_datum_t y = toml_int_in(region, "y");
		if (y.ok) {
			r->percentage.y = (int)y.u.i;
		}

		toml_datum_t width = toml_int_in(region, "width");
		if (width.ok) {
			r->percentage.width = (int)width.u.i;
		}

		toml_datum_t height = toml_int_in(region, "height");
		if (height.ok) {
			r->percentage.height = (int)height.u.i;
		}
	}
}

static void
read_touch_toml(toml_table_t *table)
{
	toml_table_t *touch = toml_table_in(table, "touch");
	if (!touch) {
		return;
	}

	int nkval = toml_table_nkval(touch);
	for (int i = 0; i < nkval; i++) {
		const char *key = toml_key_in(touch, i);
		if (!key) {
			continue;
		}

		toml_table_t *device = toml_table_in(touch, key);
		if (!device) {
			continue;
		}

		struct touch_config_entry *t = znew(*t);
		wl_list_append(&rc.touch_configs, &t->link);

		t->device_name = xstrdup(key);

		toml_datum_t map_to_output = toml_string_in(device, "mapToOutput");
		if (map_to_output.ok) {
			t->output_name = xstrdup(map_to_output.u.s);
			free(map_to_output.u.s);
		}

		toml_datum_t mouse_emulation = toml_bool_in(device, "mouseEmulation");
		if (mouse_emulation.ok) {
			t->force_mouse_emulation = mouse_emulation.u.b;
		}
	}
}

static void
read_tablet_toml(toml_table_t *table)
{
	toml_table_t *tablet = toml_table_in(table, "tablet");
	if (!tablet) {
		return;
	}

	toml_datum_t rotate = toml_int_in(tablet, "rotate");
	if (rotate.ok) {
		rc.tablet.rotation = tablet_parse_rotation((int)rotate.u.i);
	}

	toml_datum_t force_mouse = toml_bool_in(tablet, "forceMouseEmulation");
	if (force_mouse.ok) {
		rc.tablet.force_mouse_emulation = force_mouse.u.b;
	}

	toml_datum_t output = toml_string_in(tablet, "mapToOutput");
	if (output.ok) {
		xstrdup_replace(rc.tablet.output_name, output.u.s);
		free(output.u.s);
	}

	toml_datum_t area_left = toml_double_in(tablet, "areaLeft");
	if (area_left.ok) {
		rc.tablet.box.x = area_left.u.d;
	}

	toml_datum_t area_top = toml_double_in(tablet, "areaTop");
	if (area_top.ok) {
		rc.tablet.box.y = area_top.u.d;
	}

	toml_datum_t area_width = toml_double_in(tablet, "areaWidth");
	if (area_width.ok) {
		rc.tablet.box.width = area_width.u.d;
	}

	toml_datum_t area_height = toml_double_in(tablet, "areaHeight");
	if (area_height.ok) {
		rc.tablet.box.height = area_height.u.d;
	}
}

static void
read_tablet_tool_toml(toml_table_t *table)
{
	toml_table_t *tablet_tool = toml_table_in(table, "tabletTool");
	if (!tablet_tool) {
		return;
	}

	toml_datum_t motion = toml_string_in(tablet_tool, "motion");
	if (motion.ok) {
		rc.tablet_tool.motion = tablet_parse_motion(motion.u.s);
		free(motion.u.s);
	}

	toml_datum_t relative_sensitivity = toml_double_in(tablet_tool, "relativeMotionSensitivity");
	if (relative_sensitivity.ok) {
		rc.tablet_tool.relative_motion_sensitivity = relative_sensitivity.u.d;
	}

	toml_datum_t min_pressure = toml_double_in(tablet_tool, "minPressure");
	if (min_pressure.ok) {
		rc.tablet_tool.min_pressure = min_pressure.u.d;
	}

	toml_datum_t max_pressure = toml_double_in(tablet_tool, "maxPressure");
	if (max_pressure.ok) {
		rc.tablet_tool.max_pressure = max_pressure.u.d;
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
	read_window_rules_toml(toml_root);
	read_libinput_toml(toml_root);
	read_snapping_toml(toml_root);
	read_resize_toml(toml_root);
	read_margin_toml(toml_root);
	read_desktops_toml(toml_root);
	read_menu_toml(toml_root);
	read_magnifier_toml(toml_root);
	read_regions_toml(toml_root);
	read_touch_toml(toml_root);
	read_tablet_toml(toml_root);
	read_tablet_tool_toml(toml_root);

	wlr_log(WLR_INFO, "TOML config parsing completed");
}