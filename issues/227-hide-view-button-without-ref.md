# 227 — Hide the inspector "view" button when the box has no ref

## Status
open

## Current behavior

The inspector renders three buttons next to the `ref` field on every
call box: `browse` (open the file browser), `view` (open the
floating source viewer, issue 215), and the inputs/outputs sections.
The `view` button shows up for every call box regardless of whether
`box.ref` has been set yet.

Clicking `view` on a box with no ref calls
`SourceView.open_source_view(current_box.ref, current_box)`, which
falls through to `fetch_source(ref)` returning the error "no ref
set on this box." A status-bar error appears. The button is
non-functional until a ref is set; it shouldn't be visible until
then.

## Intended behavior

The `view` button is rendered only when `box.ref` is non-empty. When
the user creates a fresh box and hasn't yet picked a function via the
file browser, the button is absent. After picking a function, the
button appears. After clearing the ref (manual edit of the input
field), the button disappears again.

The `browse` button stays visible at all times — that's the entry
point to actually setting a ref.

## Suggested implementation

In `assets/js/004-inspector.js::show`, in the block that builds the
ref row:

```js
const view_btn = document.createElement('button');
// ... existing setup ...
ref_wrap.appendChild(ref_inp);
ref_wrap.appendChild(browse_btn);
if (box.ref && box.ref !== '') {
  ref_wrap.appendChild(view_btn);
}
```

The button should also appear/disappear in response to the user
typing into the ref input. Easiest way: re-render the ref row on
input. Or: keep the button always in the DOM but toggle its
`hidden` attribute on input.

Toggle-on-input is simpler:

```js
ref_inp.addEventListener('input', () => {
  current_box.ref = ref_inp.value;
  view_btn.hidden = !ref_inp.value;
  save();
});
```

Add `view_btn.hidden = !box.ref` at button creation time so the
initial render is correct.

## Relevant files

- `assets/js/004-inspector.js` — ref field row, `view_btn` setup
- `issues/completed/215-view-box-source.md` — the source viewer
  this button opens
