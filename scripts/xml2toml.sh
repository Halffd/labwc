#!/bin/sh
# XML to TOML converter for labwc
# Usage: xml2toml [input.xml] [output.toml]
#
# Reads rc.xml and converts it to TOML format.

INPUT="${1:-rc.xml}"
OUTPUT="${2:-rc.toml}"

if [ ! -f "$INPUT" ]; then
    echo "Error: Input file '$INPUT' not found"
    exit 1
fi

exec sed -E \
    -e 's/<!--.*-->//g' \
    -e '/^[[:space:]]*$/d' \
    -e ':a;N;$!ba;s/<labwc_config>//g' \
    -e 's|</labwc_config>||g' \
    -e 's/<core>/[core]/g' \
    -e 's/<decoration>([^<]*)<\/decoration>/decoration = "\1"/g' \
    -e 's/<gap>([^<]*)<\/gap>/gap = \1/g' \
    -e 's/<adaptiveSync>([^<]*)<\/adaptiveSync>/adaptiveSync = "\1"/g' \
    -e 's/<allowTearing>([^<]*)<\/allowTearing>/allowTearing = "\1"/g' \
    -e 's/<autoEnableOutputs>([^<]*)<\/autoEnableOutputs>/autoEnableOutputs = \1/g' \
    -e 's/<reuseOutputMode>([^<]*)<\/reuseOutputMode>/reuseOutputMode = \1/g' \
    -e 's/<xwaylandPersistence>([^<]*)<\/xwaylandPersistence>/xwaylandPersistence = \1/g' \
    -e 's/<primarySelection>([^<]*)<\/primarySelection>/primarySelection = \1/g' \
    -e 's/<hideMaximizeButton>([^<]*)<\/hideMaximizeButton>/hideMaximizeButton = \1/g' \
    -e 's/<promptCommand>([^<]*)<\/promptCommand>/promptCommand = "\1"/g' \
    -e 's/<placement>/[placement]/g' \
    -e 's/<policy>([^<]*)<\/policy>/policy = "\1"/g' \
    -e 's/<cascadeOffset x="([^"]*)" y="([^"]*)"/cascadeOffsetX = \1\ncascadeOffsetY = \2/g' \
    -e 's/<focus>/[focus]/g' \
    -e 's/<followMouse>([^<]*)<\/followMouse>/followMouse = \1/g' \
    -e 's/<followMouseRequiresMovement>([^<]*)<\/followMouseRequiresMovement>/followMouseRequiresMovement = \1/g' \
    -e 's/<raiseOnFocus>([^<]*)<\/raiseOnFocus>/raiseOnFocus = \1/g' \
    -e 's/<resistance>/[resistance]/g' \
    -e 's/<screenEdgeStrength>([^<]*)<\/screenEdgeStrength>/screenEdgeStrength = \1/g' \
    -e 's/<windowEdgeStrength>([^<]*)<\/windowEdgeStrength>/windowEdgeStrength = \1/g' \
    -e 's/<unSnapThreshold>([^<]*)<\/unSnapThreshold>/unSnapThreshold = \1/g' \
    -e 's/<unMaximizeThreshold>([^<]*)<\/unMaximizeThreshold>/unMaximizeThreshold = \1/g' \
    -e 's/<snapping>/[snapping]/g' \
    -e 's/<range inner="([^"]*)" outer="([^"]*)"/innerRange = \1\nouterRange = \2/g' \
    -e 's/<cornerRange>([^<]*)<\/cornerRange>/cornerRange = \1/g' \
    -e 's/<overlay enabled="([^"]*)">/overlay = \1/g' \
    -e 's/<topMaximize>([^<]*)<\/topMaximize>/topMaximize = \1/g' \
    -e 's/<resize>/[resize]/g' \
    -e 's/<popupShow>([^<]*)<\/popupShow>/popupShow = "\1"/g' \
    -e 's/<drawContents>([^<]*)<\/drawContents>/drawContents = \1/g' \
    -e 's/<minimumArea>([^<]*)<\/minimumArea>/minimumArea = \1/g' \
    -e 's/<theme>/[theme]/g' \
    -e 's/<name>([^<]*)<\/name>/name = "\1"/g' \
    -e 's/<icon>([^<]*)<\/icon>/iconTheme = "\1"/g' \
    -e 's/<fallbackAppIcon>([^<]*)<\/fallbackAppIcon>/fallbackIconTheme = "\1"/g' \
    -e 's/<cornerRadius>([^<]*)<\/cornerRadius>/cornerRadius = \1/g' \
    -e 's/<keepBorder>([^<]*)<\/keepBorder>/keepBorder = \1/g' \
    -e 's/<windowSwitcher>/[windowSwitcher]/g' \
    -e 's/<windowSwitcher preview="([^"]*)" outlines="([^"]*)"/preview = \1\noutlines = \2/g' \
    -e 's/<menu>/[menu]/g' \
    -e 's/<ignoreButtonReleasePeriod>([^<]*)<\/ignoreButtonReleasePeriod>/ignoreButtonReleasePeriod = \1/g' \
    -e 's/<showIcons>([^<]*)<\/showIcons>/showIcons = \1/g' \
    -e 's/<magnifier>/[magnifier]/g' \
    -e 's/<width>([^<]*)<\/width>/width = \1/g' \
    -e 's/<height>([^<]*)<\/height>/height = \1/g' \
    -e 's/<initScale>([^<]*)<\/initScale>/initScale = \1/g' \
    -e 's/<increment>([^<]*)<\/increment>/increment = \1/g' \
    -e 's/<useFilter>([^<]*)<\/useFilter>/useFilter = \1/g' \
    "$INPUT" > "$OUTPUT"

echo "Converted '$INPUT' to '$OUTPUT'"