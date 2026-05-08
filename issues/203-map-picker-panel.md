# 203 — Map picker panel

## Status

open

## Current behavior

On first load, the editor shows a browser `prompt()` asking for a server URL
and map name. These are stored in localStorage. The only way to switch maps
is to click "switch map", which clears localStorage and reloads — prompting
again with bare text inputs.

This is friction every time you want to work on a different map.

## Intended behavior

On load, if no server URL or map name is stored, show a proper panel (not a
browser prompt) with:
- A text input for the server URL (default: http://localhost:7700)
- A list of available maps fetched from GET /maps on the server
- Clicking a map name connects to it and loads the editor

The "switch map" button opens this panel again without clearing all of
localStorage (so the server URL is remembered).

The panel should be rendered in the sidebar, replacing the inspector content
while open. Once a map is selected, the normal editor state resumes.

## Suggested implementation steps

1. On `DOMContentLoaded`, if `soramech_map` is not in localStorage, show the
   picker panel instead of loading immediately.
2. The picker panel: server URL text input + "connect" button. On connect,
   call `API.list_maps()` and render a clickable list of map names.
3. On map name click: store URL + name in localStorage, call `API.init()`,
   close the picker, call `load_map()`.
4. "switch map" button opens the picker panel without wiping the server URL.
5. If the server is unreachable, show an error in the panel with a retry
   option.

## Related documents

- assets/js/003-api.js — list_maps()
- assets/js/005-app.js — init(), load_map()
- assets/index.html — sidebar and inspector panel
