# text.lua

Text manipulation helpers — concat and split. Pure Lua, no external
dependencies. Use as `require("text")` from another Lua function, or
copy into a map's `src/` and pick the function via the file browser.

Both functions output a single value on the box's output wire (per
the single-output rule, issue 218).

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
