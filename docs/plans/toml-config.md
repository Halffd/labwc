---
plan name: toml-config
plan description: TOML config migration
plan status: active
---

## Idea
Replace XML configuration with TOML format in labwc. libxml2 is currently used for parsing ~2092 lines of config in rcxml.c. Need to add TOML library, create schema, and rewrite config loading while maintaining functionality.

## Implementation
- 1. Add TOML parsing library (tomlc99) - compile as subproject or header-only
- 2. Create TOML config schema in rcxml.c/h - map current XML sections to TOML tables
- 3. Implement toml_read_config() function to replace rcxml_read()
- 4. Port keybinds section (keyboard keybinds with actions)
- 5. Port mousebinds section (mouse bindings with context)
- 6. Port windowRules section (window matching and actions)
- 7. Port theme section (fonts, colors, decorations)
- 8. Port libinput section (device configuration)
- 9. Port core/placement/focus/resistance/snapping sections
- 10. Port remaining sections (desktops, regions, touch, tablet, etc.)
- 11. Add validation and error messages for TOML parsing
- 12. Test with sample rc.toml config file

## Required Specs
<!-- SPECS_START -->
<!-- SPECS_END -->