// 232 — Tests for langs/lua/parser.js (issue 232).
//
// Locks in the contract `parse_functions(content)` is expected to
// hold: it returns an ordered array of `{name, inputs, outputs}`
// records for every `function M.<name>(<params>)` in the source.
// The trailing `...` is stripped from `inputs` (the editor's port
// list shows only named parameters); whether the language accepts
// extra positional args is a `langs/<name>/spec.js` concern, not
// per-function.
//
// Run from any directory:  node --test tests/232-lua-parser-test.mjs
// Override project root with SORAMECH_DIR=... when re-hosted. `node
// --test` consumes positional args as additional test files, so the
// project-DIR pattern uses an env var instead of argv here.

import { test } from 'node:test';
import { strict as assert } from 'node:assert';

const DIR = process.env.SORAMECH_DIR || '/mnt/mtwo/programs/sora/soramech';

const { parse_functions } = await import(`${DIR}/langs/lua/parser.js`);

// {{{ test('trailing ... is stripped from inputs')
test('trailing ... is stripped from inputs', () => {
  const src = `
function M.concat(sep, ...)
    return table.concat({ ... }, sep or "")
end
`;
  const fns = parse_functions(src);
  assert.equal(fns.length, 1);
  assert.equal(fns[0].name, 'concat');
  assert.deepEqual(fns[0].inputs, ['sep']);
});
// }}}

// {{{ test('plain function exposes its named parameters')
test('plain function exposes its named parameters', () => {
  const src = `
function M.split(text, sep)
    return parts
end
`;
  const fns = parse_functions(src);
  assert.equal(fns.length, 1);
  assert.equal(fns[0].name, 'split');
  assert.deepEqual(fns[0].inputs, ['text', 'sep']);
  assert.deepEqual(fns[0].outputs, ['parts']);
});
// }}}

// {{{ test('parameterless function has empty inputs')
test('parameterless function has empty inputs', () => {
  const src = `
function M.no_args()
    return 1
end
`;
  const fns = parse_functions(src);
  assert.equal(fns.length, 1);
  assert.deepEqual(fns[0].inputs, []);
});
// }}}

// {{{ test('bare identifiers in return become named outputs')
test('bare identifiers in return become named outputs', () => {
  const src = `
function M.pair(a, b)
    return x, y
end
`;
  const fns = parse_functions(src);
  assert.deepEqual(fns[0].outputs, ['x', 'y']);
});
// }}}

// {{{ test('string concat in return collapses to result')
test('string concat in return collapses to result', () => {
  const src = `
function M.greet(name)
    return "hello, " .. name
end
`;
  const fns = parse_functions(src);
  assert.deepEqual(fns[0].outputs, ['result']);
});
// }}}

// {{{ test('nested blocks do not confuse end-matching')
test('nested blocks do not confuse end-matching', () => {
  const src = `
function M.outer(x)
    if x > 0 then
        for i = 1, x do
            if i == 2 then
                local y = i
            end
        end
    end
    return result
end

function M.after(z)
    return z
end
`;
  const fns = parse_functions(src);
  assert.equal(fns.length, 2);
  assert.equal(fns[0].name, 'outer');
  assert.equal(fns[1].name, 'after');
  assert.deepEqual(fns[1].inputs, ['z']);
});
// }}}

// {{{ test('repeat/until counts as a block')
test('repeat/until counts as a block', () => {
  const src = `
function M.loop(n)
    repeat
        n = n - 1
    until n == 0
    return n
end
`;
  const fns = parse_functions(src);
  assert.equal(fns.length, 1);
  assert.equal(fns[0].name, 'loop');
  assert.deepEqual(fns[0].outputs, ['n']);
});
// }}}

// {{{ test('empty source returns empty list')
test('empty source returns empty list', () => {
  assert.deepEqual(parse_functions(''), []);
});
// }}}

// {{{ test('functions not on M are ignored')
test('functions not on M are ignored', () => {
  const src = `
local function private(x) return x end
function helper(y) return y end
function M.public(z) return z end
`;
  const fns = parse_functions(src);
  assert.equal(fns.length, 1);
  assert.equal(fns[0].name, 'public');
});
// }}}

// {{{ test('multiple functions preserve declaration order')
test('multiple functions preserve declaration order', () => {
  const src = `
function M.a() return 1 end
function M.b() return 2 end
function M.c() return 3 end
`;
  const fns = parse_functions(src);
  assert.deepEqual(fns.map(f => f.name), ['a', 'b', 'c']);
});
// }}}
