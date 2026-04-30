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

	wlr_log(WLR_INFO, "TOML config parsing is not yet fully implemented");
	toml_free(toml_root);
	toml_root = NULL;
}