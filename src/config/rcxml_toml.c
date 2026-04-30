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
#include "common/string-helpers.h"
#include "common/toml.h"
#include "config/keybind.h"
#include "config/mousebind.h"
#include "config/default-bindings.h"
#include "labwc.h"

static toml_table_t *toml_root;

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

	wlr_log(WLR_INFO, "TOML config parsing completed");
}