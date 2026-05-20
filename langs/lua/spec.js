// langs/lua/spec.js — editor-facing language metadata.
//
// The C side of the spec (spec.c) tells the pool runner how to
// load and invoke Lua functions. This file tells the *editor* the
// language-level facts the UI needs — things that are true of
// every function in the language, not facts about any particular
// function's source.
//
// `variadic_shape` answers: how does this language accept "extra"
// positional arguments at call time? "positional" means every
// function can be called with N args, period — the language's
// calling convention drops or exposes extras as it sees fit. For
// Lua that's literally always: `function f(a)` called with five
// args silently keeps the first and `...` would have captured the
// rest. The fact that the *source* doesn't write `...` is the
// function author's choice, not a runtime constraint.
//
// The editor reads this constant to decide whether the inspector's
// `var` toggle is offered for ports on a Lua box. The toggle is a
// data-model affordance for grouping N inputs onto one logical
// port — not a guarantee that the function will use them. Mis-wire
// is the user's call; the editor's job is to show what the source
// declares (via parser.js) so the user can see the arity they're
// working with.

export const LANGUAGE_SPEC = {
  name: 'lua',
  file_ext: '.lua',
  variadic_shape: 'positional',
};
