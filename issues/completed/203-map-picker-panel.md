# 203 — Map picker panel

## Status

complete

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

The map-name label in the sidebar header opens the same panel for switching
maps without clearing localStorage. The redundant top-left "switch map"
toolbar button (which used to wipe localStorage and reload) is removed.

The panel renders in the sidebar, replacing the inspector content while
open. Once a map is selected, the normal editor state resumes.

## Implementation notes

`show_map_picker` in `assets/js/005-app.js`:
- Carries its own server-URL `<input>` at the top of the panel — the picker
  reads from this input rather than from API state, so it works on
  first-time load when nothing is stored yet.
- `→` button next to the URL input (and the Enter key in the input) refreshes
  the maps list.
- Maps fetched via direct `fetch(url + '/maps')`, bypassing `API.list_maps`
  so it works before `API.init()` has been called.
- Recent maps come from `localStorage.soramech_maps_history`. Each row's
  click handler captures its own `(server_url, map_name)` so users can jump
  back to a different server entirely.
- "On this server" group lists what the picker URL serves. Click handlers
  pass the picker URL into `switch_to_map`.
- "Open map by name" input/button at the bottom for typing a name directly.
- Errors (server unreachable, non-200) render inline as `cannot reach
  server: <reason>`.

`init` in `assets/js/005-app.js`:
- If `soramech_server` and `soramech_map` are both in localStorage, the
  normal flow runs (`API.init` + `load_map`).
- Otherwise, the picker is shown immediately. Selection inside the picker
  saves to localStorage and calls `switch_to_map`, which then calls
  `load_map`. No `prompt()` anywhere.
- The `requestAnimationFrame(render)` loop starts in either case so the
  canvas is live the moment the picker is up.

The top-left `switch map` toolbar button is removed. The map-label in the
sidebar header is the only switch-maps entry point now.

## Related documents

- assets/js/003-api.js — list_maps (no longer used for first-time fetch)
- assets/js/005-app.js — init, load_map, show_map_picker, switch_to_map
- assets/index.html — sidebar, inspector panel, toolbar
