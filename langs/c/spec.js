// langs/c/spec.js — editor-facing language metadata. See
// langs/lua/spec.js for the rationale; this file is its C twin.
//
// C functions in SoraMech follow the dispatch wrapper convention
// (issue 304): every function receives `(input_data, input_sizes,
// n_inputs, out_buf, out_capacity, out_size)`. That's universal
// argc/argv — the function's source decides whether to walk the
// full array or only the leading entries.

export const LANGUAGE_SPEC = {
  name: 'c',
  file_ext: '.c',
  variadic_shape: 'positional',
};
