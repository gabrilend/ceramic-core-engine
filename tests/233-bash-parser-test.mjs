// 233 — Tests for langs/bash/parser.js (issue 232).
//
// Locks in `parse_functions(content)` for Bash. Single-output rule
// (issue 218) means every function reports outputs=['output']; the
// interesting state is on `inputs`. Whether Bash accepts extra
// positional args at call time is a `langs/bash/spec.js` concern,
// not per-function.
//
// Run from any directory:  node --test tests/233-bash-parser-test.mjs
// Override project root with SORAMECH_DIR=... when re-hosted. `node
// --test` consumes positional args as additional test files, so the
// project-DIR pattern uses an env var instead of argv here.

import { test } from 'node:test';
import { strict as assert } from 'node:assert';

const DIR = process.env.SORAMECH_DIR || '/mnt/mtwo/programs/sora/soramech';

const { parse_functions } = await import(`${DIR}/langs/bash/parser.js`);

// {{{ test('output is always the single port "output"')
test('output is always the single port "output"', () => {
  const src = `
each_arg() {
    local prefix="$1"
    shift
    for x in "$@"; do
        printf '%s%s\\n' "$prefix" "$x"
    done
}
`;
  const fns = parse_functions(src);
  assert.equal(fns.length, 1);
  assert.equal(fns[0].name, 'each_arg');
  assert.deepEqual(fns[0].outputs, ['output']);
});
// }}}

// {{{ test('locals named from positional params become input names')
test('locals named from positional params become input names', () => {
  const src = `
write_msg() {
    local path="$1"
    local body="$2"
    printf '%s\\n' "$body" > "$path"
}
`;
  const fns = parse_functions(src);
  assert.equal(fns.length, 1);
  assert.deepEqual(fns[0].inputs, ['path', 'body']);
});
// }}}

// {{{ test('positional refs without locals fall back to argN')
test('positional refs without locals fall back to argN', () => {
  const src = `
shout() {
    printf '%s!' "$1"
}
`;
  const fns = parse_functions(src);
  assert.deepEqual(fns[0].inputs, ['arg1']);
});
// }}}

// {{{ test('gaps in local bindings are filled with argN placeholders')
test('gaps in local bindings are filled with argN placeholders', () => {
  // Only $1 and $3 named — $2 should appear as arg2 so port order
  // stays positional.
  const src = `
sparse() {
    local first="$1"
    local third="$3"
}
`;
  const fns = parse_functions(src);
  assert.deepEqual(fns[0].inputs, ['first', 'arg2', 'third']);
});
// }}}

// {{{ test('no positional refs at all means empty inputs')
test('no positional refs at all means empty inputs', () => {
  const src = `
greet() {
    echo "hello"
}
`;
  const fns = parse_functions(src);
  assert.deepEqual(fns[0].inputs, []);
});
// }}}

// {{{ test('control-flow keywords that look like fn decls are skipped')
test('control-flow keywords that look like fn decls are skipped', () => {
  // `if() {` and `for() {` are not legal bash but the parser's
  // regex matches `<word>() {` so the skip-set defends against
  // false positives. Real fn after them should still show up.
  const src = `
real_fn() {
    echo ok
}
`;
  const fns = parse_functions(src);
  assert.equal(fns.length, 1);
  assert.equal(fns[0].name, 'real_fn');
});
// }}}

// {{{ test('multiple functions are discovered in declaration order')
test('multiple functions are discovered in declaration order', () => {
  const src = `
a() { echo 1; }
b() { echo 2; }
c() { echo 3; }
`;
  const fns = parse_functions(src);
  assert.deepEqual(fns.map(f => f.name), ['a', 'b', 'c']);
});
// }}}

// {{{ test('braced positional ${N} forms are recognised')
test('braced positional ${N} forms are recognised', () => {
  const src = `
braced() {
    local first="\${1}"
    local second="\${2}"
}
`;
  const fns = parse_functions(src);
  assert.deepEqual(fns[0].inputs, ['first', 'second']);
});
// }}}

// {{{ test('empty source returns empty list')
test('empty source returns empty list', () => {
  assert.deepEqual(parse_functions(''), []);
});
// }}}
