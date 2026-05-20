// langs/bash/spec.js — editor-facing language metadata. See
// langs/lua/spec.js for the rationale; this file is its Bash twin.
//
// Bash's calling convention exposes all positional args via "$@"
// regardless of whether the function references them. Every function
// can be called with N args; the function's source decides what to
// do with them.

export const LANGUAGE_SPEC = {
  name: 'bash',
  file_ext: '.sh',
  variadic_shape: 'positional',
};
