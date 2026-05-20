// 234 — Tests for langs/<name>/spec.js (issue 232).
//
// The editor-facing language metadata is one constant per language,
// not a per-function detection. This test asserts each shipped
// language declares its variadic shape so the editor's `var` toggle
// can be gated by language, not by source parsing. See issue 217's
// rewrite for the rationale.
//
// Run from any directory:  node --test tests/234-language-spec-js-test.mjs

import { test } from 'node:test';
import { strict as assert } from 'node:assert';

const DIR = process.env.SORAMECH_DIR || '/mnt/mtwo/programs/sora/soramech';

const VALID_SHAPES = new Set(['positional', 'none']);

// {{{ check_spec(language, expected_ext)
async function check_spec(language, expected_ext) {
  const mod = await import(`${DIR}/langs/${language}/spec.js`);
  const spec = mod.LANGUAGE_SPEC;
  assert.ok(spec, `LANGUAGE_SPEC export missing for ${language}`);
  assert.equal(spec.name, language);
  assert.equal(spec.file_ext, expected_ext);
  assert.ok(VALID_SHAPES.has(spec.variadic_shape),
    `unknown variadic_shape "${spec.variadic_shape}" for ${language}`);
}
// }}}

// {{{ test('lua spec declares positional variadic')
test('lua spec declares positional variadic', async () => {
  await check_spec('lua', '.lua');
  const { LANGUAGE_SPEC } = await import(`${DIR}/langs/lua/spec.js`);
  assert.equal(LANGUAGE_SPEC.variadic_shape, 'positional');
});
// }}}

// {{{ test('bash spec declares positional variadic')
test('bash spec declares positional variadic', async () => {
  await check_spec('bash', '.sh');
  const { LANGUAGE_SPEC } = await import(`${DIR}/langs/bash/spec.js`);
  assert.equal(LANGUAGE_SPEC.variadic_shape, 'positional');
});
// }}}

// {{{ test('c spec declares positional variadic')
test('c spec declares positional variadic', async () => {
  await check_spec('c', '.c');
  const { LANGUAGE_SPEC } = await import(`${DIR}/langs/c/spec.js`);
  assert.equal(LANGUAGE_SPEC.variadic_shape, 'positional');
});
// }}}
