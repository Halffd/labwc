#!/usr/bin/env python3
"""
XML to TOML converter for labwc configuration files.
Usage: xml2toml.py [input.xml] [output.toml]
"""

import sys
import xml.etree.ElementTree as ET

def quote(s):
    return f'"{s}"'

def bool_val(s):
    s = str(s).lower()
    return 'true' if s in ('yes', 'true', 'on', '1') else 'false'

def convert_xml_to_toml(xml_path, toml_path):
    tree = ET.parse(xml_path)
    root = tree.getroot()

    lines = []
    lines.append('# Labwc TOML Configuration')
    lines.append('# Converted from XML - please review and edit as needed')
    lines.append('')

    def get_text(elem, tag, default=''):
        child = elem.find(tag)
        return child.text.strip() if child is not None and child.text else default

    def process_elem(elem, indent=0):
        prefix = '  ' * indent

        for child in list(elem):
            tag = child.tag
            text = (child.text or '').strip()
            attrib = child.attrib
            children = list(child)

            # Core section
            if tag == 'core':
                lines.append('[core]')
                lines.append(f'decoration = {quote(get_text(child, "decoration", "server"))}')
                lines.append(f'gap = {get_text(child, "gap", "0")}')
                lines.append(f'adaptiveSync = {quote(get_text(child, "adaptiveSync", "no"))}')
                lines.append(f'allowTearing = {quote(get_text(child, "allowTearing", "no"))}')
                lines.append(f'autoEnableOutputs = {bool_val(get_text(child, "autoEnableOutputs", "yes"))}')
                lines.append(f'reuseOutputMode = {bool_val(get_text(child, "reuseOutputMode", "no"))}')
                lines.append(f'xwaylandPersistence = {bool_val(get_text(child, "xwaylandPersistence", "no"))}')
                lines.append(f'primarySelection = {bool_val(get_text(child, "primarySelection", "yes"))}')
                lines.append(f'hideMaximizeButton = {bool_val(get_text(child, "hideMaximizeButton", "no"))}')
                pc = get_text(child, 'promptCommand')
                if pc:
                    lines.append(f'promptCommand = {quote(pc)}')
                lines.append('')

            # Placement section
            elif tag == 'placement':
                lines.append('[placement]')
                lines.append(f'policy = {quote(get_text(child, "policy", "cascade"))}')
                co = child.find('cascadeOffset')
                if co is not None:
                    lines.append(f'cascadeOffsetX = {co.attrib.get("x", "60")}')
                    lines.append(f'cascadeOffsetY = {co.attrib.get("y", "60")}')
                lines.append('')

            # Focus section
            elif tag == 'focus':
                lines.append('[focus]')
                lines.append(f'followMouse = {bool_val(get_text(child, "followMouse", "no"))}')
                lines.append(f'followMouseRequiresMovement = {bool_val(get_text(child, "followMouseRequiresMovement", "yes"))}')
                lines.append(f'raiseOnFocus = {bool_val(get_text(child, "raiseOnFocus", "no"))}')
                lines.append('')

            # Resistance section
            elif tag == 'resistance':
                lines.append('[resistance]')
                lines.append(f'screenEdgeStrength = {get_text(child, "screenEdgeStrength", "20")}')
                lines.append(f'windowEdgeStrength = {get_text(child, "windowEdgeStrength", "20")}')
                lines.append(f'unSnapThreshold = {get_text(child, "unSnapThreshold", "20")}')
                lines.append(f'unMaximizeThreshold = {get_text(child, "unMaximizeThreshold", "150")}')
                lines.append('')

            # Snapping section
            elif tag == 'snapping':
                lines.append('[snapping]')
                r = child.find('range')
                if r is not None:
                    lines.append(f'innerRange = {r.attrib.get("inner", "10")}')
                    lines.append(f'outerRange = {r.attrib.get("outer", "10")}')
                lines.append(f'cornerRange = {get_text(child, "cornerRange", "50")}')
                ov = child.find('overlay')
                if ov is not None:
                    lines.append(f'overlay = {bool_val(ov.attrib.get("enabled", "yes"))}')
                    dl = ov.find('delay')
                    if dl is not None:
                        lines.append(f'innerDelay = {dl.attrib.get("inner", "500")}')
                        lines.append(f'outerDelay = {dl.attrib.get("outer", "500")}')
                lines.append(f'topMaximize = {bool_val(get_text(child, "topMaximize", "yes"))}')
                nc = get_text(child, 'notifyClient')
                if nc:
                    lines.append(f'tilingEvents = {quote(nc)}')
                lines.append('')

            # Resize section
            elif tag == 'resize':
                lines.append('[resize]')
                lines.append(f'popupShow = {quote(get_text(child, "popupShow", "Never"))}')
                lines.append(f'drawContents = {bool_val(get_text(child, "drawContents", "yes"))}')
                lines.append(f'minimumArea = {get_text(child, "minimumArea", "8")}')
                cr = get_text(child, 'cornerRange')
                if cr:
                    lines.append(f'cornerRange = {cr}')
                lines.append('')

            # Theme section
            elif tag == 'theme':
                lines.append('[theme]')
                lines.append(f'name = {quote(get_text(child, "name", ""))}')
                lines.append(f'iconTheme = {quote(get_text(child, "icon", ""))}')
                lines.append(f'fallbackIconTheme = {quote(get_text(child, "fallbackAppIcon", "labwc"))}')
                lines.append(f'cornerRadius = {get_text(child, "cornerRadius", "8")}')
                tb = child.find('titlebar')
                if tb is not None:
                    lines.append(f'showTitle = {bool_val(get_text(tb, "showTitle", "yes"))}')
                lines.append(f'keepBorder = {bool_val(get_text(child, "keepBorder", "yes"))}')
                lines.append('')

            # Window switcher
            elif tag == 'windowSwitcher':
                lines.append('[windowSwitcher]')
                lines.append(f'preview = {bool_val(attrib.get("preview", "yes"))}')
                lines.append(f'outlines = {bool_val(attrib.get("outlines", "yes"))}')
                lines.append(f'unshade = {bool_val(attrib.get("unshade", "yes"))}')
                osd = child.find('osd')
                if osd is not None:
                    lines.append('')
                    lines.append('[windowSwitcher.osd]')
                    lines.append(f'show = {bool_val(osd.attrib.get("show", "yes"))}')
                    if 'style' in osd.attrib:
                        lines.append(f'style = {quote(osd.attrib["style"])}')
                    if 'output' in osd.attrib:
                        lines.append(f'output = {quote(osd.attrib["output"])}')
                    if 'thumbnailLabelFormat' in osd.attrib:
                        lines.append(f'thumbnailLabelFormat = {quote(osd.attrib["thumbnailLabelFormat"])}')
                lines.append('')

            # Desktops section
            elif tag == 'desktops':
                lines.append('[desktops]')
                lines.append(f'popupTime = {get_text(child, "popupTime", "1000")}')
                prefix = get_text(child, 'prefix')
                if prefix:
                    lines.append(f'prefix = {quote(prefix)}')
                initial = get_text(child, 'initial')
                if initial:
                    lines.append(f'initial = {quote(initial)}')
                names = child.find('names')
                if names is not None:
                    name_list = [n.text.strip() for n in names.findall('name') if n.text]
                    if name_list:
                        lines.append('workspace = [' + ', '.join(quote(n) for n in name_list) + ']')
                lines.append('')

            # Regions
            elif tag == 'regions':
                for region in child.findall('region'):
                    lines.append('[[regions]]')
                    lines.append(f'name = {quote(region.attrib["name"])}')
                    lines.append(f'x = {region.attrib.get("x", "0")}')
                    lines.append(f'y = {region.attrib.get("y", "0")}')
                    lines.append(f'width = {region.attrib.get("width", "50")}')
                    lines.append(f'height = {region.attrib.get("height", "50")}')
                    lines.append('')

            # Margin
            elif tag == 'margin':
                lines.append('[margin]')
                lines.append(f'top = {attrib.get("top", "0")}')
                lines.append(f'bottom = {attrib.get("bottom", "0")}')
                lines.append(f'left = {attrib.get("left", "0")}')
                lines.append(f'right = {attrib.get("right", "0")}')
                if 'output' in attrib:
                    lines.append(f'output = {quote(attrib["output"])}')
                lines.append('')

            # Keyboard
            elif tag == 'keyboard':
                lines.append('[keyboard]')
                nl = get_text(child, 'numlock')
                if nl:
                    lines.append(f'numlock = {bool_val(nl)}')
                ls = get_text(child, 'layoutScope')
                if ls:
                    lines.append(f'layoutScope = {quote(ls)}')
                lines.append(f'repeatRate = {get_text(child, "repeatRate", "25")}')
                lines.append(f'repeatDelay = {get_text(child, "repeatDelay", "600")}')
                lines.append('')
                for kb in child.findall('keybind'):
                    key = kb.attrib.get('key', '')
                    if not key:
                        continue
                    lines.append('[[keyboard.keybind]]')
                    lines.append(f'key = {quote(key)}')
                    if kb.attrib.get('onRelease', '').lower() == 'yes':
                        lines.append('onRelease = true')
                    if kb.attrib.get('allowWhenLocked', '').lower() == 'yes':
                        lines.append('allowWhenLocked = true')
                    # Get actions
                    actions = []
                    for action in kb.findall('.//action'):
                        name = action.attrib.get('name', '')
                        if not name:
                            continue
                        action_dict = {'name': name}
                        for arg in action:
                            arg_tag = arg.tag
                            arg_text = (arg.text or '').strip()
                            if arg_tag == 'command':
                                action_dict['command'] = arg_text
                            elif arg_tag == 'menu':
                                action_dict['menu'] = arg_text
                            elif arg_tag == 'direction':
                                action_dict['direction'] = arg_text
                            elif arg_tag == 'to':
                                action_dict['to'] = arg_text
                            elif arg_tag == 'wrap':
                                action_dict['wrap'] = arg_text
                            elif arg_tag == 'region':
                                action_dict['region'] = arg_text
                        actions.append(action_dict)
                    if actions:
                        action_strs = []
                        for a in actions:
                            a_str = '{ name = ' + quote(a['name'])
                            if 'command' in a:
                                a_str += ', command = ' + quote(a['command'])
                            if 'menu' in a:
                                a_str += ', menu = ' + quote(a['menu'])
                            if 'direction' in a:
                                a_str += ', direction = ' + quote(a['direction'])
                            if 'to' in a:
                                a_str += ', to = ' + quote(a['to'])
                            if 'wrap' in a:
                                a_str += ', wrap = ' + quote(a['wrap'])
                            if 'region' in a:
                                a_str += ', region = ' + quote(a['region'])
                            a_str += ' }'
                            action_strs.append(a_str)
                        lines.append(f'actions = [{", ".join(action_strs)}]')
                    lines.append('')

            # Mouse
            elif tag == 'mouse':
                lines.append('')
                for ctx in child.findall('context'):
                    ctx_name = ctx.attrib.get('name', '')
                    if not ctx_name:
                        continue
                    lines.append(f'[mouse.{ctx_name}]')
                    lines.append('mousebinds = [')
                    for mb in ctx.findall('mousebind'):
                        btn = mb.attrib.get('button', '')
                        action = mb.attrib.get('action', '')
                        direction = mb.attrib.get('direction', '')
                        if btn or direction:
                            entry = '  { '
                            if btn:
                                entry += f'button = {quote(btn)}'
                                if action:
                                    entry += f', action = {quote(action)}'
                            else:
                                entry += f'direction = {quote(direction)}'
                                if action:
                                    entry += f', action = {quote(action)}'
                            entry += ' }'
                            lines.append(entry)
                    lines.append(']')
                    lines.append('')

            # Touch
            elif tag == 'touch':
                lines.append('[touch]')
                for dev in child.findall('device'):
                    dev_name = dev.attrib.get('category', dev.attrib.get('deviceName', ''))
                    if dev_name:
                        lines.append(f'\n[touch.{dev_name}]')
                    for k, v in dev.attrib.items():
                        if k not in ('category', 'deviceName'):
                            lines.append(f'{k} = {bool_val(v)}' if v.lower() in ('yes', 'no') else f'{k} = {quote(v)}')
                    mo = dev.find('mapToOutput')
                    if mo is not None:
                        lines.append(f'mapToOutput = {quote(mo.text.strip() if mo.text else "")}')
                    me = dev.find('mouseEmulation')
                    if me is not None:
                        lines.append(f'mouseEmulation = {bool_val(me.text)}')
                lines.append('')

            # Tablet
            elif tag == 'tablet':
                lines.append('[tablet]')
                for k in ('rotate', 'forceMouseEmulation', 'mapToOutput', 'areaLeft', 'areaTop', 'areaWidth', 'areaHeight'):
                    v = get_text(child, k)
                    if v:
                        if k in ('forceMouseEmulation',):
                            lines.append(f'{k} = {bool_val(v)}')
                        else:
                            lines.append(f'{k} = {v}')
                for mp in child.findall('map'):
                    lines.append(f'# [[tablet.map]]')
                    lines.append(f'# from = {mp.attrib.get("from", "")}')
                    lines.append(f'# to = {mp.attrib.get("to", "")}')
                lines.append('')

            # Tablet tool
            elif tag == 'tabletTool':
                lines.append('[tabletTool]')
                for k in ('motion', 'relativeMotionSensitivity', 'minPressure', 'maxPressure'):
                    v = get_text(child, k)
                    if v:
                        lines.append(f'{k} = {quote(v) if k == "motion" else v}')
                lines.append('')

            # Libinput
            elif tag == 'libinput':
                lines.append('[libinput]')
                for dev in child.findall('device'):
                    dev_name = dev.attrib.get('category', '')
                    if dev_name:
                        lines.append(f'\n[libinput.{dev_name}]')
                    for k, v in dev.attrib.items():
                        if k == 'category':
                            continue
                        if v.lower() in ('yes', 'no'):
                            lines.append(f'{k} = {bool_val(v)}')
                        elif k in ('pointerSpeed',):
                            lines.append(f'{k} = {v}')
                        else:
                            lines.append(f'{k} = {quote(v)}')
                lines.append('')

            # Window rules
            elif tag == 'windowRules':
                for wr in child.findall('windowRule'):
                    lines.append('')
                    lines.append('[[windowRules]]')
                    for k, v in wr.attrib.items():
                        lines.append(f'# {k} = {quote(v)}')
                    for k in ('identifier', 'title'):
                        tv = get_text(wr, k)
                        if tv:
                            lines.append(f'{k} = {quote(tv)}')
                    mo = get_text(wr, 'matchOnce')
                    if mo.lower() == 'yes':
                        lines.append('matchOnce = true')
                    # Actions
                    actions = []
                    for action in wr.findall('.//action'):
                        name = action.attrib.get('name', '')
                        if not name:
                            continue
                        actions.append(f'{{ name = {quote(name)} }}')
                    if actions:
                        lines.append(f'actions = [{", ".join(actions)}]')
                lines.append('')

            # Menu
            elif tag == 'menu':
                lines.append('[menu]')
                lines.append(f'ignoreButtonReleasePeriod = {get_text(child, "ignoreButtonReleasePeriod", "0")}')
                lines.append(f'showIcons = {bool_val(get_text(child, "showIcons", "yes"))}')
                lines.append('')

            # Magnifier
            elif tag == 'magnifier':
                lines.append('[magnifier]')
                lines.append(f'width = {get_text(child, "width", "400")}')
                lines.append(f'height = {get_text(child, "height", "200")}')
                lines.append(f'initScale = {get_text(child, "initScale", "1.5")}')
                lines.append(f'increment = {get_text(child, "increment", "0.1")}')
                lines.append(f'useFilter = {bool_val(get_text(child, "useFilter", "yes"))}')
                lines.append('')

    # Process root children
    for child in root:
        process_elem(child)

    # Write output
    with open(toml_path, 'w') as f:
        f.write('\n'.join(lines))

    print(f"Converted '{xml_path}' -> '{toml_path}'")

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} input.xml [output.toml]")
        sys.exit(1)
    convert_xml_to_toml(sys.argv[1], sys.argv[2] if len(sys.argv) > 2 else 'rc.toml')