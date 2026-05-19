# text.lua

Bundled string-manipulation toolbox. Pure Lua, no external
dependencies. Use as `require("text")` from another Lua function, or
copy into a map's `src/` and pick the function via the file browser.

Every function outputs a single value on the box's output wire (per
the single-output rule, issue 218). Booleans cross the wire as real
JSON booleans (the Lua driver `json.encode`s the return value);
numbers cross as JSON numbers; strings as JSON strings; arrays as
JSON arrays. Downstream Lua boxes see the native types directly.

All matching is **plain string** matching — never Lua patterns.
The `find` / `prefix` / `suffix` arguments carry punctuation
without escaping. A user who wants real pattern matching can
call `string.gsub` from a regular call box.

---

## M.concat(sep, ...) → string

Joins the variadic text arguments with the given separator.

| param | type    | description                                          |
|-------|---------|------------------------------------------------------|
| sep   | string  | separator inserted between every pair of values     |
| ...   | strings | any number of text values to join                    |

Returns: a single string with all values concatenated.

The signature is variadic so the box's `text` input can be marked
variadic in the inspector and grown to any number of slots — see
issue 217 part B for the editor mechanics.

Edge cases: `sep` may be an empty string. Zero-arg call returns the
empty string.

---

## M.split(text, sep) → table

Splits `text` into parts on a literal (non-pattern) separator. Returns
a Lua array; the driver shim JSON-encodes it for transport on the
single output wire. The downstream box decodes the array.

| param | type   | description                                          |
|-------|--------|------------------------------------------------------|
| text  | string | the input string                                     |
| sep   | string | the separator to split on (literal — no pattern matching) |

Returns: an array (Lua table with integer keys) of the parts.

Edge cases:
- `nil` text → empty array
- `nil` or empty `sep` → single-element array containing the whole input
  (avoids an infinite loop and matches common library behavior)
- Separator characters that are also Lua-pattern metacharacters (`.`,
  `*`, `[`, etc.) are matched literally — `string.find` is called with
  `plain = true`.

---

## M.upper(text) → string

Upper-cased copy of `text`. `nil` text returns the empty string.

## M.lower(text) → string

Lower-cased copy of `text`. `nil` text returns the empty string.

## M.trim(text) → string

Strips leading and trailing whitespace (the `%s` class — spaces,
tabs, newlines, etc). Whitespace inside the string is preserved.

## M.replace(text, find, repl) → string

Returns `text` with every occurrence of `find` replaced by `repl`.
Plain-string match; `find` may contain any characters without
escaping. Empty `find` returns the text unchanged.

## M.replace_first(text, find, repl) → string

Same as `M.replace` but stops after the first occurrence. Two
functions instead of a flag because the editor has no boolean-port
primitive yet — picking by function name is clearer than a third
input that takes the string `"true"`.

## M.contains(text, needle) → boolean

True if `needle` appears anywhere in `text`. Empty needle returns
**false** (asking "does it contain nothing?" is meaningless;
returning true would silently short-circuit guards).

## M.starts_with(text, prefix) → boolean

True if `text` begins with `prefix`. Empty prefix returns true
(every string begins with the empty string — matches the
conventional set-theoretic answer).

## M.ends_with(text, suffix) → boolean

True if `text` ends with `suffix`. Empty suffix returns true,
same reason as `starts_with`.

## M.length(text) → integer

Number of bytes in `text` (Lua's `#` operator). For UTF-8 input
this is the byte length, not the character count — wide
characters count as multiple bytes.

## M.substring(text, start, len) → string

Slice of `text` starting at 1-indexed `start`. If `len` is
omitted, takes everything from `start` to the end. Out-of-range
positions follow `string.sub` semantics — clamped, not errored.
