# Transcript

## User

hello claude. Today, we're going to close out every single one of these issues. Then, we're going to go through the completed issue files one by one, and justify their existence. If they cannot be justified, they will be updated until they match the correct design. Then, we will hand them off to another project who will attempt to re-create what we've built from scratch using only the issue files as a guide. If they succeed, we will know that our issue files are in the correct format. If they fail, then either they did not fill in the gaps between issue files correctly, or the issue files are too clunky, obfuscated by technical details, or weighted down by development inertia that is of no use to a set of fresh eyes, and is simply there to justify our actions. Not ideal. In any of those cases, we should attempt to re-write the issue files until the design is solid. The issue files are the product, the source-code and implementation is just how we prove that it works.

## Assistant

Understood. Before I start closing things, let me get a clear picture of what's open and what's already completed, and remind myself of the project's current shape.

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/issues/ 2>/dev/null | head -100`

### Tool result

```
217-variadic-ports-and-language-spec-shape.md
231-move-signature-parsers-into-langs.md
232-test-coverage-map.md
233-unified-routing-schema.md
239-literal-port-shrinks-to-nodule-with-name-on-left.md
301-pool-lifecycle-and-worker-init.md
302-wire-value-slot-store.md
303-language-runtime-spec.md
304-task-dispatch-layer.md
305-c-graph-loader.md
306-lua-language-spec.md
307-c-language-spec.md
308-bash-language-spec.md
309-build-system.md
310-priority-queue.md
311-integration-tests-and-run-output.md
312-same-language-wire-fast-path.md
313-research-whole-program-same-language-merge.md
314-c-json-parser.md
315-compiled-artifact-reference-counting.md
317-spec-json-bridge-for-data-boxes.md
completed
phase-1-progress.md
phase-2-progress.md
phase-3-progress.md
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/issues/completed/ 2>/dev/null | head -100`

### Tool result

```
101-project-scaffold-and-map-format.md
102-language-driver-interface.md
103-runner-graph-loader-and-validator.md
104-runner-synchronous-executor.md
105-http-server-file-crud.md
106-web-editor-canvas-and-box-rendering.md
107-web-editor-wiring-and-connections.md
108-branch-box-and-predicate-routing.md
109-data-files-persistent-and-ephemeral.md
110-phase-1-demo.md
111-bash-output-format-and-driver-test-entry.md
201-fix-box-rendering-closure-bug.md
202-fit-to-view-on-map-load.md
203-map-picker-panel.md
204-visible-interaction-hints.md
206-entry-box-designation.md
207-source-file-browser-and-port-auto-population.md
208-port-literal-values.md
209-ollama-query-library.md
210-llm-route-box.md
211-file-browser-library-dirs.md
211a-unify-src-dirs-pipeline.md
212-editor-interaction-modes.md
213-queued-inputs-and-task-model.md
214-tmp-symlink-recreation-on-reboot.md
215-view-box-source.md
216-read-file-box.md
218-single-output-driver-contract.md
219-map-compiler.md
220-root-run-script-and-entry-cleanup.md
221-iterator-box.md
222-compile-button-and-assets-directory.md
223-syntax-highlighting-via-user-lexers.md
224-editable-port-names-and-canvas-value-edit.md
225-dir-picker-show-files-and-icons.md
226-hide-output-controls-for-no-return-functions.md
227-hide-view-button-without-ref.md
228-self-loop-wires-routed-around-box.md
229-data-box-as-language-agnostic-file-io.md
230-required-inputs-and-optional-flag.md
234-middle-click-duplicate-box.md
235-literal-value-replaces-port-name-on-canvas.md
236-box-header-filename-output-shows-call-form.md
237-string-manipulation-library-expansion.md
238-editor-http-server-config-file.md
316-simple-test-runner.md
demos
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/`

### Tool result

```
Makefile
assets
build
config
demo.sh
docs
drivers
issues
langs
libs
llm-transcripts
maps
notes
run
scripts
soramech-pool
src
tests
```

## Assistant

I see 24 open issues across phases 2 and 3, and ~50 already completed. Before I start, I want to make sure I'm reading the scope right, because "close every one of these" can mean several different things:

1. **Implement-and-close**: actually finish the work each issue describes (some, like 303-language-runtime-spec or 309-build-system, are foundational and not small).
2. **Audit-and-close**: many of these may already be partly or fully implemented in the codebase — the issue just hasn't been moved. Walk each one, verify against the code, and only do the real work where something is genuinely missing.
3. **Rewrite-as-blueprint**: treat each open issue as a *design document* first, refine it until a fresh reader could rebuild from it, and only then implement.

I'd lean toward (2) as the first pass — go through the 24 open issues in dependency order, check whether the described behavior already exists, and either move-to-completed or fill the gap. That keeps us honest about what's actually built before we move into the audit phase. The risk is that some "obviously done" items are missing the design-document quality you want for the handoff, so we'd still have to revisit them in the audit phase.

A couple of questions before I move:

- Of the open issues, are there any you already know are *done in code* and just need verifying + moving? (Saves me reading every issue cold.)
- For the audit phase — when you say "justify their existence," do you mean each completed issue must stand alone as a buildable spec, or that the *set* of them in order must rebuild the project? Those are different bars.
- Is the "another project who will attempt to re-create" a real downstream consumer (another agent run? a human?), or a thought experiment we're stress-testing against?

## User

we want to implement and close. The phase 3 issues WERE up to date and correct before we started implementing them, so feel free to check the git log to find out their original shape before they started being completed. Phase 2 is mostly deprecated, but we should still implement the issue files from that phase first, just to have a stable base. I anticipate we're going to be pruning a lot of code today, because we've built a lot of systems that are grown into little shapes that work for what they're doing, but need to be replaced with larger, more broadly applicable systems that are clean and coherent and can be applied to more than just the little implementation we built.

  - Of the open issues, are there any you already know are done in code and just need verifying + moving? (Saves me reading every issue cold.)

why do you want to save yourself from reading each of these issue files? Isn't it better to know the entire scope? Are you worried about them corrupting your context somehow?

  - For the audit phase — when you say "justify their existence," do you mean each completed issue must stand alone as a buildable spec, or that the set of them in order must rebuild the project? Those are different bars.

the set of them must rebuild the project. They are encouraged to have dependencies.

  - Is the "another project who will attempt to re-create" a real downstream consumer (another agent run? a human?), or a thought experiment we're stress-testing against?

I will literally copy the issue files to a new directory and tell a downstream consumer to re-create the system. That's all we need to know for now.

## Assistant

You're right — that was lazy framing on my part. The issue files *are* the product; treating reading them as a cost rather than as the core work inverts the priority. Let me read all 24 cold, and pull the phase-3 originals from git so I see them in their intended shape before implementation drift.

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/217-variadic-ports-and-language-spec-shape.md`

### Tool result

```
1	# 217 — Variadic ports and the language-spec variadic shape
2	
3	## Status
4	open — Part A and Part B mechanics are in the codebase; Part C is
5	the active design question
6	
7	## What this issue is now
8	
9	The original issue was titled "Concat box with per-input variadic
10	ports and auto-grow/shrink." That framing has aged out:
11	
12	- There is no dedicated **concat box kind**. `M.concat(sep, ...)`
13	  lives in `libs/text.lua` and is invoked by any call box that
14	  imports it. The variadic UI work was never specific to concat.
15	- The original "split returns a JSON-encoded array on the single
16	  output wire" story was a phase-2 transport assumption. Phase 3
17	  (issue 312, same-language wire fast path) classifies each edge
18	  and only JSON-encodes when crossing a language boundary. So
19	  `M.split` returning a Lua array is correct in-language and the
20	  runtime bridges if the downstream box is a different language.
21	
22	The issue is now about the **editor's variadic-port mechanic** and
23	how it interacts with the **language spec** — specifically, what
24	the editor's `var` toggle means and what governs whether it's
25	offered.
26	
27	## Part A — `libs/text.lua` *(shipped)*
28	
29	`libs/text.lua` ships `M.concat(sep, ...)`, `M.split(text, sep)`,
30	and (via issue 237) `upper`, `lower`, `trim`, `replace`,
31	`replace_first`, `contains`, `starts_with`, `ends_with`, `length`,
32	`substring`. All are pure string operations consumed by call
33	boxes; no special box kind exists.
34	
35	`M.split` is most useful paired with the iterator box (issue 221)
36	so each part fans out to its own dispatch. Without the iterator
37	it's just "an array on a wire" — legitimate but flat.
38	
39	## Part B — Editor variadic data model *(shipped)*
40	
41	A box JSON can carry a `variadic_inputs` array of base names; slots
42	are stored as `<base>_<index>` in the `inputs` array. The inspector
43	(`assets/js/004-inspector.js`) implements:
44	
45	- `make_variadic(port_name)` — turn a plain port into a one-slot
46	  variadic group (`text` → `text_0`).
47	- `unmake_variadic(base)` — collapse back to a single port.
48	- `add_variadic_slot(base)` — append `<base>_<last+1>` (auto-grow).
49	- `remove_variadic_slot(slot_name)` — delete and renumber.
50	- `auto_grow_after_set(box, slot_name)` — append a new empty slot
51	  when the user uses the current last slot.
52	
53	Auto-grow triggers on wire-connect and on value-set. Shrinking is
54	explicit-only — `×` button per slot. Wire-disconnect and value-
55	clear do **not** shrink, deliberately, so transient editing
56	doesn't lose structure.
57	
58	The schema (`src/001-schema.lua`) accepts `variadic_inputs` as an
59	optional array of strings.
60	
61	## Part C — What `var` actually means *(active design)*
62	
63	### Wrong model (rejected)
64	
65	The earlier framing said: regex-parse each source file in the
66	editor, mark functions whose signatures end in `...` / `"$@"` /
67	`va_list` as "variadic-capable," and gate the `var` toggle on
68	that per-function flag.
69	
70	That model collapses on contact with the project's vision:
71	
72	1. **It's not a per-function fact.** Every Lua function accepts N
73	   positional args. `function f(a)` called with five args silently
74	   keeps the first; `...` in the signature is the *author's choice
75	   to capture extras*, not a runtime constraint. Bash is the same
76	   — every function gets `"$@"` whether it touches it or not. C
77	   through the dispatch wrapper (issue 304) is the same —
78	   `argc`/`argv` is the universal call shape.
79	2. **It centralises language knowledge in the editor.** Per-
80	   language regex parsers in the editor are exactly the
81	   ossification the `langs/<name>/` model exists to prevent.
82	   Adding a new language would mean writing a new regex.
83	3. **It locks the editor to a static source view.** A function the
84	   user is in the middle of editing might briefly not have `...`
85	   in the signature; the editor would flicker the toggle on/off.
86	
87	### Right model: variadic shape is a language fact
88	
89	The language declares its calling-convention shape once, in
90	`langs/<name>/spec.js`:
91	
92	```js
93	export const LANGUAGE_SPEC = {
94	  name: 'lua',
95	  file_ext: '.lua',
96	  variadic_shape: 'positional',
97	};
98	```
99	
100	`variadic_shape: 'positional'` means the language accepts N
101	positional args at call time, period. Every shipped language today
102	(Lua, Bash, C) declares this. Future languages with different
103	shapes (Python `**kwargs`, streaming generators, tagged unions)
104	declare their own shape — the editor's variadic UI eventually
105	grows variants for them, but `positional` covers everything we
106	ship.
107	
108	A language that genuinely cannot accept variadic input declares
109	`variadic_shape: 'none'`, and the editor disables the `var` toggle
110	for all its boxes. We don't have one of those today.
111	
112	### Editor behaviour under this model
113	
114	- The `var` toggle on a port is gated by **language**, not by
115	  function. For every Lua / Bash / C box today, the toggle is
116	  available.
117	- The user is responsible for matching wired arity to the
118	  function's intent. The editor surfaces what the source declares
119	  (via `parser.js`'s named parameters) so the user can see the
120	  arity the author wrote. If they wire five inputs to a function
121	  whose body only reads the first, that's a user choice — same
122	  class as wiring a string into a port the function treats as a
123	  number. The runtime doesn't police it.
124	- Compile-time (issue 309) and validator (issue 002) checks
125	  surface obvious mismatches as documentation, not as gates.
126	
127	### What `parser.js` does and doesn't do
128	
129	`langs/<name>/parser.js` continues to enumerate functions in a
130	source file and extract named parameter and return names. That
131	feeds the file browser. It does **not** compute any per-function
132	variadic property — the early `variadic_tail` field was removed
133	in this issue's pass.
134	
135	### Implementation status
136	
137	- `langs/lua/spec.js`, `langs/bash/spec.js`, `langs/c/spec.js` —
138	  shipped. Each exports `LANGUAGE_SPEC` with `variadic_shape:
139	  'positional'`.
140	- `langs/lua/parser.js`, `langs/bash/parser.js` — shipped without
141	  `variadic_tail`. Still produce inputs/outputs for the file
142	  browser.
143	- `tests/234-language-spec-js-test.mjs` — shipped. Asserts every
144	  spec.js exports a valid `LANGUAGE_SPEC` with a known
145	  `variadic_shape`.
146	- Inspector consumption of `LANGUAGE_SPEC.variadic_shape` —
147	  **not yet wired**. Today the `var` toggle is offered
148	  unconditionally; the gate-by-language step is the remaining
149	  work.
150	
151	### Remaining work
152	
153	1. `assets/js/004-inspector.js::mk_port_display` — load the active
154	   box's language spec (the same dynamic-import pattern used for
155	   lexers), and hide the `var` button when
156	   `variadic_shape !== 'positional'`. Today every shipped language
157	   is positional, so this is a future-proofing step rather than a
158	   visible change.
159	2. Inspector renders the parsed signature near the box header so
160	   the user always sees the function's declared arity. This is the
161	   documentation half of the design — replaces the rejected "gate
162	   the toggle per function" approach with "show the arity, let the
163	   user judge."
164	3. Update `docs/005-language-specs.md` with a "language spec for
165	   the editor" section pointing at `spec.js` as the canonical
166	   home for editor-facing language facts (variadic shape, future
167	   additions).
168	
169	## Future development ideas
170	
171	### Variadic shapes that aren't `positional`
172	
173	- `keyed` — Python `**kwargs`, Ruby keyword splats. Slot model is
174	  `(key, value)` pairs, user labels per slot, not auto-numbered.
175	- `tagged` — Rust enum lists, Haskell heterogeneous lists. Per-
176	  slot `(tag, value)`.
177	- `stream` — generators, channels, iterators. One slot, queued.
178	  Closer to issue 213's queued-input model than to positional
179	  variadic.
180	
181	Each shape has its own auto-grow rule and inspector controls. The
182	language spec declares which shape applies; the editor renders
183	accordingly. None of these are shipped — the positional shape
184	covers every language so far. Captured here so the design door
185	stays open.
186	
187	### Outputs
188	
189	The single-output rule (issue 218) is unchanged. Variadic is an
190	input-side concern.
191	
192	## Relevant files
193	
194	- `libs/text.lua` — Part A library functions
195	- `assets/js/004-inspector.js` — Part B variadic mechanics, future
196	  home for the spec-driven `var` gate
197	- `src/001-schema.lua` — accepts `variadic_inputs`
198	- `langs/lua/spec.js`, `langs/bash/spec.js`, `langs/c/spec.js` —
199	  the language-level `variadic_shape` declarations
200	- `langs/lua/parser.js`, `langs/bash/parser.js` — function
201	  enumeration (no per-function variadic detection)
202	- `tests/234-language-spec-js-test.mjs` — spec.js contract tests
203	- `issues/231-move-signature-parsers-into-langs.md` — moved
204	  signature parsing into `langs/<name>/parser.js`
205	- `issues/312-same-language-wire-fast-path.md` — supersedes the
206	  old "JSON-encode every wire" story Part A was framed against
207	- `issues/221-iterator-box.md` — the canonical companion to
208	  `M.split` for fan-out
209	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/231-move-signature-parsers-into-langs.md`

### Tool result

```
1	# 231 — Move per-language signature parsers out of the file browser into `langs/<name>/parser.js`
2	
3	## Status
4	complete (2026-05-19) — lua + bash parsers and their editor-facing
5	language specs moved into `langs/<lang>/`, file browser is now
6	language-agnostic. C parser explicitly deferred per the original
7	step 3. Test coverage lands under issue 232.
8	
9	## Current behavior
10	
11	`assets/js/007-filebrowser.js` carries language-specific code:
12	- `parse_lua(content)` — extracts `function M.<name>(<params>)`
13	  declarations and scans for `return` statements to enumerate
14	  outputs.
15	- `parse_bash(content)` — extracts `<name>() {` declarations and
16	  uses `parse_bash_inputs` to count `$N` references.
17	- `parse_functions(content, filename)` — dispatches on file
18	  extension to one of the above.
19	
20	C has no parser yet. Languages a user might add (Python, Rust, …)
21	have no path to a parser.
22	
23	The lexer infrastructure (issue 223, complete) already establishes
24	the right pattern: per-language files under `langs/<name>/`,
25	loaded on demand by the editor through dynamic `import()`. The
26	signature parsers belong there, not in the editor.
27	
28	## Intended behavior
29	
30	Each language directory carries a `parser.js` ES module exporting
31	a single function:
32	
33	```js
34	// langs/<name>/parser.js
35	export function parse_functions(content) {
36	  return [
37	    {
38	      name: 'write_result',
39	      inputs: ['text'],
40	      outputs: [],            // empty array → sink box (issue 226)
41	      variadic_base: null,    // or 'args' for f(a, ...)
42	    },
43	    // ...
44	  ];
45	}
46	```
47	
48	The editor's file browser drops its `parse_lua` / `parse_bash` /
49	`parse_functions` and asks the per-language parser instead. Same
50	lazy-load pattern as the lexers: a registry keyed by file
51	extension, dynamic `import()` on first need, cached per session.
52	Files with an unknown extension fall back to "no parser available"
53	— the file browser displays the file name but offers no
54	function-list step.
55	
56	### Extended return shape
57	
58	The parser returns a richer record than the current shape:
59	- `name` — function name (existing).
60	- `inputs` — array of parameter names in declaration order
61	  (existing).
62	- `outputs` — array of return-value names; empty for sinks
63	  (existing).
64	- `variadic_base` — string or null. Set when the function
65	  declaration has a variadic tail (Lua `...`, Bash `"$@"`, C
66	  `va_list`). Editor uses it to auto-flip the var toggle for that
67	  port. Existing parsers don't compute this; new field, optional
68	  for parsers that don't bother.
69	
70	### Why this matters
71	
72	The file browser is the editor's single entry point for "import
73	function from source." Any language-specific logic in it is the
74	wrong level of abstraction. Moving it puts the editor on equal
75	footing with the runtime: both treat languages as pluggable, both
76	discover capabilities through `langs/<name>/`, neither has a
77	hardcoded list.
78	
79	It also makes adding a new language a self-contained job: drop
80	`langs/<name>/parser.js`, `langs/<name>/lexer.js`, and (phase 3)
81	`langs/<name>/spec.so` into `langs/`, and the editor and the
82	runner both pick it up.
83	
84	## Suggested implementation sequence
85	
86	1. Create `langs/lua/parser.js` with the existing `parse_lua` /
87	   `parse_lua_returns` logic, exporting `parse_functions`.
88	2. Create `langs/bash/parser.js` with the existing `parse_bash` /
89	   `parse_bash_inputs` logic.
90	3. Optionally create `langs/c/parser.js` — a minimal one that
91	   extracts `int name(int a, char *b)` style declarations from
92	   header files. Defer if not needed yet.
93	4. Add a parser registry to `assets/js/007-filebrowser.js` mirroring
94	   the lexer registry in `008-source-view.js`:
95	   ```js
96	   const EXT_TO_LANG = { lua: 'lua', sh: 'bash', bash: 'bash' };
97	   const parser_cache = {};
98	   async function load_parser(lang) { /* dynamic import */ }
99	   ```
100	5. Replace `parse_functions(content, filename)` body with a lookup
101	   into the registry. The function becomes async; existing callers
102	   already `await` async file fetches, so this slots in.
103	6. Delete `parse_lua`, `parse_bash`, `parse_lua_returns`,
104	   `parse_bash_inputs`, `lua_fn_end` from `007-filebrowser.js`.
105	7. (Optional, separate change) Wire `variadic_base` into the
106	   on-select handler so the editor auto-flips the var toggle.
107	
108	## Open questions
109	
110	- **Test coverage**: parsers are easy to break with a regex tweak.
111	  A small set of fixture sources (`maps/_test/parser-fixtures/*.lua`,
112	  `*.sh`) and a JS test harness would catch regressions. Out of
113	  scope for this issue, worth a follow-up.
114	- **Parsing failure shape**: a parser that throws should be
115	  reported as "parser error" not "no parser." Add a try/catch in
116	  the registry layer with a clear message.
117	
118	## Relevant files
119	
120	- `assets/js/007-filebrowser.js` — current parsers move out
121	- `langs/lua/parser.js` — new
122	- `langs/bash/parser.js` — new
123	- `langs/lua/lexer.js`, `langs/bash/lexer.js` — peer modules; same
124	  loading pattern
125	- `assets/js/008-source-view.js` — reference implementation for
126	  the lazy lexer registry
127	- `issues/completed/223-syntax-highlighting-via-user-lexers.md` —
128	  established the `langs/<name>/<file>` server-serve route and the
129	  dynamic-import loading model
130	- `docs/005-language-specs.md` — add a "parser interface" section
131	  alongside the existing "lexer interface"
132	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/232-test-coverage-map.md`

### Tool result

```
1	# 232 — Test coverage map
2	
3	## Status
4	ongoing (test inventory; updated each time tests are added or
5	removed)
6	
7	## Why this exists
8	
9	This issue is the project-wide answer to "what's tested, what
10	isn't, and what runs the tests." It folds together two earlier
11	threads:
12	
13	- **316** (now folded in here) — a cheap, build-free unit-test
14	  runner script. Its scope lives under "Test runners → Planned"
15	  below.
16	- The parser-test work that opened this issue (Node-side tests
17	  for `langs/<name>/parser.js`).
18	
19	A related but separate issue is **311** — phase-3 integration
20	tests and the JSONL run-output format. 311's scope is the JSONL
21	writer + the `tests/maps/<name>/` fixtures driven through
22	`soramech-pool`; the coverage of those fixtures shows up in the
23	"End-to-end fixtures" section below, but the writer/format work
24	stays in 311.
25	
26	Without a coverage view, gaps accumulate silently: tests get
27	written for the area someone happens to be touching, and the rest
28	drifts uncovered. The checklist below is that view, updated every
29	time tests are added or removed.
30	
31	Every time a new test file lands or a test gap is closed, the
32	checklist below is updated. When a new module is added, a row goes
33	into the appropriate section showing it has no coverage yet.
34	
35	## How to read the table
36	
37	- `[x]` — has a dedicated test file (path in the row).
38	- `[ ]` — no dedicated test; may be exercised indirectly through
39	  integration fixtures, but not directly verified.
40	- `[~]` — partially covered; only some surface area is tested.
41	- `[—]` — intentionally untested (vendored library, thin entrypoint, etc.).
42	
43	## Editor side — JavaScript
44	
45	The editor is a browser app served by `src/005-http-server.lua`.
46	No JS test runner was wired up before the work that opened this
47	issue; the parser tests introduced one (Node's built-in
48	`node:test`). Other JS modules can follow the same pattern when
49	they get tests.
50	
51	| Module | Coverage | Test file |
52	|---|---|---|
53	| `langs/lua/parser.js` | `[x]` | `tests/232-lua-parser-test.mjs` |
54	| `langs/bash/parser.js` | `[x]` | `tests/233-bash-parser-test.mjs` |
55	| `langs/c/parser.js` | `[—]` | not written yet; issue 231 makes it optional |
56	| `langs/lua/spec.js` | `[x]` | `tests/234-language-spec-js-test.mjs` |
57	| `langs/bash/spec.js` | `[x]` | `tests/234-language-spec-js-test.mjs` |
58	| `langs/c/spec.js` | `[x]` | `tests/234-language-spec-js-test.mjs` |
59	| `langs/lua/lexer.js` | `[ ]` | — |
60	| `langs/bash/lexer.js` | `[ ]` | — |
61	| `langs/c/lexer.js` | `[ ]` | — |
62	| `assets/js/001-canvas.js` | `[ ]` | — |
63	| `assets/js/002-boxes.js` | `[ ]` | — |
64	| `assets/js/003-api.js` | `[ ]` | — |
65	| `assets/js/004-inspector.js` | `[ ]` | — (variadic helpers are pure functions, prime unit-test candidates) |
66	| `assets/js/005-app.js` | `[ ]` | — |
67	| `assets/js/006-wires.js` | `[ ]` | — |
68	| `assets/js/007-filebrowser.js` | `[ ]` | — |
69	| `assets/js/008-source-view.js` | `[ ]` | — |
70	
71	Highest-value next targets: the variadic-slot helpers in
72	`004-inspector.js` (pure functions, no DOM dependency, directly
73	consumed by issue 217's Part C inspector gate) and the lexer
74	registries in the language directories.
75	
76	## Lua libraries — `libs/*.lua`
77	
78	| Module | Coverage | Test file |
79	|---|---|---|
80	| `libs/soramech-data.lua` | `[x]` | `tests/003-data-test.lua` |
81	| `libs/text.lua` | `[ ]` | — (issue 237 expanded the surface: concat, split, upper, lower, trim, replace, replace_first, contains, starts_with, ends_with, length, substring) |
82	| `libs/files.lua` | `[ ]` | — |
83	| `libs/ollama.lua` | `[—]` | requires a live Ollama endpoint; integration-level only |
84	| `libs/dkjson.lua` | `[—]` | vendored third-party |
85	
86	`libs/text.lua` is the most important gap — every function is a
87	pure string operation with obvious edge cases (empty input, nil
88	input, no match, plain vs. pattern), exactly the shape unit tests
89	catch best.
90	
91	## Lua server side — `src/*.lua`
92	
93	| Module | Coverage | Test file |
94	|---|---|---|
95	| `src/001-schema.lua` | `[ ]` | — (validates box JSON; deserves direct accept/reject tests) |
96	| `src/002-validate-map.lua` | `[ ]` | — (CLI map validator; only exercised via integration fixtures) |
97	| `src/003-loader.lua` | `[ ]` | — |
98	| `src/004-executor.lua` | `[~]` | exercised end-to-end via `tests/004-driver-test-runner.lua` and integration maps; no unit-level coverage |
99	| `src/005-http-server.lua` | `[ ]` | — (GET/PUT boxes, connections, files, source) |
100	| `src/006-server-main.lua` | `[—]` | thin entrypoint |
101	| `src/007-runner-main.lua` | `[—]` | thin entrypoint |
102	
103	Schema and validator are the highest-value gaps — they are the
104	last line of defense against malformed map files and they're pure
105	functions over JSON-shaped tables, very testable.
106	
107	## C runtime — `src/*.c`
108	
109	Best-covered area of the project; phase 3 was built test-first.
110	
111	| Module | Coverage | Test file |
112	|---|---|---|
113	| `src/008-pool-runner.c` | `[x]` | `tests/301-pool-test.c`, `tests/303-pool-spec-init-test.c` |
114	| `src/009-slot-store.c` | `[x]` | `tests/009-slot-store-test.c` |
115	| `src/010-graph-loader.c` | `[x]` | `tests/010-graph-loader-test.c` |
116	| `src/011-spec-registry.c` | `[x]` | `tests/011-spec-registry-test.c` |
117	| `src/012-dispatch.c` | `[x]` | `tests/012-dispatch-test.c` |
118	| `src/013-jsonl-events.c` | `[x]` | `tests/013-jsonl-events-test.c` |
119	| `src/014-event-queue.c` | `[x]` | `tests/014-event-queue-test.c` |
120	| `src/015-large-value-heap.c` | `[x]` | `tests/015-large-value-heap-test.c` |
121	| `libs/json/json.c` | `[x]` | `tests/314-json-test.c` |
122	
123	## Language specs — `langs/<name>/spec.c`
124	
125	| Spec | Coverage | Test file |
126	|---|---|---|
127	| `langs/lua/spec.c` | `[x]` | `tests/306-lua-spec-test.c` |
128	| `langs/c/spec.c` | `[x]` | `tests/307-c-spec-test.c` |
129	| `langs/bash/spec.c` | `[x]` | `tests/308-bash-spec-test.c` |
130	
131	## End-to-end fixtures — `tests/maps/`
132	
133	Integration coverage owned by issue 311. Listed here so the map
134	isn't blind to it.
135	
136	| Fixture | What it exercises |
137	|---|---|
138	| `calc` | single Lua box |
139	| `hello` | data → Lua, string output |
140	| `comparator` | comparator branching (lt/eq/gt) |
141	| `iter-route` | iterator + routing |
142	| `pipeline` | five-box multilang chain (also covers the compile pipeline, issue 309) |
143	
144	## Test runners
145	
146	### Today
147	- `scripts/run-tests.sh` — integration fixtures + the JS parser
148	  tests added by the work that opened this issue. The single entry
149	  point for "run everything the build doesn't run." Folds Node-test
150	  results into the same pass/fail counter as the integration checks.
151	- `make test` — builds the C unit-test binaries, runs them, then
152	  invokes `scripts/run-tests.sh`.
153	
154	### Planned — `scripts/run-unit-tests.sh` (folded in from issue 316)
155	
156	A cheap, build-free runner for the C unit-test binaries. `make
157	test` builds and runs everything; that's fine for "ship it"
158	verification, awkward as a development loop:
159	
160	- The build phase prints `→ src/foo.c` for every file, dozens of
161	  lines before the first test runs on a clean tree.
162	- Tests run inside a Make recipe; one binary failing aborts the
163	  recipe, so later binaries that would have run aren't reported.
164	- Unit-test and integration-test output interleave in one stream.
165	
166	Goals for the new script:
167	
168	1. **Cheap.** No make recursion, no spec rebuilds, no integration
169	   tests. Under a second on a current build.
170	2. **Continues past failures.** Every binary runs; summary at the
171	   bottom names what went wrong.
172	3. **Pretty by default, mineable on request.** Per-binary one-line
173	   status by default; `--quiet` prints only totals; `--verbose`
174	   dumps each binary's full output.
175	4. **Filterable.** `scripts/run-unit-tests.sh 009` runs only
176	   binaries whose names start with `009`.
177	
178	Concept: glob `build/tests/<name>-test`, sort by filename (which
179	sorts by the project's leading numeric index), run each in turn,
180	capture stdout/stderr, parse the binary's trailing
181	`N passed, M failed` line for the verdict.
182	
183	Output shape:
184	```
185	  009-slot-store-test                          16 / 16  ok
186	  010-graph-loader-test                        13 / 13  ok
187	  015-large-value-heap-test                     8 /  8  ok
188	  ...
189	  113 / 113 tests passed (14 binaries)
190	```
191	
192	A failing binary prints its captured output below the summary
193	line. Exit code is zero iff every binary passes; non-zero with the
194	count of failing binaries otherwise.
195	
196	Convention compliance per CLAUDE.md: hard-coded `${DIR}` at top
197	with arg override, all paths under `${DIR}`, each shell command on
198	its own line, lives at `scripts/run-unit-tests.sh`.
199	
200	Open question (from 316): should the script warn when a binary's
201	source is newer than the binary? Cheap (`stat` both sides), but
202	the default should be "just run what's built" — offer it as
203	`--check-stale`.
204	
205	### Implementation steps for the new runner
206	1. Write `scripts/run-unit-tests.sh` with the layout above.
207	2. Verify it picks up every existing `build/tests/*-test` binary
208	   and produces the expected totals.
209	3. Verify `scripts/run-unit-tests.sh 015` filters to one binary.
210	4. Verify `--quiet` and `--verbose` produce useful output.
211	5. Provoke a failure (temporarily modify one test) and confirm
212	   the failure is reported, exit code non-zero, captured output
213	   visible.
214	6. Add a `make quicktest` alias or document the script in the
215	   Makefile's `help` target.
216	
217	## Suggested next test additions (ranked)
218	
219	1. **`libs/text.lua`** — pure-Lua tests next to
220	   `tests/003-data-test.lua`. Issue 237's expansion left a wide
221	   untested surface and consumers (issue 217 part C, issue 224)
222	   are about to multiply.
223	2. **`src/001-schema.lua`** — accept/reject tests for box JSON.
224	   Cheap, and the schema gates everything the executor sees.
225	3. **Inspector variadic helpers** (`assets/js/004-inspector.js`) —
226	   `parse_variadic_name`, `is_variadic_slot`, `variadic_slots_for`,
227	   `last_variadic_index`, `make_variadic`, `unmake_variadic`,
228	   `add_variadic_slot`, `remove_variadic_slot`. All pure, all
229	   feeding into the part-C work.
230	4. **Lexer round-trips** for the three shipped languages — feed a
231	   known source through `tokenize`, check the token stream.
232	5. **`langs/c/parser.js`** — write it and its tests together when
233	   issue 231 commits to a C parser.
234	
235	## Implementation log
236	
237	### 2026-05-19 — parser tests + Node runner integration
238	
239	`tests/232-lua-parser-test.mjs` and `tests/233-bash-parser-test.mjs`
240	added under Node's built-in `node:test`. They run via
241	`scripts/run-tests.sh`, which gained a `parser_tests()` section
242	that calls `SORAMECH_DIR="$DIR" node --test "$tf"` for each file
243	and folds the result into the existing pass/fail summary.
244	
245	`SORAMECH_DIR=...` overrides the project root when running outside
246	the default install path. Argv would collide with `node --test`'s
247	file-list parsing, so an env var is the right channel.
248	
249	Verified failure detection by temporarily inverting a parser
250	check — the relevant test failed as expected and the failure
251	surfaced through the shell runner.
252	
253	### 2026-05-19 — language-spec test, variadic_tail retired
254	
255	The earlier pass added a `variadic_tail` field to both parsers
256	along with assertions in the parser tests. That model — per-
257	function detection of variadic capability by source regex — was
258	fundamentally wrong for the project's vision (every shipped
259	language already accepts N positional args at call time; "is this
260	function variadic" is a question the runtime doesn't care about).
261	See issue 217's rewrite for the full reasoning.
262	
263	The fix in this pass:
264	
265	- `variadic_tail` removed from `langs/lua/parser.js` and
266	  `langs/bash/parser.js`. The parsers still extract named
267	  parameters and outputs; the `...` token is still stripped from
268	  inputs so the editor shows only named ports.
269	- `tests/232-lua-parser-test.mjs` and
270	  `tests/233-bash-parser-test.mjs` had their variadic_tail
271	  assertions removed; they now lock the trimmed contract.
272	- `langs/lua/spec.js`, `langs/bash/spec.js`, `langs/c/spec.js` —
273	  new. Each exports `LANGUAGE_SPEC` with `variadic_shape:
274	  'positional'`. This is the canonical home for editor-facing
275	  language metadata.
276	- `tests/234-language-spec-js-test.mjs` — new. Asserts every
277	  spec.js exports a valid `LANGUAGE_SPEC` with a known
278	  `variadic_shape`.
279	
280	10 / 10 passing in the full runner.
281	
282	## Relevant files
283	
284	- `scripts/run-tests.sh` — the integration + JS test driver
285	- `tests/232-lua-parser-test.mjs`, `tests/233-bash-parser-test.mjs`
286	- `issues/completed/311-integration-tests-and-run-output.md` if
287	  closed by then, otherwise `issues/311-…` — phase-3 integration
288	  tests and JSONL format. Separate concern; its fixtures appear in
289	  the End-to-end section here for coverage tracking only.
290	- `issues/completed/316-simple-test-runner.md` — folded into this
291	  issue's "Test runners → Planned" section.
292	- `issues/231-move-signature-parsers-into-langs.md` — moved the
293	  parsers under test into `langs/<name>/parser.js`.
294	- `issues/217-concat-box-and-dynamic-inputs.md` — part C consumes
295	  the `variadic_tail` field these tests guard.
296	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/233-unified-routing-schema.md`

### Tool result

```
1	# 233 — Unified `routing` schema for branching boxes
2	
3	## Status
4	open
5	
6	## Concept
7	
8	A "branching box" is a regular call box whose output, after the
9	function runs, is sent to **one of several** downstream
10	destinations rather than to all of them. Same function, same
11	inputs, same output value — only the routing decision differs.
12	Currently we have two flavors:
13	
14	- **Comparator**: routes the function's output by comparing it
15	  to a stored threshold. Three fixed branches (`lt` / `eq` /
16	  `gt`).
17	- **Iterator**: routes the function's output to one of N branches
18	  in round-robin fashion, advancing a counter on every call.
19	
20	The two are the same shape (function runs → output gets routed)
21	with different rules for picking the branch. The current schema
22	has them as separate top-level fields (`comparand` and
23	`iterator_outputs`), which makes them look like unrelated
24	features. They aren't.
25	
26	(For "send the same value to multiple destinations," the user
27	just pulls multiple wires out of a single output port — fan-out
28	already works on plain call boxes. No routing decision needed.)
29	
30	### Branch-level fan-out
31	
32	Fan-out applies on a per-branch basis even for branched boxes. If
33	the comparator's `lt` port has three wires going to three
34	different consumers, then each time the routing decision picks
35	`lt`, all three wires receive the value (a copy is pushed into
36	each consumer's input slot). On a subsequent invocation that
37	picks `eq`, only the wires hanging off the `eq` port fire.
38	
39	Each branch port acts like its own little plain-call output port
40	when it fires. The routing decision picks **which branch** fires;
41	the branch's own fan-out picks **which consumers** receive the
42	value when it does.
43	
44	## Intended behavior
45	
46	Every call box carries a `routing` field. No implicit defaulting:
47	plain boxes have `routing: { "kind": "plain" }`, the same way
48	branched boxes carry their own kind. Explicit and consistent —
49	no "what if I forget the routing field" surprises at validation
50	time.
51	
52	```json
53	{ "id": "plain-call",
54	  "kind": "call",
55	  "ref": "...", "fn": "...",
56	  "inputs": [...],
57	  "routing": { "kind": "plain" } }
58	
59	{ "id": "score-router",
60	  "kind": "call",
61	  "ref": "src/score.lua", "fn": "score",
62	  "inputs": [...],
63	  "routing": { "kind": "comparator", "comparand": 4 } }
64	
65	{ "id": "round-robin",
66	  "kind": "call",
67	  "ref": "src/dispatch_msg.lua", "fn": "dispatch_msg",
68	  "routing": { "kind": "iterator", "n_outputs": 3 } }
69	```
70	
71	Routing kinds defined in this design: `plain`, `comparator`,
72	`iterator`, `randomizer`, `weighted`, `distributor`. Of those,
73	**this issue ships `plain`, `comparator`, and `iterator` only**
74	— that's the surface area already in production, just unified
75	under one schema. The other three (`randomizer`, `weighted`,
76	`distributor`) are designed below for context but split into
77	their own follow-on issue files when this one is implemented, so
78	each ships independently.
79	
80	The `routing.kind` field picks which dispatch-layer rule decides
81	the branch. Schema validates the kind value and the kind-specific
82	parameters together.
83	
84	**The function always runs**, regardless of routing kind. The
85	function produces the value; routing decides where the value
86	goes.
87	
88	### kind="plain"
89	
90	```json
91	"routing": { "kind": "plain" }
92	```
93	
94	One output port. The function's output value fans to every wire
95	attached to that port unconditionally. No routing decision; the
96	dispatch layer skips the branch picker and pushes to all wires.
97	
98	This is the default for new boxes; the editor sets it
99	automatically when a box is created.
100	
101	### kind="comparator"
102	
103	```json
104	"routing": { "kind": "comparator", "comparand": "<number>" }
105	```
106	
107	Three output ports: `lt`, `eq`, `gt` (fixed names; ports aren't
108	renameable per the editor's port-immutability rule). The
109	function's output value is compared to `comparand`; the matching
110	branch fires.
111	
112	#### Multi-band comparator (planned brainstorm)
113	
114	The current 3-branch lt/eq/gt design is a special case of "value
115	falls into a numeric range, fire the corresponding branch." A
116	generalization:
117	
118	```json
119	"routing": {
120	  "kind": "comparator",
121	  "thresholds": [3, 7, 11, 17]
122	}
123	```
124	
125	Five output ports get named by the threshold positions —
126	`below_3`, `between_3_7`, `between_7_11`, `between_11_17`,
127	`above_17`. With `thresholds: [4]` this becomes the current
128	3-branch lt/eq/gt (with `eq` being "exactly 4"), so the legacy
129	shape is one configuration of the new one.
130	
131	**Equality bands via doubled thresholds.** Encode the current
132	`eq` semantics by repeating a threshold value: `thresholds: [3, 3]`
133	means "less than 3, exactly 3, greater than 3" — three bands, the
134	middle one being a zero-width point that fires only on exact
135	equality. With `thresholds: [3, 3, 7, 7]` you get five bands:
136	`< 3`, `== 3`, `3 < x < 7`, `== 7`, `> 7`. The editor sees a
137	list of thresholds; the runtime sees doubled values as
138	"point bands."
139	
140	This means the legacy 3-branch lt/eq/gt is exactly
141	`thresholds: [c, c]` — the eq case is just a zero-width band at
142	the comparand. Behind the scenes the editor can present the
143	single-comparand UI but store `[c, c]` so the dispatch layer has
144	one consistent representation.
145	
146	Defer the schema specifics — the single-comparand case ships
147	first; multi-band is a follow-on once we have a use case that
148	actually needs more than three.
149	
150	### kind="iterator"
151	
152	```json
153	"routing": { "kind": "iterator", "n_outputs": 3 }
154	```
155	
156	`n_outputs` output ports, named conventionally (`out_0`, `out_1`,
157	…). Every invocation reads the box's `SLOT_ATOMIC_COUNTER` slot
158	via `slot_read_inc(counter_slot, n_outputs)`; that index picks
159	the branch.
160	
161	The function still runs and produces the routed value. Iterator
162	defaults the function to a passthrough identity if the user
163	hasn't picked a `ref` / `fn` — but the user is free to attach
164	any function whose output should be round-robin-distributed.
165	
166	### kind="randomizer"
167	
168	```json
169	"routing": { "kind": "randomizer", "n_outputs": 4 }
170	```
171	
172	Picks a branch uniformly at random per invocation.
173	Implementation: hash the counter-slot value into the branch
174	index, so the distribution is deterministic-given-seed but
175	spread across branches.
176	
177	```c
178	uint32_t i = slot_read_inc(counter_slot, UINT32_MAX);
179	uint32_t branch = hash(i) % n_outputs;
180	```
181	
182	`hash` is a cheap mixing function (xorshift, FNV); not
183	cryptographic. The point is breaking up the monotonic counter so
184	consecutive calls don't go to consecutive branches.
185	
186	### kind="weighted"
187	
188	```json
189	"routing": {
190	  "kind":    "weighted",
191	  "weights": [0.8, 0.2]
192	}
193	```
194	
195	Probability-based distribution. With `[0.8, 0.2]` and two output
196	ports, 80% of invocations fire branch 0, 20% fire branch 1.
197	
198	Implementation: at compile time, weights are normalized into a
199	cumulative table on a `0..PRECISION-1` integer scale (e.g.
200	`PRECISION = 1000`). Per call:
201	
202	```c
203	uint32_t r = slot_read_inc(counter_slot, PRECISION);
204	// pre-computed cumulative: [0..799] → branch 0, [800..999] → branch 1
205	uint32_t branch = lookup_band(r, cumulative_table);
206	```
207	
208	Same counter-slot machinery as iterator and randomizer; the
209	difference is the mapping from counter to branch.
210	
211	**Inspector UI for weighted routing**: a horizontal slider with
212	N knobs dividing it into N+1 segments — except really N segments
213	since each knob is the boundary between adjacent segments. A
214	wider segment means a larger fraction of invocations flow down
215	that path. Below the slider, one line per output port:
216	
217	```
218	out_0    [▓▓▓▓▓▓▓▓░░░░░░░░░░░░]    80%
219	out_1    [░░░░░░░░░░░░░░░░▓▓▓▓]    20%
220	```
221	
222	Each line shows the output port name on the left, a visual band
223	in the middle indicating its slice of the distribution, and the
224	percentage on the right. Adjusting the slider knobs updates the
225	percentages live; the percentages are also editable directly
226	(typing `30` into the right column shifts the relevant knob).
227	
228	### kind="distributor" (load-aware)
229	
230	```json
231	"routing": { "kind": "distributor", "n_outputs": 3 }
232	```
233	
234	Sends the value to the **least-busy** downstream consumer:
235	inspects the fill levels of the input slots wired to each branch
236	and picks the branch whose downstream slot has the fewest queued
237	values.
238	
239	Implementation: dispatch action peeks the `tail - head` count on
240	each downstream input slot and picks the minimum. Tied branches
241	fall back to atomic-counter round-robin so ties don't always go
242	to the same branch.
243	
244	This is the only routing kind that's not purely
245	self-contained — it reads downstream slot state — but it doesn't
246	add any new dispatch-layer concept beyond querying slot fill
247	(which the slot store already knows).
248	
249	### Future kinds
250	
251	The pattern is: any new routing kind plugs in at one place
252	(`routing.kind` value + dispatch-layer branch picker). All
253	existing kinds and any future ones live entirely in the dispatch
254	layer + slot store + editor — never in language specs, never in
255	user-written functions.
256	
257	## Iterator + function: ships as "function always runs"
258	
259	Iterator boxes always run their function. The dispatch action
260	becomes uniform across routing kinds:
261	
262	```
263	1. Read inputs (pop / peek per port mode).
264	2. Invoke spec (always — function runs regardless of routing kind).
265	3. Pick branch via routing.kind:
266	     comparator   → compare(output, comparand)
267	     iterator     → slot_read_inc(counter_slot, n_outputs)
268	     randomizer   → hash(slot_read_inc(counter_slot, MAX)) % n_outputs
269	     weighted     → cumulative lookup
270	     distributor  → argmin(downstream fill)
271	     (no routing) → fan to all outgoing wires
272	4. Push function output to outputs[picked branch] (or all wires if
273	   no routing).
274	```
275	
276	A single dispatch path for all branching boxes; the routing rule
277	plugs into step 3.
278	
279	## Schema migration
280	
281	Few enough existing maps that we can hand-migrate them. The
282	loader does **not** carry a legacy-shape adapter; once this
283	ships, all map files are expected to use the `routing` field. If
284	a map with the old shape is opened, the schema validator rejects
285	it and the user (or a one-off migration script) updates it.
286	
287	If we end up wanting a one-off script, it walks `maps/<name>/boxes/*.json`
288	and rewrites:
289	- `comparand: X` → `routing: { kind: "comparator", comparand: X }`
290	- `iterator_outputs: [...]` → `routing: { kind: "iterator", n_outputs: len(...) }`
291	- everything else → no change
292	
293	But for the current map count, manual edits are faster than
294	writing the script.
295	
296	## Inspector UI
297	
298	A `routing` selector in the inspector replaces the separate
299	`compare` toggle and `iterator` toggle. The dropdown's options
300	expand as new routing kinds ship:
301	
302	```
303	mode:    [plain ▾ | comparator | iterator]
304	```
305	
306	Per-kind controls below the dropdown — for the kinds shipped in
307	this issue:
308	- **plain**: nothing extra.
309	- **comparator**: numeric `comparand` input. (Multi-band UI is
310	  the brainstorm above, lands in its own issue.)
311	- **iterator**: `n_outputs` integer input.
312	
313	Output ports re-render to match the routing kind: 1 dot for
314	plain, 3 fixed dots (lt/eq/gt) for comparator, N dots for
315	iterator. Port names follow the routing kind's convention; not
316	user-renameable (issue 224's read-only-port-name rule).
317	
318	## Suggested implementation sequence
319	
320	This issue ships `plain`, `comparator`, and `iterator` only —
321	the existing surface area unified under one schema. The other
322	kinds get follow-on issue files (one per kind: randomizer,
323	weighted, distributor, multi-band-comparator) opened when this
324	issue is implemented.
325	
326	1. `src/001-schema.lua`: accept `routing` field with `kind` ∈
327	   `{plain, comparator, iterator}`; reject legacy `comparand` /
328	   `iterator_outputs`. Schema also rejects unknown `kind` values
329	   so future kinds gate on their own implementation.
330	2. `assets/js/004-inspector.js`: mode dropdown replacing the two
331	   toggles; per-kind controls (none for plain, `comparand` input
332	   for comparator, `n_outputs` for iterator).
333	3. `assets/js/002-boxes.js`: render branches based on
334	   `routing.kind`.
335	4. `assets/js/006-wires.js`: connection's `from_branch` matches
336	   the routing kind's port naming.
337	5. Phase 3 dispatch (issue 304): branch on `routing.kind` for the
338	   branch-selection step. Three rule paths shipped here; new
339	   kinds add their own paths in their own issues.
340	
341	Hand-migrate existing maps before merging. Open the follow-on
342	issues for randomizer / weighted / distributor / multi-band
343	comparator at merge time so their designs aren't lost.
344	
345	## Why now
346	
347	- Editor inspector keeps gaining toggles per routing kind. A
348	  unified mode dropdown scales linearly with kinds; separate
349	  toggles don't.
350	- Phase 3 dispatch will naturally have a "select branch" step;
351	  routing it through `routing.kind` is the same code regardless
352	  of whether we ship Level B now or refactor later.
353	- The new routing kinds (randomizer, weighted, distributor) only
354	  make sense in the unified schema. Adding them as separate
355	  top-level fields would be unmaintainable.
356	
357	## Relevant files
358	
359	- `src/001-schema.lua` — schema accepts new field
360	- `assets/js/004-inspector.js` — mode dropdown
361	- `assets/js/002-boxes.js` — branch rendering
362	- `assets/js/006-wires.js` — branch matching
363	- `issues/302-wire-value-slot-store.md` — counter slot mechanism
364	  used by iterator / randomizer / weighted
365	- `issues/304-task-dispatch-layer.md` — dispatch layer's branch
366	  picker dispatches on `routing.kind`
367	
368	## Open questions
369	
370	- **Distributor under fan-out**: if a branch wire goes to
371	  multiple consumers (fan-out), the "fill level" of that branch
372	  is ambiguous. Use the max of the consumers' fills, or the sum,
373	  or something else. Probably max — the bottleneck is the
374	  slowest consumer.
375	- **Multi-band comparator threshold edges**: `x == threshold`
376	  goes to which side of the cut? Lean toward "less-than-or-equal"
377	  for consistency. Need to nail down before implementation.
378	- **Weighted ties**: floating-point weights summing to `1.0`
379	  doesn't always discretize cleanly. Compile-time normalization
380	  picks an integer scale (1000) and rounds; the last band
381	  absorbs any leftover so the ranges always sum to PRECISION.
382	
383	## Level C example (deferred)
384	
385	The big follow-on after this issue ships is a fully data-driven
386	router: one dispatch path that reads a small rule expression
387	from the box JSON and evaluates it. Example shape:
388	
389	```json
390	"routing": {
391	  "kind": "rules",
392	  "rules": [
393	    { "when": "output < 3",                   "to": "small" },
394	    { "when": "output >= 3 and output < 7",   "to": "medium" },
395	    { "when": "output >= 7",                  "to": "large" }
396	  ]
397	}
398	```
399	
400	The dispatch layer ships a small expression evaluator (a few
401	dozen lines: variables = `output`, `counter`, `fill[i]`;
402	operators = comparisons, boolean conjunction, arithmetic). Every
403	existing routing kind reduces to a "rules" expression:
404	
405	- comparator → three rules with `when: output < c / output == c / output > c`
406	- iterator → one rule using `counter % N` as the band index
407	- randomizer → one rule using `hash(counter) % N`
408	- weighted → cumulative-band rules
409	
410	The benefit: one code path in dispatch instead of five. Custom
411	routing without writing a spec callback or modifying user
412	functions. The cost: a small evaluator (still entirely in C, no
413	spec involvement).
414	
415	Worth pursuing once Level B has shipped and we have real maps
416	using the kinds — at that point the consolidation is concrete
417	rather than speculative. Open as a separate issue when ready.
418	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md`

### Tool result

```
1	# 239 — Literal-bound input ports shrink to a nodule, port name moves to the left
2	
3	## Status
4	open
5	
6	## Current behavior
7	
8	Per issue 235 (complete), an input port whose literal value is
9	set renders with the value in place of the port name. The dot
10	on the box's left edge stays full-size, and the port name
11	disappears from the canvas — only the value is visible. The
12	inspector still owns the name, and the dot still works as a
13	wire-grab target even though wiring into a port with a literal
14	value would severs that literal on connect.
15	
16	What the user reading the map can see at a glance:
17	
18	```
19	●  "hello world"
20	```
21	
22	The port's variable name (`text`, `prompt`, whatever it is in
23	the underlying function signature) is invisible on the canvas.
24	A reader has to open the inspector to recover the binding.
25	
26	## Intended behavior
27	
28	A port with a literal value rendered in three pieces, in three
29	positions, in this order left-to-right:
30	
31	```
32	text   ·   "hello world"
33	```
34	
35	- **Port name** (`text`) — moved to the LEFT of the box's edge,
36	  in the gutter just outside the box body. This is the
37	  variable's identity, and 235's value-takes-over rendering
38	  hid it. Pulling it out to the left puts the name back in
39	  plain sight WITHOUT giving up the value display.
40	- **Nodule** — a smaller, denser dot sitting where the regular
41	  port dot would. About half the radius of a wired port's dot.
42	  Visual marker only — it advertises "this port is bound" the
43	  same way the regular dot advertises "this port can be wired."
44	  Color match its kind, same as the regular dot, so the eye
45	  reads it as a port without confusion.
46	- **Literal value** (`"hello world"`) — to the RIGHT of the
47	  nodule, same position the regular port name occupies on a
48	  wired port. Truncation rules from 235 still apply (24-char
49	  limit with ellipsis, tooltip for the full value).
50	
51	A wired port stays exactly as it is today — full-size dot,
52	name to its right, no left-of-box-edge label.
53	
54	### Wire-connect targeting disabled
55	
56	A literal-bound port is no longer a valid drag-target for
57	wires. `Boxes.hit_test_port` excludes literal-bound input ports
58	from its hit search, so:
59	
60	- The cursor over a nodule does NOT highlight the port for a
61	  wire-grab.
62	- The user cannot start drawing a wire from / to a literal-bound
63	  port. To rebind, they clear the literal value in the inspector
64	  first; the port then renders as a wirable dot again.
65	
66	Today, dropping a wire onto a port with a literal value silently
67	clears the literal (per the existing inspector code, line ~656
68	in `004-inspector.js`). That behavior was a defensible
69	compromise pre-239 because the value was the only visible
70	state — losing it to a wire was reversible by re-typing.
71	Post-239 the user is explicitly indicating "this is bound, no
72	wires," so we honor that instead of dropping a value on accident.
73	
74	### Why this is better than the 235-only rendering
75	
76	- The user reading a map sees BOTH the variable identity and
77	  the bound value, on one line, without inspector lookup.
78	- The nodule is unambiguously "not a wire target," so the user
79	  doesn't waste a drag attempt only to learn the literal was
80	  silently cleared.
81	- The left-of-edge name uses canvas gutter space that is
82	  currently empty, so the box body's horizontal budget is
83	  unaffected.
84	
85	### Edge cases
86	
87	- **Multiple ports with literals**: each gets its own left-of-
88	  edge name. Boxes with several literal-bound ports show a
89	  cleanly-stacked column of names in the gutter.
90	- **Variadic slot with a literal**: same rule as any other
91	  port. The variadic group's auto-grow / hide-empty-trailing
92	  rules from issue 217 are unaffected.
93	- **Port name long enough to collide with the left-of-edge
94	  gutter of an adjacent box**: clip with ellipsis at the box's
95	  max gutter width (TBD constant; ~12 monospace chars at
96	  10px gives ~72 world-px gutter, which is fine for typical
97	  identifiers). Tooltip on hover for the full name, same as
98	  235's value tooltip.
99	
100	## Suggested implementation steps
101	
102	1. `assets/js/002-boxes.js`:
103	   - Add a `port_is_literal_bound(box, i)` helper. Returns true
104	     when the port has a value AND no incoming wire. (The same
105	     three-state check `port_label_text` already does — pull it
106	     into a shared helper.)
107	   - In `draw_box`'s input-port loop: when literal-bound, draw
108	     the nodule (half-radius, same fill/stroke style as a normal
109	     dot) instead of the full dot; render the port name to the
110	     LEFT of the dot position (textAlign right, with the box's
111	     world x as the right edge); the value label drawing
112	     continues unchanged (235 path).
113	   - `hit_test_port`: skip ports that are literal-bound on
114	     inputs. The output side is unaffected (outputs don't carry
115	     literal values).
116	2. `assets/js/006-wires.js::create_connection`: the
117	   sever-literal-on-connect path in 004-inspector.js becomes
118	   unreachable because the literal port can't be a drag target.
119	   It can stay as a defensive guard in case some other code
120	   path forces a connection.
121	3. Tests: extend the 235 case set with two new cases:
122	   - "wire attached, literal present" → still renders as wired
123	     (wire wins, no nodule).
124	   - "literal only" → renders nodule + left-name + right-value.
125	
126	## Open questions
127	
128	- **Nodule radius**: half (3px world) is a starting guess. Visual
129	  weight will tell us if it should be smaller; refine after
130	  shipping. Constant in 002-boxes.js so it's a one-line change.
131	- **Gutter clip width**: 12 chars at 10px is a guess. Could be
132	  expressed as a fraction of `BOX_W` to scale if BOX_W ever
133	  changes. Probably not worth parameterizing today.
134	- **Inspector indication of "this port is literal-bound"**: the
135	  inspector already shows the value field; that's the only
136	  binding state. No additional UI needed there — the canvas is
137	  the place where the visual rule applies.
138	
139	## Relevant files
140	
141	- `assets/js/002-boxes.js` — `draw_box` input-port path,
142	  `hit_test_port`, helpers
143	- `assets/js/006-wires.js` — sever-on-connect guard
144	- `assets/js/004-inspector.js` — sever-on-set guard (still
145	  fires when the user sets a literal through the inspector
146	  while a wire is attached; the wire-into-literal direction is
147	  the one this issue closes off)
148	- `issues/completed/235-literal-value-replaces-port-name-on-canvas.md`
149	  — the predecessor; the value-on-canvas rule this issue
150	  retains, the dot-stays-the-same-size rule this issue
151	  supersedes
152	
```

### Tool result

```
1	# 303 — Language runtime spec: pluggable per-language invocation
2	
3	## Status
4	open
5	
6	## Current behavior
7	Each language has an ad-hoc driver script (`drivers/lua.sh`, `drivers/bash.sh`,
8	`drivers/c.sh`). The driver is invoked per box call: spawn a process, run
9	the function, read stdout, exit. Adding a new language means writing a new
10	driver script and modifying the executor to know about it.
11	
12	There is no shared interface, no persistent runtime, and no way for a user
13	to add a language without touching SoraMech internals.
14	
15	## Concept
16	
17	The language runtime spec is the user-extensible interface that defines how
18	a language's functions are invoked from inside the thread pool. Every
19	language goes through this interface — there is no special path for any
20	language. Lua is a spec. C is a spec. Bash is a spec. A user adding Python
21	writes another spec.
22	
23	The pool runner does not know how to "run a Lua function." It knows how to
24	call `lang->invoke(...)`. The Lua spec is the thing that knows how to run a
25	Lua function.
26	
27	The user provides the bridging code. SoraMech does not transpile, rewrite,
28	or convert box source between languages. If the user wants Rust, the user
29	writes a Rust spec.
30	
31	## The interface
32	
33	Each language spec is a C library that exports a single symbol:
34	
35	```c
36	typedef struct {
37	    const char *name;       // "lua", "bash", "c", "python", ...
38	    const char *file_ext;   // ".lua", ".sh", ".c", ".py" — for box file resolution
39	
40	    // Once-per-worker. Returns an opaque handle stored in worker_ctx.
41	    // Called at pool startup, before any tasks run.
42	    void *(*init)(int worker_idx);
43	
44	    // Once-per-worker. Called at pool shutdown.
45	    void  (*teardown)(void *handle);
46	
47	    // Optional: called once per box source file at map load.
48	    // For languages that compile (C → .so), this is where compilation
49	    // happens. For languages that don't (Lua, Bash), this is NULL.
50	    int   (*compile)(const char *src_path, const char *out_path);
51	
52	    // The invocation. Inputs are arrays of (bytes, size) pairs,
53	    // already read from slots by the dispatch layer. The spec writes
54	    // its return value into out_buf and sets *out_size.
55	    //
56	    // Returns 0 on success, nonzero on error.
57	    int   (*invoke)(void *handle,
58	                    const char *file_path,
59	                    const char *fn_name,
60	                    const void **input_data, const int *input_sizes, int n_inputs,
61	                    void *out_buf, int out_buf_capacity, int *out_size);
62	} lang_spec_t;
63	
64	extern lang_spec_t soramech_lang_spec;
65	```
66	
67	The spec library is built as a `.so` and loaded by the pool runner at
68	startup via `dlopen` / `dlsym`. The runner iterates `langs/*/spec.so`,
69	loads each, and registers it under its `name`.
70	
71	## Slot access boundary
72	
73	Specs do not see slot IDs. The dispatch layer (issue 304) reads input
74	values from slots into raw byte buffers, hands them to `invoke`, receives
75	the output bytes, and writes them to the output slot. This decouples
76	specs from slot internals — a spec only deals with values.
77	
78	If a future language wants zero-copy access (e.g. memory-mapping a slot
79	directly into a Python bytearray), the interface can be extended. Do not
80	add it preemptively.
81	
82	## Per-language layout
83	
84	Specs live under `langs/<name>/`:
85	
86	```
87	langs/lua/
88	    spec.c              ← implements lang_spec_t (handles → lua_State)
89	    Makefile            ← builds spec.so
90	    spec.so             ← built artifact
91	
92	langs/bash/
93	    spec.c              ← bridges to bash-server.sh via Unix domain socket
94	    bash-server.sh      ← persistent subprocess started by spec.init
95	    Makefile
96	
97	langs/c/
98	    spec.c              ← uses dlopen/dlsym; compile = gcc -shared -fPIC
99	    Makefile
100	
101	langs/python/           ← user-added; not shipped
102	    spec.c
103	    Makefile
104	```
105	
106	The pool runner has no compiled-in knowledge of any specific language.
107	Even Lua is loaded as a `.so` at startup. This keeps the boundary honest
108	— there is no temptation to special-case the "shipped" languages.
109	
110	## Built-in specs (shipped, not privileged)
111	
112	SoraMech ships specs for:
113	- **Lua** — handle is a `lua_State`. `invoke` calls `luaL_loadfile` (cached)
114	  + `lua_pcall`. Args pushed as Lua strings; return read as Lua string.
115	- **C** — handle is a small `dlopen` handle cache. `compile` runs
116	  `gcc -shared -fPIC -o <out_path> <src_path>`. `invoke` does
117	  `dlsym` + direct call.
118	- **Bash** — handle is a Unix domain socket connected to a persistent
119	  bash subprocess (`bash-server.sh`). `invoke` sends a framed request,
120	  reads a framed response.
121	
122	These are the reference implementations. They demonstrate the spec
123	interface for in-process FFI (Lua, C) and for out-of-process socket
124	servers (Bash, and any future language whose runtime cannot be embedded).
125	
126	## Compilation
127	
128	The optional `compile` hook runs at map load (or at editor compile-button
129	time, issue 219). For C, it produces a `.so` next to the source. For
130	Lua and Bash, no compilation step is needed; `compile` is `NULL`.
131	
132	Compilation outputs go in the map's `compiled/bin/` directory (issue
133	222). The dispatch layer passes the compiled artifact path to `invoke`,
134	not the original source path, when a compiled artifact exists.
135	
136	## Iterator counter prepending
137	
138	The iterator counter (issue 221) is prepended to the input array by the
139	dispatch layer, not by the spec. From the spec's perspective, an
140	iterator box just has one extra input. The spec doesn't know which
141	input is the counter — that's a graph-level concept.
142	
143	## Errors crash the program
144	
145	A nonzero return from `invoke` is a fatal error. The dispatch layer
146	prints the error context (box id, function name, language) to stderr
147	and aborts the process. There is no recovery, no retry, no per-task
148	"failed" state, no fallback. There are no timeouts on stalled tasks,
149	no wait-list garbage collection, no failure propagation through the
150	graph.
151	
152	The reasoning: a task that cannot produce a correct return value is a
153	bug. Bugs do not belong in production. Building in detection and
154	recovery for bug states adds runtime cost and complexity to handle a
155	case that should not exist. If a bug surfaces in production, the
156	program crashes loudly; the developer fixes the bug and redeploys.
157	
158	The spec is still responsible for catching language-level errors (Lua
159	`error()`, C signals it can intercept, Bash nonzero exit) and
160	translating them into a nonzero return — so that the dispatch layer
161	sees a clean signal instead of an undefined post-error state. Once
162	returned, the program ends.
163	
164	## User-facing documentation
165	
166	The interface is published in `langs/lang-spec.h`, which every spec
167	implementation includes. That header is the source of truth for the
168	contract.
169	
170	User-facing docs that mention the language spec system:
171	- `README.md` — short paragraph in the features list pointing out that
172	  any language can be added by writing a spec, with a one-line example
173	  of what that means
174	- `docs/005-language-specs.md` (to be written) — the long-form guide
175	  on writing a spec: walk through the Lua spec as a small reference,
176	  show what `init` / `invoke` / `teardown` do, point at the C spec for
177	  the `compile`-using case and the Bash spec for the
178	  socket-server case
179	- `docs/000-table-of-contents.md` — index entry for the new doc
180	
181	Without these the spec system is invisible — users would never know it
182	exists or how to extend it.
183	
184	## Open questions
185	
186	(none currently)
187	
188	## Suggested implementation sequence
189	
190	1. Write `langs/lang-spec.h` — the public interface header.
191	2. Write `langs/lua/spec.c` first. Lua is the easiest because it's already
192	   our default language and the existing driver gives a reference behavior.
193	3. Write `langs/c/spec.c` — adds `compile` callback, exercises the dlopen
194	   path.
195	4. Write `langs/bash/spec.c` + `langs/bash/bash-server.sh` — exercises
196	   the out-of-process socket model. This is the proof that languages
197	   without an embeddable runtime fit the same interface.
198	5. Pool runner registry: `dlopen` every `langs/*/spec.so` at startup,
199	   register by `name`. Box files are routed to specs by `file_ext`.
200	6. Smoke test: a map with one Lua box, one C box, one Bash box, all
201	   running in the pool, all going through the spec interface. No
202	   special-cased code paths.
203	
204	## Relevant files
205	
206	- `drivers/lua.sh`, `drivers/bash.sh`, `drivers/c.sh` — current ad-hoc
207	  drivers, replaced by `langs/<name>/spec.c`
208	- `issues/301-pool-lifecycle-and-worker-init.md` — pool startup invokes
209	  spec `init` per worker per language
210	- `issues/302-wire-value-slot-store.md` — dispatch layer reads slots and
211	  passes raw bytes to specs (specs do not touch slots)
212	- `issues/304-task-dispatch-layer.md` — calls `invoke` per task
213	- `issues/222-compile-button-and-assets-directory.md` — compile button
214	  runs spec `compile` for every box
215	- `docs/004-ipc-and-threading.md` — Option 1 (FFI) and Option 3 (Unix
216	  socket) describe the two execution models specs implement
217	
218	## Implementation log
219	
220	### Registry — 2026-05-12
221	
222	`src/011-spec-registry.{c,h}` scans a `langs/` directory, dlopens
223	every `<name>/spec.so` it finds with `RTLD_NOW | RTLD_LOCAL`, and
224	exposes name + extension lookup plus enumeration. The three
225	shipped stubs (lua/c/bash) load cleanly and round-trip through
226	`spec_registry_get` and `spec_registry_for_ext`. Four unit tests
227	in `tests/011-spec-registry-test.c` cover the load, missing dir,
228	null inputs, and accessor safety.
229	
230	Deferred: hooking `spec->init(worker_idx)` into the pool's
231	`worker_main` between TLS setup and the init barrier. That's the
232	final piece that makes 303 fully wired; lands in a follow-on once
233	a spec has a real `init` body to call (issue 306 onward).
234	
235	### Per-worker init / teardown helper — 2026-05-12
236	
237	`spec_registry_init_worker(r, worker_idx, handles, cap)` and the
238	matching `_teardown_worker` walk the registry, call each spec's
239	init/teardown, and stash handles in a caller-provided array.
240	Index in the array matches index in the registry. Specs with NULL
241	init contribute NULL handles; teardown skips NULL slots. A new
242	unit test runs against the three shipped specs and now confirms
243	that the Lua spec's init returns a non-NULL `lua_State` while the
244	still-stub C and Bash specs leave their slots NULL.
245	
246	What's still ahead inside 303: actually calling the helper from
247	the pool's `worker_main` so each worker thread populates its
248	`worker_ctx_t.handles[]` before crossing the init barrier. Small
249	change; lives in `libs/task-pool/pool.c` and needs the runner to
250	pass a `spec_registry_t *` into `pool_create` (or to a new
251	`pool_init_specs` API). Lands in the next pass.
252	
253	### Pool worker hook — 2026-05-12
254	
255	`libs/task-pool/pool.{c,h}` grew a per-worker init callback:
256	`pool_set_worker_init(p, cb, user)` and `pool_init_barrier` now
257	returns an int (0 ok, -1 if any worker's callback failed). The
258	init barrier became a three-stage handshake so callbacks
259	registered between `pool_create` and `pool_init_barrier` can't
260	race the workers — stage 1 parks workers until the barrier
261	authorises them, stage 2 runs each worker's callback, stage 3
262	releases everyone into the task loop.
263	
264	`tests/303-pool-spec-init-test.c` wires `spec_registry_init_worker`
265	into the pool callback (a two-line trampoline) and asserts that
266	spawned tasks see a non-NULL `lua_State` in
267	`pool_current_worker->handles[lua_idx]`, with each worker getting
268	its own distinct state. The pool-internal callback test in
269	`tests/301-pool-test.c` covers the success and failure paths
270	without the registry. 303 is now functionally complete end to
271	end — the registry loads specs, the helper inits them per
272	worker, and the pool's worker_main calls them through the hook.
273	
```

## Assistant

Let me continue reading the rest of the open issues, and load task tracking since this is exactly the kind of multi-issue work that benefits from it.

## Assistant

_Tools:_

- `ToolSearch (...)`

### Tool result

```

```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/301-pool-lifecycle-and-worker-init.md`

### Tool result

```
1	# 301 — Thread pool lifecycle and per-worker initialization
2	
3	## Status
4	open
5	
6	## Current behavior
7	The executor (`src/004-executor.lua`) runs boxes synchronously in a
8	single Lua coroutine. There is no thread pool, no pthreads, and no
9	per-thread language runtime state.
10	
11	## Phase model
12	
13	Phase 2 is the synchronous interpreter path. It is the development
14	target while editor work is in progress.
15	
16	Phase 3 is the thread pool path. It is the only execution path going
17	forward. The synchronous interpreter does not stay around as an
18	alternative entry point — once phase 3 lands, the pool runner
19	replaces it. There is no "fallback to synchronous" mode.
20	
21	### Phase 2 → 3 refactor surface
22	
23	The following components are replaced (not extended) in phase 3:
24	
25	| Phase 2 file                  | Phase 3 replacement                  |
26	|-------------------------------|--------------------------------------|
27	| `src/003-loader.lua`          | C loader inside `src/008-pool-runner.c` |
28	| `src/004-executor.lua`        | C task dispatch layer (issue 304)    |
29	| `src/007-runner-main.lua`     | `src/008-pool-runner.c`              |
30	| `drivers/*.sh`                | Language runtime spec (issue 303)    |
31	
32	All threading-pool-adjacent functionality is C code that lives next to
33	the pool. There is no embedded Lua runtime for graph loading or
34	dispatch — Lua appears only as a language runtime that boxes can use,
35	on equal footing with C, Bash, and any future language.
36	
37	## Intended behavior
38	
39	### Pool source
40	The thread pool is **SoraMech-owned**, written from scratch under
41	`libs/task-pool/`. The 3d-rts pool at
42	`/home/ritz/programming/ai-stuff/games/3d-rts/libs/900-task-pool.h`
43	is the **design reference**, not a dependency. We need full control
44	over the spawn primitive, the worker context, the init barrier
45	(below), and iterator-pinning extensions — all reasons not to inherit
46	an external implementation.
47	
48	The pool exposes (at minimum):
49	
50	```c
51	pool_t *pool_create(int n_workers);
52	void    pool_destroy(pool_t *p);
53	void    pool_spawn(pool_t *p, action_fn_t fn, void *arg);
54	void    pool_wait_quiescent(pool_t *p);   // blocks until active count hits zero
55	void    pool_init_barrier(pool_t *p);     // see "Init barrier" below
56	```
57	
58	3d-rts's frame-ring + park-on-slot machinery is referenced for
59	patterns; we don't reimplement features we don't need.
60	
61	### Where the pool lives in the process
62	The pool is owned by the map runner's C entry point. One pool per map
63	run. It is created after the graph is loaded and validated, and
64	destroyed after the run completes.
65	
66	The C entry point (`src/008-pool-runner.c`):
67	
68	```c
69	int main(int argc, char **argv) {
70	    // 1. parse map path from argv
71	    // 2. load and validate graph (C loader, no Lua involved)
72	    // 3. determine which language runtimes the map uses
73	    // 4. allocate per-input-port slots for every box (issue 302)
74	    // 5. pool = pool_create(n_workers)
75	    // 6. init per-worker language runtime handles for each language present
76	    // 7. pool_init_barrier(pool)  ← block until every worker is fully initialized
77	    // 8. push every input port's literal value into its slot
78	    // 9. spawn tasks for boxes whose input set is now satisfied
79	    // 10. pool_wait_quiescent(pool)
80	    // 11. write last-run.jsonl
81	    // 12. pool_destroy(pool)
82	}
83	```
84	
85	### Init barrier
86	
87	Workers are not safe to dispatch tasks to until **every worker has
88	fully completed init for every language spec the map uses**. If the
89	main thread spawns a task before a worker's `lua_State` is up, the
90	task lands on a null handle.
91	
92	`pool_init_barrier` is the join point:
93	
94	1. Each worker's startup runs all `lang->init(worker_idx)` calls in
95	   a defined order, populating its `worker_ctx_t.handles[]`.
96	2. After completing its inits, each worker increments a shared
97	   atomic `workers_ready` counter and blocks on a condition
98	   variable.
99	3. The main thread waits until `workers_ready == n_workers`.
100	4. Main thread broadcasts the condition variable; all workers
101	   release simultaneously and begin pulling tasks.
102	
103	This is a one-shot barrier — it runs once at pool startup. After
104	quiescence it doesn't re-engage; teardown takes a different path.
105	
106	The barrier also protects against partial init failures: if any
107	worker's `init` returns an error, the barrier never completes and
108	the main thread can detect it via a timeout or per-worker status
109	flag.
110	
111	### Graph loading: ported to C
112	The graph loader is ported from `src/003-loader.lua` to C and lives
113	inside (or alongside) `008-pool-runner.c`. The pool runner has no
114	embedded Lua state for loader purposes. JSON parsing uses a small
115	vendored C JSON library (cJSON or similar; decision deferred to
116	implementation).
117	
118	Lua appears in the runner only when a box's language is Lua — and even
119	then, only inside that worker's language runtime handle. The pool
120	runner itself does not link to liblua at the top level.
121	
122	### Per-worker language runtimes
123	No language is treated as the "native" or preferred language at the
124	thread level. Every box — regardless of language — goes through the
125	same task dispatch path. The per-box cost is determined by that box's
126	invocation wrapper, not by any map-level language setting.
127	
128	Each worker thread holds a set of language runtime handles, one per
129	language it may be asked to run. These are initialized once at pool
130	startup, before any tasks are submitted, and torn down at shutdown:
131	
132	```c
133	typedef struct {
134	    void   *handles[N_LANGS];   // opaque per-language runtime state
135	    int     thread_idx;
136	} worker_ctx_t;
137	```
138	
139	Only the language handles actually needed by the map are initialized.
140	If the map contains no Bash boxes, no Bash handle is started for any
141	worker. The graph loader determines which languages are present and
142	passes that set to the pool runner at startup.
143	
144	The per-language initialization protocol is defined by the language
145	runtime spec (issue 303). The pool runner calls each spec's `init`
146	function at startup and its `teardown` function at shutdown.
147	
148	### Worker context access
149	Worker context is stored in thread-local storage (`__thread
150	worker_ctx_t *current_worker`), set by each worker immediately after
151	the thread is started. This avoids per-task `pthread_self()` lookups.
152	
153	### Pool size
154	Number of worker threads defaults to the number of logical CPUs
155	(`sysconf(_SC_NPROCESSORS_ONLN)`), capped at a compile-time maximum
156	(`MAX_WORKERS`, initially 16). Can be overridden by an environment
157	variable `SORAMECH_WORKERS=N`.
158	
159	### Slots are owned by input ports (issue 302)
160	Each box has one ring-buffer slot per input port, allocated at
161	graph load time and durable for the run. The pool runner walks
162	every box during the load step and populates a per-box slot table.
163	Slots are not allocated per-task; tasks pop and push but never
164	allocate. See issue 302 for the slot lifecycle and lifetime model.
165	
166	### Quiescence
167	The runner tracks an active-task counter, incremented on `pool_spawn`
168	and decremented when a task reaches `ACT_DONE`. The main thread waits
169	on a condition variable that signals when the counter reaches zero.
170	
171	This naturally handles iterators that re-queue themselves: each
172	re-queue increments the counter, each completion decrements. When the
173	counter reaches zero, the run is over.
174	
175	Deadlock is not a concern at runtime. The graph validator (running at
176	load) rejects non-iterator cycles and inputs without producers. Errors
177	inside a task crash the program (issue 303), so there is no
178	half-completed-producer case to worry about. An iterator parking
179	because its upstream queue is empty is not deadlock — it is the
180	natural end-of-stream condition; once the active-task counter reaches
181	zero with the iterator parked, the run ends cleanly.
182	
183	## Open questions
184	- Vendored JSON library: cJSON, jsmn, or hand-rolled. cJSON is heavier
185	  but easiest. Decision deferred.
186	
187	## Suggested implementation sequence
188	1. Vendor `900-task-pool.h` + `.c` into `libs/task-pool/`.
189	2. Vendor a JSON parser into `libs/json/`.
190	3. Port `003-loader.lua` to C inside `src/008-pool-runner.c`.
191	4. Write the pool lifecycle skeleton: pool_create, worker context TLS,
192	   active-task counter, quiescence wait.
193	5. Add `Makefile` target `soramech-pool` that compiles the C runner and
194	   links against the vendored pool and JSON parser.
195	6. Smoke test: single-box Lua map, pool with 2 workers, box runs, result
196	   written. No task graph complexity yet.
197	
198	## Relevant files
199	- `libs/task-pool/` — SoraMech-built pool (to be created)
200	- `/home/ritz/programming/ai-stuff/games/3d-rts/libs/900-task-pool.h` — design reference only
201	- `src/003-loader.lua` — current Lua loader, to be ported to C
202	- `src/004-executor.lua` — current synchronous executor, to be replaced
203	- `src/007-runner-main.lua` — current Lua runner, to be replaced
204	- `issues/302-wire-value-slot-store.md` — per-port slot model
205	- `issues/303-language-runtime-spec.md` — per-language init/call/teardown
206	- `issues/304-task-dispatch-layer.md` — replaces 004-executor.lua
207	- `docs/004-ipc-and-threading.md` — threading roadmap and IPC options
208	
209	## Implementation log
210	
211	### Pool skeleton — 2026-05-12
212	
213	What shipped:
214	- `libs/task-pool/pool.h` + `pool.c` — SoraMech-owned pool with N
215	  pthread workers, a singly-linked FIFO queue under one mutex,
216	  atomic active-task counter with its own cv for quiescence wait,
217	  one-shot init barrier with workers_ready / init_released
218	  predicates under a third mutex. Worker count defaults to logical
219	  CPUs (capped at `POOL_MAX_WORKERS=16`), overridable via
220	  `SORAMECH_WORKERS=N`. Recursive `pool_spawn` from inside an
221	  action works — the queue mutex is re-entered cleanly.
222	- TLS `pool_current_worker` for actions that need their worker
223	  index or the back-pointer to the pool.
224	- `tests/301-pool-test.c` — 8 unit tests: create/destroy with no
225	  tasks, default worker count, cap-at-max, single spawn, 1000-task
226	  spawn-and-quiesce, TLS context observable from inside actions,
227	  recursive spawn (depth 10 → 11 invocations), and 8 concurrent
228	  producer threads each pushing 500 tasks while 4 workers drain
229	  (4000 total). All pass under STRICT.
230	- `src/008-pool-runner.c` now loads the graph via 305 and stands up
231	  the pool: `./soramech-pool tests/maps/hello` prints the box count,
232	  entry box, and worker count, then quiesces cleanly with no tasks.
233	
234	What's deferred to follow-ons within 301:
235	- **Per-worker spec init** between `pool_create` and
236	  `pool_init_barrier`. The hook is documented in `worker_main`;
237	  lands with issue 303 once specs can be `dlopen`ed and
238	  per-worker handles allocated.
239	- **Priority queue.** Issue 310 covers the "wired in, no-op
240	  behaviorally" upgrade. Current queue is plain FIFO.
241	
242	Neither blocks 304 (dispatch) from starting.
243	
244	### Per-worker teardown hook — 2026-05-12
245	
246	Symmetric counterpart to `pool_set_worker_init`. Registered via
247	`pool_set_worker_teardown(p, cb, user)`; the worker thread runs
248	the callback right before exiting, after the task loop drains.
249	The runner uses it to call `spec_registry_teardown_worker`,
250	which sends bash subprocesses `QUIT`, closes their socket fds,
251	and waitpids them. Without this, soramech-pool was leaking
252	orphan bash processes (16 per run) that piled up across runs
253	and slowed startup. Verified: zero orphans after a fresh run.
254	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/302-wire-value-slot-store.md`

### Tool result

```
1	# 302 — Per-task slot store with wire-held references
2	
3	## Status
4	open
5	
6	## Current behavior
7	Wire values are passed through stdout pipes and transient strings. Values
8	have no addressable location, no reference count, and no cross-language
9	accessibility. Each value vanishes once the pipe is drained.
10	
11	## Concept
12	
13	### Slots belong to input ports, not tasks
14	
15	Each box has **one slot per input port**, allocated when the graph
16	loads and persisting for the life of the run. A wire is a routing
17	declaration: "the producer's output port pushes a copy of its value
18	into this consumer's named input slot." The producer never owns a
19	slot — its output is a routing event, not a stored value.
20	
21	When a producer fires, the dispatch layer copies the value into
22	every downstream input slot wired to that output. Fan-out is N
23	pushes (one per consumer); fan-in is N producers all pushing into
24	the same consumer's slot. Both behaviors fall out of the same
25	push-into-input-slot mechanic.
26	
27	Slots live in process heap memory, accessible to every worker
28	thread. Cross-process visibility is not required: language specs
29	(issue 303) only ever receive byte pointers — never raw slot
30	addresses — so out-of-process specs (Bash) work via sockets, not
31	shared memory.
32	
33	### Tasks are ephemeral; box state is durable
34	
35	A **task** is one invocation: pop one value from each of the box's
36	input slots, run the box's logic, push the output to every
37	downstream input slot. Tasks have no persistent identity — they're
38	just stack frames on workers. Tasks are spawned dynamically by the
39	dispatch layer when a box's input set is ready (every input slot
40	holds at least one value).
41	
42	The **box** is the durable thing: it owns its input port slots, any
43	counter state (for iterators), its language spec handle, and so
44	on. The box exists from graph load until run end. Tasks come and
45	go.
46	
47	This is uniform across iterator and non-iterator boxes. Both run N
48	times where N depends on how many invocations their inputs supply
49	(possibly 1, possibly thousands). The difference is just what the
50	task does:
51	- **Plain call task**: invoke spec on the popped values, push the
52	  return to outputs.
53	- **Iterator task**: read box's counter, route the popped value to
54	  output port `iterator_outputs[counter]`, increment counter mod N.
55	  No spec invocation.
56	- **Comparator task**: invoke spec on the popped values, compare
57	  the return against `comparand`, route to the matching `lt` /
58	  `eq` / `gt` branch.
59	
60	### All slots are ring buffers; read mode varies
61	
62	There is one slot type. It is a ring buffer of `n_cells` cells,
63	each of `cell_capacity` bytes. The number of cells (`n_cells`) and
64	the read mode are both compile-time properties of the input port:
65	
66	- **1-cell + peek mode**: the value the producer wrote stays in the
67	  cell. Tasks read via `slot_peek` — read without draining — and
68	  every subsequent task on this consumer reads the same value
69	  unless the producer overwrites it. Used for **single-push wires**
70	  (a literal, or a wire from a producer box whose invocation count
71	  is exactly 1). Multiple consumer tasks read the value as many
72	  times as needed; the cell is never empty.
73	- **N-cell + pop mode**: a queue. Producers push; consumers
74	  `slot_pop` — drain one cell per task. Used for **multi-push wires**:
75	  any wire whose producer has more than one task invocation
76	  during the run. The producer's pushes accumulate, the consumer
77	  drains in pop order.
78	
79	Compile-time analysis classifies each wire. A producer is
80	"runs-once" if no transitive ancestor is an iterator; otherwise
81	it's "runs-N-times." Wires from runs-once producers get 1-cell
82	peek slots; wires from runs-N-times producers get N-cell pop slots.
83	
84	`n_cells` for pop slots is sized for worst-case backlog (or a
85	default like 64 with growth on demand). Lists grow if the producer
86	outruns the consumer past the pre-allocation.
87	
88	Fan-in: multiple producers pushing into the same N-cell pop slot
89	land in arrival order. The consumer pops FIFO unless the cell
90	carries an ordering tag (see "Cell tagging" below).
91	
92	### Atomic-counter slot mode
93	
94	A slot allocated with `SLOT_ATOMIC_COUNTER` carries a single
95	4-byte unsigned integer cell, read via a special op:
96	
97	```c
98	uint32_t slot_read_inc(slot_store_t *s, slot_id_t id, uint32_t mod);
99	// Returns the current value mod `mod`, then atomically increments
100	// the underlying counter. Concurrent readers each get a distinct
101	// value. Counter wraps at 2^32; mod handles N-branch routing.
102	```
103	
104	Used for iterator counters (issue 304): the iterator box has an
105	auto-allocated `SLOT_ATOMIC_COUNTER` slot at load time; the
106	dispatch action calls `slot_read_inc(counter_slot, n_branches)`
107	to pick its output branch. Multiple iterator tasks can read
108	concurrently from the same counter slot in parallel — each gets a
109	unique routing index — without any per-box mutable state and
110	without the serial dependency of a self-push counter.
111	
112	The slot's "cell" is just an `atomic_uint32_t`. No push is ever
113	performed; reads do the work. From the dispatch action's
114	perspective it's just another slot; from the slot store's
115	perspective the read op is special-cased.
116	
117	### Cell tagging (for parallel-iterator ordering)
118	
119	A cell can optionally carry a 4-byte ordering tag alongside its
120	size and data:
121	
122	```
123	Cell layout (N-cell pop slot)
124	Offset  Size  Field
125	------  ----  -----
126	0       4     filled_size
127	4       4     tag      (0 if untagged; iterator counter snapshot otherwise)
128	8       …     data
129	```
130	
131	Slots created for wires from parallel iterators (issue 304) are
132	tagged. Pop on a tagged slot returns the cell with the lowest tag,
133	not the head cell — preserving iterator-counter order across pushes
134	that arrived out of order from parallel iterator workers.
135	
136	Untagged slots default tag to 0 and pop FIFO at no extra cost. The
137	tag-aware pop is a per-slot mode flag set at allocation time.
138	
139	### Asynchrony
140	The store is fully asynchronous. Slots are allocated, filled, read, and
141	freed at any time, on any thread, with no global lock. Synchronization
142	is per-slot. There is no central allocator-wide mutex.
143	
144	## Slot layout
145	
146	Each slot is a header followed by `n_cells` cells, each `cell_capacity`
147	bytes. The header tracks ring-buffer state and lifetime:
148	
149	```
150	Slot header
151	Offset  Size  Field
152	------  ----  -----
153	0       4     cell_capacity   (bytes per cell, set at allocation)
154	4       4     n_cells         (ring size, set at allocation)
155	8       4     head            (read index, mod n_cells)
156	12      4     tail            (write index, mod n_cells; cells_filled = tail - head)
157	16      1     touched         (per-slot lock: 0=free, 1=another thread mutating)
158	17      3     _pad
159	20      …     cells           (n_cells * (4-byte size + cell_capacity bytes))
160	```
161	
162	Each cell stores a 4-byte filled-size followed by `cell_capacity`
163	bytes of value data. Cells have wildly different capacities across
164	slots — capacity is per-slot, not global. The C convention of passing
165	sizes alongside arrays applies: the task that allocates a slot knows
166	how big it needs the cells.
167	
168	`touched` is a single-byte flag toggled around any header mutation
169	(push, pop, refcount changes). A reader that finds `touched == 1`
170	spins until it clears. This is the simplest possible per-slot lock —
171	if contention proves to matter, replace with a proper mutex without
172	changing the API.
173	
174	Writes to a cell complete with a memory barrier before `tail` is
175	advanced, so a reader that observes `head < tail` is guaranteed to
176	see the complete value at the head cell.
177	
178	## Lifetime
179	
180	A slot — the named input port on a box — is owned by its box and
181	lives from graph load until run end. That hasn't changed. What
182	has changed is the values that pass through slots: they are
183	reference-counted memory chunks served by a single unified
184	allocator (see "Allocation strategy" below), not malloc'd memory
185	that lives the whole run.
186	
187	Every value handed to a consumer carries a reference. When a
188	consumer is done with a value, it drops its reference. When the
189	last reference drops, the chunk holding the value goes back to
190	the allocator's free-lists, available for the next request. Two
191	neighbor-merge steps and an on-demand sweep keep fragmentation
192	under control (described in the allocator section).
193	
194	The earlier note in this document — "no per-slot reference
195	counting" — referred to the box's own slot record, not to the
196	values flowing through it. The slot record itself is still
197	allocated once at graph load and not freed until run end. The
198	values inside the slot are the things that get refcounted, freed,
199	and recycled. The distinction matters: there's one slot per
200	input port, and many values flow through that slot over the life
201	of a run.
202	
203	Run termination is still governed by the pool's active-task
204	counter (issue 301): when no task is running and no further
205	spawns are pending, the run is over. The allocator and refcount
206	machinery don't influence termination — they just ensure memory
207	gets returned to the free-lists as values become unreferenced
208	during the run.
209	
210	### No wait lists
211	
212	Tasks never park on slots. Tasks are spawned by the dispatch
213	layer's spawn-on-input-arrival rule and run to completion; they
214	never block. A slot that has no value yet simply means the
215	spawn rule hasn't fired for that consumer yet. No wait list,
216	no per-slot wake-up traversal — just a check after each push.
217	
218	## Backing store
219	
220	Slots are allocated from process heap memory. The slot store owns a
221	per-run allocator (described under "Allocation strategy" below) that
222	hands out memory chunks to slots. Plain `malloc`-class memory is
223	sufficient — every worker thread in the process can read and write
224	it, and no spec ever needs to access it from a child process.
225	
226	Synchronization within a slot uses the per-slot `touched` flag for
227	header mutations. There is no allocator-wide mutex on the
228	read/write path; only the per-size-class free lists in the
229	allocator have their own locks.
230	
231	## Slot creation
232	
233	Slot creation happens at **graph load**, not per task. The graph
234	loader walks every box, allocates one ring buffer per input port,
235	and parks the slot on the box's per-port slot table. The dispatch
236	layer never allocates new slots at runtime in the steady state —
237	all slots that will ever exist are created up front.
238	
239	```c
240	slot_id_t slot_alloc(slot_store_t *store, int cell_capacity, int n_cells);
241	```
242	
243	`cell_capacity` and `n_cells` come from compile-time analysis of
244	the graph. Boxes downstream of iterators get larger `n_cells` to
245	absorb the iterator's worst-case backlog.
246	
247	If a slot's pre-allocated `n_cells` proves too small at runtime
248	(producer outpaces consumer beyond the pre-allocation), the slot's
249	ring grows — see "Allocation strategy" below.
250	
251	## C API surface
252	
253	```c
254	slot_id_t slot_alloc       (slot_store_t *s, int cell_capacity, int n_cells, int flags);
255	void      slot_push        (slot_store_t *s, slot_id_t id, const void *data, int size, uint32_t tag);
256	int       slot_peek        (slot_store_t *s, slot_id_t id, void *buf, int buf_size); // 1-cell: read without draining
257	int       slot_pop         (slot_store_t *s, slot_id_t id, void *buf, int buf_size); // N-cell: drain head (or lowest-tag)
258	uint32_t  slot_read_inc    (slot_store_t *s, slot_id_t id, uint32_t mod);            // SLOT_ATOMIC_COUNTER only
259	int       slot_has_value   (slot_store_t *s, slot_id_t id);                          // for spawn-rule check
260	```
261	
262	Flags include:
263	- `SLOT_TAGGED`: cells carry ordering tags; `slot_pop` returns the
264	  lowest-tag cell.
265	- `SLOT_ATOMIC_COUNTER`: slot holds a single atomic uint32; reads
266	  via `slot_read_inc` only. `slot_push` / `slot_peek` / `slot_pop`
267	  are not valid on atomic-counter slots.
268	
269	Push always takes a `tag` argument; untagged slots ignore it (or
270	treat 0 as "no order").
271	
272	1-cell peek slot use: producer `push` once at startup (literal) or
273	when its single invocation completes (single-push wire). All
274	consumer tasks `peek` to read the value as many times as their box
275	runs. The cell is never empty after the initial push.
276	
277	N-cell pop slot use: producers `push` repeatedly. Consumer tasks
278	`pop` one cell each. For tagged slots, `pop` returns the
279	lowest-tag cell; for untagged, FIFO. Pop on an empty slot is a
280	bug — the spawn rule guarantees the slot is non-empty before a
281	task spawns.
282	
283	There is no `slot_ref` / `slot_unref` — slot lifetime is the run.
284	
285	## Submission timing
286	
287	Tasks are spawned dynamically in response to slot pushes. Each box
288	has small per-box state used by the dispatch layer:
289	
290	```
291	box_runtime_state {
292	    slot_id_t  *input_slots;     // one per input port (set at load)
293	    int        *port_modes;      // PEEK or POP, per input port
294	    int         n_inputs;
295	    slot_id_t   counter_slot;    // iterator only: SLOT_ATOMIC_COUNTER slot;
296	                                 // 0 / unused otherwise
297	};
298	```
299	
300	The spawning rule:
301	- When a producer pushes to one of a box's input slots, the
302	  dispatch layer checks **does every input slot have a value
303	  available?** "Available" means:
304	  - For peek-mode (1-cell) slots: cell has been written at least
305	    once. Once a peek slot is filled, it stays available — every
306	    subsequent spawn-check on this box passes the peek-port check.
307	  - For pop-mode (N-cell) slots: queue is non-empty.
308	- If yes, spawn a `dispatch_task_t` and submit to the pool. The
309	  task pops (or peeks) one value from each input slot when it
310	  runs.
311	- Each push that completes the input set triggers exactly one
312	  spawn. Multiple pushes accumulate in pop slots; each pop
313	  consumes one, and if more remain, the next push (or check)
314	  re-fires the spawn rule.
315	
316	This makes the pool's queue the rate buffer: pushes accumulate
317	in pop slots (possibly multiple per slot), and one task per
318	accumulated value gets spawned.
319	
320	Tasks never block on slots. They're spawned after inputs are
321	ready, run to completion, and disappear. Blocking-and-waking
322	machinery is unnecessary — the spawn-on-arrival rule is the
323	synchronization primitive.
324	
325	The pool's queue is the only queue. There is no separate "blocked
326	tasks" structure.
327	
328	## Allocation strategy: one unified allocator for every value
329	
330	There is one allocator. It serves both values whose size is known
331	from the graph at compile time and values whose size only becomes
332	known when they're produced (LLM responses, dynamic arrays, anything
333	where the upper bound isn't a useful number). The earlier draft of
334	this document split these into two stores — a size-class slot
335	allocator for the known case, a separate "large-value heap" for the
336	unknown case. That split was a mistake. They are the same problem
337	and they reuse the same machinery; treating them as two stores led
338	to the unknown-size path being a monotonically-growing pool that
339	never reclaims, which is a memory leak by design.
340	
341	### How the allocator learns sizes ahead of time
342	
343	Before the run starts, the loader walks every box in the graph and
344	reads the size that box declares for its output. A box that always
345	produces a 32-byte number declares 32. A box that always produces a
346	4-megabyte image declares 4 megabytes. The loader collects every
347	distinct declared size, deduplicates them, and creates one free-list
348	per distinct size. Each list is pre-populated with enough chunks to
349	cover the static demand for that size.
350	
351	Boxes whose output size depends on runtime — the LLM case — declare
352	"size unknown." The loader doesn't create a pre-warmed list for those
353	boxes; it knows their values will be allocated on demand.
354	
355	Both kinds share the same underlying memory region. The difference is
356	only whether the loader had a list pre-warmed for the value's size.
357	
358	### Two doors, one allocator
359	
360	**Door 1 — declared-size allocation.** Given a request for size N,
361	where N matches a declared size class: pop the head of that class's
362	free-list. One step. This is the common path for the vast majority
363	of values; nearly every box in a typical graph has a declared output
364	size.
365	
366	**Door 2 — runtime-size allocation.** Given a request for an unknown
367	size at runtime: find the smallest size class whose chunks are large
368	enough. If that class is exhausted, try the next-larger. If the
369	chunk handed out is oversized for the request, slice the unused tail
370	into a residual chunk and push that residual onto the appropriate
371	smaller free-list. The residual is only kept if it's at least as large
372	as the smallest declared size class — anything smaller is left as
373	internal waste rather than fragmenting the lists with stubs.
374	
375	Both doors fall through to the same fragmentation-recovery and growth
376	machinery below when their free-lists can't satisfy a request.
377	
378	### Reference counting drives deallocation
379	
380	Every value the allocator hands out is reference-counted. The dispatch
381	layer increments the count when handing a value to a consumer and
382	decrements it when the consumer is done. When the count reaches zero,
383	the chunk is returned to the allocator.
384	
385	The return-to-allocator step does two things atomically: pushes the
386	chunk onto the appropriate free-list AND checks the chunk's two
387	physical neighbors. If either neighbor is also free, the deallocator
388	merges them into a larger chunk and pushes the merged chunk onto the
389	free-list for the merged size. This eager neighbor-merge is O(1) and
390	keeps small-scale fragmentation from accumulating on the hot path.
391	
392	Reference counting requires touching every place that hands out a
393	pointer to an allocator-managed value. That cost is paid once. After
394	the wiring exists, both doors above benefit from the same recycling.
395	
396	### When does the bigger sweep run
397	
398	The eager neighbor-merge above catches the common case. It misses
399	patterns where free chunks are scattered with allocated chunks between
400	them — the cheap merge can't see past its immediate neighbors.
401	
402	A deeper sweep that walks the whole address-ordered chunk list
403	looking for runs of contiguous free chunks runs on two deterministic
404	triggers, both decoupled from wall-clock time:
405	
406	1. **At the moment an allocation would otherwise fail.** When a
407	   request can't be satisfied from any free-list — including after
408	   trying larger classes and slicing — the system runs the deep sweep
409	   before falling back to asking the operating system for more memory.
410	   If the sweep produces a chunk large enough to satisfy the request,
411	   the allocation succeeds and no growth happens. This is the
412	   mandatory trigger: the sweep is the immediate alternative to
413	   monotonic growth, and it fires exactly when its work has value.
414	
415	2. **At quiescence — moments when no task is running.** The dispatch
416	   layer already recognizes these moments because it uses them to
417	   decide a run is complete. Between batches is a natural opportunity
418	   because no tasks are reading or writing chunks at that instant.
419	   This is the opportunistic trigger.
420	
421	The mandatory trigger is enough on its own. The opportunistic trigger
422	is a tunable knob that can be enabled later if profiling shows
423	fragmentation surviving the cheap merge faster than the mandatory
424	trigger catches up.
425	
426	A third option was considered — trigger when the count of small free
427	fragments crosses a threshold — but rejected for now. It works, but
428	it needs the threshold tuned, and the two triggers above cover the
429	same ground without that tuning step.
430	
431	### Physical-order chunk list (for the merge steps)
432	
433	Both the eager neighbor-merge and the deeper sweep need to know
434	which chunks are physically adjacent in memory. The size-class
435	free-lists don't carry that information — they're indexed by size,
436	not address.
437	
438	The supporting structure is a doubly-linked list of all chunks (free
439	and allocated) in address order. Splits and merges are O(1) given a
440	chunk pointer. Neighbor lookup is O(1). The cost is pointer-chasing
441	during the deeper sweep, but the sweep only runs at the two triggers
442	above, not on the hot path.
443	
444	If profiling later shows traversal cost hurting at sweep time, the
445	list can be swapped for a sorted-by-address array of `(start, end)`
446	pairs with an auxiliary index — the allocator's external API doesn't
447	change.
448	
449	### Why one allocator instead of two
450	
451	The split design existed because the original two-tier scheme had a
452	"large-value heap" carved out for variable-size payloads, on the
453	theory that mixing variable and fixed sizes in the same pool would
454	complicate the fixed-size path. That reasoning doesn't survive
455	contact with reference counting and free-list recycling: with
456	refcount-driven free, the fixed-size path stays fast because most
457	requests match a declared class and pop in one step regardless of
458	what else is in the region.
459	
460	Splitting them also meant the variable-size pool had to grow
461	forever, because writing the same allocator twice — once with
462	reclamation, once without — was more work than was budgeted in early
463	phase 3. That's the position the current code is in. Unified gets us
464	the reclamation everywhere, with no extra design work.
465	
466	### Growth (last resort)
467	
468	When both doors fail and the deep sweep can't produce a fitting
469	chunk, the allocator grows by asking the operating system for a new
470	region, dividing it into chunks per the declared size classes (or
471	into a single large chunk if the request was for an unknown size
472	beyond all declared classes), and threading the new chunks into the
473	free-lists and the address-ordered list.
474	
475	Growth is the rare case. With pre-warmed free-lists sized to static
476	demand and refcount-driven recycling, a well-behaved graph never
477	reaches growth — every allocation either pops a pre-warmed chunk or
478	recycles one whose last reference just dropped.
479	
480	## No type tags
481	
482	Slots store opaque bytes. The producer wrote N bytes; the consumer
483	reads N bytes. What those bytes mean is the language spec's business
484	on each side, and the wire's metadata in the graph (issue 219's
485	compiler can check producer-output type vs consumer-input type at
486	build time). The slot store does not interpret values — it does not
487	need to know if they are strings, numbers, JSON, images, or anything
488	else. Removed the type-tag field from the header for that reason.
489	
490	## Suggested implementation sequence
491	1. Write `src/slot-store.h` and `src/slot-store.c` — the C API above.
492	2. Unit test single-value slots: thread A writes, thread B reads,
493	   refcount decrements, slot freed when last ref dropped.
494	3. Unit test ring-buffer slots: multiple producers push, single consumer
495	   drains in order, no values lost or duplicated.
496	4. Integrate into `src/008-pool-runner.c`: shared memory created at
497	   startup, store handle passed to the dispatch layer.
498	
499	## Relevant files
500	- `src/008-pool-runner.c` — pool entry point, owns the shared memory region
501	- `libs/task-pool/900-task-pool.h` — pool API (refcounting is complementary)
502	- `issues/301-pool-lifecycle-and-worker-init.md` — pool lifecycle
503	- `issues/304-task-dispatch-layer.md` — task dispatch reads/writes slots
504	- `issues/213-queued-inputs-and-task-model.md` — queued input model that
505	  ring-buffer slots implement
506	- `docs/004-ipc-and-threading.md` — IPC options; shm_open + mmap is Option 2
507	
508	## Implementation log
509	
510	### Core milestone — 2026-05-12
511	
512	What shipped:
513	- `src/009-slot-store.h` — the public API exactly as the design
514	  describes: `slot_alloc` / `slot_push` / `slot_peek` / `slot_pop` /
515	  `slot_read_inc` / `slot_has_value` / `slot_fill_count`. Plus the
516	  store lifecycle pair and a small inspection helper for tests.
517	- `src/009-slot-store.c` — implementation. Plain-malloc per slot
518	  (size-class free lists deferred); ring slots and atomic-counter
519	  slots both supported; the SLOT_FLAG_TAGGED variant of pop scans
520	  for the lowest-tag filled cell. Per-slot spinlock via
521	  `atomic_flag`. Atomic counter via `atomic_fetch_add_explicit`,
522	  no lock.
523	- `tests/009-slot-store-test.c` — 12 unit tests covering peek
524	  semantics, FIFO pop, tagged pop ordering (in-order and
525	  interleaved), atomic counter (sequential + 8-thread concurrent),
526	  bad-argument paths, ring-full / empty edge cases, and a
527	  multi-thread producer/consumer race over 5000 values.
528	- Makefile additions: `test` target builds test binaries via a
529	  pattern rule and runs them. Per-test prerequisites listed
530	  explicitly so each binary links the minimal set of source `.o`s.
531	- File index counter advanced to `009`.
532	
533	Verified clean build and 12/12 tests pass in `make STRICT=1`
534	(`-Werror -Wextra -Wpedantic`).
535	
536	What's deferred to a follow-on within 302:
537	- **Size-class free lists with coalescing.** The current
538	  implementation `malloc`s each slot independently. That's correct
539	  but unoptimized. Slots are allocated once at graph load and never
540	  freed until run end, so under the current usage pattern free-list
541	  reuse buys nothing. Lands once profiling justifies it (or once
542	  long-running maps surface allocation pressure).
543	- **Large-value heap for variable-size payloads.** Slots currently
544	  store fixed-size cells; very large outputs (multi-MB LLM
545	  responses, dynamically-sized arrays) will need the two-tier
546	  scheme described under "Large-value heap" above. Blocks no
547	  in-flight phase 3 work until a graph wants variable-size outputs.
548	
549	Neither deferred item blocks 301 / 304 / 305 / 306 / 307 / 308.
550	
551	### Large-value heap milestone — 2026-05-19
552	
553	The variable-size payload follow-on landed in two layers:
554	
555	1. **`src/015-large-value-heap.{c,h}`** — chunked arena allocator.
556	   Default 64 KB chunks; oversized allocations (larger than the
557	   default chunk) get their own exact-fit chunk so one large value
558	   doesn't bloat the regular bump chunks. Pointer stability is the
559	   load-bearing property: chunks never move once allocated, so a
560	   consumer's pointer survives any number of subsequent producer
561	   allocations. Eight unit tests in
562	   `tests/015-large-value-heap-test.c` cover the create/destroy
563	   round-trip, single + multi alloc within a chunk, oversized
564	   allocations getting their own chunks, pointer stability across
565	   forced growth, zero-size allocations returning non-NULL, the
566	   stats accessors, and an eight-thread / eight-thousand-alloc
567	   concurrency stress test that asserts every returned pointer is
568	   unique and writable.
569	
570	2. **Slot-store integration.** Added `SLOT_FLAG_LARGE_VALUE` to
571	   `slot_alloc`. Cell layout for LARGE_VALUE slots is
572	   `filled_size (4 bytes) + pad/tag (4 bytes) + stable pointer
573	   (8 bytes)` — the 8-byte payload offset keeps the pointer
574	   naturally aligned regardless of whether SLOT_FLAG_TAGGED is
575	   also set. The slot store lazy-creates one shared lvh heap on
576	   the first LARGE_VALUE allocation; the heap is destroyed
577	   alongside the store. Push allocates from the heap *outside*
578	   the slot spinlock (the heap has its own mutex; nesting is
579	   fine, but reducing time-under-spinlock keeps the lock fast for
580	   other producers). Peek and pop follow the cell's pointer and
581	   memcpy the requested bytes back into the caller's buffer.
582	
583	Four new slot-store tests cover the new path:
584	`large_value_push_pop` (100 KB payload, double-peek, then pop);
585	`large_value_buf_too_small` (oversized payload + small dest →
586	-1, cell preserved); `large_value_multi_push_fifo` (three
587	different-size values pushed and popped in order); and
588	`large_value_tagged_ordering` (LARGE_VALUE + TAGGED → pops
589	return lowest-tag-first with bytes intact).
590	
591	What this unblocks: any box that wants to emit a variable-size
592	output can now declare its downstream consumer slots with
593	SLOT_FLAG_LARGE_VALUE and produce payloads whose size isn't
594	known at graph-load time. The wiring from declared box output
595	type to the LARGE_VALUE flag in `graph_attach_runtime` lands as
596	a follow-on once a fixture or real graph requires it; the
597	foundation is in place.
598	
599	### Producer-driven LARGE_VALUE wiring — 2026-05-19
600	
601	The "wiring from declared box output type" deferred above is
602	now in place. During `graph_attach_runtime`, each consumer
603	input slot's flags get OR'd with `SLOT_FLAG_LARGE_VALUE` if any
604	producer feeding that port declares `output_capacity == 0`. The
605	rule covers both data boxes (which default to 0 because the
606	field isn't in their JSON schema) and call boxes that opt into
607	variable-size output by setting `"output_capacity": 0`. A
608	mixed-language fan-in where any producer is variable-size
609	upgrades the whole slot to LARGE_VALUE — the conservative call,
610	since the slot can only carry one format and the LVH path
611	handles small payloads correctly too.
612	
613	A new test in `tests/010-graph-loader-test.c`
614	(`variable_size_producer_lvh_wiring`) attaches the hello fixture
615	with a deliberately tight default_cell_bytes of 512, then
616	asserts a 2 KB push succeeds on greet's `name` port (fed by
617	data box `who`) and fails on greet's `salutation` port (literal
618	only, no producer). The discriminator is the slot's capacity:
619	without LARGE_VALUE the 2 KB push exceeds 512 and is rejected;
620	with it the heap absorbs the bytes and the push lands.
621	
622	The remaining phase-3 checklist item "Variable-size outputs
623	work via the large-value heap" is now satisfied end-to-end at
624	the slot-store boundary. A graph fixture that exercises the
625	path on the dispatch side — a Lua box returning a multi-KB
626	string and a downstream box reading it — is a one-line fixture
627	addition and lands with the 6-more-integration-fixtures task.
628	
629	What's still deferred within 302:
630	- **Size-class free lists with coalescing.** Still no need.
631	- **Per-allocation free in the large-value heap.** The heap
632	  grows monotonically for the life of a run. Long-running
633	  iterator loops that emit large values will pin memory until
634	  run end. Acceptable for current fixtures; a size-class
635	  allocator with eager-coalesce is the upgrade path if profiling
636	  flags it.
637	
638	### Retraction of the monotonic-growth deferral — 2026-05-19
639	
640	The two deferrals listed immediately above this entry no longer
641	reflect the design. Monotonic growth was being described as
642	"acceptable for current fixtures." It isn't acceptable — it's a
643	memory leak by design, and any long-running workload (multi-day
644	runs, anything driven by an LLM in a loop, anything that
645	accumulates) will reproduce it as a real problem. Calling it
646	deferred made it easier to skip; calling it a leak makes it clear
647	why the skip isn't safe.
648	
649	The design above this implementation log has been rewritten
650	accordingly. There are no longer two stores. There is one
651	allocator that serves both compile-time-known sizes and
652	runtime-only-known sizes from a shared pool, with reference
653	counting driving deallocation, an O(1) eager neighbor-merge on
654	return, and a deeper sweep on the two triggers described in the
655	allocator section ("at the moment an allocation would otherwise
656	fail" and "at quiescence between task batches"). Both triggers
657	are deterministic — they react to the program's own behavior
658	rather than to wall-clock time.
659	
660	What this means in terms of work remaining:
661	
662	- The chained-block region currently serving variable-size values
663	  gets replaced by the unified allocator above. The slot-store
664	  cells that hold large-value pointers stay; what they point into
665	  changes from the bump-allocated chain to the size-class
666	  allocator's chunks.
667	- The slot store's small-fixed-size path adopts the same
668	  allocator. Today that path malloc's each slot's cell-array
669	  separately; under the new design every cell-array is a chunk
670	  from the unified allocator.
671	- The dispatch layer learns to increment and decrement reference
672	  counts when it hands values from producer to consumer. This is
673	  the widest change in scope — many places — but each individual
674	  change is one line. The new allocator depends on these calls
675	  being correct: a missed decrement leaks; a missed increment
676	  frees a value that's still in use.
677	- The address-ordered chunk list (described in the allocator
678	  section) is added alongside the size-class lists.
679	- The deeper sweep is implemented and wired into the two trigger
680	  points.
681	
682	The implementation has not been started; this entry is the
683	design commitment, not a completion notice. The earlier
684	"acceptable for current fixtures" framing is withdrawn.
685	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/303-language-runtime-spec.md`

## Assistant

Setting up tasks and continuing to read the remaining open issues in parallel.

## Assistant

_Tools:_

- `TaskCreate: Read all 24 open issue files (phase 2: 217, 231, 232, 233, 239; phase 3: 301-317 except 316) plus phase progress files. The issues are the product — must understand the full scope before any implementation.`

### Tool result

```
Task #1 created successfully: Read all 24 open issues cold
```

## Assistant

_Tools:_

- `TaskCreate: For each phase-3 open issue, pull its pre-implementation shape from git log. User stated phase-3 issues were "up to date and correct before we started implementing them" — drift during implementation needs to be reconciled.`

### Tool result

```
Task #2 created successfully: Audit phase 3 issues vs git log originals
```

## Assistant

_Tools:_

- `TaskCreate: Phase 2 is mostly deprecated but should be implemented first to have a stable base. Open: 217, 231, 232, 233, 239.`

### Tool result

```
Task #3 created successfully: Implement and close phase-2 stragglers
```

## Assistant

_Tools:_

- `TaskCreate: Phase 3 thread pool work. Open: 301-317 (16 issues). Expect significant pruning — systems grown into shapes that work for narrow cases but need broader coherent replacements.`

### Tool result

```
Task #4 created successfully: Implement and close phase-3 issues
```

## Assistant

_Tools:_

- `TaskCreate: After all issues closed: walk through completed/ directory, justify each issue's existence. Set must collectively rebuild the project from scratch. Update those that fail. Final test: copy issue files to a new directory, hand to a downstream consumer to rebuild.`

### Tool result

```
Task #5 created successfully: Justify each completed issue for blueprint handoff
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

### Tool result

```
1	# 304 — Task dispatch layer (C, replaces synchronous executor)
2	
3	## Status
4	open
5	
6	## Current behavior
7	The synchronous executor at `src/004-executor.lua` walks the graph in
8	dependency order, calling each box's driver, capturing stdout, decoding
9	JSON, and propagating values to downstream boxes. Single-threaded,
10	blocking, runs in Lua coroutines.
11	
12	## Concept
13	
14	The task dispatch layer is the C component that runs inside each pool
15	worker. The pool calls a single C action — `dispatch_action` — per
16	task. That action is the one place in the runtime that:
17	
18	- Reads from and writes to slots
19	- Calls `lang_spec->invoke`
20	- Picks the output branch per `routing.kind` (issue 233)
21	- Pushes the function's output to the picked branch's downstream
22	  input slots, triggering downstream task spawns
23	
24	There is no Lua-level executor in the phase 3 path. `004-executor.lua`
25	is replaced wholesale.
26	
27	## Task struct
28	
29	A task is one ephemeral invocation of a box. Its struct carries
30	just the box ID:
31	
32	```c
33	typedef struct {
34	    int  box_id;          // index into the graph's box array
35	} dispatch_task_t;
36	```
37	
38	There is no per-task counter. Iterator counter state lives in a
39	`SLOT_ATOMIC_COUNTER` slot owned by the box (see issue 302); the
40	action reads and atomically increments it via `slot_read_inc`
41	when it needs the routing index.
42	
43	There is no `input_slots` or `output_slot` array on the task —
44	the box's input slots are durable per-box state (see issue 302).
45	The action looks them up through `box_runtime_state[box_id]`.
46	
47	Output is a routing event, not a stored value. When the action
48	finishes, it pushes copies of the return value into the input
49	slots of every downstream box wired to this output (or only the
50	matching branch for comparators and iterators).
51	
52	Tasks are spawned by the dispatch layer's "spawn on input ready"
53	rule (issue 302). Each spawn allocates a fresh `dispatch_task_t`
54	on the pool's task allocator, pre-populated with the box ID. The
55	action's first read of the input slots is what drains them — pop
56	or peek one cell per input port, per the port's mode.
57	
58	## The dispatch action
59	
60	```c
61	action_result_t dispatch_action(task_ctx_t *ctx, void *arg);
62	```
63	
64	Phases, in order:
65	
66	### 1. Read inputs into byte buffers
67	
68	Tasks are spawned only when every input port has a value
69	available (issue 302's spawn-on-input-ready rule), so the action
70	does not need to check or block — input availability is the spawn
71	precondition.
72	
73	For each input port:
74	- **Peek-mode (1-cell) port**: `slot_peek` into a heap buffer.
75	  The cell is not drained; subsequent tasks on this box read the
76	  same value.
77	- **Pop-mode (N-cell) port**: `slot_pop` into a heap buffer. One
78	  cell drains; values queued behind it remain. For tagged slots,
79	  pop returns the lowest-tag cell; for untagged, FIFO.
80	
81	Variable-size payloads work via the large-value heap (issue 302):
82	the cell holds a `{size, offset}` handle that points into the
83	heap; the spec follows the handle if it needs the bytes.
84	
85	Buffers are freed when the action ends.
86	
87	### 3. Dispatch is uniform across routing kinds
88	
89	Every call box runs its function (phase 4) and pushes the
90	function's output (phase 5). The only thing that varies is which
91	downstream branch receives the push (phase 5b — the branch-pick).
92	
93	Boxes carry an optional `routing` field (issue 233):
94	- No `routing`: plain call. Function runs; output fans to all
95	  outgoing wires unconditionally.
96	- `routing.kind: "comparator"`: function runs; output is
97	  compared to `comparand`; only the matching `lt` / `eq` / `gt`
98	  branch fires.
99	- `routing.kind: "iterator"`: function runs; output is routed to
100	  `outputs[slot_read_inc(counter_slot, n_outputs)]`.
101	- `routing.kind: "randomizer" / "weighted" / "distributor"`:
102	  function runs; output is routed by the kind-specific rule.
103	
104	The function always runs. There is no "iterator skips invoke"
105	case — the iterator differs from a plain call only in how the
106	function's output is routed, not in whether the function runs.
107	Iterators that want passthrough behavior use an identity function
108	(or the editor defaults to one when no `ref`/`fn` is set).
109	
110	### 4. Invoke (every call box)
111	Look up the box's language spec by its `lang` field. Find the
112	worker's language handle in `current_worker->handles[lang_idx]`.
113	Call `lang->invoke` with:
114	- `handle` — the per-worker language runtime state
115	- `file_path`, `fn_name` — the box's function source
116	- `input_data[]`, `input_sizes[]` — pointers and sizes for each
117	  input buffer
118	- `out_buf`, `out_buf_capacity`, `*out_size` — the output buffer the
119	  spec writes into
120	
121	The spec is the bridge between the language's native call convention
122	and bytes. Inside `invoke` it:
123	1. Loads the function (cached: e.g. `luaL_loadfile` once per file,
124	   `dlsym` once per function).
125	2. Pushes the input bytes into the language's native types (Lua
126	   strings on the Lua stack, C pointers in registers, etc.).
127	3. Calls the function and receives the return value in the language's
128	   native form.
129	4. Serializes the return value back into bytes in `out_buf` and
130	   sets `*out_size`.
131	5. Returns 0 for success, nonzero for any failure.
132	
133	Nonzero return aborts the program (issue 303).
134	
135	The output buffer for `invoke` is heap-allocated to the box's
136	declared maximum output size. If the box's output is variable-size
137	(uses the large-value heap, issue 302), the spec writes into that
138	heap and stores the handle in `out_buf`.
139	
140	### 5. Write output (push to downstream input slots)
141	
142	The action picks the branch (or branches) and pushes the
143	function's output. Branch picking dispatches on `routing.kind`:
144	
145	- **No routing field** (plain call): push to every outgoing
146	  connection.
147	- **comparator**: compare output to `comparand`, push to the
148	  matching `lt` / `eq` / `gt` connection only.
149	- **iterator**: `idx = slot_read_inc(box->counter_slot,
150	  n_outputs)`; push to the connection at branch `out_idx`.
151	- **randomizer / weighted / distributor**: kind-specific rule,
152	  same shape (compute `idx`, push to that one connection). See
153	  issue 233 for the rules.
154	
155	For routing kinds that derive an order tag (iterator,
156	randomizer, weighted), the push carries `tag = idx` so consumers
157	downstream of parallel iterators preserve order.
158	
159	After each push, the spawn-on-input-ready check (issue 302) fires
160	on the receiving box. If that push completes its input set, a
161	fresh task spawns for the consumer.
162	
163	### 6. (No phase 6)
164	
165	Slots are durable for the run; tasks don't unref. Run termination
166	is governed by the pool's active-task counter (issue 301). The
167	dispatch action goes from phase 5 directly to phase 7.
168	
169	### 7. Post-action: nothing for the action itself
170	
171	Routing already happened in phase 5. Counter advancement, where
172	applicable, happened inside `slot_read_inc` during phase 5 (the
173	read-and-increment is the same op). No box-level mutable state
174	needs touching here.
175	
176	### 8. Return ACT_DONE
177	The action completes. The pool decrements the active-task counter
178	(issue 301). If the counter reaches zero and the runner is waiting,
179	the run ends.
180	
181	## Initial submission
182	
183	At startup, the pool runner walks the graph and pushes the
184	literal-input values (from `value` fields on input ports) directly
185	into the corresponding input slots. Boxes whose input set becomes
186	fully populated by literals — entry-point boxes — immediately
187	satisfy the spawn-on-input-ready condition, and their first task
188	gets queued. From there, every subsequent task spawn follows from
189	the regular phase-5 push → spawn-check chain.
190	
191	Iterator counter state starts at 0; literal-only entry iterators
192	spawn their first task at startup and increment from there.
193	
194	## Iterator counter and parallel iteration
195	
196	The iterator's counter lives in a `SLOT_ATOMIC_COUNTER` slot
197	allocated for the box at graph load time (issue 302). The
198	dispatch action reads and increments it atomically, mid-action:
199	
200	```c
201	uint32_t branch_idx = slot_read_inc(box->counter_slot, n_iter_outputs);
202	slot_push(downstream[branch_idx].input_slot, output_buf, output_size,
203	          /* tag = */ branch_idx);
204	```
205	
206	`slot_read_inc` returns the current counter value `mod
207	n_iter_outputs` and atomically advances the underlying counter.
208	Concurrent iterator tasks on different workers each call
209	`slot_read_inc` independently and each get a unique value —
210	**iterator tasks parallelize across workers**, picking distinct
211	branches without coordination.
212	
213	No box-side mutable state, no per-task counter snapshot. The
214	counter is just data in a slot the dispatch layer reads.
215	
216	## Cross-iterator pairing under parallel iteration
217	
218	With parallel iterator tasks, push ordering at the consumer is
219	non-deterministic — task 6 of iterator A may push to consumer C
220	before task 5 if 6 happened to land on a faster worker. FIFO pop
221	at C would consume them out of iteration order.
222	
223	The fix is **counter-tagged pushes**. Each iterator push (in phase
224	5) carries the counter value the action read for this invocation:
225	
226	```c
227	slot_push(downstream.input_slot, output_buf, output_size,
228	          /* tag = */ branch_idx_or_counter_snapshot);
229	```
230	
231	Consumer slots downstream of iterators are allocated with the
232	`SLOT_TAGGED` flag (issue 302). `slot_pop` on a tagged slot
233	returns the cell with the lowest tag — preserving iterator-counter
234	order regardless of push race.
235	
236	Compile-time analysis decides which slots are tagged: any slot
237	whose feeding wire originates (transitively, through a fan-out
238	chain) at an iterator branch gets tagged. Slots not downstream of
239	iterators are untagged and pop FIFO at no extra cost.
240	
241	Plain-call and comparator pushes carry tag = 0 (or the snapshot of
242	their own iterator-ancestor's counter, if applicable, propagated
243	through their phase-5 pushes). Tags compose naturally through
244	multi-hop chains.
245	
246	### Cross-iterator pairing of multiple iterator inputs
247	
248	When non-iterator consumer C has two input ports wired from two
249	different iterators A and B, each input port has its own tagged
250	queue. Both queues pop in tag order independently:
251	- C's task K pops the K-th tag from A's queue (which equals A's
252	  K-th invocation's value).
253	- The same K-th tag from B's queue.
254	
255	Pairing by tag = `(A_K, B_K)` for every K, deterministic
256	regardless of which iterator's tasks run faster.
257	
258	## Routing kinds (issue 233)
259	
260	Every call box runs its function. Routing kinds differ only in
261	how the function's output is dispatched to downstream
262	connections:
263	
264	| `routing.kind`   | Output ports        | Branch picker                                                |
265	|------------------|---------------------|--------------------------------------------------------------|
266	| (absent / plain) | single port         | fan to all outgoing connections                              |
267	| `comparator`     | `lt` / `eq` / `gt`  | compare(output, comparand) → lt/eq/gt                        |
268	| `iterator`       | `out_0` … `out_N-1` | `slot_read_inc(counter_slot, N)`                             |
269	| `randomizer`     | `out_0` … `out_N-1` | `hash(slot_read_inc(counter_slot, MAX)) % N`                 |
270	| `weighted`       | `out_0` … `out_N-1` | cumulative-band lookup against a counter scaled to PRECISION |
271	| `distributor`    | `out_0` … `out_N-1` | argmin over downstream slot fill levels                      |
272	
273	Plain call (no `routing` field) skips the branch picker entirely
274	— phase 5 fans the output to all wires. The function still runs
275	the same as for any branched call. Issue 233 carries the schema
276	and the per-kind UI.
277	
278	## Open questions
279	
280	(none currently — earlier questions resolved as follows:)
281	
282	- Output buffer sizing: heap-allocated to the box's declared output
283	  size at runtime. No fixed cap. Variable-size outputs go through the
284	  large-value heap (issue 302).
285	- Queued-input slot lifetime: producers hold refs while alive,
286	  consumer holds a ref while consuming, slot freed when all drop.
287	  References are the producer-liveness tracking mechanism.
288	- Comparator output encoding: dispatch layer handles comparison
289	  semantics directly. No spec involvement, no encoded "branch tag" in
290	  the slot — the branch is a routing-time value computed by the
291	  dispatch layer from the input.
292	
293	## Suggested implementation sequence
294	
295	1. Define `dispatch_task_t` and the `dispatch_action` skeleton.
296	2. Implement phase 1 (input check + ACT_BLOCK on slot wait list).
297	3. Implement phase 2 (input read into heap-allocated buffers).
298	4. Implement phase 4 (invoke for plain call boxes).
299	5. Implement phase 5 (output write via slot_push).
300	6. Implement phase 6 (unref consumed single-value inputs).
301	7. Smoke test: `hello` map (one plain call box, no routing) end-to-end
302	   through the pool. Assert the output slot contains the expected
303	   value.
304	8. Implement plain-call fan-out firing (phase 7, plain case).
305	9. Implement comparator routing (phase 3 + phase 7 comparator case).
306	   Test `branch-test` map.
307	10. Implement iterator routing and self-respawn (phase 3 + phase 7
308	    iterator case). Test an iterator map.
309	11. Wire up entry-box initial submission in the pool runner.
310	
311	## Relevant files
312	
313	- `src/004-executor.lua` — synchronous executor that this layer replaces
314	- `issues/301-pool-lifecycle-and-worker-init.md` — pool that runs this action
315	- `issues/302-wire-value-slot-store.md` — slot API used here
316	- `issues/303-language-runtime-spec.md` — `invoke` interface called here
317	- `issues/221-iterator-box.md` — iterator routing and self-rescheduling
318	- `issues/completed/108-branch-box-and-predicate-routing.md` — comparator routing
319	- `issues/213-queued-inputs-and-task-model.md` — queued-input slot semantics
320	
321	## Implementation log
322	
323	### Skeleton — 2026-05-12
324	
325	`src/012-dispatch.{c,h,info.md}` ships the types and the action's
326	signature with a stub body that increments a diagnostic counter
327	on the context and frees the task. `dispatch_ctx_t` bundles the
328	graph, slot store, spec registry, and pool — every task carries a
329	pointer to the context so the action has everything it needs
330	without globals. `dispatch_spawn(ctx, box_id, priority)`
331	allocates a task and calls `pool_spawn`.
332	
333	Two skeleton tests in `tests/012-dispatch-test.c` build the full
334	runtime (graph + slots + registry + pool) and submit either one
335	task per box, or 200 tasks at varied priorities, and assert that
336	the counter advances by the right amount.
337	
338	What's still ahead inside 304:
339	- Reading inputs from per-port slots (peek vs pop based on
340	  compile-time wire classification — that classification work
341	  itself is the deferred size-class step in 305).
342	- Invoking the per-worker spec handle. Needs 303's pool hook in
343	  place so each worker has its handles populated; right now the
344	  context's `specs` registry is reachable but worker handles
345	  aren't routed through the worker context.
346	- Picking the output branch per `routing.kind` and pushing to
347	  consumer slots; this is the routing-table dispatch from the
348	  unified schema in issue 233.
349	- Spawn-on-input-ready chaining: each push to a consumer slot
350	  triggers a check, which may spawn the consumer's next task.
351	
352	### Plain-call dispatch + spawn-on-ready — 2026-05-12
353	
354	`src/012-dispatch.c` now has a real action body. For each task:
355	1. Look up the box from the graph.
356	2. Read every input port via `slot_peek` into a heap buffer.
357	3. Dispatch on `box->kind`:
358	   - **BOX_CALL** — resolve the per-worker spec handle via
359	     `pool_current_worker->handles[box->spec_idx]`, call
360	     `spec->invoke(handle, ref_path, fn_name, inputs, sizes,
361	     n_present, out_buf, out_cap, &out_size)`.
362	   - **BOX_DATA** — open `map_dir/box->path`, read up to
363	     `out_capacity` bytes into the output buffer.
364	   - **BOX_FILE_WRITE** — read inputs "path" and "text",
365	     `fwrite` text to the resolved path.
366	4. Capture the output for inspection (optional), push to every
367	   outgoing connection's input slot, and fire
368	   `dispatch_spawn_if_ready` on each consumer.
369	5. Emit `task_start` / `task_end` events through the optional
370	   event queue (issue 311) with monotonic-clock durations.
371	
372	Three end-to-end fixture maps cover the major paths:
373	
374	- `tests/maps/calc/` — single Lua call box with two literal
375	  inputs. `add(17, 25) = "42"`.
376	- `tests/maps/hello/` — two-box data → Lua pipeline.
377	  `who → "World"` → `greet → "Hello, World!"`.
378	- `tests/maps/pipeline/` — five-box multilang chain.
379	  `data "5" → Lua double "10" → C addone "11" →
380	  Bash shout "11!" → file_write` writes `11!` to disk.
381	
382	Four dispatch tests in `tests/012-dispatch-test.c` exercise the
383	calc fixture (with and without literals), a counter-burst
384	verifying many concurrent tasks against the same box, and the
385	skeleton dispatch path.
386	
387	Still ahead inside 304:
388	- **Iterator multi-spawn.** Each iterator task should re-spawn
389	  itself when its input queue still has values; that needs the
390	  N-cell pop-mode slot shape from 302 and a different
391	  `box_ever_spawned` rule.
392	- **Routing kinds beyond plain.** Comparator routes by output
393	  value; iterator/randomizer/weighted/distributor route by the
394	  atomic counter or downstream fill. The hooks in `box->routing`
395	  are populated by 305; only the dispatch branch picker is
396	  missing.
397	- **Variable-size payloads via the large-value heap** (issue 302).
398	
399	### Comparator + iterator routing — 2026-05-12
400	
401	`push_routed(ctx, b, bytes, size)` now branches on
402	`b->routing.kind`:
403	- **PLAIN** — fan to every outgoing connection (existing behavior).
404	- **COMPARATOR** — parse the output as a double via `strtod`,
405	  compare against `b->routing.comparand`, push to the
406	  matching `lt` / `eq` / `gt` connection only.
407	- **ITERATOR** — `slot_read_inc(ctx->slots, b->counter_slot_id,
408	  n_outputs)` picks the branch index, then push to the
409	  `out_<idx>` connection only.
410	- **RANDOMIZER / WEIGHTED / DISTRIBUTOR** — fall back to plain
411	  for now; deferred to follow-ons.
412	
413	A new `push_branch(ctx, b, branch, bytes, size)` helper filters
414	connections by `from_branch` and pushes to each match (also
415	firing the spawn-on-input-ready check on each consumer).
416	
417	The randomizer/weighted/distributor enum values stay reserved
418	in the routing-kind table; the dispatch falls through to plain
419	so nothing deadlocks.
420	
421	Two fixture maps cover the two routing kinds end-to-end:
422	
423	- `tests/maps/comparator/` — `classify` (lua, comparator,
424	  `comparand=5`) feeds three sinks `low`/`mid`/`high` via
425	  `from_branch="lt"/"eq"/"gt"`. With literal `v="8"`, only the
426	  `high` sink runs and captures `"HIGH:8"`.
427	- `tests/maps/iter-route/` — `iter` (lua, iterator,
428	  `n_outputs=3`) feeds three sinks `a`/`b`/`c` via
429	  `from_branch="out_0"/"out_1"/"out_2"`. First invocation
430	  (counter=0) routes to `a`; manual `dispatch_spawn` calls then
431	  walk through `b` and `c` as the counter advances.
432	
433	Three new tests in `tests/012-dispatch-test.c`: comparator
434	routing, single-fire iterator routing, and a 3-call round-robin
435	iterator covering all three branches.
436	
437	### Task-id correlation — 2026-05-12
438	
439	`dispatch_task_t` gained a `task_id` field assigned at
440	`dispatch_spawn` time (atomic fetch-add on
441	`ctx->next_task_id`). The same id correlates the
442	`task_submit` / `task_start` / `task_end` events emitted
443	through the event queue. `task_submit` lands on the spawning
444	thread (worker_idx = -1 since the task hasn't been picked up
445	yet); start/end land on the dispatching worker with its real
446	`thread_idx`.
447	
448	### Iterator multi-spawn + N-cell pop reads — 2026-05-12
449	
450	Three pieces together unlock real iteration:
451	
452	1. **Per-input slot mode.** `box->input_slot_modes[i]` is set by
453	   `graph_attach_runtime` to either `SLOT_MODE_PEEK` (single
454	   cell, the legacy path) or `SLOT_MODE_POP` (multi-cell ring;
455	   each read drains one cell). `read_inputs` picks `slot_pop`
456	   or `slot_peek` based on the mode.
457	2. **multi_spawn propagation.** Iterators are marked
458	   `multi_spawn = 1`; a forward BFS propagates the flag through
459	   the connection graph so every box reachable from an iterator
460	   is also `multi_spawn`. Multi-spawn boxes get N-cell pop slots
461	   (currently 16 cells); single-spawn boxes keep their 1-cell
462	   peek slots.
463	3. **Auto-re-spawn.** After a multi-spawn box's action completes
464	   successfully, it calls `dispatch_spawn` on itself if any POP
465	   input still has queued values. A single push wakes the entire
466	   chain — an iterator with three queued inputs walks the counter
467	   through all three branches without external intervention.
468	
469	The single-spawn guard now applies only to single-spawn boxes.
470	Multi-spawn ones spawn on every push; the per-slot atomic-flag
471	spinlock serializes pops so two concurrent tasks each get a
472	distinct cell.
473	
474	`test_iterator_multi_fire` in `tests/012-dispatch-test.c` is the
475	headline test: push three values to the iterator's input slot,
476	spawn once, all three branches a/b/c fire with the right tagged
477	output. The atomic counter walks 0 → 1 → 2 across the
478	auto-re-spawned tasks.
479	
480	### Cell-tagged ordering across parallel iterator workers — 2026-05-19
481	
482	Earlier multi-spawn slot pushes carried `tag = 0`; pop order was
483	fill order (which is FIFO under the per-slot spinlock but
484	non-deterministic across producer interleaving).
485	
486	Tag propagation is now wired through:
487	
488	- `push_one_connection` / `push_branch` take an explicit `tag`
489	  argument forwarded to `slot_push`.
490	- Iterator routing passes the freshly-incremented counter as
491	  the tag; randomizer and weighted routing pass their counter
492	  snapshot so any downstream tagged slot still serves in stable
493	  order; plain/comparator pushes pass `0`.
494	- `graph_attach_runtime` sets `SLOT_FLAG_TAGGED` on every
495	  multi-spawn box's input slots. The slot store's tagged-pop
496	  rule (lowest tag first; FIFO among ties via filled-index
497	  scan) means that even when two iterator tasks run on
498	  different workers in opposite order, the downstream consumer
499	  reads them in invocation order.
500	
501	Verified by re-running all 6 fixture maps and the dedicated
502	`test_iterator_multi_fire` test (which now exercises tagged
503	input slots).
504	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/305-c-graph-loader.md`

### Tool result

```
1	# 305 — C graph loader (replaces 003-loader.lua)
2	
3	## Status
4	open
5	
6	## Current behavior
7	The phase 2 loader at `src/003-loader.lua` reads a map directory's
8	`boxes/*.json`, `data/*.json`, and metadata, decodes JSON via dkjson,
9	runs schema validation, builds the in-memory graph table, and detects
10	entry boxes. It is Lua. The synchronous executor consumes its output.
11	
12	The phase 3 pool runner (issue 301) cannot embed a Lua state for
13	loading — Lua appears in the runner only as a per-box language runtime,
14	never as the runner's own scripting layer. The loader is therefore
15	ported to C and lives inside (or alongside) `src/008-pool-runner.c`.
16	
17	## Concept
18	
19	The C graph loader is the first phase of the pool runner's startup. It:
20	1. Walks the map directory.
21	2. Parses every JSON file.
22	3. Validates schema and connection topology.
23	4. Builds a single `graph_t` struct that the pool runner and dispatch
24	   layer consume.
25	
26	The loader is a one-shot at startup. It crashes the program on any
27	validation failure (per the hard-crash policy in issue 303). There is
28	no recovery, no skip-bad-boxes mode.
29	
30	## Inputs
31	
32	The map directory layout (unchanged from phase 2):
33	
34	```
35	maps/<name>/
36	    meta.json          ← entry box, src_dirs, run-level metadata
37	    boxes/*.json       ← one file per box
38	    data/*.json        ← static data values (literal inputs)
39	    src/*              ← box function source files (resolved via meta.src_dirs)
40	```
41	
42	The loader is given the map directory path as a C string.
43	
44	## Output: `graph_t`
45	
46	```c
47	typedef struct {
48	    int           n_boxes;
49	    box_t        *boxes;            // array of box descriptors
50	    int           n_connections;
51	    connection_t *connections;
52	    int           n_size_classes;
53	    int          *size_classes;     // distinct slot sizes the graph uses
54	    int           n_entry_boxes;
55	    int          *entry_box_ids;    // indices into boxes[]
56	    int           n_languages;
57	    const char  **languages;        // distinct language names used
58	} graph_t;
59	
60	typedef struct {
61	    char         *id;               // user-defined box id ("classifier", "stamp", …)
62	    char         *kind;             // "call" | "data"
63	    char         *lang;             // "lua" | "c" | "bash" | ...; NULL for data and iterator/comparator
64	    char         *ref;               // file path; NULL for data/iterator/comparator
65	    char         *fn;                // function name; NULL for data/iterator/comparator
66	    int           n_inputs;
67	    input_decl_t *inputs;
68	    int           output_capacity;  // bytes; 0 if variable-size (uses large-value heap)
69	    int           is_comparator;
70	    int           is_iterator;
71	    int           n_iter_outputs;
72	    char        **iter_output_names;
73	    // …
74	} box_t;
75	
76	typedef struct {
77	    int   from_box_id;
78	    char *from_branch;              // NULL for plain call; "lt"/"eq"/"gt" for comparator;
79	                                    // iterator output name otherwise
80	    int   to_box_id;
81	    char *to_input_name;
82	    int   to_input_index;
83	} connection_t;
84	```
85	
86	Strings are owned by the graph (allocated from a single arena attached
87	to the graph; freed when the graph is destroyed at the end of the run).
88	
89	## Loading phases
90	
91	### 1. Directory walk
92	Open the map directory, enumerate `boxes/`, `data/`, and the optional
93	`meta.json`. Each entry is queued for parsing.
94	
95	### 2. JSON parse
96	Use the vendored JSON parser (issue 301 lists cJSON / jsmn / hand-rolled
97	as candidates). Each file becomes a parsed JSON tree. Parse errors are
98	fatal — print the file path and the parser's error message, abort.
99	
100	### 3. Schema validation
101	Box kinds and what they mean at runtime:
102	
103	- **`call`** is a verb — a box that runs a function (or, for iterator
104	  and comparator variants, a dispatch-layer routing primitive). Each
105	  call box becomes a `dispatch_task_t` in the pool when its inputs
106	  are ready.
107	- **`data`** is a noun — a literal value embedded in the map. It has
108	  no function and produces no task in the pool. Its output slot is
109	  allocated and filled at startup by the loader and remains filled
110	  for the duration of the run; downstream consumers read it like any
111	  other slot.
112	
113	Required fields per kind:
114	- `id`, `kind` always required.
115	- For `kind == "call"`: either (a) `lang`, `ref`, `fn` (plain call
116	  box), or (b) `iterator_outputs` (iterator box, no `ref`/`fn`), or
117	  (c) `comparator` field set (comparator box, no `ref`/`fn`).
118	- For `kind == "data"`: a `value` field; no `inputs` or `connections`
119	  (other than outgoing wires from this box's output slot).
120	
121	Box ids must be unique across the map. Connection endpoints must
122	reference valid box ids and valid input names.
123	
124	Schema rules mirror `src/001-schema.lua`. The C validator is a port of
125	that file's logic, simplified by the absence of port-named branching
126	(removed in issue 108 cleanup).
127	
128	### 4. Topology validation
129	- Every connection's `from_box` and `to_box` exist.
130	- Every connection's `to_input_name` matches a declared input on the
131	  destination box.
132	- No non-iterator cycles. The validator runs DFS; any cycle that does
133	  not pass through an iterator box is rejected.
134	- Every required input has at least one connection feeding it (or is
135	  fed by a `data` box).
136	
137	Validation failure aborts the program with a precise error message —
138	which box, which connection, what was wrong.
139	
140	### 5. Slot size class enumeration
141	Walk every box and queued-input declaration to enumerate the distinct
142	slot sizes the graph will need. Each unique size becomes a size class
143	in the slot allocator (issue 302). Output:
144	
145	```c
146	graph->size_classes      = { 64, 256, 1024, 65536 };  // example
147	graph->n_size_classes    = 4;
148	```
149	
150	The slot allocator pre-populates each class's free list with
151	`n_workers × (count of boxes whose output is this size)` slots. This
152	covers the worst case — every worker concurrently producing an output
153	at this size class — without overshooting. Demand beyond this triggers
154	growth (issue 302); the per-thread multiplier is the right ceiling for
155	the static count, while leaving headroom for iterators that allocate
156	per-invocation slots.
157	
158	### 6. Entry-box detection
159	A box is an entry box if either:
160	- It has zero inputs, or
161	- All of its inputs are connected only to `data` boxes (literal values
162	  with no upstream computation).
163	
164	Entry boxes are submitted to the pool first; everything else fires from
165	post-action routing in the dispatch layer.
166	
167	A map can have any number of entry boxes. The user lays out the
168	initial program state via `data` boxes and entry-point call boxes; the
169	runner submits all entries at once, and the graph propagates from
170	there. There is no singular `main`.
171	
172	### 7. Language enumeration
173	Collect the distinct `lang` values across all plain call boxes. The
174	pool runner uses this set to decide which language specs (issue 303)
175	to load and which per-worker handles to initialize.
176	
177	Iterator and comparator boxes have no `lang` and contribute nothing
178	here.
179	
180	### Note on built-in primitives vs shipped libraries
181	
182	There are two distinct categories of "things the user gets out of the
183	box," and they live in different places:
184	
185	- **Dispatch-layer primitives** are graph-topology operations whose
186	  semantics are routing, not data transformation. Iterator and
187	  comparator are the two so far. They have no `lang` / `ref` / `fn` —
188	  the dispatch layer implements their behavior directly. Splitter
189	  / fan-out is already free via wire fan-out and does not need its
190	  own box kind. Possible future primitives: a "gate" (forward only
191	  when a control input is truthy), a "merge" (forward whichever
192	  input arrives first). These are not on the roadmap; flagged here
193	  so the door stays open.
194	- **Shipped libraries** are data-transformation utilities — JSON
195	  parse/serialize, file read/write, string concat/split, regex,
196	  shell-out. These are language-specific (string semantics differ
197	  between Lua, C, and Bash) and ship as `libs/<name>.<ext>` per
198	  language. They are not built into the dispatch layer; they are
199	  ordinary call-box source files that users wire into their maps.
200	  `libs/ollama.lua` is an existing example.
201	
202	The distinction matters: dispatch-layer primitives change the graph
203	model; shipped libraries don't. A dispatch primitive can do things a
204	function call cannot (like routing on a counter); a library is just
205	code that happens to be vendored.
206	
207	## Error policy
208	
209	Any failure in any phase aborts the program. The loader prints:
210	- The map path
211	- The phase that failed (parse / schema / topology / etc.)
212	- The specific file or box that caused the failure
213	- The parser's or validator's diagnostic message
214	
215	Then `exit(1)`. There is no partial-load mode.
216	
217	## Open questions
218	
219	- JSON library choice: cJSON (heaviest, easiest, well-maintained),
220	  jsmn (tiny, no allocations, manual tree walking), or hand-rolled
221	  (smallest dependency footprint, more code). Decision deferred to
222	  implementation time.
223	### String storage
224	
225	A "string arena" is a single `malloc`-backed buffer the graph loader
226	uses to store every string it owns (box ids, file paths, function
227	names, branch tags) packed end-to-end. Each string is a `char *`
228	pointing into the arena. Allocating a string is just bumping the
229	arena's offset; freeing happens once when the arena is destroyed at
230	the end of the run, in a single `free(arena_buffer)`.
231	
232	This is faster and simpler than calling `malloc` per string: one
233	`free` for thousands of strings, no fragmentation, and lookup is
234	just a pointer.
235	
236	The arena starts small and grows on demand by `realloc`-ing to a
237	larger size when the current buffer fills up. Because every stored
238	string is referenced by a `char *` into the arena, a `realloc` that
239	moves the buffer would invalidate all the pointers — so the loader
240	either uses a series of fixed-size chunks (a chunk list rather than a
241	single growable buffer) or pre-sizes the arena from the file count
242	to avoid mid-load realloc. Chunk-list is the standard answer.
243	
244	## Suggested implementation sequence
245	
246	1. Vendor the chosen JSON parser into `libs/json/`.
247	2. Define `graph_t`, `box_t`, `connection_t`, `input_decl_t` in
248	   `src/008-pool-runner.c` (or a sibling header).
249	3. Implement phase 1 (directory walk) and phase 2 (parse). Smoke test:
250	   load `maps/hello`, print the parsed tree.
251	4. Implement phase 3 (schema validation) one rule at a time. Each
252	   rule has a unit test against a malformed map fixture.
253	5. Implement phase 4 (topology validation), including the cycle
254	   detector. Test against `maps/branch-test` (valid) and a fixture
255	   with a non-iterator cycle (invalid).
256	6. Implement phase 5 (size class enumeration). Output should match
257	   the box outputs declared in `maps/hello` and `maps/classify-demo`.
258	7. Implement phases 6 (entry boxes) and 7 (languages). Verify against
259	   known maps.
260	8. Wire the loader into `src/008-pool-runner.c`'s `main` so the pool
261	   runner starts by loading and validating before doing anything
262	   else.
263	
264	## Relevant files
265	
266	- `src/003-loader.lua` — phase 2 loader, ported by this issue
267	- `src/001-schema.lua` — phase 2 schema validator, also ported
268	- `src/008-pool-runner.c` — pool runner that owns and uses the loader
269	- `issues/301-pool-lifecycle-and-worker-init.md` — pool lifecycle that
270	  consumes the graph
271	- `issues/302-wire-value-slot-store.md` — slot allocator that consumes
272	  the size class list
273	- `issues/303-language-runtime-spec.md` — spec registry that consumes
274	  the language list
275	- `issues/304-task-dispatch-layer.md` — dispatch layer that consumes
276	  the box and connection arrays
277	- `issues/219-map-compiler.md` — the compiler is the other consumer of
278	  the same validation logic; the C loader and the compiler should
279	  share validation code if possible
280	
281	## Implementation log
282	
283	### Phase 1 + 2 + early phase 3 — 2026-05-12
284	
285	What shipped:
286	- `src/010-graph-loader.h` — public types and API. `graph_t` is
287	  opaque; the box / connection / input declaration shapes are
288	  public so the dispatch layer (issue 304) can walk them directly.
289	  Routing kinds align with issue 233 — `routing_t` with `kind`
290	  plus per-kind parameters, no legacy `comparand` /
291	  `iterator_outputs` top-level fields.
292	- `src/010-graph-loader.c` — implementation. Pipeline: open the
293	  map dir, parse `meta.json` via the project JSON parser
294	  (issue 314), walk every `.json` under `boxes/`, parse each into
295	  a `box_t`, enforce kind-specific required fields (ref/routing
296	  for call; path for data; inputs only for file_write), validate
297	  id uniqueness across boxes. Errors return a malloc'd
298	  `<file>:<line>: <message>` diagnostic via the `**err`
299	  out-parameter; the pool runner's main is expected to print and
300	  abort.
301	- `tests/maps/hello/` and `tests/maps/branching/` — fixture maps
302	  exercising the call/data combo, comparator routing (lt/eq/gt
303	  branches all present), and iterator routing (n_outputs=3).
304	- `tests/010-graph-loader-test.c` — 7 unit tests covering
305	  successful loads of both fixtures, missing map directory,
306	  NULL argument, unknown box kind, missing routing, and
307	  duplicate box id. Malformed-shape tests use mkdtemp scratch
308	  dirs so the tracked fixtures stay valid.
309	- Memory model: one `json_arena_t` per loaded graph holds every
310	  string and every parsed tree from every file; the box,
311	  connection, and input declaration arrays are plain malloc.
312	  `graph_destroy` releases all of it.
313	- `.gitignore` anchored to `/maps/` so `tests/maps/` is tracked.
314	- Makefile per-test dependency line for `010-graph-loader-test`
315	  linking the loader and the JSON parser.
316	
317	Verified `make STRICT=1` builds cleanly; 33 tests across three
318	suites (slot store 12, graph loader 7, JSON 14) all pass.
319	
320	What's deferred to follow-on iterations within 305:
321	- **Phase 4 — topology validation.** Connection endpoints store
322	  string refs only. The next pass resolves `to_box` to integer
323	  indices, verifies endpoints exist, and detects non-iterator
324	  cycles via DFS.
325	- **Phase 5 — slot size class enumeration.** The loader needs to
326	  walk every box output and produce the distinct `cell_capacity`
327	  values the slot store's allocator pre-populates.
328	- **Phase 6 — entry-box detection.** A box with zero inputs or
329	  with all inputs fed only by `data` boxes is an entry. The pool
330	  runner spawns initial tasks for these.
331	- **Phase 7 — language enumeration.** Distinct `lang` values
332	  across call boxes, handed to the spec registry (issue 303).
333	- The remaining routing kinds from issue 233 (randomizer,
334	  weighted, distributor) — the parser currently rejects them with
335	  a precise message.
336	
337	None of those block 301 / 303 / 306. Topology and language
338	enumeration land before 304 (the dispatch layer) since the
339	dispatch action needs both.
340	
341	### Topology endpoint resolution — 2026-05-12
342	
343	After `load_boxes` succeeds, a `resolve_topology` pass walks every
344	outgoing connection on every box and resolves the `to_box` string
345	to its index in `graph->boxes`, and `to_input` to its index in
346	the target box's `inputs[]`. Both string refs are preserved on
347	`connection_t` for diagnostics. Misses surface as
348	`box 'X' has a connection to nonexistent box 'Y'` or
349	`... nonexistent input 'Z'`. Two new fixture-based failure tests
350	plus an assertion on the resolved indices in the happy-path
351	`load_hello` test. The `branching` fixture grew three more box
352	files (`small`, `exact`, `big`) so its three comparator branches
353	have real targets now.
354	
355	Still deferred within 305: slot size class enumeration, entry-box
356	detection, language enumeration.
357	
358	### Non-iterator cycle detection — 2026-05-12
359	
360	Standard DFS with white/gray/black coloring; the only twist is
361	that we skip outgoing edges from iterator-routing boxes, since
362	cycles passing through an iterator are legitimate (the iterator's
363	input queue eventually empties and the loop terminates). Two new
364	fixture-based tests cover the reject case (`a → b → a`, no
365	iterator) and the allowed case (`a → iter → b → a`).
366	
367	### Runtime attach (slot allocation + spec resolution) — 2026-05-12
368	
369	`graph_attach_runtime(g, slots, specs, default_cell_bytes, err)`
370	walks every box and:
371	- allocates one 1-cell peek slot per declared input port via
372	  `slot_alloc`, storing the slot id in `box->input_slot_ids[i]`;
373	- resolves the spec for each call box by looking up `box->lang`
374	  in the registry (or, as a fallback, by matching the file
375	  extension of `box->ref`), storing the result on
376	  `box->spec_idx`.
377	
378	The graph also remembers its `map_dir` so the dispatch layer can
379	resolve relative paths for data boxes and file_write boxes.
380	
381	What's still deferred:
382	- **Language enumeration on the graph.** We resolve per-box but
383	  don't expose the distinct set of languages a map uses, so
384	  workers init every spec rather than only the ones in use.
385	
386	### Per-input slot modes + multi-spawn propagation — 2026-05-12
387	
388	`graph_attach_runtime` now picks the slot mode per input port:
389	- A new BFS pass (`propagate_multi_spawn`) seeds every iterator-
390	  routing call box as `multi_spawn = 1` and propagates the flag
391	  forward through the connection graph. Every box reachable from
392	  an iterator is now flagged.
393	- For multi-spawn boxes, each input slot is allocated as a
394	  16-cell pop ring. For single-spawn boxes, the legacy 1-cell
395	  peek slot is used.
396	- `box->input_slot_modes[]` carries the per-port mode
397	  (`SLOT_MODE_PEEK` / `SLOT_MODE_POP`) so the dispatch action
398	  picks the right read op.
399	
400	This is the compile-time wire classification the architecture
401	doc described, with a single-step forward propagation rule. A
402	proper per-wire classification (an iterator-rooted wire is
403	multi-push regardless of the immediate consumer's status) would
404	allow finer-grained mode picks; the current rule is conservative
405	in the right direction (slots that *could* be POP are POP).
406	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/306-lua-language-spec.md`

### Tool result

```
1	# 306 — Lua language spec implementation (reference spec)
2	
3	## Status
4	open
5	
6	## Current behavior
7	Lua boxes today run via `drivers/lua.sh`, which spawns a fresh `luajit`
8	process per box invocation, feeds it stdin, and reads stdout. Process
9	spawn cost dominates; each call is its own fresh interpreter. There is
10	no persistent Lua state between calls.
11	
12	## Concept
13	The Lua language spec implements `lang_spec_t` (issue 303) for Lua. It
14	is the first reference implementation of the spec interface and the
15	proof that the in-process FFI model works. It is also the simplest
16	case — no compile step, no out-of-process server.
17	
18	Each worker thread gets its own `lua_State`. States are not shared.
19	Per-thread states need no locking on the call path; the state's
20	internals are only ever touched by the worker that owns it.
21	
22	`langs/lua/spec.c` is built into `langs/lua/spec.so` and loaded by
23	the pool runner via `dlopen` at startup, like any other spec.
24	
25	## Per-worker state
26	
27	Init creates a fresh `lua_State` and configures it:
28	
29	```c
30	void *lua_init(int worker_idx) {
31	    lua_State *L = luaL_newstate();
32	    luaL_openlibs(L);
33	    set_package_path(L);   // libs/, map src/, meta.json src_dirs
34	    return L;
35	}
36	
37	void lua_teardown(void *handle) {
38	    lua_close((lua_State *)handle);
39	}
40	```
41	
42	`set_package_path` writes `package.path` and `package.cpath` to
43	include exactly one directory: the map's own `src/`. Everything the
44	map needs — vendored libs, helper modules, anything — is copied into
45	`src/` by the compile step (issue 222). The compiled map is then a
46	self-contained directory: ship the directory to someone else and it
47	runs without further setup.
48	
49	This is a tightening of the phase 2 path, which also included
50	SoraMech's central `libs/` and any `meta.json src_dirs`. The new rule
51	is: one place to look for functions, the map's own `src/`, full stop.
52	Compile is the step that resolves dependencies into that location.
53	
54	## File-and-function caching
55	
56	A naive `invoke` would `luaL_loadfile` and look up the function on
57	every call. Both are unnecessary work — the file's chunk does not
58	change during a run, and the function reference is stable.
59	
60	The Lua spec keeps a per-handle cache: for each box source file
61	loaded, store the table returned by the file's chunk as a Lua
62	registry key (`LUA_REGISTRYINDEX`). On first invocation:
63	1. `luaL_loadfile(L, file_path)` — loads the chunk.
64	2. `lua_pcall(L, 0, 1, 0)` — runs the chunk, expecting a returned
65	   table.
66	3. Store the table in the registry under a key derived from `file_path`.
67	
68	On subsequent invocations:
69	1. Look up the registry entry by `file_path`.
70	2. Index it by `fn_name` to get the function reference.
71	3. Skip `luaL_loadfile` entirely.
72	
73	The cache is per-`lua_State` and therefore per-worker; no
74	synchronization is needed.
75	
76	## Box file convention
77	
78	A Lua box file returns a table whose keys are the box's exported
79	function names:
80	
81	```lua
82	-- maps/<name>/src/classify.lua
83	local M = {}
84	
85	function M.classify(text)
86	    if text:match("^%d+$") then return "numeric"
87	    elseif text:match("[a-z]") then return "alpha"
88	    else return "other"
89	    end
90	end
91	
92	return M
93	```
94	
95	The box JSON references it by `ref = "classify.lua"`, `fn = "classify"`.
96	The spec loads `classify.lua`, stores its returned table, and indexes
97	into it with `"classify"` to get the function.
98	
99	This is the existing phase 2 convention — kept as-is.
100	
101	## Input marshalling
102	
103	Inputs arrive at `invoke` as `(void *bytes, int size)` pairs. The
104	spec is responsible for converting each input's bytes into the Lua
105	value type the user's function expects. The user's function sees
106	arguments as native Lua values — never a raw byte buffer that the
107	function has to parse itself.
108	
109	Each input has a declared type in the box JSON
110	(`{"name": "count", "type": "number"}`). The graph loader (issue 305)
111	records this type on the `input_decl_t` and passes the type alongside
112	the bytes when calling `invoke`. The Lua spec dispatches on type:
113	
114	| Declared type | Lua spec action                                             |
115	|---------------|-------------------------------------------------------------|
116	| `string`      | `lua_pushlstring(L, bytes, size)`                           |
117	| `number`      | parse with `strtod`, `lua_pushnumber`                       |
118	| `integer`     | parse with `strtoll`, `lua_pushinteger`                     |
119	| `boolean`     | strcmp against `"true"` / `"false"`, `lua_pushboolean`      |
120	| `json`        | call `dkjson.decode` on the bytes, push the resulting table |
121	| `bytes`       | `lua_pushlstring` (raw, may contain nulls)                  |
122	| `any`         | treat as `string` for now                                   |
123	
124	The Lua function then takes typed arguments:
125	
126	```lua
127	function M.add(a, b)            -- both already numbers
128	    return a + b
129	end
130	
131	function M.classify(payload)    -- payload is already a table
132	    return payload.kind
133	end
134	```
135	
136	The user does not write `tonumber` or `dkjson.decode` calls inside
137	their box function. The spec did that work before the function was
138	called.
139	
140	If a parse fails (`json` input that isn't valid JSON, `number` input
141	that isn't a number), the spec returns nonzero and the dispatch layer
142	aborts (per issue 303).
143	
144	## Return marshalling
145	
146	The Lua function returns one or more values. The spec converts the
147	return into bytes for the output slot:
148	
149	| Return shape           | Spec action                                                   |
150	|------------------------|---------------------------------------------------------------|
151	| Single string          | `lua_tolstring` → copy into `out_buf`                         |
152	| Single number          | `snprintf("%.17g", ...)` into `out_buf`                       |
153	| Single boolean         | write `"true"` or `"false"`                                   |
154	| Single table           | `dkjson.encode` into `out_buf`                                |
155	| Single nil             | `*out_size = 0` (zero-byte output)                            |
156	| Multiple values        | wrap into a table `{v1, v2, ...}` and `dkjson.encode`         |
157	
158	`nil` is a valid return. The spec writes zero bytes and sets
159	`*out_size = 0`. Downstream consumers receive a zero-byte slot value;
160	how they interpret it depends on their declared input type (typed
161	`number` consumer fails to parse; typed `string` consumer gets an
162	empty string; typed `json` consumer parses zero bytes as `null` via
163	dkjson). We cannot demand that user functions never return nil, so we
164	handle it.
165	
166	Multiple return values are wrapped into a table and JSON-encoded.
167	Phase 2 silently dropped extra returns; phase 3 keeps them — the
168	serialized form is `[v1, v2, ...]`.
169	
170	Output buffer is sized to the box's declared `output_capacity`
171	(issue 305's `box_t`). If the serialized value exceeds that, abort
172	(per the hard-crash policy in issue 303). For variable-size outputs
173	the box declares `output_capacity = 0` and uses the large-value heap
174	(issue 302); the spec writes into that heap and stores the handle in
175	`out_buf`.
176	
177	dkjson is the table serializer in both directions (input parsing
178	above and output serialization here). It is already vendored.
179	
180	## Error handling
181	
182	Every `lua_pcall` is wrapped:
183	```c
184	int rc = lua_pcall(L, n_args, 1, 0);
185	if (rc != LUA_OK) {
186	    fprintf(stderr, "[lua spec] %s:%s — %s\n",
187	            file_path, fn_name, lua_tostring(L, -1));
188	    lua_pop(L, 1);
189	    return 1;  // dispatch layer aborts the program
190	}
191	```
192	
193	Lua errors (`error(...)`, runtime errors, syntax errors during file
194	load) all surface here as nonzero `pcall` returns. The spec prints
195	the box context and the Lua error message to stderr, then returns
196	nonzero. The dispatch layer aborts.
197	
198	There is no try-and-recover. Per issue 303, errors crash the program.
199	
200	## Build
201	
202	```
203	langs/lua/
204	    spec.c          ← lang_spec_t implementation
205	    Makefile        ← builds spec.so
206	    spec.so         ← built artifact (gitignored)
207	```
208	
209	The Makefile target:
210	```make
211	spec.so: spec.c
212		$(CC) -shared -fPIC -O2 -Wall \
213		    -I$(LUA_INCLUDE) \
214		    -o $@ $< \
215		    -L$(LUA_LIB) -lluajit-5.1
216	```
217	
218	LuaJIT is the assumed runtime — it matches the phase 2 driver and the
219	project preference (LuaJIT-compatible Lua, no 5.4 syntax). The build
220	links against the system LuaJIT shared library.
221	
222	## Open questions
223	
224	(none currently — earlier questions resolved as follows:)
225	
226	- nil return: valid. Zero-byte output, downstream consumers
227	  interpret per their declared input type.
228	- Multiple return values: wrapped into a table and JSON-encoded via
229	  dkjson. No values dropped.
230	- Table serializer: dkjson, already vendored. Used in both
231	  directions (input parsing and output serialization).
232	
233	## Suggested implementation sequence
234	
235	1. Set up `langs/lua/` directory with a Makefile that builds an empty
236	   `spec.so` exporting a stub `soramech_lang_spec`.
237	2. Implement `init` and `teardown`. Verify the pool runner can
238	   `dlopen` the spec and call `init` per worker without errors.
239	3. Implement file-and-function caching — load a file, store its
240	   returned table in the registry, look it up later.
241	4. Implement input marshalling: push bytes as strings, count args.
242	5. Implement `lua_pcall` invocation with error wrapping.
243	6. Implement string return marshalling. Smoke test: `maps/hello`
244	   end-to-end through the pool.
245	7. Implement number return marshalling.
246	8. Implement table-via-JSON return marshalling.
247	9. Add `package.path` resolution for libs/, src/, and `meta.src_dirs`.
248	10. Test against `maps/classify-demo` and `maps/driver-test` (Lua
249	    portions).
250	
251	## Relevant files
252	
253	- `drivers/lua.sh` — phase 2 Lua driver, retired by this spec
254	- `langs/lang-spec.h` — interface header (issue 303)
255	- `issues/303-language-runtime-spec.md` — spec contract this implements
256	- `issues/304-task-dispatch-layer.md` — dispatch layer that calls
257	  `invoke`
258	- `issues/302-wire-value-slot-store.md` — large-value heap for
259	  variable-size returns
260	- `libs/dkjson` (vendored) — table-to-JSON serialization
261	- `maps/hello`, `maps/classify-demo` — Lua-only smoke test maps
262	
263	## Implementation log
264	
265	### Init + invoke + teardown — 2026-05-12
266	
267	`langs/lua/spec.c` now exports a real `lang_spec_t`:
268	
269	- `init(worker_idx)` creates a `lua_State` and opens the standard
270	  libraries via `luaL_openlibs`. Returns the state as the per-
271	  worker handle.
272	- `teardown(handle)` calls `lua_close`.
273	- `invoke(handle, file_path, fn_name, inputs, sizes, n, out_buf,
274	  out_cap, out_size)` loads the file with `luaL_loadfile`,
275	  executes the chunk (which is expected to return a module
276	  table), pulls the named function out of that table, pushes each
277	  input as a Lua string, calls via `lua_pcall`, and copies the
278	  return as a string into the caller's buffer. The state's stack
279	  is rewound to its baseline depth at every entry/exit so
280	  cross-call leaks don't accumulate. Errors at every step
281	  surface as nonzero return and a stderr message.
282	
283	Five end-to-end tests in `tests/306-lua-spec-test.c` exercise the
284	init/teardown round-trip, a `greet(name, salutation)` call with
285	both args, the same call with one arg (the Lua side defaults
286	salutation to "Hello"), and the two failure modes (missing
287	function, missing file). The test loads the spec through the
288	real spec registry via dlopen, so this is the actual artifact
289	the pool runner will consume.
290	
291	Deferred:
292	- **Rich return types.** The current invoke serializes the return
293	  with `lua_tolstring`, which handles strings/numbers/booleans/
294	  nil via tostring semantics. Tables, closures, and other
295	  structured values land with the fast-path work in issue 312.
296	
297	### Per-worker module cache — 2026-05-12
298	
299	`lua_init` now seeds an empty Lua table at
300	`LUA_REGISTRYINDEX[LUA_CACHE_KEY]`. `lua_invoke` looks up the
301	box's `file_path` in that table; on a hit it skips the
302	`luaL_loadfile + lua_pcall` round-trip and uses the cached
303	module table directly; on a miss it loads, runs the chunk,
304	verifies the module table shape, and stashes the result in the
305	cache before proceeding.
306	
307	The cache is per-worker (each worker has its own `lua_State`),
308	which means no locking. It's also per-file-path string, so two
309	boxes that share a source file share a module table on each
310	worker — calling `M.greet` from one box doesn't reload the
311	chunk when another box later calls `M.farewell` on the same
312	file.
313	
314	Cache invalidation is intentionally absent for now: once a file
315	is loaded, its module table sticks for the run. If the box
316	source changes mid-run, the worker won't notice. Fine for the
317	read-once-then-execute usage; revisit when there's a use case
318	for hot-reload.
319	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/307-c-language-spec.md`

### Tool result

```
1	# 307 — C language spec implementation
2	
3	## Status
4	open
5	
6	## Current behavior
7	C boxes today run via `drivers/c.sh`, which compiles the source on
8	demand (mtime-based recompile to a binary), then spawns the binary as
9	a subprocess and reads stdout. Process spawn cost dominates; each call
10	is its own freshly-launched executable. The phase 2 driver compiles
11	to a standalone binary, not a `.so`.
12	
13	## Concept
14	The C language spec implements `lang_spec_t` (issue 303) for C. It is
15	the second reference implementation, and the first one that exercises
16	the `compile` callback. After compile, invocation is in-process via
17	`dlopen` / `dlsym` — the user's compiled function is called directly
18	through a function pointer, with no subprocess and no IPC.
19	
20	This is the "Option 1 — LuaJIT FFI" model from
21	`docs/004-ipc-and-threading.md`, applied to C: same-process call,
22	nanosecond-overhead invocation, native-typed access. The only thing
23	the C spec adds over the Lua spec is the compile step.
24	
25	## Per-worker state
26	
27	`init` returns a small struct holding the dlopen-handle cache:
28	
29	```c
30	typedef struct {
31	    void  *so_handles[MAX_BOXES];   // per-file .so handle, indexed by file id
32	    void  *fn_ptrs   [MAX_BOXES];   // per-(file, fn) cached function pointer
33	    int    n_cached;
34	    // (a hash table could replace these arrays if MAX_BOXES grows large)
35	} c_handle_t;
36	
37	void *c_init(int worker_idx);
38	void  c_teardown(void *handle);    // dlclose every cached handle
39	```
40	
41	Each worker thread has its own cache. The same `.so` will end up
42	loaded once per worker, since `dlopen` with the same path on the same
43	process returns a refcounted handle (the underlying code segment is
44	shared across all callers — the per-worker `dlopen` is just a
45	refcount bump after the first).
46	
47	## Compile callback
48	
49	The C spec's `compile` callback receives the box's metadata in
50	addition to the source and output paths, so it knows the function's
51	signature and can generate the wrapper:
52	
53	```c
54	int c_compile(const char *src_path, const char *out_path, const box_t *box);
55	```
56	
57	(This is a small extension to `lang_spec_t` in issue 303 — `compile`
58	gains a `box_t` pointer. Lua's compile is NULL, so unaffected.)
59	
60	The callback:
61	1. Reads `box->inputs`, `box->returns`, `box->headers`, `box->link_libs`,
62	   `box->cflags` from the box metadata.
63	2. Generates a wrapper `.c` file (see "Wrapper generation" below) in
64	   the map's `compiled/bin/` directory next to where the `.so` will
65	   land.
66	3. Forks and execs the system `cc` with:
67	   - `-shared -fPIC -O2 -Wall` (defaults)
68	   - The user's `cflags`, if any
69	   - The wrapper `.c` and the user's `src_path` (if `ref` is set)
70	   - `-o out_path`
71	   - `-l<lib>` for each entry in `box->link_libs`
72	4. Waits for the compiler, returns 0 on success, 1 on failure.
73	
74	If the resulting `.so` is newer than the user's source and the
75	wrapper, compile is skipped (mtime check, same as phase 2).
76	
77	The compiler is `cc` — the system default. `gcc` on most Linux,
78	`clang` on macOS. No SoraMech-specific compiler dependency.
79	
80	### cflags in box JSON
81	The `cflags` field on the box JSON is a string of additional compiler
82	flags (`-O0 -g`, `-mavx2`, etc.). It is set by the user in the
83	inspector — a "Compile flags" text input shown when a C-language box
84	is selected. The editor saves it as part of the box's JSON; reads
85	preserve the field; saves write it back. Nothing overwrites it.
86	
87	## Box file convention: any C function, no boilerplate
88	
89	**The rule, stated as a rule:** a box author writes code as if
90	SoraMech does not exist. The function they write must be a
91	function their non-SoraMech compiler can compile, their
92	non-SoraMech editor can lint, their non-SoraMech test harness
93	can call. No SoraMech-prefixed macros. No SoraMech-shaped
94	return types. No `#include "soramech.h"`. Nothing in the
95	author's source betrays that the function will ever cross
96	SoraMech's wire.
97	
98	SoraMech bends to the language. The language never bends to
99	SoraMech. This is non-negotiable across every spec; it's the
100	reason for the spec interface in the first place.
101	
102	Users do not write a SoraMech-specific C signature. They reference
103	any C function — whether one they wrote themselves, or one from a
104	library (zlib, OpenCV, libcurl, anything they have headers for) —
105	and the C spec generates a thin wrapper that bridges SoraMech's
106	bytes-in / bytes-out ABI to the function's natural typed C
107	signature.
108	
109	### What the user provides
110	Either:
111	- A `.c` file containing the function, referenced by `ref` in the
112	  box JSON; or
113	- A library function from a header, no `.c` file of their own — the
114	  `ref` field is omitted, the function name and headers are
115	  declared in the box JSON, and the library is named in
116	  `link_libs`.
117	
118	### Box JSON for a user-written function
119	
120	```json
121	{
122	  "id": "classify",
123	  "kind": "call",
124	  "lang": "c",
125	  "ref": "classify.c",
126	  "fn": "classify",
127	  "inputs": [
128	    {"name": "text",   "type": "string"},
129	    {"name": "weight", "type": "int"}
130	  ],
131	  "returns": "int"
132	}
133	```
134	
135	The user's `.c` file is naturally written:
136	
137	```c
138	// maps/<name>/src/classify.c
139	int classify(const char *text, int weight) {
140	    if (text[0] >= '0' && text[0] <= '9') return weight * 1;
141	    return weight * 2;
142	}
143	```
144	
145	No header to include, no macro, no `int classify(SM_BOX_ARGS)`.
146	Just a function with a typed signature.
147	
148	### Box JSON for a library function (no user source)
149	
150	```json
151	{
152	  "id": "compress",
153	  "kind": "call",
154	  "lang": "c",
155	  "fn": "compress2",
156	  "headers": ["<zlib.h>"],
157	  "link_libs": ["z"],
158	  "inputs": [
159	    {"name": "src",   "type": "bytes"},
160	    {"name": "level", "type": "int"}
161	  ],
162	  "returns": "bytes"
163	}
164	```
165	
166	The wrapper includes `<zlib.h>`, calls `compress2`, and the linker
167	pulls in `libz` via `-lz`. No user-written `.c` file.
168	
169	### Wrapper generation
170	
171	The compile callback emits a wrapper `.c` for each C box. The
172	wrapper has a fixed entry-point name (`sm_box_entry`) that the spec
173	always `dlsym`s. It declares the user's function (extern), unpacks
174	SoraMech's input bytes into typed C arguments per the box's `inputs`
175	declaration, calls the user's function, and writes the return value
176	into `sm_out_buf` per the box's `returns` declaration.
177	
178	For `classify` above, the generated wrapper is roughly:
179	
180	```c
181	// generated: classify_wrap.c
182	extern int classify(const char *, int);
183	
184	int sm_box_entry(const void **inputs, const int *sizes, int n,
185	                 void *out_buf, int out_cap, int *out_size) {
186	    const char *text   = (const char *)inputs[0];
187	    int         weight = atoi((const char *)inputs[1]);
188	    int         result = classify(text, weight);
189	    *out_size = snprintf(out_buf, out_cap, "%d", result);
190	    return 0;
191	}
192	```
193	
194	The wrapper is mechanical — same template for every box, varying
195	only in the input-marshalling lines and the return-encoding line.
196	The compile step writes it to `compiled/bin/<box-id>_wrap.c` and
197	compiles `<box-id>_wrap.c` plus the user's source (if any) into a
198	single `.so`.
199	
200	This means the C spec implements typed function calls without
201	libffi (which would do the marshalling at runtime via the C ABI
202	metadata). The same effect is achieved at compile time with
203	straightforward generated code, no runtime dependency.
204	
205	## Input marshalling (in the generated wrapper)
206	
207	The wrapper converts SoraMech's `(bytes, size)` input pairs into the
208	typed C arguments the user's function expects, based on the
209	declared input types in the box JSON:
210	
211	| Declared type | Wrapper action                                                  |
212	|---------------|-----------------------------------------------------------------|
213	| `string`      | `(const char *)inputs[i]`                                       |
214	| `int`         | `atoi((const char *)inputs[i])`                                 |
215	| `long`        | `strtoll(...)`                                                  |
216	| `double`      | `strtod(...)`                                                   |
217	| `bool`        | `strcmp("true", inputs[i]) == 0`                                |
218	| `bytes`       | `(const void *)inputs[i]`, paired with `sizes[i]` for the size  |
219	| `json`        | `cJSON_ParseWithLength(inputs[i], sizes[i])` → `cJSON *`        |
220	
221	The `json` case uses the same vendored cJSON the graph loader uses
222	(issue 305) — one JSON library shared across the runner. When a box
223	input is declared `"type": "json"`, the wrapper passes a `cJSON *`
224	to the user's function. The user navigates the tree with cJSON's
225	API (`cJSON_GetObjectItem`, `cJSON_GetArrayItem`, etc.) and the
226	wrapper frees the parsed tree after the function returns.
227	
228	If a parse fails (a `json` input that is not valid JSON, an `int`
229	input that does not parse as an integer), the wrapper aborts the
230	program with the box id and the input position (per issue 303).
231	
232	## Output encoding (in the generated wrapper)
233	
234	The wrapper takes the user function's return value and serializes
235	it into `sm_out_buf`, based on the declared `returns` type:
236	
237	| Declared `returns` | Wrapper action                                              |
238	|--------------------|-------------------------------------------------------------|
239	| `string`           | the user returns `const char *`; `strcpy` + set `out_size` |
240	| `int`              | `snprintf("%d", ret)`                                       |
241	| `long`             | `snprintf("%lld", ret)`                                     |
242	| `double`           | `snprintf("%.17g", ret)`                                    |
243	| `bool`             | write `"true"` or `"false"`                                 |
244	| `bytes`            | the user returns `(void *, int)` via an out-parameter pair (see below) |
245	| `json`             | the user returns `cJSON *`; `cJSON_PrintUnformatted` → bytes |
246	| `void`             | `*out_size = 0` (zero-byte output)                          |
247	
248	For `bytes` returns, the user function's signature has the form
249	`int my_fn(... const char *out_buf, int out_cap, int *out_size)` —
250	extra parameters that the wrapper passes through from
251	`sm_out_buf` / `sm_out_capacity` / `sm_out_size`. This handles
252	library functions like `compress` that write their output via an
253	out-pointer rather than a return value. The box JSON declares
254	`"returns": "bytes"` and the wrapper threads the buffer through.
255	
256	If the encoded value exceeds `sm_out_capacity`, the wrapper aborts
257	(per issue 303). For boxes whose output capacity is not known at
258	compile time, see "Variable-size outputs" below — the wrapper
259	routes through the large-value heap.
260	
261	## Variable-size outputs
262	
263	For boxes whose output capacity is not known at compile time (issue
264	302's large-value heap): the box JSON declares `output_capacity = 0`
265	and the wrapper allocates from the large-value heap on each call.
266	For `bytes` and `json` returns, the wrapper transparently picks the
267	heap path when the box is variable-size — the user's function is
268	unchanged either way. The handle (pointer + size into the
269	large-value heap) is what ends up in `sm_out_buf`.
270	
271	For string returns, similarly: if the returned string exceeds the
272	declared fixed `output_capacity`, the wrapper falls back to the
273	large-value heap. This decision is made at compile time based on the
274	box JSON, not at runtime.
275	
276	## Thread safety of user code
277	
278	The same `.so` is loaded by every worker thread that may call into
279	it. **Static variables and globals declared in the user's C source
280	are shared mutable state across all workers** unless explicitly
281	made `const`.
282	
283	A note on `static` in C: it does not mean immutable. `static` at
284	file scope means "this symbol has internal linkage, only visible
285	inside this `.c` file." `static` inside a function means "this
286	variable persists across calls." Either way, the variable itself is
287	mutable — `static int counter = 0; counter++;` is perfectly legal
288	and does what it says, just with internal linkage. Immutable in C
289	is `const`.
290	
291	So:
292	- `static int counter = 0;` at file scope — racy, shared across workers.
293	- `static int x = 0;` inside a function body — racy, shared across workers calling that function.
294	- `const int max = 100;` — safe, immutable.
295	- `static const int max = 100;` — safe, immutable.
296	
297	Box functions must be reentrant — no mutable shared state without
298	explicit locking. The C spec does not enforce this; the rule lives
299	in `docs/005-language-specs.md` (to be written) for box authors to
300	read.
301	
302	### Library functions that need global init
303	
304	Many real C libraries require a one-time setup call before any of
305	their functions work — `curl_global_init` for libcurl,
306	`OPENSSL_init_crypto` for OpenSSL, etc. A SoraMech box that calls
307	into such a library cannot just be wired in cold; the library's
308	state has to exist before the call.
309	
310	Two patterns handle this without changing the spec interface:
311	
312	1. **Init box wired as an entry point.** The user creates a small C
313	   box whose function is the library's init call (e.g.
314	   `curl_global_init`). They mark it as an entry box and wire it
315	   such that any box using libcurl is downstream of it. The init
316	   runs once at startup before any consumer fires. Process-global
317	   library state — which is what most C libraries actually use — is
318	   set up by the time anyone needs it. Pairs with a teardown box
319	   wired as a terminal node if needed.
320	
321	2. **Per-worker init via spec extension** (future). For libraries
322	   that need per-worker setup (each thread gets its own handle),
323	   the C spec gains an optional per-box `worker_init_fn` field in
324	   the box JSON. The spec calls that function once per worker
325	   thread during pool startup, before any box invoke. Not in scope
326	   yet — pattern (1) covers the common case.
327	
328	The init-box approach is the recommended starting point. The
329	spec-extension path is the escape hatch when (1) does not fit.
330	
331	A future fallback exists: `dlmopen`, a Linux variant of `dlopen`
332	that loads each library into its own isolated "namespace," giving
333	each worker its own copy of the library's symbols (statics
334	included). Two threads using two different namespaces would not
335	share static state. Less portable than `dlopen` (Linux-only), so
336	not the default — flagged here as the escape hatch if static-
337	variable races become a real problem.
338	
339	## Build
340	
341	```
342	langs/c/
343	    spec.c              ← lang_spec_t implementation
344	    soramech-c.h        ← user-facing header (shipped to maps via compile step)
345	    Makefile            ← builds spec.so
346	    spec.so             ← built artifact (gitignored)
347	```
348	
349	Spec Makefile target:
350	```make
351	spec.so: spec.c
352		$(CC) -shared -fPIC -O2 -Wall -o $@ $< -ldl
353	```
354	
355	Linking against `libdl` for `dlopen` / `dlsym` is the only
356	dependency. No interpreter, no helper library beyond libc.
357	
358	### What `dlopen` and `dlsym` do
359	- `dlopen("path/to/file.so", flags)` loads a shared library at
360	  runtime. The OS loader maps the library's code into the process's
361	  memory and returns a handle. Same idea as Windows'
362	  `LoadLibrary`.
363	- `dlsym(handle, "name")` looks up a symbol — a function or a
364	  variable — by name inside an already-loaded library, and returns
365	  a pointer to it. Same idea as Windows' `GetProcAddress`.
366	
367	Together they are how a program loads and calls code that wasn't
368	linked at compile time. Plugin systems are the classic use case.
369	Here, every C box compiles to its own `.so`; the spec `dlopen`s the
370	`.so` once per worker, `dlsym`s `sm_box_entry` once per file, and
371	calls the resulting function pointer per invocation.
372	
373	The user's C box files are compiled by the `compile` callback at map
374	load time, not by this Makefile. The Makefile here builds only the
375	spec itself.
376	
377	## Error handling
378	
379	- `dlopen` failure (missing `.so`, link errors): print path + `dlerror()`
380	  to stderr, return 1, dispatch layer aborts.
381	- `dlsym` failure (missing function): print file/fn + `dlerror()`,
382	  return 1, abort.
383	- Compile failure: `cc` exit nonzero, print compiler stderr (already
384	  routed there by `cc`), return 1, abort.
385	- User function returns nonzero: propagate that as the spec's return,
386	  abort.
387	
388	No retry, no fallback. Per issue 303.
389	
390	## Open questions
391	
392	(none currently — earlier questions resolved as follows:)
393	
394	- Per-box `cflags`: yes, add the field to the box JSON. The
395	  inspector shows a "Compile flags" text input when a C-language
396	  box is selected (phase 2 inspector work).
397	- Typed signatures: handled via compile-time wrapper generation
398	  (see "Box file convention" above). No libffi runtime dependency.
399	- Static-variable isolation via `dlmopen`: deferred. Current rule
400	  is "C box authors write reentrant code"; if that breaks down in
401	  practice, fall back to per-worker `dlmopen` namespaces (Linux-only).
402	
403	## Suggested implementation sequence
404	
405	1. Set up `langs/c/` directory with a Makefile that builds an empty
406	   `spec.so` exporting a stub `soramech_lang_spec`.
407	2. Implement `init` and `teardown` with the dlopen-handle cache.
408	3. Implement `compile` — fork/exec `cc` with the standard flags.
409	   Smoke test: compile a single hello-world `.c` file.
410	4. Implement `invoke` — dlsym, direct call, propagate return.
411	5. Write `soramech-c.h` with the `SM_BOX_ARGS` macro and the basic
412	   input/output helpers.
413	6. Smoke test: a C box that takes one string input, writes a string
414	   output, runs end-to-end through the pool.
415	7. Add the typed input helpers (`sm_in_int`, `sm_in_double`, etc.).
416	8. Add the typed output helpers.
417	9. Test against `maps/driver-test` (C portions).
418	10. Add variable-size output support (`sm_out_alloc`, `sm_out_handle`).
419	11. Document in `docs/005-language-specs.md` once it exists, with a
420	    note on thread safety.
421	
422	## Relevant files
423	
424	- `drivers/c.sh` — phase 2 C driver, retired by this spec
425	- `langs/lang-spec.h` — interface header (issue 303)
426	- `issues/303-language-runtime-spec.md` — spec contract this implements
427	- `issues/304-task-dispatch-layer.md` — dispatch layer that calls
428	  `invoke`
429	- `issues/302-wire-value-slot-store.md` — large-value heap
430	- `issues/222-compile-button-and-assets-directory.md` — where compiled
431	  `.so`s live (`compiled/bin/`)
432	- `issues/306-lua-language-spec.md` — sister spec; same shape, no
433	  compile step
434	- `maps/driver-test` — multi-language test map including C boxes
435	
436	## Implementation log
437	
438	### Compile + invoke — 2026-05-12
439	
440	`langs/c/spec.c`:
441	- `compile(src, out)` forks gcc with `-shared -fPIC -O2 -Wall`.
442	- `init` allocates a 32-entry dlopen cache; `teardown` walks it.
443	- `invoke` looks up the .so in the cache or dlopens it, dlsyms
444	  the named function, and calls it through a fixed signature:
445	  `int fn(const void **inputs, const int *sizes, int n,
446	  void *out_buf, int out_capacity, int *out_size)`.
447	
448	Convention: C box functions all match that signature. The
449	optional typed-wrapper mode (per-box signature parsing +
450	generated marshaller) is deferred until a box wants types that
451	diverge from the byte-array shape.
452	
453	`tests/maps/hello/src/echo.c` ships two fixture functions; five
454	tests in `tests/307-c-spec-test.c` compile and invoke them, plus
455	the missing-symbol and bad-source failure modes.
456	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/308-bash-language-spec.md`

### Tool result

```
1	# 308 — Bash language spec implementation
2	
3	## Status
4	open
5	
6	## Current behavior
7	Bash boxes today run via `drivers/bash.sh`, which `source`s the box
8	file and invokes the function in a fresh subshell per call. Like the
9	phase 2 Lua and C drivers, every invocation is a new process; spawn
10	cost dominates. There is no persistent Bash interpreter between
11	calls.
12	
13	## Concept
14	
15	The Bash language spec implements `lang_spec_t` (issue 303) for Bash.
16	Unlike the Lua and C specs, Bash cannot be embedded in the runner
17	process — there is no "libbash.so" to link against, and no in-process
18	API for executing Bash code. The Bash spec uses **Option 3 — Unix
19	domain socket servers** from `docs/004-ipc-and-threading.md`: each
20	worker thread starts and talks to its own persistent Bash subprocess
21	over a Unix domain socket. The subprocess stays alive for the whole
22	run, accepting one box invocation at a time.
23	
24	This makes Bash the proof case that the spec interface accommodates
25	languages that cannot be embedded. The spec contract from the
26	dispatch layer's perspective is the same: byte buffers in, byte
27	buffers out, abort on nonzero return. Where those bytes go (a
28	`lua_State`, a function pointer, or a socket) is the spec's
29	business.
30	
31	## Architecture
32	
33	```
34	Worker thread N                  Bash subprocess N
35	─────────────────                 ─────────────────
36	spec.invoke(...)                  /tmp/soramech-<run-id>-bash-N.sock
37	   |                                       |
38	   send framed request    ──────────►      receive
39	   recv framed response   ◄──────────      execute box function
40	                                           send framed response
41	```
42	
43	One Bash subprocess per worker thread, addressed by a per-worker
44	socket path. The subprocess runs `bash-server.sh` (shipped alongside
45	`spec.c`), which `source`s box files on demand, calls the named
46	function, and writes back the result.
47	
48	The subprocess is single-threaded by Bash's nature, but that is
49	fine: one worker thread sends one request at a time. A 16-worker
50	pool means 16 Bash subprocesses, each handling its own thread's
51	requests serially. The pool's parallelism comes from running 16
52	worker threads (and 16 subprocesses) concurrently — not from any
53	one Bash subprocess.
54	
55	## Per-worker state
56	
57	`init` for the Bash spec:
58	1. Picks a socket path: `/tmp/soramech-<run-id>-bash-<worker_idx>.sock`.
59	2. Forks a child process.
60	3. Child execs `bash-server.sh <socket-path>`.
61	4. Parent waits briefly for the socket to exist, then connects via
62	   `connect(AF_UNIX, ...)`.
63	5. Returns a handle holding the connected socket fd.
64	
65	```c
66	typedef struct {
67	    int   sock_fd;
68	    pid_t server_pid;
69	    char  socket_path[256];
70	} bash_handle_t;
71	```
72	
73	`teardown`:
74	1. Sends a `{ "op": "shutdown" }` framed message.
75	2. `close(sock_fd)`.
76	3. `waitpid(server_pid)` to reap the subprocess.
77	4. `unlink(socket_path)`.
78	
79	## Wire protocol
80	
81	Length-prefix framing, identical in both directions:
82	- 4 bytes: big-endian message length, in bytes.
83	- N bytes: message body, JSON-encoded.
84	
85	This handles bytes containing newlines, nulls, or any other character
86	without ambiguity.
87	
88	### Request
89	
90	```json
91	{
92	  "op": "invoke",
93	  "file": "/abs/path/to/box.sh",
94	  "fn": "process",
95	  "args": ["arg0_bytes", "arg1_bytes", ...]
96	}
97	```
98	
99	### Response (success)
100	
101	```json
102	{
103	  "ok": true,
104	  "out": "result_bytes"
105	}
106	```
107	
108	### Response (error)
109	
110	```json
111	{
112	  "ok": false,
113	  "err": "bash error message",
114	  "stderr": "any captured stderr"
115	}
116	```
117	
118	A nonzero exit from the box function, a syntax error during `source`,
119	or a Bash signal all surface as `"ok": false`. The spec returns
120	nonzero from `invoke`, the dispatch layer aborts (issue 303).
121	
122	## bash-server.sh
123	
124	The subprocess script is small. Roughly:
125	
126	```bash
127	#!/usr/bin/env bash
128	# bash-server.sh <socket-path>
129	SOCKET="$1"
130	
131	# Set up a listening socket using socat (or python -c 'socket...' fallback).
132	# Read length-prefix framed messages, dispatch.
133	while read_request; do
134	    case "${OP}" in
135	        invoke)
136	            if ! declare -F "${FN}" > /dev/null; then
137	                source "${FILE}"
138	            fi
139	            OUTPUT="$( "${FN}" "${ARGS[@]}" 2>/tmp/bash-stderr )"
140	            RC=$?
141	            if [ "${RC}" -eq 0 ]; then
142	                send_response_ok "${OUTPUT}"
143	            else
144	                send_response_err "$(< /tmp/bash-stderr)"
145	            fi
146	            ;;
147	        shutdown)
148	            exit 0
149	            ;;
150	    esac
151	done
152	```
153	
154	The actual implementation has more error checking and uses `socat`
155	or a small inline tool to handle framed socket I/O (Bash has no
156	native length-prefix socket reader). `socat` is broadly available;
157	for systems without it, a 30-line C tool can ship alongside.
158	
159	The `source`-once-per-file caching is automatic from Bash's
160	perspective: once a file is sourced, its functions are defined in
161	the shell's environment. Subsequent calls reuse them.
162	
163	## Box file convention
164	
165	A Bash box file defines functions by name:
166	
167	```bash
168	# maps/<name>/src/process.sh
169	process() {
170	    local input="$1"
171	    local count="$2"
172	    echo "${input}-${count}"
173	}
174	```
175	
176	The box JSON references it as `ref = "process.sh"`, `fn = "process"`.
177	Inputs become positional args (`$1`, `$2`, …). The function's stdout
178	is captured and returned as the output value.
179	
180	This is the existing phase 2 convention — kept as-is.
181	
182	## Input marshalling
183	
184	Inputs are passed as positional arguments to the Bash function. Each
185	input is bytes; Bash treats them as strings (with the usual caveat
186	that null bytes are not survivable in Bash variables). Type-aware
187	marshalling like the Lua and C specs do is not applicable — Bash has
188	one type, "string." Numeric and boolean inputs arrive as their
189	text form (`"42"`, `"true"`); the function uses arithmetic expansion
190	or `[[ ]]` tests as needed.
191	
192	## Output marshalling
193	
194	The function's stdout is captured by `$(...)` and sent back as the
195	output bytes. A `nil` / empty equivalent is just an empty string.
196	The output type declared in the box JSON is advisory for downstream
197	consumers — the bytes themselves are whatever the function `echo`ed.
198	
199	## Variable-size outputs
200	
201	For variable-size outputs (issue 302's large-value heap), the
202	spec.c side allocates from the large-value heap on receiving the
203	response, copies the response bytes there, stores the handle in
204	`out_buf`. The Bash subprocess does not interact with the heap —
205	it just sends back its full output, and the spec.c places it.
206	
207	## Idle cost
208	
209	Each Bash subprocess at rest is blocked on `read` from its socket.
210	Zero CPU. Roughly 3–5 MB RSS per process. For a 16-worker pool that
211	is 50–80 MB total. Acceptable.
212	
213	The subprocesses start at pool init and stay alive until pool
214	shutdown. There is no per-call subprocess churn.
215	
216	## Build
217	
218	```
219	langs/bash/
220	    spec.c            ← lang_spec_t implementation (socket client)
221	    bash-server.sh    ← persistent subprocess
222	    Makefile          ← builds spec.so
223	    spec.so           ← built artifact
224	```
225	
226	Spec Makefile target:
227	```make
228	spec.so: spec.c
229		$(CC) -shared -fPIC -O2 -Wall -o $@ $<
230	```
231	
232	No external dependencies beyond libc. The spec uses the standard
233	POSIX socket API. `bash-server.sh` requires `bash` and `socat`
234	(or the bundled framing helper) at runtime.
235	
236	## Error handling
237	
238	- Subprocess fails to start (missing `bash` or `bash-server.sh`):
239	  `init` returns NULL. Pool runner aborts.
240	- Subprocess dies mid-run (segfault, killed): `read` returns 0 or
241	  short; `invoke` returns nonzero, dispatch layer aborts.
242	- Box function nonzero exit: `"ok": false` in response, spec
243	  returns nonzero, abort.
244	- Wire protocol corruption (bad length prefix, malformed JSON):
245	  `invoke` returns nonzero, abort.
246	
247	No retry, no respawn. Per issue 303, errors crash the program.
248	
249	## Open questions
250	
251	- `socat` vs a bundled C framing helper: `socat` is broadly
252	  available but is not always installed by default (Alpine, minimal
253	  containers). A small bundled tool removes the external dependency.
254	  Decide based on what's convenient when implementing.
255	- Per-worker stderr capture: the example uses a shared
256	  `/tmp/bash-stderr` which would race between workers. Real
257	  implementation needs per-worker stderr (e.g., a pipe back to the
258	  subprocess that reads in a separate Bash background job, or just
259	  per-worker stderr files).
260	- Bundled framing helper: if we ship one, it lives in
261	  `langs/bash/bash-frame` and is built by the Makefile. Decision
262	  deferred.
263	
264	## Suggested implementation sequence
265	
266	1. Set up `langs/bash/` directory with a Makefile that builds an
267	   empty `spec.so` exporting a stub `soramech_lang_spec`.
268	2. Write `bash-server.sh` standalone — accepts framed requests on
269	   a socket, sources files, calls functions, writes framed
270	   responses. Test by hand with `socat` from the command line.
271	3. Implement `init` in `spec.c`: fork, exec `bash-server.sh`, wait
272	   for socket, connect, return handle.
273	4. Implement `teardown`: send shutdown, close socket, reap, unlink.
274	5. Implement `invoke`: build request JSON, send framed, read framed
275	   response, parse, copy output bytes, return.
276	6. Smoke test: a Bash box with one input, one output, runs end-to-end
277	   through the pool.
278	7. Add error-path handling: bad responses, dead subprocess, nonzero
279	   exit codes.
280	8. Test against `maps/driver-test` (Bash portions).
281	9. Add variable-size output support: route large responses through
282	   the large-value heap.
283	10. Decide socat-vs-bundled-framer based on implementation experience.
284	
285	## Relevant files
286	
287	- `drivers/bash.sh` — phase 2 Bash driver, retired by this spec
288	- `langs/lang-spec.h` — interface header (issue 303)
289	- `issues/303-language-runtime-spec.md` — spec contract this implements
290	- `issues/304-task-dispatch-layer.md` — dispatch layer that calls
291	  `invoke`
292	- `issues/302-wire-value-slot-store.md` — large-value heap for
293	  variable-size returns
294	- `docs/004-ipc-and-threading.md` — Option 3 (Unix domain sockets),
295	  the model this spec uses
296	- `issues/306-lua-language-spec.md` — sister spec, in-process Lua
297	- `issues/307-c-language-spec.md` — sister spec, in-process C with
298	  compile step
299	- `maps/driver-test` — multi-language test map including Bash boxes
300	
301	## Implementation log
302	
303	### fork+exec invoke — 2026-05-12
304	
305	First-pass implementation: every invoke forks bash with an inline
306	`-c` script that sources the box file and calls the named
307	function with the inputs as positional args. Child's stdout is
308	read through a pipe — that becomes the output bytes. Slow (~1 ms
309	per call), but it gets bash boxes running end-to-end. The
310	persistent Unix-domain-socket model lands as a follow-on.
311	
312	### Persistent socketpair + line-protocol — 2026-05-12
313	
314	Replaced the fork+exec path with a persistent bash subprocess per
315	worker thread. Each worker's `bash_init` does
316	`socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC)`, forks bash,
317	and execs `langs/bash/bash-server.sh` with the child-side socket
318	fd dup'd to stdin and stdout. The `SOCK_CLOEXEC` flag is load-
319	bearing: without it, concurrent worker forks would inherit each
320	other's parent-side fds, the bash subprocesses would never see
321	EOF, and orphan processes would pile up.
322	
323	The on-the-wire protocol is line-oriented:
324	
325	```
326	Request:                          Response:
327	  <n_args>\n        (or QUIT)       <exit_status>\n
328	  <file_path>\n                     <output_length>\n
329	  <fn_name>\n                       <output_bytes>
330	  <arg1>\n
331	  <arg2>\n
332	  ...
333	```
334	
335	`bash-server.sh` keeps an associative array of sourced files so a
336	box source is sourced exactly once per worker — analogous to the
337	Lua spec's per-worker module cache (issue 306).
338	
339	Per-worker teardown plumbed through the pool (issue 301's new
340	`pool_set_worker_teardown` callback) sends `QUIT\n`, closes the
341	parent fd, and `waitpid`s the subprocess with a 1-second
342	short-poll fallback to `SIGTERM`. Verified: zero orphan bash
343	processes after a `soramech-pool` run.
344	
345	Limitations of the current protocol:
346	- Arguments containing literal newlines are not supported
347	  (line-oriented framing). Box authors who need them base64-encode.
348	- Single-request/single-response; long-running bash boxes
349	  serialize behind themselves on the same worker. Multi-worker
350	  pools dispatch in parallel, so this is rarely the bottleneck.
351	
352	`tests/maps/hello/src/echo.sh` ships three fixture functions.
353	Five tests in `tests/308-bash-spec-test.c` cover happy paths
354	plus the missing-function failure mode; all pass against the
355	persistent-subprocess implementation.
356	
357	### Self-locating `bash-server.sh` — 2026-05-19
358	
359	Original implementation hard-coded the server path as
360	cwd-relative (`langs/bash/bash-server.sh`), which defeated the
361	compile-pipeline portability goal in issue 309. Replaced with a
362	`dladdr()`-based lookup that takes the dirname of *this*
363	`spec.so` and joins `bash-server.sh` to it. So when a compiled
364	artifact ships `compiled/langs/bash/{spec.so,bash-server.sh}`
365	side-by-side, the spec finds the script no matter what cwd the
366	runner was invoked from.
367	
368	`SORAMECH_BASH_SERVER` still overrides the lookup, and there's
369	still a cwd-relative fallback — but the fallback emits a stderr
370	warning so it can't silently mask a packaging bug. `-ldl` added
371	to `langs/bash/Makefile` for the new `dladdr` dependency.
372	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/309-build-system.md`

### Tool result

```
1	# 309 — Build system & Makefile orchestration
2	
3	## Status
4	open
5	
6	## Current behavior
7	Phase 2 has no compiled binary. The runner is a Lua script
8	(`src/007-runner-main.lua`) invoked via `luajit`. Drivers are shell
9	scripts that compile per-box C source on demand. There is no
10	top-level Makefile, no vendoring of C dependencies, no link step
11	beyond per-driver compile.
12	
13	Phase 3 introduces a real C binary (the pool runner) plus a set of
14	language spec `.so` files. They need to compile in the right order
15	with the right flags, and the build needs to be reproducible from a
16	clean checkout.
17	
18	## Concept
19	
20	A top-level `Makefile` at the project root orchestrates the entire
21	phase 3 build. It builds:
22	
23	1. The pool runner binary: `soramech-pool` (statically links the
24	   SoraMech-owned task pool, the vendored JSON parser, the slot
25	   store, the graph loader, the dispatch layer, the spec registry).
26	2. Each language spec: `langs/<name>/spec.so`, built by a small
27	   per-spec Makefile.
28	3. (Optional) Unit test binaries.
29	
30	Each component has its own Makefile if it has non-trivial build
31	rules. The top-level Makefile delegates to them via `$(MAKE) -C
32	<dir>`. A clean checkout becomes a working build with `make`.
33	
34	## Directory layout
35	
36	```
37	soramech/
38	├── Makefile                       ← top-level orchestration
39	├── soramech-pool                  ← built binary (gitignored)
40	├── src/
41	│   ├── 008-pool-runner.c          ← main entry, pool lifecycle (issue 301)
42	│   ├── slot-store.c / .h          ← per-task slot allocator (issue 302)
43	│   ├── graph-loader.c / .h        ← C loader, replaces 003-loader.lua (issue 305)
44	│   ├── dispatch.c / .h            ← dispatch action (issue 304)
45	│   └── spec-registry.c / .h       ← spec dlopen/registry (issue 303)
46	├── libs/
47	│   ├── task-pool/                 ← SoraMech-built thread pool
48	│   │   ├── pool.h
49	│   │   └── pool.c
50	│   └── json/                      ← vendored JSON parser (cJSON, jsmn, etc.)
51	│       ├── cjson.h
52	│       └── cjson.c
53	├── langs/
54	│   ├── lua/
55	│   │   ├── Makefile               ← builds spec.so
56	│   │   ├── spec.c
57	│   │   └── spec.so                ← built (gitignored)
58	│   ├── c/
59	│   │   ├── Makefile
60	│   │   ├── spec.c
61	│   │   ├── soramech-c.h           ← user-facing helper header
62	│   │   └── spec.so
63	│   └── bash/
64	│       ├── Makefile
65	│       ├── spec.c
66	│       ├── bash-server.sh
67	│       └── spec.so
68	└── build/                         ← intermediate object files (gitignored)
69	```
70	
71	`build/` holds intermediate `.o` files; binaries land alongside their
72	sources or at the project root. `.so` and `.o` files are gitignored;
73	`make clean` removes them.
74	
75	## Top-level targets
76	
77	```make
78	.PHONY: all runner specs clean test
79	
80	all: runner specs
81	
82	runner: soramech-pool
83	
84	soramech-pool: $(POOL_OBJECTS)
85		$(CC) -O2 -Wall -pthread -o $@ $^ -ldl
86	
87	specs:
88		$(MAKE) -C langs/lua
89		$(MAKE) -C langs/c
90		$(MAKE) -C langs/bash
91	
92	clean:
93		rm -f soramech-pool
94		rm -rf build/
95		$(MAKE) -C langs/lua clean
96		$(MAKE) -C langs/c clean
97		$(MAKE) -C langs/bash clean
98	
99	test: all
100		./scripts/run-tests.sh
101	```
102	
103	`POOL_OBJECTS` resolves to the `.o` files for every `src/*.c`,
104	`libs/task-pool/*.c`, and `libs/json/*.c`. A pattern rule compiles
105	each source into `build/`.
106	
107	## Per-language spec Makefiles
108	
109	Each `langs/<name>/Makefile` follows the same shape. Lua's:
110	
111	```make
112	LUA_INCLUDE ?= /usr/include/luajit-2.1
113	LUA_LIB    ?= /usr/lib/x86_64-linux-gnu
114	
115	CFLAGS  = -shared -fPIC -O2 -Wall -I$(LUA_INCLUDE)
116	LDFLAGS = -L$(LUA_LIB) -lluajit-5.1
117	
118	spec.so: spec.c
119		$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
120	
121	clean:
122		rm -f spec.so
123	```
124	
125	C's spec links `-ldl`. Bash's links nothing beyond libc. The pattern
126	is uniform.
127	
128	LuaJIT's include and lib paths are set via environment variables so
129	the build works on systems where they live elsewhere (macOS via
130	homebrew, Alpine, etc.). Defaults match Debian/Ubuntu.
131	
132	## Build modes
133	
134	```
135	make                  ← release: -O2 -Wall
136	make DEBUG=1          ← debug:   -O0 -g -DDEBUG -Wall -Wextra
137	make STRICT=1         ← strict:  release + -Werror -Wextra -Wpedantic
138	```
139	
140	`DEBUG=1` adds `-g` for symbols and `-DDEBUG` for opt-in debug
141	prints in the C sources. `STRICT=1` fails the build on any warning;
142	useful in CI.
143	
144	## Vendoring
145	
146	Vendored libraries live under `libs/` and are checked into the repo
147	as plain source — no submodules, no fetches at build time. To
148	update a vendored library, copy the new sources in and commit. The
149	vendored directory carries a small `VENDOR.md` noting the upstream
150	source and the version that was copied.
151	
152	Initial vendored libraries:
153	- `libs/task-pool/` — SoraMech-built thread pool (issue 301)
154	- `libs/json/` — JSON parser (issue 305 picks cJSON or similar)
155	
156	Future additions land alongside these.
157	
158	## Spec discovery at runtime
159	
160	The pool runner finds language specs by scanning the `langs/`
161	directory at startup, opening each `langs/<name>/spec.so` via
162	`dlopen`, and reading the `soramech_lang_spec` symbol via `dlsym`
163	(per issue 303). The path to `langs/` is resolved relative to the
164	runner binary's location: the runner reads `/proc/self/exe`
165	(Linux), follows the symlink, and looks for `langs/` next to it.
166	
167	This means the binary is location-flexible: the user can drop the
168	project tree anywhere, and `./soramech-pool` finds its specs
169	without a config file.
170	
171	If the user wants to override (custom spec dirs for development),
172	`SORAMECH_LANGS_DIR=/path/to/langs ./soramech-pool` overrides the
173	auto-detected location.
174	
175	## Compile and package a map for standalone execution
176	
177	Folded in from issue 219 (closed). The build system also handles the
178	"package a map for deployment" step that the editor's Compile button
179	(issue 222) and a CLI tool both invoke.
180	
181	The compile step turns an editable map directory into a self-contained
182	deployable directory:
183	
184	```
185	maps/<name>/compiled/
186	    pool-runner          ← copy (or symlink) of soramech-pool
187	    src/                 ← every source file the map uses
188	    bin/                 ← per-box compiled .so files (C boxes)
189	    langs/               ← copy of language spec .so files used
190	    manifest.json        ← every box, its language, artifact path, build version
191	```
192	
193	Running `./compiled/pool-runner` executes the map without reference to
194	the editor, the live `src/` outside `compiled/`, or any external
195	SoraMech installation.
196	
197	### Steps the compile step performs
198	
199	1. **Walk the graph.** Reuse the C loader (issue 305) to enumerate
200	   boxes, languages, and source files.
201	2. **Copy source.** Every file referenced by `box.ref` plus
202	   transitively-required libs lands in `compiled/src/`. The map's
203	   local `src/` is the only path `package.path` searches at runtime
204	   (issue 306), so all dependencies must be present.
205	3. **Compile per-box artifacts.** For each language present, invoke
206	   that spec's `compile` callback (issue 303). The C spec emits a
207	   wrapper from the box's signature into `compiled/bin/` (issue 307).
208	   Lua and Bash have no compile step; their files are copied verbatim.
209	4. **Copy spec libraries.** `langs/<name>/spec.so` for every language
210	   used in the map is copied to `compiled/langs/<name>/spec.so`. The
211	   pool runner's spec discovery (above) finds them relative to the
212	   binary location.
213	5. **Copy or symlink the runner binary.** `--portable` copies; the
214	   default symlinks to save disk on the dev machine.
215	6. **Write the manifest.** `compiled/manifest.json` lists every box,
216	   its language, its compiled artifact path (or source file for
217	   interpreted languages), and the SoraMech build version.
218	
219	### Incremental compile
220	
221	mtime comparison per artifact: if the source is newer than the
222	compiled output, recompile that one; otherwise skip. The C spec's
223	`compile` callback (issue 307) implements this per-box; the package
224	step inherits it.
225	
226	### Editor invocation (issue 222)
227	
228	The Compile button calls `POST /maps/<name>/compile`. The server
229	shells out to a `soramech-compile` CLI (or invokes the same code
230	in-process). Output streams back to the editor for display. The
231	editor itself never runs the compiled output; the user invokes
232	`./compiled/pool-runner` from a terminal.
233	
234	### CLI invocation
235	
236	Standalone `soramech-compile <map-dir>` runs the same pipeline
237	without the server. Used in CI and headless deployment.
238	
239	### Errors
240	
241	Any failure aborts the compile and surfaces a precise message:
242	which box, which file, what failed. Partial output is left in
243	`compiled/` for inspection but the manifest is not written, so the
244	deployment is recognizably broken until a clean compile succeeds.
245	
246	### Open questions (compile step)
247	
248	- **Multi-architecture**: a directory built on x86_64 won't run on
249	  arm64. Out of scope — users compile on the target. Could later
250	  add cross-compile if the SoraMind cluster goes mixed-arch.
251	- **Source-copy granularity**: copy the file referenced by `box.ref`
252	  plus a static-analysis sweep for `require` / `source` /
253	  `#include`, or just copy the entire `libs/` tree. Lean toward
254	  static analysis with a fallback whole-tree copy if analysis fails.
255	
256	## Open questions
257	
258	- macOS: `.so` becomes `.dylib`, and there's no `/proc/self/exe`.
259	  Use `_NSGetExecutablePath`. Cross-platform spec discovery is a
260	  small platform-#ifdef. Fold into the spec registry implementation.
261	- Out-of-tree build (a separate `build/` directory holding all
262	  artifacts): nice-to-have, not blocking. Current layout uses
263	  in-tree `.o` files in `build/` already — full out-of-tree means
264	  the `.so`s live in `build/langs/lua/spec.so` instead of next to
265	  the source. Defer.
266	- Static vs dynamic linking of vendored libs: vendored sources
267	  compile straight into the runner binary. No separate `.so` for
268	  the task pool or JSON parser. Right call given how small they are.
269	
270	## Suggested implementation sequence
271	
272	1. Write the top-level Makefile with empty `runner`, `specs`,
273	   `clean` targets. Verify `make` succeeds against an empty source
274	   tree.
275	2. Build the SoraMech thread pool into `libs/task-pool/` (3d-rts
276	   referenced as design inspiration). Add a
277	   `VENDOR.md`.
278	3. Vendor cJSON (or chosen JSON parser) into `libs/json/`. Add a
279	   `VENDOR.md`.
280	4. Add the pool-runner build rule. Compiles `src/*.c` plus vendored
281	   libs into `soramech-pool`. Initially the runner is just `int
282	   main() { return 0; }` to verify the link.
283	5. Add `langs/lua/Makefile`. Build `spec.so` from a stub `spec.c`.
284	6. Add `langs/c/Makefile`. Same shape.
285	7. Add `langs/bash/Makefile`. Same shape.
286	8. Add `make DEBUG=1` and `make STRICT=1` modes.
287	9. Wire `make test` into a placeholder script (real tests come from
288	   issue 311).
289	
290	## Relevant files
291	
292	- `Makefile` — top-level (to be created)
293	- `langs/lua/Makefile`, `langs/c/Makefile`, `langs/bash/Makefile`
294	  — per-spec (to be created)
295	- `libs/task-pool/`, `libs/json/` — vendored sources (to be added)
296	- `issues/301-pool-lifecycle-and-worker-init.md` — pool runner this
297	  builds
298	- `issues/303-language-runtime-spec.md` — spec discovery uses
299	  `dlopen`/`dlsym`
300	- `issues/306-lua-language-spec.md`, `307-c-language-spec.md`,
301	  `308-bash-language-spec.md` — per-spec build details
302	- `issues/311-integration-tests-and-run-output.md` — `make test`
303	  consumers
304	- `issues/222-compile-button-and-assets-directory.md` (completed) —
305	  the editor button this compile step services
306	- `issues/completed/219-map-compiler.md` — original issue, folded
307	  into this one
308	
309	## Implementation log
310	
311	### Scaffolding milestone — 2026-05-12
312	
313	What's in place:
314	- `Makefile` at the project root with `all` / `runner` / `specs` /
315	  `clean` / `test` / `help` targets, and `DEBUG=1` / `STRICT=1`
316	  build modes. Source discovery is wildcard-based across
317	  `src/*.c`, `libs/task-pool/*.c`, `libs/json/*.c` — additional C
318	  sources land as future issues compile them in automatically.
319	- `src/008-pool-runner.c` — minimal `main()` that parses argv,
320	  prints usage on `--help`, prints a "scaffold build" banner and
321	  exits 0 on a map-dir argument. Plus `src/008-pool-runner.info.md`.
322	- `langs/lang-spec.h` — the public `lang_spec_t` C contract from
323	  issue 303 (the four basic callbacks; the fast-path callbacks
324	  from issue 312 land later).
325	- `langs/{lua,c,bash}/spec.c` + `Makefile` + `spec.info.md` — each
326	  builds a `spec.so` that exports `soramech_lang_spec` with all
327	  callbacks `NULL`. Verified with `nm -D langs/*/spec.so` — the
328	  symbol is exported at `0x4020 D` in every spec.
329	- `.clangd` config so the editor LSP resolves the include path the
330	  same way the Makefile does.
331	- `.file-index-counter` set to `008`.
332	
333	Verified `make` clean → build → run cycle works in default,
334	`DEBUG=1`, and `STRICT=1` modes.
335	
336	What's still ahead inside 309:
337	- Vendoring `libs/task-pool/` (lands with 301).
338	- Vendoring `libs/json/` (lands with 305 — choice of cJSON / jsmn
339	  decided at implementation time).
340	- Spec discovery via `/proc/self/exe` (lands with 303's spec
341	  registry).
342	- Compile / package step for standalone `compiled/pool-runner`
343	  (lands with 305 + 307).
344	- Real `make test` (lands with 311).
345	
346	These follow-ons depend on their respective issues; this issue
347	stays open until they're all wired in.
348	
349	### Compile / package pipeline — 2026-05-19
350	
351	`scripts/soramech-compile.sh <map>` now produces a portable
352	`<map>/compiled/` directory laid out as:
353	
354	```
355	compiled/
356	  meta.json, boxes/, src/, data/, *.txt   # verbatim copies
357	  langs/<lang>/spec.so                    # per-language plugin
358	  langs/bash/bash-server.sh               # only if bash boxes exist
359	  bin/<box>.so                            # pre-compiled C boxes
360	  pool-runner                             # *real copy* of soramech-pool
361	  manifest.json                           # n_boxes, languages, etc.
362	```
363	
364	Two subtle correctness points landed alongside the script:
365	
366	1. **pool-runner must be a real copy, not a symlink.** The runner
367	   discovers `langs/` via `readlink("/proc/self/exe")`. A symlink
368	   resolves to its target, so a symlinked pool-runner would pick
369	   up the project tree's `langs/` instead of the bundled one. A
370	   real copy makes the artifact self-describing — `compiled/`
371	   stands alone with no reference to the source map directory.
372	
373	2. **The bash spec self-locates `bash-server.sh`.** Previously
374	   the bash spec used a cwd-relative path
375	   (`langs/bash/bash-server.sh`), defeating portability. It now
376	   uses `dladdr()` on its own symbol to find the loaded `.so`
377	   path, takes `dirname()`, and looks for `bash-server.sh` next
378	   to itself. SORAMECH_BASH_SERVER still overrides; the
379	   cwd-relative path remains as a fallback (with a stderr
380	   warning so the fallback isn't silent). This required adding
381	   `-ldl` to `langs/bash/Makefile`.
382	
383	Verified end-to-end: `scripts/run-tests.sh` includes a new
384	`compile pipeline (portable run)` integration check that runs
385	soramech-compile on the pipeline fixture, invokes the compiled
386	artifact from `/tmp` (so cwd cannot accidentally satisfy any
387	relative path), and asserts both the captured stdout and the
388	`file_write` destination match the expected values.
389	
390	Per-map `compiled/` directories under `tests/maps/*/` are
391	gitignored.
392	
393	The follow-ons enumerated above are now all in place; the issue
394	is effectively done.
395	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/310-priority-queue.md`

### Tool result

```
1	# 310 — Priority queue: wired in, no-op behaviorally
2	
3	## Status
4	open
5	
6	## Current behavior
7	The SoraMech thread pool's `pool_spawn` (modeled on 3d-rts's API)
8	takes a priority parameter:
9	
10	```c
11	task_id_t pool_spawn(task_pool_t *pool,
12	                     action_fn_t  *actions,
13	                     void        **action_args,
14	                     int           n_actions,
15	                     int           priority);
16	```
17	
18	Higher priority tasks are dispatched ahead of lower priority tasks
19	when multiple are ready. The pool's queue handles the ordering.
20	
21	Phase 3 needs to specify what SoraMech does with this knob. The
22	simplest answer is the right one for now: do nothing.
23	
24	## Decision
25	
26	Every dispatch task is submitted at priority `1`. The compile-time
27	constant `MAX_PRIORITY` is set to `1`. There is no graph-level or
28	box-level concept of priority. The runner does not expose any user-
29	facing knob for it.
30	
31	The mechanism is wired through the codebase — every `pool_spawn`
32	call passes a priority, the constant is referenced rather than
33	hard-coded inline — but the behavior is identical to a single-
34	priority pool. Tasks run in submission order (modulo whatever
35	ordering the pool guarantees within a single priority level).
36	
37	## Why wire it in if it does nothing
38	
39	Two reasons:
40	
41	1. **Future hooks land as config changes, not refactors.** When a
42	   real priority use case appears (allocator cleanup at lowest
43	   priority; user-marked urgent boxes; iterator queue draining
44	   prioritized over new entry submission), raising `MAX_PRIORITY`
45	   and emitting different priorities at the right call sites is a
46	   line edit, not a structural change. If the priority parameter
47	   were stripped out, adding it later means going back through every
48	   `pool_spawn` callsite and threading a new argument.
49	
50	2. **The pool's API requires it.** `pool_spawn` takes a priority.
51	   Callers must pass something. Passing `1` consistently is the
52	   straightforward expression of "no priority concept yet."
53	
54	## Specific call sites
55	
56	```c
57	// src/dispatch.c — every dispatch task
58	pool_spawn(pool, &dispatch_action, &task_arg, 1, SM_PRIORITY_DEFAULT);
59	
60	// src/008-pool-runner.c — entry-box submission at startup
61	pool_spawn(pool, &dispatch_action, &task_arg, 1, SM_PRIORITY_DEFAULT);
62	
63	// src/slot-store.c — deferred allocator cleanup task (issue 302)
64	pool_spawn(pool, &cleanup_action, &cleanup_arg, 1, SM_PRIORITY_DEFAULT);
65	
66	// langs/bash/spec.c — ... none, the spec doesn't spawn pool tasks
67	```
68	
69	`SM_PRIORITY_DEFAULT` is `1`. Defined in `src/008-pool-runner.h` (or
70	a sibling header) so all callers use the same name.
71	
72	## Future hooks (not in scope, but called out)
73	
74	When `MAX_PRIORITY` is raised, the natural assignments are:
75	
76	| Future use case                                     | Priority |
77	|-----------------------------------------------------|----------|
78	| User-marked "urgent" boxes (UI / interactive)       | 3        |
79	| Iterator continuation when its queue is non-empty   | 2        |
80	| Default dispatch task                               | 1        |
81	| Allocator cleanup task (deferred coalescing, 302)   | 0        |
82	
83	These are speculative. The actual priority schema is decided when
84	there is a workload that demonstrates the need.
85	
86	A box-level priority field could go in the box JSON (`"priority":
87	2`) and be plumbed through `dispatch_task_t`. The dispatch action
88	reads it, passes it to `pool_spawn` for any successor tasks. This
89	is a future change.
90	
91	## Open questions
92	
93	(none currently — this is intentionally a small change to the call
94	sites. The design conversation about what priorities should mean is
95	deferred until concrete need.)
96	
97	## Suggested implementation sequence
98	
99	1. Define `SM_PRIORITY_DEFAULT` (= 1) and `SM_MAX_PRIORITY` (= 1)
100	   in a runner-wide header.
101	2. Make every `pool_spawn` call site pass `SM_PRIORITY_DEFAULT`.
102	3. Set the pool's max priority at `pool_create` time using
103	   `SM_MAX_PRIORITY` (if the pool API takes a max-priority hint).
104	4. That's it. No tests required beyond verifying the constants
105	   compile and the pool runs.
106	
107	## Relevant files
108	
109	- `src/008-pool-runner.c` — calls `pool_spawn` for entry boxes
110	- `src/dispatch.c` — calls `pool_spawn` for successor tasks
111	- `src/slot-store.c` — calls `pool_spawn` for deferred cleanup
112	- `libs/task-pool/900-task-pool.h` — `pool_spawn` priority parameter
113	- `issues/301-pool-lifecycle-and-worker-init.md` — pool lifecycle
114	- `issues/304-task-dispatch-layer.md` — dispatch task submission
115	- `issues/302-wire-value-slot-store.md` — deferred cleanup tasks
116	
117	## Implementation log
118	
119	### API + ordering — 2026-05-12
120	
121	`pool_spawn` gained a fourth argument: `int priority`. Higher
122	priorities dequeue first; ties between equal priorities resolve
123	to FIFO. Linear-scan insertion (strict less-than at the
124	comparison) on the singly-linked queue — small queue, O(n) walk
125	is fine. A unit test in `tests/301-pool-test.c` queues six tasks
126	of varied priority before the init barrier releases the
127	single-worker pool, and verifies the execution order matches the
128	priority/FIFO rule.
129	
130	Existing call sites all pass priority 0; behaviorally the queue
131	is still a plain FIFO. 304's dispatch action can grow into
132	priority-aware scheduling without another signature change.
133	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/311-integration-tests-and-run-output.md`

### Tool result

```
1	# 311 — Integration tests & run output (`last-run.json`)
2	
3	## Status
4	open
5	
6	## Current behavior
7	
8	Phase 2 writes a `last-run.json` after every map run, recording per-
9	box invocations, inputs, outputs, and any errors. The editor reads
10	it to display a run summary. Phase 2 testing is ad-hoc: a few maps
11	in `maps/` (`hello`, `branch-test`, `classify-demo`, `driver-test`)
12	that the developer runs by hand to verify the synchronous executor
13	still works.
14	
15	Phase 3 changes the shape of the problem: many concurrent
16	invocations, possibly thousands per run when iterators are in play,
17	written from multiple worker threads. The phase 2 "build a single
18	table at end of run" approach does not survive that. And the
19	existing maps cover only a small slice of the new surface (no
20	queued inputs, no large iterator counts, no fan-in).
21	
22	This issue covers two things that go together: the format and
23	writer for the run output, and the integration test plan that
24	exercises the phase 3 runtime end-to-end.
25	
26	## Part A — Run output
27	
28	### Decoupling from the editor
29	
30	The editor (phase 2) is being scoped down to a pure map editor — it
31	does not know how to run maps and does not display run results.
32	`last-run.json` is therefore not a UI feed. It exists for:
33	
34	- LLM consumption — agents reading the file to understand what a
35	  run did
36	- CLI-driven inspection — a future viewer utility (out of scope
37	  here) that pretty-prints the file
38	- Test harness consumption — the integration tests below diff the
39	  written file against expected output
40	
41	Decoupling means the writer's design is driven by these consumers,
42	not by editor rendering needs.
43	
44	### File format: JSON Lines
45	
46	The phase 2 single-document format does not work when the runner is
47	streaming events from many threads. Use JSON Lines instead — one
48	JSON object per line, each line is one event, the file grows
49	monotonically as the run proceeds.
50	
51	Event types:
52	
53	```jsonl
54	{"event":"run_start", "ts":1234567890.123, "map":"maps/classify-demo", "n_workers":16}
55	{"event":"task_submit", "ts":..., "task_id":42, "box_id":"classify", "worker_idx":-1}
56	{"event":"task_start", "ts":..., "task_id":42, "worker_idx":3}
57	{"event":"task_end", "ts":..., "task_id":42, "worker_idx":3, "duration_us":127, "output_size":12}
58	{"event":"slot_alloc", "ts":..., "slot_id":5, "size":256, "n_cells":1}
59	{"event":"slot_free", "ts":..., "slot_id":5}
60	{"event":"run_end", "ts":..., "duration_us":1850, "n_tasks":7}
61	```
62	
63	JSONL is append-only, which makes thread-safe writing tractable: a
64	single mutex around the write call serializes the lines, and each
65	line is self-contained so a partial line on crash is recoverable
66	(reader skips the broken trailing line).
67	
68	The reader (LLM, viewer, test harness) parses one line at a time;
69	total file size doesn't have to fit in memory.
70	
71	### What gets logged
72	
73	Per-task: submission, start, end, duration, output size. Inputs
74	and outputs are NOT logged by default — for iterator boxes
75	producing thousands of invocations, logging every value bloats
76	the file unbounded. There is a verbosity switch:
77	
78	- `SORAMECH_LOG_VALUES=1` — log inputs and outputs (truncated past
79	  some byte limit, e.g. 4 KB) per task. For debugging.
80	- Default: log task lifecycle but not values.
81	
82	Slot allocator events (alloc, free, coalesce, grow) at a separate
83	verbosity level. Off by default; on under `SORAMECH_LOG_SLOTS=1`.
84	For diagnosing memory issues.
85	
86	### Writer mechanism
87	
88	A single dedicated writer thread inside the runner consumes events
89	from a multi-producer single-consumer ring buffer. Worker threads
90	push event records (small fixed-size structs) into the ring; the
91	writer pulls them, serializes to JSON, and writes lines to the
92	file. This avoids per-event mutex contention — the ring buffer is
93	the only shared state, and a well-written MPSC ring is lock-free
94	on the producer side.
95	
96	Alternative considered: each worker writes its own log file, merge
97	at end. Simpler concurrency, but readers would have to merge by
98	timestamp — and any cross-event reasoning (was task A still
99	running when task B ended?) requires the merge anyway.
100	
101	The writer thread is started after the pool is created and joined
102	before `pool_destroy`. It also handles `run_end` as the very last
103	event, including the final summary.
104	
105	### File location
106	
107	`<map-dir>/last-run.jsonl` (note: `.jsonl` not `.json` — the
108	extension reflects the format change). Overwritten on every run.
109	
110	A rotated archive (`last-run.jsonl.1`, `.2`, …) is out of scope.
111	Users who want history copy the file before re-running.
112	
113	## Part B — Integration tests
114	
115	### Scope
116	
117	End-to-end tests of the pool runner against real map directories.
118	Each test is:
119	
120	1. A map directory under `tests/maps/<name>/`
121	2. An expected `last-run.jsonl` (or a comparison spec) under
122	   `tests/expected/<name>.jsonl`
123	3. A test driver script that runs the map and diffs
124	
125	The test driver is `scripts/run-tests.sh`. It iterates
126	`tests/maps/`, runs each, diffs against expected (with a normalizer
127	that drops timestamps, durations, worker indices — anything non-
128	deterministic), and reports pass/fail.
129	
130	### Test maps
131	
132	Coverage targets:
133	
134	| Test map                  | What it exercises                                    |
135	|---------------------------|------------------------------------------------------|
136	| `hello`                   | Single Lua box, one input, one output                |
137	| `data-only`               | Data box → call box. No inputs, just stored values   |
138	| `fan-out-3`               | One output → three consumer boxes (peek, refcount)   |
139	| `fan-in-iterator`         | Three producers → iterator with queued input         |
140	| `comparator-routing`      | Comparator with all three branches (lt/eq/gt)        |
141	| `iterator-100`            | Iterator with 100 input values, round-robin to 3 paths |
142	| `multilang`               | One Lua box, one C box, one Bash box, all wired      |
143	| `large-output`            | Box producing a 1 MB string (variable-size heap)     |
144	| `cycle-detector-fail`     | Non-iterator cycle — load must abort                 |
145	| `missing-input-fail`      | Connection references a nonexistent box — load aborts|
146	| `box-error-fail`          | Box function returns nonzero — runner aborts         |
147	
148	The `*-fail` tests verify error behavior — the runner is expected
149	to exit nonzero with a precise error message. The driver matches
150	exit code and stderr against expected.
151	
152	### Test driver
153	
154	```bash
155	# scripts/run-tests.sh
156	for test_dir in tests/maps/*/; do
157	    name=$(basename "${test_dir}")
158	    expected="tests/expected/${name}.jsonl"
159	
160	    actual=$( "${SOURCE_DIR}/soramech-pool" "${test_dir}" )
161	    rc=$?
162	
163	    if [ -f "${expected}.exit" ]; then
164	        # *-fail tests: assert exit code and stderr substring
165	        check_failure "${name}" "${rc}" "${test_dir}/last-run.jsonl"
166	    else
167	        # success tests: diff normalized last-run.jsonl
168	        check_success "${name}" "${test_dir}/last-run.jsonl" "${expected}"
169	    fi
170	done
171	```
172	
173	`normalize_jsonl` strips timestamps, durations, worker indices,
174	slot ids, and task ids (all non-deterministic) and sorts events by
175	event type. The result should be deterministic across runs.
176	
177	### Phase 3 demo map
178	
179	A single map under `issues/completed/demos/phase-3/` that
180	demonstrates everything working together:
181	
182	- Multiple entry-point data boxes
183	- Lua, C, and Bash boxes all wired together
184	- An iterator with a queued input
185	- A comparator routing into different downstream paths
186	- A large-output box (variable-size heap)
187	- Non-trivial graph topology (≥ 10 boxes)
188	
189	Run with `./run.sh` from the demo directory. Prints a short summary
190	of what happened (parsing the `last-run.jsonl`) so the demo is
191	visible without a viewer utility.
192	
193	This satisfies the standing project rule (CLAUDE.md) that each
194	phase produces a runnable demo in `issues/completed/demos/`.
195	
196	### Test infrastructure constraints
197	
198	- Tests must not depend on the editor or any HTTP server
199	- Tests must not depend on internet access
200	- Tests must run in under 60 seconds total on a developer machine
201	- Tests must be deterministic when normalized (any
202	  non-determinism is normalized out, not papered over with retries)
203	
204	## Open questions
205	
206	- Whether to log inputs/outputs by default. Useful for development,
207	  noisy for production. Current decision: opt-in via env var,
208	  default off.
209	- Slot-event logging: also opt-in. Useful for diagnosing memory
210	  issues. Off by default.
211	- File format: JSON Lines is the choice. An alternative was a
212	  protobuf or msgpack binary log, which is faster to write but
213	  harder to inspect by hand. JSONL wins on inspectability.
214	- Whether to keep timestamps as Unix epoch floats or ISO 8601
215	  strings. Epoch floats are smaller and easier to diff. Stick with
216	  floats.
217	
218	## Suggested implementation sequence
219	
220	### Run output (Part A)
221	1. Define the event struct and JSONL line format. Start with
222	   `run_start`, `task_start`, `task_end`, `run_end`.
223	2. Implement the MPSC ring buffer for events.
224	3. Implement the writer thread: pulls from ring, serializes,
225	   appends a line to the output file.
226	4. Wire event emission into the pool runner: `run_start` at
227	   startup, `task_start` and `task_end` in the dispatch action,
228	   `run_end` after quiescence.
229	5. Add the verbosity env vars and the optional event types.
230	
231	### Tests (Part B)
232	6. Create `tests/maps/` and `tests/expected/` directory structure.
233	7. Write the `hello` test as the first end-to-end smoke check.
234	8. Write the test driver `scripts/run-tests.sh` and the JSONL
235	   normalizer.
236	9. Add tests one at a time as each runtime feature lands. The
237	   `*-fail` tests can land before their corresponding successful
238	   tests if the failure path is implemented first.
239	10. Build the phase 3 demo map under `issues/completed/demos/phase-3/`
240	    once the rest of phase 3 is implemented.
241	
242	## Relevant files
243	
244	- `scripts/run-tests.sh` — test driver (to be written)
245	- `tests/maps/`, `tests/expected/` — test fixtures (to be created)
246	- `issues/completed/demos/phase-3/` — phase 3 demo (to be built)
247	- `src/008-pool-runner.c` — emits run_start / run_end
248	- `src/dispatch.c` — emits task_start / task_end
249	- `src/slot-store.c` — emits slot_alloc / slot_free (verbose mode)
250	- `issues/301-pool-lifecycle-and-worker-init.md` — pool lifecycle
251	- `issues/302-wire-value-slot-store.md` — slot events
252	- `issues/304-task-dispatch-layer.md` — task lifecycle events
253	- `issues/305-c-graph-loader.md` — load-time errors
254	- `issues/309-build-system.md` — `make test` invokes this
255	- `docs/004-ipc-and-threading.md` — blocking semantics relevant to
256	  test design
257	
258	## Implementation log
259	
260	### JSON writer half — 2026-05-12
261	
262	`libs/json/json.c` had stub writer bodies left over from 314's
263	parser milestone; this pass filled them in. The writer maintains
264	a small depth stack (capped at 16) where each entry packs two
265	booleans — "next element is first" and "this level is an
266	object." That's enough to handle comma placement and pick the
267	right close bracket. Overflow of the caller-supplied buffer is
268	sticky on `w->err`; `json_writer_finish` reports it.
269	
270	Eight new writer tests in `tests/314-json-test.c` cover the
271	primitives, escape encoding, empty and non-empty containers,
272	nesting depth, overflow detection, and a round-trip where the
273	writer emits an object that the parser then decodes and the test
274	verifies via the accessor API.
275	
276	Still ahead for 311: the JSONL event writer (a thin wrapper that
277	serializes `task_start` / `task_end` / `run_start` / `run_end`
278	records using this writer), the MPSC ring buffer the worker
279	threads push events into, and the dedicated writer thread that
280	drains them.
281	
282	### Event emitters — 2026-05-12
283	
284	`src/013-jsonl-events.{c,h}` ships the per-event emit functions
285	on top of issue 314's bounded writer:
286	
287	- `jsonl_emit_run_start(w, ts, map, n_workers)`
288	- `jsonl_emit_task_submit(w, ts, task_id, box_id, worker_idx)`
289	- `jsonl_emit_task_start(w, ts, task_id, worker_idx)`
290	- `jsonl_emit_task_end(w, ts, task_id, worker_idx, duration_us, output_size)`
291	- `jsonl_emit_run_end(w, ts, duration_us, n_tasks)`
292	
293	Each emit serializes into a fixed 512-byte buffer, appends a
294	newline, and writes to a `FILE *` under a per-writer mutex. The
295	MPSC ring + dedicated writer thread aren't there yet — the mutex
296	is a placeholder until dispatch is producing events fast enough
297	to justify the contention.
298	
299	`tests/013-jsonl-events-test.c` writes a full sequence, slurps
300	the file back, and parses each line through 314's JSON parser to
301	verify the event shape. A concurrent-emit test runs 8 producer
302	threads each emitting 200 events into the same writer and
303	verifies every line round-trips.
304	
305	Still ahead inside 311: slot-event opt-in
306	(`SORAMECH_LOG_SLOTS=1`), inputs/outputs opt-in
307	(`SORAMECH_LOG_VALUES=1`), the MPSC ring + writer thread, and
308	the integration-test harness map fixtures.
309	
310	### Multi-producer event queue + writer thread — 2026-05-12
311	
312	`src/014-event-queue.{c,h}` ships the producer-side queue and the
313	dedicated writer thread. Producers (worker threads + the main
314	thread) call `event_queue_run_start` / `task_submit` / `task_start`
315	/ `task_end` / `run_end`; each emit appends a small fixed-size
316	event record under a brief mutex and signals a CV. The writer
317	thread sleeps on the CV, snapshots the queue head into a local
318	batch under the lock, then drains the batch outside the lock —
319	JSON encoding + fwrite happen with zero producer contention.
320	
321	Four unit tests in `tests/014-event-queue-test.c` cover single
322	producer / multi-thread burst (8 producers × 250 events each =
323	2000 lines, all round-trip through the JSON parser) / shutdown-
324	drain / pending-count.
325	
326	The dispatch action (issue 304) opt-in emits task_start and
327	task_end events with monotonic-clock durations via the queue;
328	the pool runner emits run_start and run_end. `soramech-pool`
329	writes to `map_dir/tmp/last-run.jsonl` when that directory
330	exists, falling back to `/tmp/soramech-last-run.jsonl`.
331	
332	What's still ahead inside 311:
333	- Lock-free Vyukov MPSC ring with sequence numbers — the
334	  architecture doc's preferred design. The mutex queue is a
335	  placeholder; upgrade once contention shows up.
336	- Opt-in slot events and per-task input/output dumps.
337	
338	### Integration test runner — 2026-05-12
339	
340	`scripts/run-tests.sh` runs every fixture map under `tests/maps/`
341	through `soramech-pool` and asserts the expected box-output
342	substrings show up in stderr. `make test` invokes the script
343	after the unit tests so a full `make test` covers everything
344	from slot operations all the way through end-to-end Lua / C /
345	Bash dispatch via JSONL logging.
346	
347	Six fixture checks at the moment:
348	- `calc`            — single Lua `add → 42`
349	- `hello`           — data → Lua, `greet → "Hello, World!"`
350	- `comparator`      — `high` fires, `low` / `mid` don't
351	- `iter-route`      — single-fire iterator picks `out_0` → `a`
352	- `pipeline`        — five-box multilang chain
353	- `pipeline file`   — verifies the file_write sink wrote
354	  `"11!"` to `/tmp/soramech-pipeline-out.txt`
355	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/312-same-language-wire-fast-path.md`

### Tool result

```
1	# 312 — Same-language wire fast path: skip JSON for adjacent same-language boxes
2	
3	## Status
4	open
5	
6	## Current behavior
7	
8	The wire format between boxes is JSON (`docs/004-ipc-and-threading.md`,
9	issue 302's "no type tags" — slot store stores opaque bytes; the
10	language spec encodes/decodes). Every wire pays the JSON encode +
11	decode cost on every value pass, even when the producer and the
12	consumer are the same language and could use a faster
13	language-native serialization.
14	
15	For a Lua → Lua wire passing a 10-element table:
16	1. Producer's spec: `cjson.encode(table)` → string of N bytes
17	2. `slot_push(slot, string, N)`
18	3. `slot_peek(slot, buf, N)`
19	4. Consumer's spec: `cjson.decode(buf)` → Lua table
20	
21	Steps 1 and 4 are the cost. They're language-agnostic and unnecessary
22	when both endpoints share a language.
23	
24	## Intended behavior
25	
26	Each language spec exposes **two invoke paths**:
27	
28	```c
29	typedef struct {
30	    const char *name;
31	    const char *file_ext;
32	
33	    void *(*init)(int worker_idx);
34	    void  (*teardown)(void *handle);
35	    int   (*compile)(...);
36	
37	    // Universal path — JSON bytes in, JSON bytes out
38	    int   (*invoke_json)  (void *handle, /* common args */);
39	
40	    // Fast path — language-native bytes in, language-native bytes out
41	    int   (*invoke_native)(void *handle, /* common args */);
42	
43	    // Bridges used when a value crosses a language boundary
44	    int   (*native_to_json)(const void *src, int src_size,
45	                            void *dst, int dst_capacity, int *dst_size);
46	    int   (*json_to_native)(const void *src, int src_size,
47	                            void *dst, int dst_capacity, int *dst_size);
48	} lang_spec_t;
49	```
50	
51	`invoke_native` is required (every spec implements one). Per-language
52	"native" format:
53	- **Lua**: `lua_dump` for closures, `cjson` or msgpack for tables, raw
54	  bytes for strings/numbers. Spec's choice as long as it's
55	  symmetric with its own decoder.
56	- **C**: raw struct memcpy. The compile-time wrapper (issue 307)
57	  knows the types; serialization is `memcpy(slot_buf, &val, sizeof
58	  val)`.
59	- **Bash**: strings only — Bash has no other native form, so the
60	  fast path is identical to the JSON path. Implement as a thin
61	  alias.
62	
63	`invoke_json` is the universal interop format. Required for any
64	language that participates in cross-language wires. A spec that
65	only ever talks to itself can skip `invoke_json` (compile error if
66	a wire crosses to it from another language).
67	
68	## Compile-time wire classification
69	
70	Every wire gets a `fast_path: true | false` flag set at build time
71	(issue 309 build step):
72	
73	```python
74	for wire in graph.wires:
75	    src_lang = box_at(wire.from).lang
76	    dst_lang = box_at(wire.to).lang
77	    wire.fast_path = (src_lang == dst_lang)
78	```
79	
80	The flag becomes part of the compiled manifest. Runtime never
81	re-evaluates language pairing.
82	
83	## Per-edge format negotiation
84	
85	Classification is **per edge**, not per box. A box always
86	executes its user function in native form. The format on the
87	wire is decided independently for each input edge and each
88	output edge based on the language pair across that edge:
89	
90	```
91	input edge native?   = (producer.lang == this_box.lang)
92	output edge native?  = (this_box.lang == consumer.lang)
93	```
94	
95	So a Lua box sitting in the middle of `[C → Lua → C]` works
96	like this:
97	
98	- Input edge from the C producer: cross-lang, so the slot
99	  carries JSON. On read, the Lua spec calls `json_to_native`
100	  to lift it into a Lua value.
101	- The box's user function runs natively in Lua. It returns a
102	  Lua value.
103	- Output edges to the two C consumers: cross-lang, so the spec
104	  calls `native_to_json` once and pushes JSON bytes to each
105	  consumer's slot.
106	
107	A Lua box in `[Lua → Lua → C]` works differently *along its
108	own edges*:
109	
110	- Input edge from the Lua producer: same-lang, slot carries
111	  native binary. No conversion on read.
112	- User function runs.
113	- Output edges: one is same-lang (push native), the other is
114	  cross-lang (call `native_to_json`, push JSON). The producer
115	  pushes once to each consumer's slot in the right format.
116	
117	This means **same-lang neighbors get the fast path even when
118	some other neighbor is cross-lang**. The classification doesn't
119	collapse on the first cross-language edge it finds; it stays
120	per-edge.
121	
122	### Why per-edge, not per-box
123	
124	The earlier design considered an "all same-lang or fall back
125	to JSON for all" rule (the simpler classification). It's
126	strictly dominated by the per-edge rule:
127	
128	- The dominant case is "a Lua sub-graph occasionally talks to
129	  C." Per-box would force the Lua-to-Lua interior to round-trip
130	  through JSON for every box that has *any* C neighbor.
131	- Per-edge keeps the cost local to the actual cross-lang
132	  boundary. The interior of any same-lang island skips JSON;
133	  only the boundary edges pay.
134	- The implementation cost is identical: same number of bits per
135	  box, just stored per-port/per-edge instead of per-box.
136	
137	### What each box records at graph load
138	
139	For each box:
140	
141	- `input_edge_native[i]` — one bit per input port. True iff the
142	  producer feeding that port has the same language as this box.
143	- `output_edge_native[j]` — one bit per outgoing connection
144	  (not per output port — a single output port can fan to
145	  multiple consumers, each of which is classified independently
146	  because the language pair may differ across consumers).
147	
148	Both arrays are computed during `graph_attach_runtime` and live
149	on the box record. No runtime re-evaluation.
150	
151	## Runtime dispatch
152	
153	The dispatch action calls `invoke_native` always (when the spec
154	provides one). The boundary work is per-edge in two places:
155	input-read and output-push.
156	
157	### Input-read: per-input-port
158	
159	```c
160	for (int i = 0; i < n_inputs; i++) {
161	    slot_pop(input_slots[i], wire_buf, wire_buf_cap);
162	    if (b->input_edge_native[i]) {
163	        // wire bytes are native form already; hand to invoke_native as-is
164	        native_inputs[i] = wire_buf;
165	    } else {
166	        // wire bytes are JSON; lift into native form
167	        spec->json_to_native(ctx, wire_buf, wire_size,
168	                             native_buf, native_cap, &native_size);
169	        native_inputs[i] = native_buf;
170	    }
171	}
172	spec->invoke_native(ctx, fn, native_inputs, n_inputs,
173	                    native_out, native_out_cap, &native_out_size);
174	```
175	
176	### Output-push: per-outgoing-connection
177	
178	```c
179	for (int j = 0; j < b->n_connections; j++) {
180	    if (b->output_edge_native[j]) {
181	        // consumer is same-lang; push native bytes directly
182	        slot_push(consumer_slot, native_out, native_out_size, tag);
183	    } else {
184	        // consumer is cross-lang; serialize to JSON once per such edge
185	        spec->native_to_json(ctx, native_out, native_out_size,
186	                             json_buf, json_buf_cap, &json_size);
187	        slot_push(consumer_slot, json_buf, json_size, tag);
188	    }
189	}
190	```
191	
192	If multiple cross-lang consumers share the producer's language,
193	the dispatch can cache the JSON encoding from the first such
194	edge and reuse it for the rest (optimization, not required for
195	correctness).
196	
197	If the spec's `invoke_native` is NULL (e.g., bash), the dispatch
198	falls back to the universal `invoke` (JSON-in, JSON-out) and
199	treats every edge as JSON. This is the slow path; it's always
200	correct.
201	
202	## Edge cases
203	
204	- **All-same-language graph**: every wire is fast-path; `invoke_json`
205	  never runs. Spec writers can stub it out.
206	- **All-different-language graph**: every wire is JSON;
207	  `invoke_native` handles bytes that came back from `json_to_native`.
208	- **One same-language wire, several cross-language wires from the same
209	  producer**: the producer writes JSON. Same-language consumer
210	  decodes via `json_to_native` (still cheaper than full encode +
211	  decode round-trip — only one direction of conversion is paid,
212	  not two).
213	- **Type changes mid-development**: a wire that was Lua→Lua becomes
214	  Lua→C as the user changes a downstream box's language. Compile
215	  flips the flag; the producer now writes JSON. No runtime
216	  surprises, no migration.
217	
218	## Implementation cost
219	
220	The two-invoke spec interface adds maybe 30 lines per language
221	spec. The compile-time wire classification is one pass over the
222	graph. The dispatch action's input-read branch is one `if` on the
223	wire flag. None of this is hot-path complex; the gain is one less
224	JSON encode/decode per same-language hop, which is the whole point.
225	
226	## Future: whole-program same-language compilation
227	
228	A bigger optimization beyond this issue: at compile time, merge
229	all same-language boxes into one translation unit. The C spec
230	becomes whole-program; the Lua spec produces one bytecode chunk
231	covering every Lua function. Box-boundary crossings become regular
232	function calls — no slot store, no dispatch layer, no spec
233	invocation. Only cross-language wires retain the slot machinery.
234	
235	This is a phase-3.5+ optimization and a much bigger lift —
236	requires the spec's `compile` callback to coordinate across all
237	boxes of its language, plus a runtime mode that bypasses the
238	dispatch layer for fully-merged regions of the graph. The
239	wire-level fast path in this issue is the immediate, smaller win.
240	
241	## Suggested implementation sequence
242	
243	1. **Spec interface** (issue 303): add `invoke_native`,
244	   `invoke_json`, `native_to_json`, `json_to_native` callbacks.
245	   Update the contract doc.
246	2. **Compile step** (issue 309): walk wires, set `fast_path` flag,
247	   set `output_format` per box.
248	3. **Dispatch action** (issue 304): branch on `fast_path` /
249	   `output_format` when reading inputs and when writing the
250	   output.
251	4. **Per-language specs** (306, 307, 308): implement both invoke
252	   paths. Bash's fast path is a thin alias around the JSON path.
253	   C's fast path is `memcpy`. Lua's fast path picks a native
254	   encoding (cjson for tables; raw bytes for primitives).
255	5. **Tests**: smoke tests confirming a 100k-iteration Lua→Lua
256	   pipeline with the fast path runs measurably faster than the
257	   same pipeline with the JSON path, and that mixed-language
258	   pipelines still work.
259	
260	## Relevant files
261	
262	- `issues/303-language-runtime-spec.md` — adds the new callbacks
263	- `issues/304-task-dispatch-layer.md` — branch on fast_path
264	- `issues/309-build-system.md` — wire-flag enrichment
265	- `issues/306-lua-language-spec.md`, `307-c-language-spec.md`,
266	  `308-bash-language-spec.md` — per-spec native implementations
267	- `issues/302-wire-value-slot-store.md` — slot still stores opaque
268	  bytes; the format negotiation is at the spec layer, not the
269	  store
270	
271	## Implementation note: ask the user about type-annotated cross-language encodings
272	
273	Before implementing this issue, the implementer must ask the user
274	to decide on **type-annotated wire encodings** for cross-language
275	hops — primitives go raw bytes (4-byte int as 4 bytes, not as the
276	JSON string `"42"`), strings go length-prefixed UTF-8, structured
277	types go JSON. That decision affects the spec interface (whether
278	`json_to_native` is enough or whether we need a `decode_wire(type,
279	bytes)` shape) and the wire-format byte layout. Don't proceed with
280	the implementation without explicit user direction on this; the
281	two designs aren't trivially interchangeable later.
282	
283	## Open questions
284	
285	- **Whether a spec must implement `invoke_json`**: leaning yes, so
286	  any spec can talk to any other spec. A spec that wants to
287	  shortcut can declare cross-language wires unsupported via
288	  compile-time error, but that's an opt-out, not the default.
289	- **How to share the bridge functions across consumers**: native_to_json
290	  on the producer side runs once per output (not per consumer), and
291	  the JSON bytes go in the slot. Each cross-language consumer pays
292	  json_to_native independently. Same-language consumers pay nothing.
293	  Symmetrically: if the producer chose native and there's one
294	  cross-language consumer, that consumer pays native_to_json then
295	  json_to_native_their_lang. Two conversions for one cross-language
296	  hop in this case. Probably fine; it's a corner case.
297	
298	## Implementation status
299	
300	What's in place (per the round summaries on 2026-05-12 to
301	2026-05-19):
302	
303	- `lang_spec_t` extended with `invoke_native`, `invoke_json`,
304	  `native_to_json`, `json_to_native` callback slots (issue 303
305	  spec interface).
306	- Each of the three specs declares all four callbacks. Today
307	  they all alias to the single existing `invoke` implementation;
308	  the actual specialization (real native_to_json / json_to_native
309	  bodies, real per-language binary native form) is not yet
310	  implemented.
311	- `graph_attach_runtime` computes a per-box `use_native_invoke`
312	  bit (true iff all input producers and all output consumers
313	  share this box's language). The dispatch picks
314	  `invoke_native > invoke_json > invoke` based on that single
315	  bit.
316	
317	What's still ahead:
318	
319	- **Replace per-box `use_native_invoke` with per-edge
320	  classification.** The current bit is the simpler approximation
321	  of the per-edge design above. It needs to be split into
322	  `b->input_edge_native[i]` and `b->output_edge_native[j]` arrays
323	  computed at graph load, with the dispatch's input-read and
324	  output-push paths each consulting the relevant array.
325	- **Per-language native binary serializations.** Each spec
326	  needs a real `native_to_json` and `json_to_native` (also
327	  required by issue 317 for data boxes), plus a real
328	  `invoke_native` that operates on whatever the spec considers
329	  its native handoff format.
330	- The type-annotated cross-language encoding decision flagged
331	  above is still pending user direction.
332	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/313-research-whole-program-same-language-merge.md`

### Tool result

```
1	# 313 — Research: whole-program same-language compilation
2	
3	## Status
4	research / low priority / blocked until rest of phase 3 is complete
5	
6	## Concept
7	
8	Instead of one box = one function call going through the dispatch
9	layer + slot store + spec invocation, **merge all same-language
10	boxes at compile time into a single translation unit** for that
11	language. Box-to-box transitions within a language region become
12	ordinary function calls — the optimizer can inline across them, the
13	language runtime stays warm, and the slot store and dispatch layer
14	don't run for those transitions at all. Only cross-language wires
15	retain the slot machinery.
16	
17	For C: every C box becomes a function in one big `.c` file, with
18	the build emitting one `.so`. Wire endpoints become direct calls;
19	the optimizer inlines based on its usual heuristics.
20	
21	For Lua: every Lua function gets pre-compiled into one bytecode
22	chunk loaded into one shared `lua_State` per worker. Wire
23	endpoints become direct `lua_pcall`s into that chunk — no JSON
24	encode/decode, no slot push/pop, no spec re-entry per call.
25	
26	For Bash: probably no benefit — Bash is already process-per-call;
27	fusing scripts doesn't help much. Skip.
28	
29	## Why this is research, not a feature
30	
31	Three open questions that need code-level investigation before
32	the design is concrete:
33	
34	1. **Loop and iterator boundaries**: a same-language region with an
35	   iterator inside it has dynamic re-spawning. Is the iterator
36	   merged in (a loop in the generated code) or does it remain a
37	   spec-mediated boundary? If the latter, the merge buys less.
38	2. **Compile time**: whole-program optimization is slow. Does the
39	   incremental-compile model (issue 309's mtime check) still apply,
40	   or do edits require a full rebuild of the merged region?
41	3. **Debug story**: a stack trace inside the merged region needs to
42	   point back to a specific box. Either source-map-style mapping or
43	   per-function preserved names — both have costs.
44	
45	The right way to answer these is to prototype: take a small all-Lua
46	graph (like the hello map) and see what a merged version looks
47	like, both for runtime cost and for build-system complexity. The
48	prototype should be able to fall back to the per-box path if the
49	merge fails for any reason.
50	
51	## Why blocked until rest of phase 3 completes
52	
53	The merge optimization piggybacks on the per-box model — same
54	specs, same compile callback, same dispatch layer for cross-language
55	wires. Building it before the per-box model is settled risks
56	designing against a target that's still moving. After phase 3 ships
57	and the per-box path is stable, the merge becomes a clean
58	"everything except spec invocation gets shorter for this region."
59	
60	It's also genuinely lower priority than getting the runtime working
61	at all. The same-language fast path (issue 312) captures the
62	~2x JSON-skip win without any of this complexity. The merge wins a
63	further factor only if profile data shows function-call overhead
64	matters at all after 312 ships.
65	
66	## Scope of the research output
67	
68	The deliverable is a written design (not code) with:
69	- Concrete answers to the three questions above.
70	- A worked example: the merged C generated from a 5-box C-only
71	  graph, showing what the build artifact looks like.
72	- Estimated runtime cost of the per-box path vs the merged path
73	  on that example.
74	- A recommendation: ship as a feature, ship as an opt-in flag, or
75	  shelve indefinitely.
76	
77	After the design is reviewed, a separate implementation issue
78	opens (or doesn't, depending on the recommendation).
79	
80	## Relevant files
81	
82	- `issues/312-same-language-wire-fast-path.md` — the immediate JSON-
83	  skip win this builds on
84	- `issues/307-c-language-spec.md` — `compile` callback that would
85	  grow whole-program-aware semantics
86	- `issues/306-lua-language-spec.md` — Lua-side bytecode pre-compile
87	- `issues/309-build-system.md` — how the merge integrates with the
88	  compile/package step
89	
90	## Open questions (for the research, not for now)
91	
92	- Cross-language wires inside the merged region: the merged region
93	  still has cross-language outputs that exit to other regions. The
94	  spec interface still applies at those boundaries, so the merge is
95	  region-internal optimization, not a global rearchitecture.
96	- C ABI for inlined wrappers: with whole-program optimization the
97	  wrapper-per-box generation might collapse to nothing. If the
98	  optimizer can prove a wire endpoint is always called from the
99	  merged region with known types, it can skip the wrapper.
100	- Iterators-with-side-effects: an iterator self-rescheduling its
101	  own task is normally dispatch-layer machinery. Inside a merged
102	  region, it could become a `while` loop. Whether that's safe
103	  depends on whether other tasks are blocked on the iterator's
104	  output slots — which they shouldn't be in a same-language region,
105	  but the analysis has to confirm.
106	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/314-c-json-parser.md`

### Tool result

```
1	# 314 — A small C JSON parser, written for SoraMech
2	
3	## Status
4	open
5	
6	## Current behavior
7	
8	The phase 3 design assumes a vendored JSON parser sits under
9	`libs/json/` — issues 305 (graph loader), 311 (JSONL run output),
10	and 309 (build system) all reference it. The original plan was to
11	drop in cJSON / jsmn / similar.
12	
13	After thinking about it: a vendored generic parser is one more
14	external surface to learn and to debug, with idioms that don't
15	match the rest of the runtime, and complications around large
16	inputs / streaming / arena allocation that we'd want to special-case
17	anyway. SoraMech needs JSON in two specific places:
18	
19	1. **Reading** map files (`meta.json`, `boxes/*.json`,
20	   `data/*.json`) at graph load. Inputs are kilobyte-scale, parsed
21	   once, validated immediately. The output is a tree that the
22	   graph loader walks once.
23	2. **Writing** run-log events into `tmp/last-run.jsonl` (issue
24	   311). Output is per-event, line-oriented, ASCII-safe; the writer
25	   thread serialises one event struct per call.
26	
27	Both are narrow enough that a small, opinionated parser tailored to
28	the project beats a general-purpose library on every dimension that
29	matters to us.
30	
31	## Intended behavior
32	
33	A C library under `libs/json/`, vendored as plain source, that
34	SoraMech owns end to end. Two halves:
35	
36	### Parser (DOM, arena-allocated)
37	
38	```c
39	typedef struct json_arena json_arena_t;
40	typedef struct json_node  json_node_t;
41	
42	typedef enum {
43	    JSON_NULL, JSON_BOOL, JSON_NUMBER,
44	    JSON_STRING, JSON_ARRAY, JSON_OBJECT
45	} json_kind_t;
46	
47	json_arena_t *json_arena_create(void);
48	void          json_arena_destroy(json_arena_t *a);
49	
50	/* Parse a NUL-terminated string into a tree owned by the arena.
51	 * Returns NULL on syntax error; *err_offset and *err_msg are filled
52	 * with the position and a short diagnostic. */
53	json_node_t *json_parse(json_arena_t *a,
54	                        const char *src,
55	                        int *err_offset, const char **err_msg);
56	
57	/* Parse a file. Convenience over json_parse + a file read. */
58	json_node_t *json_parse_file(json_arena_t *a,
59	                             const char *path,
60	                             int *err_line, const char **err_msg);
61	
62	/* Inspect. */
63	json_kind_t   json_kind        (const json_node_t *n);
64	int           json_is_null     (const json_node_t *n);
65	int           json_bool_value  (const json_node_t *n);
66	double        json_number_value(const json_node_t *n);
67	const char   *json_string_value(const json_node_t *n);   /* NUL-terminated */
68	int           json_array_size  (const json_node_t *n);
69	json_node_t  *json_array_at    (const json_node_t *n, int i);
70	int           json_object_size (const json_node_t *n);
71	const char   *json_object_key  (const json_node_t *n, int i);
72	json_node_t  *json_object_value(const json_node_t *n, int i);
73	json_node_t  *json_object_get  (const json_node_t *n, const char *key);
74	```
75	
76	All strings, arrays, and objects are arena-allocated. The whole
77	parse tree lives in one `json_arena_t`; destroying the arena
78	frees every node and every string in one go. No per-node `free`.
79	
80	`json_object_get` is the common path for "pull the `id` field off
81	this box" — linear scan over the object's keys; object sizes in
82	this project are < 20, so the simplicity wins over a hash map.
83	
84	### Writer (streaming, no DOM)
85	
86	```c
87	typedef struct {
88	    char  *buf;
89	    int    cap;
90	    int    used;
91	    int    err;       /* 0 ok; nonzero = truncation / OOM */
92	} json_writer_t;
93	
94	void json_writer_init    (json_writer_t *w, char *buf, int cap);
95	void json_writer_object  (json_writer_t *w);   /* '{' */
96	void json_writer_end     (json_writer_t *w);   /* '}' or ']' */
97	void json_writer_array   (json_writer_t *w);
98	void json_writer_key     (json_writer_t *w, const char *key);
99	void json_writer_string  (json_writer_t *w, const char *s);
100	void json_writer_int     (json_writer_t *w, long long v);
101	void json_writer_number  (json_writer_t *w, double v);
102	void json_writer_bool    (json_writer_t *w, int v);
103	void json_writer_null    (json_writer_t *w);
104	
105	/* Returns the number of bytes written (excludes NUL terminator),
106	 * or -1 if w->err is set. */
107	int  json_writer_finish  (json_writer_t *w);
108	```
109	
110	The writer maintains a small state stack (depth ≤ 16 is plenty
111	for our event records) so it knows when to emit commas between
112	elements. The writer never allocates — the caller hands in a fixed
113	buffer, the writer reports overflow via `w->err`. This matches the
114	JSONL event-writer design (issue 311): one record per fixed-size
115	struct, serialised into a per-write buffer of bounded size.
116	
117	### What the parser supports
118	
119	- JSON per RFC 8259: objects, arrays, strings, numbers (parsed as
120	  `double`), `true`, `false`, `null`.
121	- UTF-8 strict. Escape sequences `\"`, `\\`, `\/`, `\b`, `\f`,
122	  `\n`, `\r`, `\t`. `\uXXXX` for BMP code points; surrogate pairs
123	  handled. Decoded strings are NUL-terminated; embedded NULs in
124	  source strings are rejected (we don't need them and they make
125	  the strings interaction with the rest of the C code awkward).
126	- Strict syntax. Trailing commas, comments, unquoted keys: all
127	  errors with a precise message.
128	
129	### What it does NOT support
130	
131	- **Streaming / SAX**. Our inputs are small and parsed once. If
132	  some future use case wants streaming (a 10-MB LLM response that
133	  is itself JSON), revisit then.
134	- **No integer / no big-number type**. Numbers are `double`. The
135	  graph loader's integer-looking fields (`output_capacity`,
136	  `n_outputs`) are read via `(int)json_number_value(n)`. Documented
137	  in the loader, not in the parser.
138	- **No pretty-print**. The writer emits compact JSON only (one
139	  whitespace token between primitives, no spaces, no newlines).
140	  JSONL needs compact lines anyway.
141	- **No mutation API on parsed trees**. Trees are read-only; if a
142	  caller wants to construct JSON, it uses the writer.
143	
144	### Errors
145	
146	`json_parse` returns NULL on syntax error and fills `*err_offset`
147	with the byte offset where parsing failed, and `*err_msg` with a
148	short static-string description ("expected ':' after key",
149	"unterminated string", "invalid escape sequence"). The graph loader
150	turns this into a precise diagnostic ("maps/<name>/boxes/<id>.json:
151	line 12: unterminated string").
152	
153	OOM during parsing (arena out of memory) is a fatal error — the
154	arena uses a chunked-buffer allocator that calls `malloc` when it
155	grows; if that fails, the parser reports `*err_msg = "out of
156	memory"` and returns NULL. The hard-crash policy of the rest of
157	the runtime (issue 303) applies upstream.
158	
159	## Why now
160	
161	The graph loader (issue 305) is the immediate consumer. The JSONL
162	writer (issue 311) is the next one. Building the parser ourselves
163	keeps the dependency surface to zero and gives us idioms (arenas,
164	error reporting style, allocation policy) that match the slot
165	store and the dispatch layer.
166	
167	## Suggested implementation sequence
168	
169	1. **Arena allocator** (`libs/json/arena.c` + `.h`). Chunked
170	   bump allocator: each chunk is a `malloc`'d buffer; allocate by
171	   advancing an offset; chain chunks when one fills. Free chunks
172	   in `json_arena_destroy`.
173	2. **Parser** (`libs/json/json.c` + `.h`). Hand-rolled recursive
174	   descent. ~600 lines. One internal struct per kind, packed into
175	   a `json_node_t` union with a small tag.
176	3. **Writer** (in `libs/json/json.c` alongside). State stack for
177	   comma placement; bounded buffer; clean error reporting via
178	   `w->err`.
179	4. **Unit tests** (`tests/314-json-test.c`). Conformance cases:
180	   nesting, escapes, surrogate pairs, error positions, writer
181	   bracket-comma correctness, round-trip for a meta.json-shaped
182	   input.
183	5. **Wire into the Makefile.** `libs/json/*.c` is already in the
184	   wildcard from issue 309; no Makefile edit needed beyond
185	   confirming it picks up.
186	
187	## Relevant files
188	
189	- `libs/json/` — vendored sources (to be created here, not
190	  imported from upstream).
191	- `issues/305-c-graph-loader.md` — the immediate consumer.
192	- `issues/311-integration-tests-and-run-output.md` — the writer's
193	  consumer (JSONL events).
194	- `issues/309-build-system.md` — Makefile already discovers
195	  `libs/json/*.c` via wildcard.
196	- `docs/001-architecture.md` — references the JSON parser in the
197	  phase 3 runtime layout.
198	
199	## Open questions
200	
201	- **Comments.** RFC 8259 forbids them; cJSON optionally allows
202	  `//` and `/* */`. Box files are produced by the editor, not
203	  hand-written, so we don't need them. Stay strict.
204	- **`json_node_t` storage layout.** Tagged union (`json_kind_t` +
205	  union of payloads) is simplest. Could also pack the kind into
206	  low bits of a pointer for size — defer until profiling shows
207	  node-count pressure.
208	- **Object key lookup performance.** Linear scan is fine for
209	  objects with 2–20 keys (every object in a SoraMech map). If
210	  data files ever grow to thousands of keys, add an optional
211	  hash-table mode.
212	
213	## Implementation log
214	
215	### Parser milestone — 2026-05-12
216	
217	What shipped:
218	- `libs/json/json.h` — full public surface for both halves.
219	- `libs/json/json.c` — arena allocator (chunked bump, 16 KB
220	  default chunk), recursive-descent parser, every accessor. The
221	  parser handles every JSON kind, all escape sequences including
222	  BMP `\uXXXX` and surrogate pairs, with precise error positions.
223	  Numbers go through `strtod` after a small NUL-terminated scratch
224	  buffer (heap fallback for literals over 64 bytes — exceptionally
225	  rare). Arrays and objects build via a temporary linked list in
226	  the arena, finalized to fixed-size pointer arrays at close time;
227	  the link nodes are abandoned (recovered when the arena is
228	  destroyed).
229	- `tests/314-json-test.c` — 14 unit tests: every primitive,
230	  string escapes, U+00E9 single-escape, U+1F600 surrogate pair,
231	  empty / non-empty / nested arrays and objects, whitespace
232	  insensitivity, six distinct error-position cases, a realistic
233	  meta.json-shaped parse, file loading with line-numbered errors,
234	  arena growth across the 16 KB chunk boundary, and accessor
235	  safety on the wrong kind.
236	- Makefile per-test dependency line for `314-json-test`.
237	
238	Verified `make STRICT=1` (`-Werror -Wextra -Wpedantic`) builds
239	cleanly; 14/14 parser tests and the 12 slot-store tests all pass.
240	
241	What's deferred:
242	- **Stricter leading-zero rule.** RFC 8259 disallows `0\d+`; we
243	  currently accept `01` as `1`. None of our inputs produce that
244	  shape, so it's a tightening that lands when something needs it.
245	
246	### Writer half — 2026-05-12
247	
248	Filled in the writer bodies (object/array open+close+key+
249	primitives) on top of the depth-stack design declared in the
250	header. Eight new writer tests cover comma placement, escape
251	emission, empty and non-empty containers, nesting, overflow
252	detection, and a round-trip back through the parser. Done.
253	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/315-compiled-artifact-reference-counting.md`

### Tool result

```
1	# 315 — Reference-counted compiled-map artifacts
2	
3	## Status
4	open
5	
6	## Current behavior
7	
8	`scripts/soramech-compile.sh` from issue 309 is destructive at the
9	top:
10	
11	```bash
12	rm -rf "$COMPILED"
13	mkdir -p "$COMPILED" "$COMPILED/boxes" "$COMPILED/src" ...
14	```
15	
16	Whatever lived in `<map>/compiled/` before the rebuild is gone. If
17	another process — a long-running runner, a different shell, a
18	sibling tool that opened the artifact yesterday — had pinned to
19	that exact compiled layout (a particular `pool-runner` binary, a
20	particular `spec.so` ABI, a particular set of pre-compiled C
21	boxes), the rebuild yanks the rug. The holder either crashes,
22	silently retains stale `dlopen` handles, or starts reading new
23	files mid-flight.
24	
25	This is fine in single-user development. It stops being fine the
26	moment a SoraMech artifact is the kind of thing other programs
27	hold onto across time.
28	
29	## Intended behavior
30	
31	A compiled artifact carries its own reference count. Other
32	programs that depend on a particular build of a map are expected
33	to *acquire* a reference on startup and *release* it on shutdown.
34	When `soramech-compile` is asked to rebuild a map that has any
35	live references, it does NOT clobber the existing directory.
36	Instead it builds a sibling directory next door, leaving the
37	referenced one untouched until its holders are done with it.
38	
39	The reference count is **expected to be unreliable**. A crashing
40	holder doesn't get the chance to release; an SSH session that
41	drops doesn't either. The design accepts that staleness is the
42	common case, not the exception — every reference entry carries
43	enough metadata to be independently validated and (eventually)
44	reaped. The reference count is a hint and a courtesy, not a
45	guarantee.
46	
47	This is conceptually a copy-on-write filesystem for compiled
48	artifacts: writes (rebuilds) fork the universe when readers
49	exist; readers can keep reading the version they pinned to;
50	generations get garbage-collected when nothing observable still
51	points at them.
52	
53	## Concept
54	
55	A compiled directory contains a `.refs` file (hidden, dot-prefixed
56	so it doesn't clutter directory listings). It's append-only by
57	design — concurrent acquires never need to take a lock; concurrent
58	releases write a separate marker without rewriting the file.
59	
60	Each reference has three parts:
61	
62	1. **A date.** When the reference was acquired. Lets a janitor
63	   prune references older than some threshold even if their back
64	   pointer can't be validated.
65	2. **A back-pointer.** The identity of the holder — enough
66	   information that an outside observer can ask "is this holder
67	   still alive, and does it still want this reference?" without
68	   trusting the count alone. Concrete candidates:
69	   - process PID + start time (PID alone is not enough; PIDs
70	     recycle)
71	   - a path to a marker file the holder maintains (presence of
72	     the marker = the holder is still live and aware of the
73	     ref)
74	   - some combination (PID + a marker path)
75	3. **An opaque ref id.** A short string the holder can use later
76	   to identify *its* reference when it wants to release, so it
77	   doesn't accidentally release someone else's.
78	
79	A release writes an entry referencing the original acquire id.
80	Net live refs = acquires whose id isn't in the release set.
81	
82	## Reference file format
83	
84	A line-oriented log. Each line is one of two record types:
85	
86	```
87	acquire <id> <iso8601-date> <pid> <pid-start-time-ticks> <marker-path>
88	release <id> <iso8601-date>
89	```
90	
91	Append-only. Holders open with `O_APPEND` and write a single line
92	per operation. Atomic on POSIX as long as the line fits in
93	`PIPE_BUF` (4 KB on Linux) — well within budget for these short
94	records.
95	
96	Reading: tail through the file, build a set of acquire ids,
97	subtract release ids; remaining ids are nominally live. Then
98	validate each survivor's back pointer.
99	
100	A `.refs.lock` file is used only for compaction (rewriting the
101	log to drop matched acquire/release pairs) and for the
102	fork-to-sibling decision during compile. Steady-state reads /
103	writes don't lock.
104	
105	## Acquire / release API
106	
107	A small helper utility, probably a shell script for portability
108	since holders are written in many languages:
109	
110	```
111	scripts/soramech-ref.sh acquire <compiled-dir> [--id <id>] [--marker <path>]
112	scripts/soramech-ref.sh release <compiled-dir> --id <id>
113	scripts/soramech-ref.sh list    <compiled-dir>          # JSON, including staleness verdicts
114	scripts/soramech-ref.sh reap    <compiled-dir>          # compact + drop dead refs
115	```
116	
117	`acquire` prints the assigned id to stdout; the holder is
118	responsible for plumbing it to wherever it'll need to call
119	release. If no `--id` was supplied, the script generates one
120	(timestamp + random suffix).
121	
122	`list` walks the log and prints a JSON array of `{id, date,
123	holder, valid}` entries. Validation reads the back pointer's
124	liveness signal (PID exists & start-time matches, or marker file
125	exists).
126	
127	`reap` is destructive — it rewrites the log to remove entries
128	whose holders failed validation. Run periodically; never
129	automatically as part of compile (compile must not destroy state
130	its caller might care about).
131	
132	## Compile-time fork logic
133	
134	`soramech-compile.sh` consults `<map>/compiled/.refs` (and any
135	sibling generation directories' `.refs` files) before deciding
136	where to write:
137	
138	1. If `<map>/compiled/` doesn't exist → build there, fresh.
139	2. If it exists but has zero live references → build there, after
140	   `rm -rf`. Same destructive behavior as today.
141	3. If it exists with one or more live references → pick a sibling
142	   directory name (see below) and build there. The original
143	   `compiled/` stays intact.
144	
145	After a fork-build, the caller has a new directory; `manifest.json`
146	in the new directory records the fork relationship (`forked_from`
147	field pointing at the previous generation's path).
148	
149	### Sibling directory naming
150	
151	Two reasonable schemes; pick whichever the implementer prefers:
152	
153	- **Generation index:** `compiled.0`, `compiled.1`, `compiled.2`...
154	  Always pick `compiled.N` where N is one more than the highest
155	  existing generation. Easy to grep for; index order = build
156	  order.
157	- **Timestamp suffix:** `compiled.2026-05-19T2347` (ISO 8601
158	  compact). Order-preserving lexicographically; the name itself
159	  tells the operator when it was built.
160	
161	Whichever wins, the script should also leave a `compiled` symlink
162	pointing at the newest generation, so callers who want "the
163	latest" don't have to enumerate.
164	
165	## Stale reference handling
166	
167	A reference is stale if any of the following hold:
168	
169	- The PID no longer exists on the system.
170	- The PID exists but its start time (`/proc/<pid>/stat` field 22)
171	  doesn't match the recorded value — different process, recycled
172	  PID.
173	- The marker file path was supplied at acquire and is now
174	  missing.
175	
176	Stale refs are *tolerated*: compile-time fork still triggers if
177	ANY ref exists, stale or otherwise — the caller can run
178	`soramech-ref.sh reap` first if they want to consolidate. The
179	philosophy is: the artifact directory belongs to its readers
180	until proven otherwise, and the cost of an extra build dir is
181	trivial compared to the cost of a use-after-rebuild crash.
182	
183	## Gitignore considerations
184	
185	Every generation (`compiled/`, `compiled.0/`, `compiled.1/`,
186	`compiled.2026-05-19T2347/`, the `compiled` symlink) is a derived
187	artifact and should never be tracked. The existing
188	`.gitignore` line `tests/maps/*/compiled/` covers the canonical
189	name; add a sibling pattern to cover generations:
190	
191	```
192	tests/maps/*/compiled/
193	tests/maps/*/compiled.*/
194	```
195	
196	The `.refs` file itself lives *inside* the compiled directory,
197	so it inherits the same gitignore.
198	
199	## Suggested implementation steps
200	
201	1. Pick the back-pointer format. Recommend PID + start time +
202	   optional marker path. Document the choice with a comment at
203	   the top of the ref helper.
204	2. Write `scripts/soramech-ref.sh` with `acquire`, `release`,
205	   `list`, `reap`. Append-only writes for the first two, log
206	   rewrite under `.refs.lock` for the last.
207	3. Add a small `tests/315-refs-test.sh` that exercises:
208	   acquire → list (1 entry, valid) → release → list (0 valid) →
209	   acquire → kill the holder process → list (1 entry, stale) →
210	   reap → list (0).
211	4. Teach `soramech-compile.sh` to read `.refs` and pick the
212	   target directory. Implement the sibling-naming scheme.
213	   `compiled` becomes a symlink to the newest generation.
214	5. Add a `forked_from` field to `manifest.json` when forking.
215	6. Update `.gitignore` for the new generation pattern.
216	7. Integration check in `scripts/run-tests.sh`: compile the
217	   pipeline fixture twice — second time with a live `acquire` —
218	   assert a sibling generation was created and the original
219	   pinned generation still runs unchanged.
220	
221	## Open questions
222	
223	- Should the runner (`soramech-pool`) itself acquire a reference
224	  when it opens a compiled directory? On the one hand, yes —
225	  it's the canonical case the system protects against. On the
226	  other, soramech-pool runs are usually short-lived and the
227	  acquire/release overhead per run is unwelcome. Possible
228	  compromise: only acquire if pool runs longer than N seconds.
229	- How does a fork chain end? If `compiled.5` is built while
230	  `compiled.4` had live refs but `compiled.4`'s refs are now all
231	  stale (caller never released, then died), do we ever prune
232	  `compiled.4`? Probably yes, via `soramech-ref.sh reap-all
233	  <map>` which walks every generation and drops the ones whose
234	  refs all validate as dead.
235	- Is the `.refs` log itself something that should be versioned
236	  (`.refs.v1`)? If we ever change the line format, in-flight
237	  holders won't know how to read it. Versioning the filename
238	  avoids the problem.
239	- Does the editor (`maps/<x>/` user-facing) want the same
240	  mechanism? Probably yes eventually, but out of scope for this
241	  issue — start with `compiled/` artifacts only.
242	
243	## Relevant files
244	
245	- `scripts/soramech-compile.sh` — issue 309's compile pipeline,
246	  modified here to fork on live references
247	- `scripts/soramech-ref.sh` — new helper for acquire / release /
248	  list / reap
249	- `tests/315-refs-test.sh` — new
250	- `.gitignore` — extended pattern for generation directories
251	- `scripts/run-tests.sh` — new integration check
252	- `issues/309-build-system.md` — parent issue; references this
253	  one's fork behavior
254	
255	## Notes (philosophy)
256	
257	The unreliability is a feature, not a bug. A reliable reference
258	count would require a kernel-level mechanism (file locks held
259	for the lifetime of the holder), and that's a tower of
260	operational complexity for marginal value. By accepting that
261	holders sometimes lie and sometimes vanish, we get a system
262	that's robust to crashes, network drops, and SIGKILL — at the
263	cost of needing a "reap" pass to catch up with reality
264	occasionally. The back pointer is what makes that catch-up
265	possible: it's the bridge between what the log claims and what
266	the world actually shows.
267	
268	A reference is a wish, not a contract. The compile pipeline
269	honors wishes by default and only stops honoring them when
270	nothing in the world supports the wish anymore. That's how
271	respectful software treats its peers.
272	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/317-spec-json-bridge-for-data-boxes.md`

### Tool result

```
1	# 317 — Language spec JSON bridge for data boxes
2	
3	## Status
4	in progress — Lua bridges fidelity-preserving (stage 2 landed
5	2026-05-19); C and Bash still stubs
6	
7	## Current behavior
8	
9	A data box (`kind: data` in box JSON) emits a value the rest of
10	the graph routes downstream. Today the runner reads the box's
11	`ref` file as raw bytes and emits them — there's no concept of
12	"authoring this datum in a particular language using that
13	language's natural structures." If you want to declare a Lua
14	table as a data box's value, you either:
15	
16	- Hand-translate the table to JSON, save it as a `.json` file,
17	  and lose the Lua syntax niceties; or
18	- Make it a call box that returns the table, paying for a full
19	  spec invocation just to read a constant; or
20	- Hard-code the bytes inline in the box's `data` field, with no
21	  syntactic help from any host language.
22	
23	This is the wrong shape. **A box author writes idiomatic code in
24	the language of their choice.** That's the rule 307 establishes
25	for call boxes: a C-box author writes a plain C function, no
26	SoraMech-specific glue. The data-box equivalent should hold —
27	a Lua-data-box author should write a Lua file that returns
28	whatever they want returned (a table, a string, a number), and
29	SoraMech should know how to lift that into the graph as a JSON
30	wire value without the author writing a single SoraMech-specific
31	character.
32	
33	## Intended behavior
34	
35	Every language spec gains two callbacks, declared in
36	`lang_spec_t` (issue 303):
37	
38	```c
39	/* Serialize a native-language value into JSON text.
40	 *
41	 *   ctx       — per-worker handle from spec.init.
42	 *   src       — caller-meaningful pointer to the native value.
43	 *               Format is spec-specific (see "Native handle
44	 *               representation" below).
45	 *   src_size  — size hint, or 0 if the spec interprets `src`
46	 *               without a length.
47	 *   out_buf   — destination buffer for JSON text (caller-owned).
48	 *   out_cap   — capacity of out_buf.
49	 *   out_size  — set to the bytes written.
50	 *
51	 * Returns 0 on success, -1 on error. Writes an error message
52	 * via the spec's existing error-reporting plumbing. */
53	int (*native_to_json)(void *ctx,
54	                      const void *src, uint32_t src_size,
55	                      char *out_buf, uint32_t out_cap, uint32_t *out_size);
56	
57	/* Inverse: parse JSON text into a native-language value. The
58	 * native value is stored wherever the spec considers natural
59	 * (Lua: pushed to the worker's lua_State stack; C: written into
60	 * a typed struct supplied by `dst`; Bash: written as a textual
61	 * representation back into dst).
62	 *
63	 *   ctx       — per-worker handle.
64	 *   src       — JSON text.
65	 *   src_size  — JSON text length in bytes.
66	 *   dst       — caller-supplied target; semantics per spec.
67	 *   dst_cap   — capacity, where meaningful.
68	 *   dst_size  — set to the bytes / native units consumed.
69	 *
70	 * Returns 0 on success, -1 on error. */
71	int (*json_to_native)(void *ctx,
72	                      const char *src, uint32_t src_size,
73	                      void *dst, uint32_t dst_cap, uint32_t *dst_size);
74	```
75	
76	These are the same fields 312's same-language fast path
77	reserves; this issue is the one that requires every spec to
78	**actually implement them**, driven by the data-box use case.
79	(312 needs them too, but its driver is performance; 317 needs
80	them for correctness — a data box can't function without them.)
81	
82	### The rule, restated
83	
84	The author of a data box writes a `.lua` / `.c` / `.sh` /
85	`.<whatever>` file that returns or produces a value using
86	*their language's native idioms*. No SoraMech macros. No
87	SoraMech includes. No SoraMech-specific JSON shape constraints
88	beyond "what your spec's native_to_json can convert."
89	
90	If the author returns a Lua table, the Lua spec's
91	`native_to_json` converts it. If they return a C struct, the C
92	spec's `native_to_json` walks it according to the type declared
93	in the box. If they print JSON-ish text from a bash script, the
94	bash spec's `native_to_json` is a no-op identity (the script
95	already produced JSON).
96	
97	The contract belongs to the spec author, not the box author.
98	That's the inversion: **SoraMech bends to the language, not the
99	other way around.**
100	
101	## Native handle representation
102	
103	The shape of `src` / `dst` in the callbacks is per-spec. Each
104	spec documents what it accepts:
105	
106	- **Lua spec**: `src` is the index of a value already pushed
107	  onto the worker's `lua_State`. `src_size` is unused. The
108	  spec's `native_to_json` reads the value at that stack index
109	  and writes JSON. `json_to_native` pushes the parsed JSON value
110	  onto the stack, and `dst_size` reports the number of stack
111	  slots used (always 1 for SoraMech's flat-value model).
112	- **C spec**: `src` is a typed pointer + the box's declared
113	  output type drives the walk. `src_size` is the type's
114	  storage footprint. Mirror image for `json_to_native`. (This
115	  ties into 307's typed-wrapper-generation work — the same
116	  type system describes both the wrapper signature and the
117	  JSON shape.)
118	- **Bash spec**: `src` is text the script produced.
119	  `native_to_json` validates and reshapes to JSON if needed (a
120	  bash script that prints `42` becomes JSON `42`; a script that
121	  prints `{"a":1}` passes through unchanged). `json_to_native`
122	  is similarly a near-identity. The spec author is encouraged
123	  to require scripts produce well-formed JSON in the first
124	  place rather than write a bash-syntax parser.
125	
126	The dispatch layer doesn't know or care which representation a
127	spec uses. It only ever passes `src` and `src_size` to the
128	spec's callback. The spec is opaque to the runtime; that's the
129	whole point.
130	
131	## Data box semantics
132	
133	A data box's `data` field becomes optional. The box gains
134	optional `lang` and `ref` fields:
135	
136	```json
137	{
138	  "id": "config",
139	  "kind": "data",
140	  "lang": "lua",
141	  "ref": "config.lua"
142	}
143	```
144	
145	At graph load, the runner:
146	
147	1. Resolves the spec for `lang`.
148	2. Calls the spec's `init` (already done at worker startup).
149	3. At box-fire time, asks the spec to execute the `ref` file
150	   and produce a native value (this is essentially the same
151	   "load this file, run it, get its return value" path the
152	   spec already implements for call boxes — call boxes call a
153	   named function; data boxes take the file's return value).
154	4. Calls the spec's `native_to_json` to convert the native
155	   value to JSON text.
156	5. Pushes that JSON text down the wire as the box's output —
157	   same as today's data path, just with the value sourced
158	   from a language-native file.
159	
160	If the box has no `lang` and a literal `data` field, the legacy
161	inline-data path applies unchanged.
162	
163	## Why every spec must implement these
164	
165	The same JSON the data box emits is consumed by some downstream
166	box (call, comparator, file_write, whatever). That downstream
167	box, if it's in a different language, will eventually need
168	*its* spec's `json_to_native` to convert the JSON back. So the
169	two callbacks are paired — every spec that participates in the
170	graph needs both. A spec that implements `native_to_json` but
171	not `json_to_native` can only be data-box sources; a spec with
172	only `json_to_native` can only be call-box sinks. We require
173	both so any language can play any role.
174	
175	Specs that haven't been updated yet should set both callbacks
176	to a stub that returns -1 with a clear error message ("spec
177	`<name>`: native_to_json not implemented; this box's value
178	can't be routed cross-language"). The hard-fail makes the gap
179	loud rather than producing wrong output.
180	
181	## Suggested implementation steps
182	
183	1. Extend the `lang_spec_t` declaration in
184	   `langs/lang-spec.h` if not already done by 312's
185	   infrastructure work. (Per the round summary, the slot is
186	   reserved.)
187	2. Implement `native_to_json` and `json_to_native` in each
188	   spec:
189	   - **Lua spec**: walk the value at the stack index. Use the
190	     existing C JSON writer (`libs/json/json.c`). Handle Lua
191	     tables both as arrays (1-indexed contiguous integer keys)
192	     and as objects. Refuse functions, userdata, threads.
193	   - **C spec**: depends on 307's typed-wrapper machinery. For
194	     now, a stub that only supports scalar types (int, double,
195	     string) is enough to get data boxes working in C.
196	   - **Bash spec**: validate-and-pass-through. The script is
197	     expected to produce JSON. `native_to_json` runs the JSON
198	     parser to validate; `json_to_native` is identity.
199	3. Extend the data box runtime path in `src/012-dispatch.c`
200	   (or wherever data boxes dispatch from) to consult `lang` /
201	   `ref` when set and route through the spec callback chain.
202	4. Add a fixture: `tests/maps/cross-lang-data/` with a Lua
203	   data box, a C consumer, and a Bash consumer all reading the
204	   same value. The integration check asserts each consumer
205	   sees the right data.
206	5. Update each spec's `*.info.md` to document the native handle
207	   shape its callbacks expect.
208	
209	## Relevant files
210	
211	- `langs/lang-spec.h` — extend the contract
212	- `langs/lua/spec.c`, `langs/c/spec.c`, `langs/bash/spec.c` —
213	  implement the callbacks
214	- `langs/{lua,c,bash}/spec.info.md` — document native handle
215	  semantics per spec
216	- `src/012-dispatch.c` — data-box dispatch path
217	- `tests/maps/cross-lang-data/` — new fixture (see step 4)
218	- `issues/307-c-language-spec.md` — same author-writes-natural-
219	  code principle; this issue extends it from call boxes to data
220	  boxes
221	- `issues/312-same-language-wire-fast-path.md` — reuses the same
222	  spec callbacks for a different (performance) reason
223	
224	## Open questions
225	
226	- Bash's `json_to_native` is a near-identity because the
227	  scripts already deal in text. Is there value in having it
228	  validate and reformat for consistency? Probably yes; cheap
229	  insurance against malformed downstream input.
230	- Should the Lua spec auto-detect array-vs-object tables, or
231	  require the box author to annotate? Auto-detect (contiguous
232	  1-indexed integer keys → array; otherwise object) is what
233	  every other lua-to-json library does and is intuitive enough.
234	- C spec's `native_to_json` for arbitrary structs is a real
235	  rabbit hole. Tie scope to 307 — the same type declarations
236	  drive both the wrapper and the JSON walk. Until 307 lands,
237	  only scalar types are supported for C data boxes.
238	
239	## Implementation log
240	
241	### 2026-05-19 — Lua bridges landed
242	
243	Implemented the first concrete pair of bridges. Scope: only the
244	Lua spec; C and Bash still NULL their bridge slots and fall back
245	to the uniform-byte default.
246	
247	**Design decisions made during this round (kept for later
248	implementers of the C / Bash bridges):**
249	
250	- **Bridge signature has the worker handle as the first arg.**
251	  The previous declaration in `lang-spec.h` was missing it; without
252	  it Lua can't reach its `lua_State`. C and Bash will need it for
253	  the same reason once they grow real bridges.
254	
255	- **Stack-protocol convention for the native side.** The bridge
256	  reads / writes the producer's working store directly, not the
257	  `src` / `dst` buffers. For Lua: `native_to_json` consumes the
258	  value at the top of the `lua_State` (pops on success); `json_to_native`
259	  pushes the reconstructed value to the top. The `src` / `dst`
260	  buffers carry only the JSON side of the conversion. This keeps
261	  the wire format opaque to specs and avoids inventing a "native
262	  byte" serialization just so it can fit through a buffer.
263	
264	- **Wire primitive vocabulary.** Three JSON-with-extensions
265	  primitives are reserved for the harder types; this round did not
266	  emit any of them, but every future bridge must agree on the
267	  names: `$ref` (shared-memory pointer + length), `$function_pointer`
268	  (decomposed callable: address + signature), `$lang_opaque`
269	  (last-resort sealed envelope routed only between same-language
270	  endpoints — escape hatch, not a default).
271	
272	- **No handles, no back-routing.** The wire must carry data the
273	  destination spec can reconstruct on its own — never a token that
274	  says "operations on this value cross back to the source spec."
275	  See `memory/feedback_cross_language_pitfalls.md` for the longer
276	  argument; the principle constrains every bridge implementation.
277	
278	- **Type ambiguities are resolved by wiring, not by annotation.**
279	  When a function signature has hidden pairings (a `char *` with a
280	  separate length argument, a `union` with a discriminator, a
281	  callback with a context pointer), the destination spec exposes
282	  *each* parameter as its own input slot on the box. The user wires
283	  them according to the function's English docs. The system never
284	  prompts the user with vocabulary words it has no general way to
285	  ask about.
286	
287	**Files touched:**
288	
289	- `langs/lang-spec.h` — `lang_bridge_fn` now takes `void *handle`
290	  as its first parameter; the doc comment documents the stack
291	  protocol.
292	- `langs/lua/spec.c` — added `encode_value`, `decode_node`,
293	  `lua_native_to_json`, `lua_json_to_native`. Wired the last two
294	  into `soramech_lang_spec`.
295	- `langs/lua/Makefile` — folds `libs/json/json.c` straight into
296	  `spec.so` so the spec stays self-contained.
297	- `tests/317-lua-bridge-test.c` — new round-trip test (JSON →
298	  lua_State → JSON, compare).
299	- `Makefile` — added the test's dependency line.
300	
301	**Tests:** 5/5 in `317-lua-bridge-test`; full suite green.
302	
303	### 2026-05-19 — stage 2: Lua array fidelity
304	
305	Stage 1 lost `null` inside arrays — `[true,false,null]` round-tripped
306	as `[true,false]`. Stage 2 fixes it by applying the n-field
307	convention from the fidelity directive below.
308	
309	What changed in the code:
310	
311	- `decode_node` (JSON → Lua): the JSON_ARRAY case now creates a Lua
312	  table with a string key `n` set to the array's length, then
313	  fills integer keys 1..n. JSON `null` elements use `lua_rawseti`
314	  with nil — which deletes the key, leaving an absent slot. The
315	  table now carries enough information to reconstruct the array
316	  losslessly.
317	
318	- `encode_value` (Lua → JSON): table emission reads `n` first. If
319	  the field is a non-negative integer, the table is treated as a
320	  declared-length array — the encoder walks 1..n and emits null
321	  for any missing slot via the existing nil → null path. Tables
322	  without `n` fall through to the existing array-vs-object
323	  heuristic for idiomatic Lua values.
324	
325	What the tests prove now:
326	
327	- `[]`, `[null]`, `[null,null]`, `[null,1,2]`, `[1,null,3]`,
328	  `[1,2,null]`, `[true,false,null]` all round-trip exactly.
329	
330	What this costs:
331	
332	- Lua tables that have crossed the bridge carry a `n` field
333	  visible to `pairs()`. The fidelity directive accepts this:
334	  "We can intrude on idiomatic Lua because we're fundamentally
335	  translating from Lua to JSON." Box authors who write Lua
336	  tables by hand and want them treated as JSON arrays can also
337	  set `n` themselves; otherwise the existing heuristic still
338	  catches `{1, 2, 3}`-shaped tables.
339	
340	## Fidelity directive (added 2026-05-19, supersedes earlier "lossy is OK" thinking)
341	
342	The wire format (JSON) is canonical. **A value leaving a box's
343	output slot and entering the wire must not lose fidelity.** The
344	bridge intrudes on the language as needed to achieve that. The
345	language's idioms are upheld on the in-language side (inside the
346	box's source file); on the wire side the bridge is allowed —
347	required — to use whatever encoding preserves the shape.
348	
349	This reverses the earlier instinct that lossy round-trips were
350	"the box author's problem." They're the bridge's problem. The
351	box author writes idiomatic Lua / C / bash; the bridge converts
352	without dropping anything.
353	
354	### Concrete rules per language
355	
356	- **Lua → JSON** (and JSON → Lua → JSON round-trip): preserve
357	  array shape including embedded nulls. The decoder pushes
358	  arrays as `{n = N, [1] = ..., [2] = ..., ...}` where `n` is the
359	  intended length and missing integer keys are real nil slots.
360	  The encoder reads `n` if present and emits a full-length JSON
361	  array with `null` at every missing slot. Idiomatic Lua tables
362	  without `n` continue to encode via the existing
363	  array-vs-object heuristic — `n` is only required when the
364	  caller cares about distinguishing "trailing nil" from "shorter
365	  array." Same scheme handles intermixed nil-in-middle too.
366	
367	- **Lua → JSON** (type annotations): the bridge may emit a
368	  type-tagged JSON shape when the destination language needs
369	  more than JSON's native types convey (e.g. int vs float when
370	  the Lua value is whole-numbered but the destination expects a
371	  float, or vice versa). The tag lives in the JSON itself, not
372	  in a sidecar protocol. Suggested shape: a `$type` key inside a
373	  small wrapper object, only emitted when the encoder can tell
374	  the unwrapped value would be ambiguous downstream.
375	
376	- **C → JSON**: C functions have declared types — emit those
377	  types as part of the JSON, so any downstream language can
378	  honour them without guessing. Lua receivers may use the type
379	  to coerce (e.g. force `2` instead of `2.0`); receivers that
380	  don't care can ignore the type tag.
381	
382	- **JSON → C**: when the wire value doesn't fit the destination
383	  C type (a float arriving at an int slot, a string at a number
384	  slot), the C bridge **truncates / coerces and records a
385	  warning** at map-compile time. The warning surfaces in the
386	  compile output, not at run time. Silent dropping is forbidden;
387	  hard-failing the compile is too aggressive — the user may
388	  know the truncation is fine.
389	
390	- **JSON → Lua / JSON → bash**: same coercion rule, with
391	  language-appropriate fallbacks. The receiver records the
392	  warning.
393	
394	### Why this lives in the bridge, not the box author
395	
396	A box author writes a function in their language. They don't
397	write a SoraMech-aware serializer. The bridge — which is the
398	spec author's responsibility, not the box author's — owns every
399	detail of how their language's values reach JSON and how JSON
400	reaches their language's values. The box author writes
401	idiomatic code; the bridge does whatever is necessary on the
402	JSON side, including unwrapping `n`-fielded tables back into
403	idiomatic Lua at decode time so subsequent in-language calls
404	see clean tables.
405	
406	This is the same principle the rest of 317 already states: the
407	spec bends to the language at the box surface, and bends the
408	wire shape to the language at the bridge. The language never
409	bends to the wire.
410	
411	**Next implementation slices:**
412	
413	1. ~~**Lua bridges, stage 2 — fidelity preservation.**~~ Done
414	   2026-05-19. The n-field convention is implemented; every JSON
415	   array shape round-trips faithfully. See the stage-2 entry in
416	   the implementation log above.
417	2. Bash bridges — near-identity (scripts are already producing
418	   text). `native_to_json` validates the text is well-formed JSON;
419	   `json_to_native` is identity. No `lua_State`-style scratch
420	   needed, so the bridge can ignore `handle`. Type fidelity is
421	   trivial because bash already deals only in text.
422	3. C bridges — depends on 307's typed wrapper work. Until 307
423	   lands, scalar types (int / double / string) is enough to
424	   unblock data-box semantics. The coerce-and-warn rule from
425	   the fidelity directive governs how mismatched types behave.
426	4. Wire the bridges into the dispatch layer (issue 312's
427	   wire-classification path) so cross-language edges actually
428	   round-trip through them.
429	5. Add a map-compile warning channel that the C / Lua / bash
430	   bridges feed into when they truncate or coerce. Surface
431	   the warnings in the compile output — silent loss is
432	   forbidden by the fidelity directive.
433	
```

## Assistant

_Tools:_

- `TaskUpdate: ?`

### Tool result

```
Updated task #1 status
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
1	# Phase 3 Progress
2	
3	Goal: replace the Lua synchronous runner with a C thread pool runner
4	that executes box graphs in parallel.
5	
6	The phase is fully designed; implementation is now underway.
7	Issues 301–313 are the architectural specification. As each issue
8	moves into implementation, its row is marked "in progress"; on
9	completion it's moved to `issues/completed/`.
10	
11	Implementation began **2026-05-12** with the 309 build scaffolding.
12	
13	## Issues
14	
15	| ID  | Title                                                       | Status   |
16	|-----|-------------------------------------------------------------|----------|
17	| 301 | thread pool lifecycle and per-worker initialization         | in progress · pool + per-worker init/teardown hooks done |
18	| 302 | per-task slot store with wire-held references               | in progress · core + large-value heap (variable-size payloads) |
19	| 303 | language runtime spec (pluggable per-language invocation)   | in progress · registry + per-worker init + pool hook + lang filtering |
20	| 304 | task dispatch layer (C, replaces synchronous executor)      | in progress · plain + comparator + iterator + randomizer + weighted routing + cell-tagged iterator ordering |
21	| 305 | C graph loader (replaces `003-loader.lua`)                  | in progress · attach + lang enum + 312 fast-path classification |
22	| 306 | Lua language spec implementation                            | in progress · init + invoke + teardown + per-worker module cache done |
23	| 307 | C language spec implementation                              | in progress · compile + invoke + lazy .c→.so done, typed wrapper deferred |
24	| 308 | Bash language spec implementation                           | in progress · persistent socketpair + line-protocol invoke + dladdr-based server lookup |
25	| 309 | build system & Makefile orchestration                       | in progress · scaffolding + spec discovery + soramech-compile portable artifact |
26	| 310 | priority queue: wired in, no-op behaviorally                | in progress · API + ordering done |
27	| 311 | integration tests & run output (`last-run.jsonl`)           | in progress · writer + queue + thread + LOG_VALUES + LOG_SLOTS + run-tests.sh + compile-pipeline check |
28	| 312 | same-language wire fast path (skip JSON for Lua→Lua etc.)   | in progress · lang_spec_t extended + classification + dispatch resolution |
29	| 313 | research: whole-program same-language merge                 | research / blocked |
30	| 314 | small C JSON parser written for this project                | in progress · parser + writer + strict leading-zero |
31	| 315 | reference-counted compiled-map artifacts                    | open · design only |
32	| 316 | simple test-runner script                                   | folded into 232 |
33	| 317 | language spec JSON bridge for data boxes                    | open · design only |
34	
35	## Phase goal checklist
36	
37	- [x] SoraMech-owned task pool builds clean (3d-rts as design reference)
38	- [x] Slot store unit-tested (single-value + ring buffer)
39	- [x] C graph loader passes the project's existing maps
40	- [x] Lua spec runs `tests/maps/hello` end-to-end
41	- [x] C spec compiles and runs a fixed-signature box
42	- [x] Bash spec runs an out-of-process box via persistent
43	      socketpair + line-protocol
44	- [x] Pool runner runs a multi-language map with concurrent workers
45	- [x] Iterator box routes via dispatch layer (with multi-spawn
46	      re-fire via N-cell pop slots and auto-re-spawn)
47	- [x] Comparator box routes via dispatch layer (via spec output)
48	- [x] Variable-size outputs work via the large-value heap
49	- [x] `last-run.jsonl` written for every run
50	- [ ] All 11 integration test maps pass
51	- [ ] Phase 3 demo map runs and produces expected output
52	- [ ] Phase 2 synchronous runner retired (`src/004-executor.lua`,
53	      `src/007-runner-main.lua`, `src/003-loader.lua`, `drivers/*.sh`)
54	
55	## Implementation order suggestion
56	
57	Designed to surface concrete artifacts early so subsequent issues can
58	build on them.
59	
60	1. **309 (build system)** — top-level Makefile, vendored deps, empty
61	   stubs for runner and specs. Verify everything compiles.
62	2. **302 (slot store)** — pure C, unit-testable, no pool dependency.
63	3. **305 (graph loader)** — pure C, parses existing phase 2 maps.
64	4. **301 (pool lifecycle)** — wire pool startup, per-worker context,
65	   active-task counter, quiescence.
66	5. **303 (lang spec interface)** — header file + stub spec to validate
67	   the loading mechanism.
68	6. **306 (Lua spec)** — easiest reference. Smoke test: `maps/hello`
69	   end-to-end.
70	7. **304 (dispatch layer)** — connects everything. First fully-running
71	   box.
72	8. **310 (priority queue)** — small change, can land any time.
73	9. **307 (C spec)** — adds compile callback and wrapper generation.
74	10. **308 (Bash spec)** — adds out-of-process execution.
75	11. **311 (tests + run output)** — formalizes what "phase 3 works"
76	    means; runs continuously from this point.
77	
78	Issue 222 (compile button placeholder) lands in phase 2 but its
79	backend is wired in here once 307 and the per-spec compile orchestration
80	exist.
81	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

### Tool result

```
1	# Phase 2 Progress
2	
3	Goal: an editor you can actually use to build maps, plus the graph
4	model the phase 3 runtime will consume.
5	
6	## Issues
7	
8	| ID   | Title                                          | Status   |
9	|------|------------------------------------------------|----------|
10	| 201  | fix box rendering closure bug                  | complete |
11	| 202  | fit-to-view on map load                        | complete |
12	| 203  | map picker panel                               | complete |
13	| 204  | visible interaction hints                      | complete |
14	| 206  | entry box designation                          | won't implement |
15	| 207  | source file browser and port auto-population   | complete |
16	| 208  | port literal values                            | complete |
17	| 209  | ollama query library                           | complete |
18	| 210  | comparator wire branching                      | complete |
19	| 211  | file browser library directories               | complete |
20	| 211a | unify src/ into same directory pipeline        | complete |
21	| 212  | editor interaction modes                       | complete |
22	| 213  | queued inputs and task model                   | won't implement |
23	| 219  | map compiler                                   | won't implement (folded into 309) |
24	| 214  | tmp symlink recreation on reboot               | complete |
25	| 215  | view source button                             | complete |
26	| 223  | syntax highlighting via user-written lexers    | complete |
27	| 224  | editable port names (canvas) — values stay in inspector | complete |
28	| 225  | dir picker shows files and dir/file icons      | complete |
29	| 226  | hide output controls for no-return functions   | complete |
30	| 227  | hide view button when box has no ref           | complete |
31	| 228  | self-loop wires routed around the box body     | complete |
32	| 229  | `read` / `write` box kinds (language-agnostic IO) | complete (phase 3 + editor; phase 2 mirror skipped per teardown) |
33	| 230  | required inputs and the optional flag           | complete (compile-time; runtime arg-list shortening deferred) |
34	| 231  | move signature parsers into langs/              | complete |
35	| 232  | test coverage map (parser tests landed; tracks gaps) | ongoing |
36	| 233  | unified routing schema (comparator+iterator)    | open     |
37	| 234  | middle-click duplicates the selected box        | complete |
38	| 235  | literal value replaces port name on the canvas  | complete |
39	| 236  | box header = filename, output label = `fn()`    | complete |
40	| 237  | string-manipulation library expansion           | complete |
41	| 238  | editor HTTP server config file                  | complete |
42	| 216  | read-file box                                  | complete |
43	| 217  | variadic ports and the language-spec variadic shape | A+B mechanics in; C model rewritten — spec.js drives the gate |
44	| 218  | enforce single-output driver contract          | complete |
45	| 220  | root run script and entry point cleanup        | complete |
46	| 221  | iterator box (editor surface only)             | complete |
47	| 222  | compile button and assets directory            | complete |
48	
49	## Phase goal checklist
50	
51	- [x] Boxes render on map load
52	- [x] Clicking a box opens the inspector
53	- [x] Ref/fn set by browsing src/ files; ports auto-derived
54	- [x] Map loads centered on screen (fit-to-view)
55	- [x] Map can be switched from a list, not a text prompt
56	- [x] Empty canvas shows interaction hints
57	- [—] Entry box marker (won't implement — phase 3 auto-detects, see 305)
58	- [ ] Comparator routing replaces the old branch box (issue 210)
59	- [x] Iterator box with auto-grow output slots (editor; runtime in phase 3 / issue 304)
60	- [x] Compile button (issue 222) — placeholder UI for the phase 3 build
61	
62	## Notes on graph model changes during phase 2
63	
64	These are not new features so much as model corrections that shape
65	what phase 3 will consume:
66	
67	- **Single-output rule** (218, complete): every box has exactly one
68	  output wire. The driver contract emits one JSON value, not an
69	  array.
70	- **Comparator-based branching** (210, complete): the old `branch`
71	  box kind with named ports is removed. A box may carry a
72	  `comparand`; the dispatch layer fires the connection whose
73	  `from_branch` matches `lt` / `eq` / `gt`.
74	- **Iterator routing** (221): a box with `iterator_outputs` is a
75	  pure routing primitive; the dispatch layer rotates through the
76	  declared output names. Iterators have no `ref` / `fn`.
77	- **Connection schema**: `from_branch` field replaces the old
78	  `from_output` / `from_port` dual-field design.
79	
```

## Assistant

Done reading. Before I dive into implementation, here's the lay of the land — because what's "deferred" versus "implemented" varies dramatically across the 21 open issues, and you have a real choice about scope.

## Three groups

**Group A — done in code; just move the file** (7 issues)
- **231** — status header literally says `complete (2026-05-19)`
- **301** pool lifecycle, **303** spec registry, **308** bash subprocess, **309** build/compile pipeline, **310** priority queue, **314** JSON parser+writer
- Each has a real implementation log ending in a green state. No outstanding code work.

**Group B — meaningful outstanding work, but bounded** (10 issues)
- **217** — inspector gate on `LANGUAGE_SPEC.variadic_shape` not wired (mechanical)
- **232** — `scripts/run-unit-tests.sh` planned but not written
- **233** — dispatch implements the unified schema, but `src/001-schema.lua` + the inspector mode-dropdown were never updated; phase-2 maps still carry legacy `comparand`/`iterator_outputs`
- **239** — nodule rendering not started
- **304** — randomizer/weighted/distributor routing kinds fall back to plain (the issue ships them as future kinds, but the schema and dispatch table reserve them)
- **305** — phase 5 (slot size class enumeration) skipped; loader uses a default cell capacity; phase 7 partial (every worker inits every spec rather than only ones in use)
- **306** — rich return marshalling (tables→JSON, multi-return) not built; current invoke uses `lua_tolstring`
- **311** — mutex placeholder where a lock-free MPSC ring was specified; opt-in slot events / value dumps not implemented
- **312** — per-box `use_native_invoke` bit lives where per-edge `input_edge_native[i]` / `output_edge_native[j]` arrays were specified; no real per-language native serializations; **the type-annotated cross-language encoding decision is explicitly flagged as "must ask user before implementing"**
- **317** — Lua bridges shipped with fidelity preservation; C bridges blocked on 307; Bash bridges + dispatch wiring open

**Group C — design-committed major work, not yet built** (3 issues)
- **302** — the bottom of the file is a *retraction* of the earlier "monotonic growth is acceptable" deferral. The new committed design is a unified allocator with refcounting, eager neighbor-merge, a deeper sweep on two triggers, and an address-ordered chunk list. None of this is implemented. The current store still mallocs per-slot and the large-value heap still grows monotonically.
- **307** — the issue specifies "a box author writes a C function as if SoraMech doesn't exist" with per-box compile-time wrapper generation driven by typed input/return declarations. The current implementation is a uniform fixed `(inputs, sizes, n, out, cap, out_size)` signature — i.e., the *opposite* of the design.
- **315** — refcounted compiled-map artifacts (.refs file, fork-on-live-refs, reap). Not started. No driver beyond philosophy — no one is currently pinning artifacts.

Plus **313** which is research-only, explicitly blocked-by-everything-else.

## The scope decision is yours

These three groups are very differently shaped work. A few honest framings:

1. **Group A + the mechanical bits of B** (217 inspector gate, 232 runner script, 239 rendering) is one focused session — maybe four hours of careful implementation. Closes ~10 issues.

2. **All of A + B** is a multi-day endeavor. The hard bits are 233 (schema migration touches every map file), 304 (designing real randomizer/weighted/distributor — though you flagged these for follow-on issues), 306 (table marshalling has real surface), 311 (MPSC ring is non-trivial), 312 (blocked on the user-direction question on type annotations).

3. **All of A + B + C** is a project, not a session. Especially 302 (refcounting threaded through every place that holds a value pointer — "many places, each one-line, but the new allocator depends on all of them being correct").

You also mentioned **expecting to prune**. Reading these cold, the candidates for pruning are:
- **315** — design without a driver. No current holder is pinning artifacts. The whole issue can be deferred to a follow-on or written off.
- **313** — research that explicitly waits.
- **304's three deferred routing kinds** (randomizer/weighted/distributor) — your own issue says they ship as follow-on issues, not in 304. Could split now.
- **312's per-edge classification** vs the current per-box bit — possibly premature optimization. The bit works; the per-edge split may be design-perfectionism. Worth asking whether the current shape can just be re-described as the design.

A few specific things I need direction on before implementing anything:

- **312's type-annotated encoding question** — the issue explicitly says "do not proceed without explicit user direction." Type-annotated wire (4-byte int as 4 bytes, length-prefixed UTF-8 strings, JSON for structures), or stay JSON-everything-with-a-`$type`-wrapper? The Lua bridge has already gone with JSON-everything; if that's the chosen direction we just need to confirm and reword 312.
- **302's allocator rewrite** — is this a real commitment we want to ship today, or is the current code+log the actual state and the "retraction" entry should itself be retracted (or split into its own issue 318)?
- **307's wrapper generation** — same question. The implemented uniform-signature path works for the current fixtures. The "any C function" goal is materially bigger. Is the issue lying about what was built?

Which way do you want to point me? I'd suggest answering 302, 307, and 312's pending question first because they reshape the work for everything downstream.

## User

Let's start with cleaning up A. That way, we have a clearer picture of what to do moving forward. I anticipate we can finish in an hour or two.

## Assistant

Sounds good. Setting up sub-tasks for Group A and starting with verification.

## Assistant

_Tools:_

- `TaskUpdate: 231 is the only phase-2 Group A issue. Verify parsers landed in langs/*/parser.js + tests; update status; move to completed/; update phase-2-progress.md.`

### Tool result

```
Updated task #3 subject, description, status
```

## Assistant

_Tools:_

- `TaskUpdate: For each: verify implementation matches log, update status to complete, move to completed/, update phase-3-progress.md. Batch git commit at end.`

### Tool result

```
Updated task #4 subject, description, status
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/langs/lua/parser.js /mnt/mtwo/programs/sora/soramech/langs/bash/parser.js /mnt/mt...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/langs/bash/parser.js
/mnt/mtwo/programs/sora/soramech/langs/lua/parser.js
/mnt/mtwo/programs/sora/soramech/tests/232-lua-parser-test.mjs
/mnt/mtwo/programs/sora/soramech/tests/233-bash-parser-test.mjs
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/libs/task-pool/pool.c /mnt/mtwo/programs/sora/soramech/libs/task-pool/pool.h /mnt...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/libs/task-pool/pool.c
/mnt/mtwo/programs/sora/soramech/libs/task-pool/pool.h
/mnt/mtwo/programs/sora/soramech/src/008-pool-runner.c
/mnt/mtwo/programs/sora/soramech/tests/301-pool-test.c
/mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/src/011-spec-registry.c /mnt/mtwo/programs/sora/soramech/src/011-spec-registry.h ...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/langs/bash/spec.c
/mnt/mtwo/programs/sora/soramech/langs/c/spec.c
/mnt/mtwo/programs/sora/soramech/langs/lang-spec.h
/mnt/mtwo/programs/sora/soramech/langs/lua/spec.c
/mnt/mtwo/programs/sora/soramech/src/011-spec-registry.c
/mnt/mtwo/programs/sora/soramech/src/011-spec-registry.h
/mnt/mtwo/programs/sora/soramech/tests/011-spec-registry-test.c
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/langs/bash/spec.c /mnt/mtwo/programs/sora/soramech/langs/bash/bash-server.sh /mnt...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/langs/bash/bash-server.sh
/mnt/mtwo/programs/sora/soramech/langs/bash/spec.c
/mnt/mtwo/programs/sora/soramech/tests/308-bash-spec-test.c
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/Makefile /mnt/mtwo/programs/sora/soramech/scripts/soramech-compile.sh /mnt/mtwo/p...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/Makefile
/mnt/mtwo/programs/sora/soramech/langs/bash/Makefile
/mnt/mtwo/programs/sora/soramech/langs/c/Makefile
/mnt/mtwo/programs/sora/soramech/langs/lua/Makefile
/mnt/mtwo/programs/sora/soramech/scripts/soramech-compile.sh
```

## Assistant

_Tools:_

- `Bash: grep -n "priority" /mnt/mtwo/programs/sora/soramech/libs/task-pool/pool.h | head -20`

### Tool result

```
115: * `priority` is a signed-int weight; higher priorities are dequeued
116: * before lower ones. Ties between equal-priority tasks resolve to
118: * queue behaviorally a plain FIFO — the priority machinery is here
121:void    pool_spawn(pool_t *p, pool_action_t fn, void *arg, int priority);
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/libs/json/json.c /mnt/mtwo/programs/sora/soramech/libs/json/json.h /mnt/mtwo/prog...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/libs/json/json.c
/mnt/mtwo/programs/sora/soramech/libs/json/json.h
/mnt/mtwo/programs/sora/soramech/tests/314-json-test.c
```

## Assistant

All seven verified on disk. Now updating Status headers and moving them in batch. 231 already says complete, the other six say "open" — flipping them.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/301-pool-lifecycle-and-worker-init.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/303-language-runtime-spec.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/308-bash-language-spec.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/309-build-system.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/310-priority-queue.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/314-c-json-parser.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/301-pool-lifecycle-and-worker-init.md has been updated successfully.
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/303-language-runtime-spec.md has been updated successfully.
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/308-bash-language-spec.md has been updated successfully.
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/309-build-system.md has been updated successfully.
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/310-priority-queue.md has been updated successfully.
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/314-c-json-parser.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: git mv issues/231-move-signature-parsers-into-langs.md issues/completed/ && git mv issues/301-pool-lifecycle-and-work...`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/issues/ | grep -v completed`

### Tool result

```
217-variadic-ports-and-language-spec-shape.md
232-test-coverage-map.md
233-unified-routing-schema.md
239-literal-port-shrinks-to-nodule-with-name-on-left.md
302-wire-value-slot-store.md
304-task-dispatch-layer.md
305-c-graph-loader.md
306-lua-language-spec.md
307-c-language-spec.md
311-integration-tests-and-run-output.md
312-same-language-wire-fast-path.md
313-research-whole-program-same-language-merge.md
315-compiled-artifact-reference-counting.md
317-spec-json-bridge-for-data-boxes.md
phase-1-progress.md
phase-2-progress.md
phase-3-progress.md
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

Now updating phase-3-progress.md to mark the six phase-3 issues complete.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Bash: git status`

### Tool result

```
On branch master
Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
	renamed:    issues/231-move-signature-parsers-into-langs.md -> issues/completed/231-move-signature-parsers-into-langs.md
	renamed:    issues/301-pool-lifecycle-and-worker-init.md -> issues/completed/301-pool-lifecycle-and-worker-init.md
	renamed:    issues/303-language-runtime-spec.md -> issues/completed/303-language-runtime-spec.md
	renamed:    issues/308-bash-language-spec.md -> issues/completed/308-bash-language-spec.md
	renamed:    issues/309-build-system.md -> issues/completed/309-build-system.md
	renamed:    issues/310-priority-queue.md -> issues/completed/310-priority-queue.md
	renamed:    issues/314-c-json-parser.md -> issues/completed/314-c-json-parser.md

Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   issues/completed/301-pool-lifecycle-and-worker-init.md
	modified:   issues/completed/303-language-runtime-spec.md
	modified:   issues/completed/308-bash-language-spec.md
	modified:   issues/completed/309-build-system.md
	modified:   issues/completed/310-priority-queue.md
	modified:   issues/completed/314-c-json-parser.md
	modified:   issues/phase-3-progress.md
```

## Assistant

Staging the status updates and running tests before commit.

## Assistant

_Tools:_

- `Bash: git add issues/completed/301-pool-lifecycle-and-worker-init.md issues/completed/303-language-runtime-spec.md issues/c...`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -40`

### Tool result

```
  parse_file                                   ok
  parse_file_error_line                        ok
  arena_grows                                  ok
  kind_safety                                  ok
  writer_primitives                            ok
  writer_string_escapes                        ok
  writer_object                                ok
  writer_array                                 ok
  writer_nested                                ok
  writer_empty_containers                      ok
  writer_overflow                              ok
  writer_round_trip                            ok

  22 passed, 0 failed

317-lua-bridge-test:
  scalars                                      ok
  arrays                                       ok
  objects                                      ok
  truncation                                   lua spec: native_to_json: output buffer (4 bytes) too small
ok
  parse_error                                  lua spec: json_to_native: parse error at byte 1: expected string
ok

  5 passed, 0 failed

integration tests (fixture maps):
  calc                                         ok
  hello                                        ok
  comparator                                   ok
  iter-route                                   ok
  pipeline                                     ok
  read-literal                                 ok
  pipeline write output                        ok
  compile pipeline (portable run)              ok
  232-lua-parser-test.mjs                      ok
  233-bash-parser-test.mjs                     ok
  234-language-spec-js-test.mjs                ok

  11 passed, 0 failed
```

## Assistant

All 38 unit-test binaries + 11 integration checks green. Committing.

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
close out the seven shipped issues whose code had already landed

phase 2: the editor's ...`

### Tool result

```
[master 86bee91] close out the seven shipped issues whose code had already landed
 8 files changed, 37 insertions(+), 12 deletions(-)
 rename issues/{ => completed}/231-move-signature-parsers-into-langs.md (100%)
 rename issues/{ => completed}/301-pool-lifecycle-and-worker-init.md (97%)
 rename issues/{ => completed}/303-language-runtime-spec.md (98%)
 rename issues/{ => completed}/308-bash-language-spec.md (97%)
 rename issues/{ => completed}/309-build-system.md (97%)
 rename issues/{ => completed}/310-priority-queue.md (93%)
 rename issues/{ => completed}/314-c-json-parser.md (96%)
```

## Assistant

_Tools:_

- `TaskUpdate: ?`

### Tool result

```
Updated task #3 status
```

## Assistant

_Tools:_

- `TaskUpdate: 301, 303, 308, 309, 310, 314 all closed in commit 86bee91 with status headers flipped, progress doc updated, full test suite green.`

### Tool result

```
Updated task #4 subject, description, status
```

## Assistant

Group A closed. State of the open list now:

**Phase 2 stragglers (4):** 217, 232, 233, 239
**Phase 3 in flight (10):** 302, 304, 305, 306, 307, 311, 312, 313, 315, 317

Tests still green (22 + 5 + 11). Commit `86bee91`.

Where do you want to point me next? My honest read on the remaining 14:

- **232 and 313** aren't traditional close-able issues — 232 is an ongoing test inventory (it gets updated as tests are added; it doesn't "complete" the way a feature does), and 313 is research that's explicitly blocked until the rest of phase 3 lands. Worth deciding whether to redefine them so they *can* close, or just leave them in a deliberate non-closing state.
- **315** has no driver — nobody is pinning artifacts today. Strong candidate for outright deferral / writing off rather than implementing.
- **302's allocator rewrite, 307's per-box wrapper generation, and 312's per-edge classification** are the three "the design says X, the code does Y" gaps I flagged earlier. Want to resolve those design questions before I touch any code — otherwise I'd be implementing toward a target you may want to move.
- **217, 239, 233 (schema-side), 311 (MPSC ring + opt-in logs), 304 (extra routing kinds), 305 (size-class enumeration), 306 (rich return marshalling)** are bounded coding work. Tractable in this session if we go in order.
- **317** has the C and Bash bridges still to write; the C side depends on 307's resolution.

Which thread do you want me to pull on next? Two ways I'd suggest, in order:

1. Resolve the three design questions (302, 307, 312) first — they reshape downstream work, and 307's answer unblocks 317.
2. Walk the bounded-coding items in 217 → 239 → 233 → 304 → 305 → 306 → 311 order — physical implementation work that doesn't depend on decisions yet to be made.

## User

Oh, we should have one git commit per issue. Can you re-do the git commits? Ideally, just stage the specific changes made for the issue file that's being committed.

## Assistant

Fair — that's the right shape. Let me undo the bundled commit and redo as seven.

## Assistant

_Tools:_

- `Bash: git reset HEAD~1`

## Assistant

_Tools:_

- `Bash: git status`

### Tool result

```
Unstaged changes after reset:
D	issues/231-move-signature-parsers-into-langs.md
D	issues/301-pool-lifecycle-and-worker-init.md
D	issues/303-language-runtime-spec.md
D	issues/308-bash-language-spec.md
D	issues/309-build-system.md
D	issues/310-priority-queue.md
D	issues/314-c-json-parser.md
M	issues/phase-3-progress.md
```

### Tool result

```
On branch master
Changes not staged for commit:
  (use "git add/rm <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	deleted:    issues/231-move-signature-parsers-into-langs.md
	deleted:    issues/301-pool-lifecycle-and-worker-init.md
	deleted:    issues/303-language-runtime-spec.md
	deleted:    issues/308-bash-language-spec.md
	deleted:    issues/309-build-system.md
	deleted:    issues/310-priority-queue.md
	deleted:    issues/314-c-json-parser.md
	modified:   issues/phase-3-progress.md

Untracked files:
  (use "git add <file>..." to include in what will be committed)
	issues/completed/231-move-signature-parsers-into-langs.md
	issues/completed/301-pool-lifecycle-and-worker-init.md
	issues/completed/303-language-runtime-spec.md
	issues/completed/308-bash-language-spec.md
	issues/completed/309-build-system.md
	issues/completed/310-priority-queue.md
	issues/completed/314-c-json-parser.md

no changes added to commit (use "git add" and/or "git commit -a")
```

## Assistant

_Tools:_

- `Bash: git checkout HEAD -- issues/phase-3-progress.md`

### Tool result

```
(Bash completed with no output)
```

## Assistant

Clean. Now committing one issue at a time.

## Assistant

_Tools:_

- `Bash: git add issues/231-move-signature-parsers-into-langs.md issues/completed/231-move-signature-parsers-into-langs.md`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
231 — language-specific signature parsers move into langs/

The editor's file browser ...`

### Tool result

```
The user doesn't want to proceed with this tool use. The tool use was rejected (eg. if it was a file edit, the new_string was NOT written to the file). To tell you how to proceed, the user said:
make sure you include the files that were changed in that commit's implementation, ideally just the specific lines that were modified to implement it...
```

## Assistant

Let me look at how prior closing commits were shaped so I understand what you're after.

## Assistant

_Tools:_

- `Bash: git show --stat efeb79b | head -40`

### Tool result

```
commit efeb79b71435fe284a414fce92361361eadede0a
Author: gabrilend <gabrilend@gmail.com>
Date:   Tue May 19 21:35:35 2026 -0700

    229 — read and write box kinds, with the editor surface to match
    
    The old data / file_write kinds always wore the wrong names. A box
    named "data" doesn't say what it does; a sink named "file_write"
    hides that it's a sink in a noun. Rename to verbs — read and write —
    and reshape both to fit the data they actually move.
    
    A read box now emits a value. Sometimes that value lives in a file
    and you give the box a path; sometimes the value is a small literal
    you type in the inspector. Setting an inline value hides the path
    port on the canvas — the box that has no file no longer pretends to
    have one.
    
    A write box no longer disappears the moment it commits to disk. It
    emits the boolean true downstream, so graphs that want to chain on
    "and now the file exists" can fire whatever comes next; graphs that
    don't care drop the boolean per the unwired-output rule. The same
    rule already held informally everywhere; making it visible at the
    sink puts it in writing.
    
    The editor's kind dropdown, the box colours, the header labels, the
    inspector layout, and the architecture doc all line up behind the
    new names. One end-to-end fixture exercises the inline-literal
    path. The cross-language slice — where a Lua table reaches a C
    struct natively across the wire — remains a documented next step,
    because the bridges from 317 and the slot store don't yet share a
    shape. Phase 2 was skipped on purpose; its days are numbered.
    
    Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>

 assets/js/002-boxes.js                             |  43 +-
 assets/js/004-inspector.js                         |  48 ++-
 docs/001-architecture.md                           |  41 +-
 docs/HTML/assets/demos.js                          |   4 +-
 .../229-data-box-as-language-agnostic-file-io.md   | 255 -----------
 .../229-data-box-as-language-agnostic-file-io.md   | 477 +++++++++++++++++++++
```

## Assistant

I want to make sure I read your guidance right before I redo this. Looking back at how `efeb79b` (229) was shaped — that commit had the issue file move *plus* the actual source code changes that implemented it (`002-boxes.js`, `004-inspector.js`, `docs/001-architecture.md`, etc.), all in one atomic commit.

Our seven issues are different though: each was implemented across many earlier commits over the past couple weeks (the implementation logs show dates from 2026-05-12 through 2026-05-19). There are no currently-uncommitted source-code changes for them — the only working-tree changes are the issue file moves + status flips + the progress doc.

A few ways I can read your direction. Which is it?

1. **Each commit contains just the issue's own file move + status edit + the one progress-doc line**, nothing else. (This was my plan; the rejection might mean my staging missed the progress-doc line for 231 specifically — but 231 is phase-2 and doesn't have a phase-3-progress entry.)

2. **Squash the historical implementation commits into the closing commit per issue** — i.e., rewrite history so each issue's implementation work and its closure live in a single commit. This is destructive but defensible (we're on master with no remote pushed for these).

3. **You actually have uncommitted source-file changes** sitting in the worktree somewhere I should pick up. (I don't see any — `git status` only shows the issue moves + progress doc — but maybe I'm missing a directory.)

I think you meant (1), and the rejection was about staging cleanliness rather than scope. If so, I'll proceed: 231 alone, then each phase-3 issue with its single progress-doc line. Confirm or redirect?

## User

prior commits might be mangled too. But we should try and do the correct thing when we can.

## Assistant

Understood — historical commits are what they are; we do it right from here. Proceeding with one commit per issue.

## Assistant

_Tools:_

- `Bash: git add issues/231-move-signature-parsers-into-langs.md issues/completed/231-move-signature-parsers-into-langs.md`

### Tool result

```
Exit code 128
fatal: pathspec 'issues/231-move-signature-parsers-into-langs.md' did not match any files
```

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
231 — language-specific signature parsers move into langs/

The editor's file browser ...`

### Tool result

```
<tool_use_error>Cancelled: parallel tool call Bash(git add issues/231-move-signature-parser…) errored</tool_use_error>
```

## Assistant

_Tools:_

- `Bash: git add -A issues/231-move-signature-parsers-into-langs.md issues/completed/231-move-signature-parsers-into-langs.md ...`

### Tool result

```
Exit code 128
fatal: pathspec 'issues/231-move-signature-parsers-into-langs.md' did not match any files
```

## Assistant

_Tools:_

- `Bash: git rm issues/231-move-signature-parsers-into-langs.md && git add issues/completed/231-move-signature-parsers-into-la...`

### Tool result

```
Exit code 128
fatal: pathspec 'issues/231-move-signature-parsers-into-langs.md' did not match any files
```

## Assistant

_Tools:_

- `Bash: git ls-files issues/231-move-signature-parsers-into-langs.md issues/completed/231-move-signature-parsers-into-langs.md`

### Tool result

```
issues/completed/231-move-signature-parsers-into-langs.md
```

## Assistant

_Tools:_

- `Bash: git status --short`

### Tool result

```
 D issues/301-pool-lifecycle-and-worker-init.md
 D issues/303-language-runtime-spec.md
 D issues/308-bash-language-spec.md
 D issues/309-build-system.md
 D issues/310-priority-queue.md
 D issues/314-c-json-parser.md
R  issues/231-move-signature-parsers-into-langs.md -> issues/completed/231-move-signature-parsers-into-langs.md
?? issues/completed/301-pool-lifecycle-and-worker-init.md
?? issues/completed/303-language-runtime-spec.md
?? issues/completed/308-bash-language-spec.md
?? issues/completed/309-build-system.md
?? issues/completed/310-priority-queue.md
?? issues/completed/314-c-json-parser.md
```

## Assistant

231 is already staged as a rename. Just commit.

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
231 — language-specific signature parsers move into langs/

The editor's file browser ...`

### Tool result

```
[master 1b14cf8] 231 — language-specific signature parsers move into langs/
 1 file changed, 0 insertions(+), 0 deletions(-)
 rename issues/{ => completed}/231-move-signature-parsers-into-langs.md (100%)
```

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/301-pool-lifecycle-and-worker-init.md && git add issues/completed/301-pool-lifecycle...`

### Tool result

```
 D issues/303-language-runtime-spec.md
 D issues/308-bash-language-spec.md
 D issues/309-build-system.md
 D issues/310-priority-queue.md
 D issues/314-c-json-parser.md
R  issues/301-pool-lifecycle-and-worker-init.md -> issues/completed/301-pool-lifecycle-and-worker-init.md
?? issues/completed/303-language-runtime-spec.md
?? issues/completed/308-bash-language-spec.md
?? issues/completed/309-build-system.md
?? issues/completed/310-priority-queue.md
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
301 — thread-pool lifecycle and per-worker initi...`

### Tool result

```
[master d1bff73] 301 — thread-pool lifecycle and per-worker initialization
 2 files changed, 5 insertions(+), 2 deletions(-)
 rename issues/{ => completed}/301-pool-lifecycle-and-worker-init.md (97%)
```

## Assistant

Acknowledged — staying forward-looking with one-commit-per-issue. 231 and 301 already done. Continuing with 303.

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/303-language-runtime-spec.md && git add issues/completed/303-language-runtime-spec.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
303 — pluggable language-runtime spec, equal foo...`

### Tool result

```
(Bash completed with no output)
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

### Tool result

```
[master ab1d2b8] 303 — pluggable language-runtime spec, equal footing for every language
 2 files changed, 5 insertions(+), 2 deletions(-)
 rename issues/{ => completed}/303-language-runtime-spec.md (98%)
```

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/308-bash-language-spec.md && git add issues/completed/308-bash-language-spec.md`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
308 — bash boxes through a persistent socket ser...`

### Tool result

```
[master 43f33c6] 308 — bash boxes through a persistent socket server, no per-call spawn
 2 files changed, 6 insertions(+), 2 deletions(-)
 rename issues/{ => completed}/308-bash-language-spec.md (97%)
```

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/309-build-system.md && git add issues/completed/309-build-system.md`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
309 — top-level build and the portable-artifact ...`

### Tool result

```
[master c249ea8] 309 — top-level build and the portable-artifact pipeline
 2 files changed, 7 insertions(+), 2 deletions(-)
 rename issues/{ => completed}/309-build-system.md (97%)
```

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/310-priority-queue.md && git add issues/completed/310-priority-queue.md`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
310 — priority parameter wired through the spawn...`

### Tool result

```
[master 707f5ab] 310 — priority parameter wired through the spawn surface
 2 files changed, 7 insertions(+), 2 deletions(-)
 rename issues/{ => completed}/310-priority-queue.md (93%)
```

## Assistant

_Tools:_

- `Bash: git update-index --remove issues/314-c-json-parser.md && git add issues/completed/314-c-json-parser.md`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-3-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: git add issues/phase-3-progress.md && git commit -m "$(cat <<'EOF'
314 — the project's own small C JSON parser and ...`

### Tool result

```
[master bb404f5] 314 — the project's own small C JSON parser and writer
 2 files changed, 7 insertions(+), 2 deletions(-)
 rename issues/{ => completed}/314-c-json-parser.md (96%)
```

## Assistant

_Tools:_

- `Bash: git log --oneline -10 && echo "---" && git status`

### Tool result

```
bb404f5 314 — the project's own small C JSON parser and writer
707f5ab 310 — priority parameter wired through the spawn surface
c249ea8 309 — top-level build and the portable-artifact pipeline
43f33c6 308 — bash boxes through a persistent socket server, no per-call spawn
ab1d2b8 303 — pluggable language-runtime spec, equal footing for every language
d1bff73 301 — thread-pool lifecycle and per-worker initialization
1b14cf8 231 — language-specific signature parsers move into langs/
efeb79b 229 — read and write box kinds, with the editor surface to match
82690ad 317 stage 2 — Lua arrays preserve every JSON shape
a4d5cd0 217 — reopened as variadic ports + language-spec variadic shape
---
On branch master
nothing to commit, working tree clean
```

## Assistant

Group A is closed properly now — seven commits, one per issue, each carrying just the issue file move (and for the six phase-3 ones, the matching single progress-doc line). Working tree clean.

```
bb404f5 314 — the project's own small C JSON parser and writer
707f5ab 310 — priority parameter wired through the spawn surface
c249ea8 309 — top-level build and the portable-artifact pipeline
43f33c6 308 — bash boxes through a persistent socket server, no per-call spawn
ab1d2b8 303 — pluggable language-runtime spec, equal footing for every language
d1bff73 301 — thread-pool lifecycle and per-worker initialization
1b14cf8 231 — language-specific signature parsers move into langs/
```

Remaining open: **14 issues** — phase-2 stragglers 217, 232, 233, 239; phase-3 in-flight 302, 304, 305, 306, 307, 311, 312, 313, 315, 317.

Where to next? My recommendation, smallest-to-largest:

1. **Decide on 232 and 313's closability** — neither is a normal feature issue. 232 is an ongoing test inventory that gets touched whenever tests change; 313 is research blocked until phase 3 finishes. Easy 5-minute conversation: do we redefine them so they can close, or leave them in a deliberate non-closing state and skip them in the audit phase?
2. **Walk the three design-question issues (302, 307, 312)** — these reshape the work for everything downstream of them. 312 explicitly says "ask the user before implementing." Worth resolving before I touch code.
3. **Then bounded coding**: 217 → 239 → 233 → 305 → 306 → 304 → 311 → 315 → 317.

Want to start with the 232/313 conversation, or the 302/307/312 design questions?

## User

● Group A is closed properly now — seven commits, one per issue, each carrying just the issue file move (and for the six phase-3 ones, the matching single progress-doc line). Working tree clean.

sorry I was distracted managing another team - did you get the source-code that each issue file changed bundled in with the issue file commits? That's what we want - to explicitely match source-code to issue file to git commit. They should be a trinity, where each directly corresponds to the other. This is in-progress organization that is crucial for business development opportunities moving forward.

## Assistant

No — to be straight with you: the seven commits I just made only contain the issue file move + (for phase-3 issues) the single progress-doc line. None of them include the source code that implemented the work, because that source was committed long ago across many historical commits that bundled work for multiple issues at once.

You said earlier you were OK leaving the historical mess alone ("if you wanna do it the right way we can, but it'll be a big effort"). So the trinity for these seven would require going back through ~30 commits since 2026-05-12, untangling which lines belonged to which issue, and re-stitching. Concretely:

- **301**: scattered across the commits that built `libs/task-pool/pool.{c,h}`, `tests/301-pool-test.c`, the worker teardown callback, etc.
- **303**: spans `src/011-spec-registry.{c,h}`, `tests/011-spec-registry-test.c`, plus the lang-spec.h evolution.
- **308**: bash spec's fork+exec first pass, then the socketpair rewrite, then the dladdr lookup — three separate commits at minimum.
- **309**: scaffolding milestone + compile pipeline + bash-server self-location — also multi-commit.
- **310**: the small priority-queue addition that touched pool.c.
- **314**: parser milestone + writer half + strict leading-zero — three commits.
- **231**: parser-tests + variadic_tail retirement.

Some of those historical commits also touched code that belongs to *other* open issues (e.g., the slot-store work, the dispatch work, the loader work). Untangling cleanly is genuinely hard — you'd end up with merge conflicts because the same files were edited under different issues' work.

**Two options, your call:**

1. **Accept the seven Group A commits as-is** — they document the closure, even if the implementation history was bundled. Apply the trinity model strictly to the remaining 14 issues from here on. The audit phase later will catch any places where the issue file's design and the code disagree.

2. **Do the history reorganization** — `git reset` back to before the seven closures, identify each implementation commit's lines per-issue (probably via `git log -p` reading), cherry-pick or rebuild commits with `git rebase -i` to fold them into single per-issue commits. This is a multi-hour effort and the result risks losing some context from the original commit messages.

My honest recommendation is (1) plus a note in the audit phase that the seven Group A issues' implementation history was bundled — so when we hand off to the downstream consumer, they at least know that. The trinity discipline applies cleanly from issue #8 onward, which is the work that actually defines the model going forward.

Which way?

## User

● No — to be straight with you: the seven commits I just made only contain the issue file move + (for phase-3 issues) the single progress-doc line.

thank you, I know it's difficult to be wrong, or to make mistakes. Let's work through it together. I'm here for you, and you won't have to do any extra work because of this diversion. We'll just get through it bit by bit at a time.

None of them include the source code that implemented the work, because that source was committed long ago across many historical
  commits that bundled work for multiple issues at once.

Hmmmm, I see. So it was a landscape that was already too enmeshed to properly disambiguate. That's... Not ideal, but it's fine. Let's just move forward from here. How about we work on the group B issues and try to apply this pattern to future commits moving forward?

## Assistant

Thanks. Moving on cleanly is the right call.

I'd like to start with **217** as the warm-up — it's the smallest open issue and the closure cleanly exercises the trinity pattern. It has three remaining bullets per its own implementation log:

1. Wire `004-inspector.js::mk_port_display` to load the active box's language spec and hide the `var` toggle when `variadic_shape !== 'positional'` (future-proofing — every shipped language is positional today, so no visible change, just the gate's mechanism).
2. Render the parsed function signature near the box header in the inspector, so users see the author's declared arity.
3. Add a "language spec for the editor" section to `docs/005-language-specs.md` pointing at `spec.js` as the canonical home for editor-facing language facts.

That's roughly 30–45 minutes of work. The resulting commit would bundle the JS changes, the docs addition, the issue file move into `completed/`, and the `phase-2-progress.md` row update — proper trinity.

A few things to confirm before I touch code:

- **`docs/005-language-specs.md`** — does this file exist yet? The issue references it as "to be written." If it doesn't exist, I'll create it as part of 217 (small enough to be in scope) and add it to `docs/table-of-contents.md` per your project convention.
- **Inspector signature rendering** — the issue says "Inspector renders the parsed signature near the box header." I'd render it as a read-only line under the box title showing `fn_name(param1, param2, ...)` from `parser.js`'s output. Does that match what you had in mind, or did you envision something different (e.g., per-port label hints)?

Want me to proceed on 217 with those interpretations? Or pick a different starting issue?

## User

  - docs/005-language-specs.md — does this file exist yet? The issue references it as "to be written." If it doesn't exist, I'll create it as part of 217 (small enough to be in scope) and add it to docs/table-of-contents.md per your project convention.

so the 005 number will probably be very different now, that's okay. We'll have to re-number the files eventually. I think the language spec is still being written - don't worry about writing it yourself, we're still trying to nail down exactly what a language spec is and means.

  - Inspector signature rendering — the issue says "Inspector renders the parsed signature near the box header." I'd render it as a read-only line under the box title showing fn_name(param1, param2, ...) from parser.js's output. Does that match what you had in mind, or did you
  envision something different (e.g., per-port label hints)?

the... name of the function? that should go next to the output port as a label. The name of the file that the function is from should be the box name header, but that one should be editable in the inspector, but not the box itself. The box should display the value that is defined in the inspector. The output function name should not be editable.

## Assistant

Got it — skip the docs file (still being designed), and the signature-rendering is already accomplished by 236 (header = filename, output port labeled with `fn()`). That collapses 217's remaining work to just the `var`-toggle gate.

Let me read the inspector to find the right insertion point.

## Assistant

_Tools:_

- `Bash: grep -n "mk_port_display\|var_button\|variadic\|LANGUAGE_SPEC\|spec\.js" /mnt/mtwo/programs/sora/soramech/assets/js/0...`

### Tool result

```
16:  //   - non-variadic input is just `text`
17:  //   - variadic group has slots `text_0`, `text_1`, ..., `text_N`
18:  //   - the base name `text` is listed in box.variadic_inputs
23:  function parse_variadic_name(name) {
28:  function is_variadic_slot(box, port_name) {
29:    const p = parse_variadic_name(port_name);
31:    return (box.variadic_inputs || []).includes(p.base);
34:  function variadic_slots_for(box, base) {
37:      const pv = parse_variadic_name(p.name);
44:  function last_variadic_index(box, base) {
45:    const slots = variadic_slots_for(box, base);
169:  // Toggle a non-variadic input ON: snip every wire targeting the
171:  // box.variadic_inputs.
174:  // matches the user's mental model: variadic-on and variadic-off
180:  // Special case: a port literally named `...` (Lua variadic that
183:  async function make_variadic(port_name) {
193:    // 2. Rename the port and add base to variadic_inputs.
195:    current_box.variadic_inputs  = current_box.variadic_inputs || [];
196:    if (!current_box.variadic_inputs.includes(base)) {
197:      current_box.variadic_inputs.push(base);
205:  // Toggle a variadic group OFF (collapse to single port `<base>`).
208:  // base from box.variadic_inputs.
211:  // "shape change clears wires" rule as make_variadic. Symmetric.
212:  async function unmake_variadic(base) {
213:    const slots = variadic_slots_for(current_box, base);
232:    // 3. Remove base from variadic_inputs.
233:    current_box.variadic_inputs = (current_box.variadic_inputs || [])
244:  async function add_variadic_slot(base) {
245:    const next = last_variadic_index(current_box, base) + 1;
252:  // Remove a single variadic slot at `slot_name`. Other slots keep
256:  // and produced surprising sidebar updates. The variadic helpers
259:  async function remove_variadic_slot(slot_name) {
260:    const pv = parse_variadic_name(slot_name);
263:    // The first slot is permanent — un-variadic instead if you want
266:      status_msg('cannot remove slot 0 — toggle variadic off instead', 'error');
298:  // as the variadic input toggle.
337:  // every grow. Mirrors the variadic auto-grow pattern.
354:  // need to compact like the variadic numeric indexes).
438:  // no-op once a newer slot exists below it. Mirrors the variadic
497:  // currently the unmake_variadic path. Mirrors sever_output_wires
```

## Assistant

_Tools:_

- `Bash: grep -n "spec\.js\|LANGUAGE_SPEC\|lexer\.js\|dynamic.*import\|import(" /mnt/mtwo/programs/sora/soramech/assets/js/*.j...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/assets/js/008-source-view.js:51:      const mod = await import('/langs/' + lang + '/lexer.js');
/mnt/mtwo/programs/sora/soramech/assets/js/007-filebrowser.js:18:      const mod = await import('/langs/' + lang + '/parser.js');
```

## Assistant

_Tools:_

- `Bash: grep -n "'var'\|\"var\"\|var-toggle\|var_toggle\|>var<\|→ make_variadic\|make_variadic(\|button.*var" /mnt/mtwo/pro...`

### Tool result

```
183:  async function make_variadic(port_name) {
212:  async function unmake_variadic(base) {
607:        //   1. Plain port — "var" toggle turns it on
617:          var_btn.textContent   = 'var';
619:          var_btn.onclick       = () => make_variadic(p.name);
626:          var_btn.onclick       = () => unmake_variadic(pv.base);
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
580	        const top_row = document.createElement('div');
581	        top_row.className = 'port-display-row';
582	
583	        // Variadic context (issue 217 part B):
584	        //   in_var_group = is part of a variadic group (one of multiple slots)
585	        //   pv           = { base, index } parsed from the slot name
586	        const pv           = current_box ? parse_variadic_name(p.name) : null;
587	        const in_var_group = current_box && is_variadic_slot(current_box, p.name);
588	
589	        // Read-only name label. Port names come from the parsed
590	        // function signature (file browser populates them); editing
591	        // them in the inspector is gone — the original 208 rationale
592	        // (prevent typo drift between source and box JSON) holds, and
593	        // post-fact rename was just a user tag anyway.
594	        const name_el = document.createElement('span');
595	        name_el.className   = 'port-name-display';
596	        name_el.textContent = p.name || '';
597	        top_row.appendChild(name_el);
598	
599	        if (p.type && p.type !== 'any') {
600	          const type_el = document.createElement('span');
601	          type_el.className   = 'port-type';
602	          type_el.textContent = ':' + p.type;
603	          top_row.appendChild(type_el);
604	        }
605	
606	        // Variadic control button. Three states:
607	        //   1. Plain port — "var" toggle turns it on
608	        //   2. First slot of a variadic group — "var ×" collapses back
609	        //   3. Non-first slot — "×" removes that slot
610	        const btn_style = 'background:none;border:1px solid #2a2f45;border-radius:3px;' +
611	          'color:#6c72a0;cursor:pointer;font-family:monospace;font-size:9px;' +
612	          'padding:1px 5px;line-height:1.4;';
613	
614	        if (!in_var_group) {
615	          const var_btn = document.createElement('button');
616	          var_btn.style.cssText = btn_style;
617	          var_btn.textContent   = 'var';
618	          var_btn.title         = 'mark this input as variadic (grows to N slots)';
619	          var_btn.onclick       = () => make_variadic(p.name);
620	          top_row.appendChild(var_btn);
621	        } else if (pv.index === 0) {
622	          const var_btn = document.createElement('button');
623	          var_btn.style.cssText = btn_style + 'border-color:#4a9eff;color:#4a9eff;';
624	          var_btn.textContent   = 'var ×';
625	          var_btn.title         = 'collapse back to a single non-variadic input';
626	          var_btn.onclick       = () => unmake_variadic(pv.base);
627	          top_row.appendChild(var_btn);
628	        } else {
629	          const x_btn = document.createElement('button');
630	          x_btn.style.cssText = btn_style;
631	          x_btn.textContent   = '×';
632	          x_btn.title         = 'remove this slot';
633	          x_btn.onclick       = () => remove_variadic_slot(p.name);
634	          top_row.appendChild(x_btn);
635	        }
636	
637	        // `opt` toggle (issue 230). Flips port.optional between true
638	        // and undefined (absent = false). An optional port doesn't
639	        // require a wire or literal — the compile-time check from
640	        // 230 allows it through. Amber styling so it reads as a
641	        // different category from the blue `var` family.
642	        const opt_btn = document.createElement('button');
643	        const opt_on  = ports[i].optional === true;
644	        opt_btn.style.cssText = btn_style + (opt_on
645	          ? 'border-color:#d4762a;color:#d4762a;'
646	          : '');
647	        opt_btn.textContent = 'opt';
648	        opt_btn.title       = 'mark this input as optional — the box runs even if unwired';
649	        opt_btn.onclick     = () => {
650	          ports[i].optional = opt_on ? undefined : true;
651	          save();
652	          show(current_box, on_change_cb, on_delete_cb);
653	        };
654	        top_row.appendChild(opt_btn);
655	
656	        block.appendChild(top_row);
657	
658	        // Value input — full-width row below the name. Empty means
659	        // "no literal" (wire supplies the value at runtime).
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/008-source-view.js`

### Tool result

```
35	    probe.remove();
36	    return w;
37	  }
38	  const CHAR_W = measure_char_w();
39	  // }}}
40	
41	  // {{{ Lexer registry (issue 223)
42	  // Lazy-loads a per-language tokenizer the first time a viewer asks
43	  // for that language. Cached afterwards. Files with an unknown
44	  // extension or a missing lexer fall back to plain rendering.
45	  const EXT_TO_LANG = { lua: 'lua', sh: 'bash', bash: 'bash', c: 'c', h: 'c' };
46	  const lexer_cache = {};   // lang_name → { tokenize } | null (no lexer)
47	
48	  async function load_lexer(lang) {
49	    if (lang in lexer_cache) return lexer_cache[lang];
50	    try {
51	      const mod = await import('/langs/' + lang + '/lexer.js');
52	      lexer_cache[lang] = mod;
53	      return mod;
54	    } catch (e) {
55	      console.warn('no lexer for ' + lang + ':', e.message);
56	      lexer_cache[lang] = null;
57	      return null;
58	    }
59	  }
60	
61	  // Render `text` into `body` with syntax-highlighted spans. Tokens
62	  // are gap-free per the lexer contract; we just walk them and emit
63	  // one span per token. Plain rendering (no spans) is the fallback
64	  // when the lexer can't be loaded or the file extension is unknown.
65	  function render_with_lexer(body, text, tokens) {
66	    if (!tokens) {
67	      body.textContent = text;
68	      return;
69	    }
```

## Assistant

_Tools:_

- `Bash: grep -n "box\.lang\|current_box\.lang\|\.lang " /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js | head -15`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Bash: grep -n "EXT_TO_LANG\|ext_to_lang\|file_ext\|\.lua\|\.sh\|\.c['\"]" /mnt/mtwo/programs/sora/soramech/assets/js/004-in...`

### Tool result

```
155:  // "write-result" instead of "src/write-result.lua" — the full path
```

## Assistant

_Tools:_

- `Bash: cat /mnt/mtwo/programs/sora/soramech/langs/lua/spec.js`

### Tool result

```
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
```

## Assistant

_Tools:_

- `Bash: grep -n "^  function show\|async function show\|function show(\|current_box =\|current_spec" /mnt/mtwo/programs/sora/...`

### Tool result

```
761:  function show_content(title_text, render_fn) {
804:  function show(box, on_changed, on_delete) {
```

## Assistant

_Tools:_

- `Bash: grep -n "current_box\.ref\|box\.ref\|ref\.split\|\\.lua['\"]\\|\\.sh['\"]\\|\\.c['\"]" /mnt/mtwo/programs/sora/sorame...`

### Tool result

```
155:  // "write-result" instead of "src/write-result.lua" — the full path
156:  // is still on box.ref for the runtime, only the display is short.
304:    delete current_box.ref;
327:    if (current_box.ref === undefined) current_box.ref = '';
735:      if (current_box.ref !== filename) {
738:      current_box.ref    = filename;
943:    ref_disp.title     = box.ref || '';   // full path on hover
944:    ref_disp.textContent = basename_no_ext(box.ref) || '(none)';
957:    view_btn.hidden = !box.ref;
959:      if (!current_box || !current_box.ref) {
963:      SourceView.open_source_view(current_box.ref, current_box);
979:    fn_btn.disabled    = !box.ref;
983:      FileBrowser.render_at_fn_list(fields, current_box.ref, async (filename, fn) => {
987:        if (current_box.ref !== filename) {
990:        current_box.ref    = filename;
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
800	  }
801	  // }}}
802	
803	  // {{{ show
804	  function show(box, on_changed, on_delete) {
805	    current_box  = box;
806	    on_change_cb = on_changed;
807	    on_delete_cb = on_delete || null;
808	    panel.classList.remove('hidden');
809	
810	    // title: editable label input + read-only id subtitle. The
811	    // placeholder shows the canvas-side default (issue 236) — basename
812	    // of ref for a call box, the kind name otherwise — so the user
813	    // can see what would render with the field left empty.
814	    title.innerHTML = '';
815	    const lbl_inp = document.createElement('input');
816	    lbl_inp.type        = 'text';
817	    lbl_inp.value       = box.label || '';
818	    lbl_inp.placeholder = Boxes.default_header(box);
819	    lbl_inp.className   = 'inspector-title-inp';
820	    lbl_inp.addEventListener('input', () => { current_box.label = lbl_inp.value; save(); });
821	    title.appendChild(lbl_inp);
822	    const id_sub = document.createElement('div');
823	    id_sub.className   = 'inspector-id-sub';
824	    id_sub.textContent = box.id;
825	    title.appendChild(id_sub);
826	
827	    fields.innerHTML = '';
828	
829	    // delete button — always visible at the top of the inspector
830	    if (on_delete_cb) {
831	      const del_btn = document.createElement('button');
832	      del_btn.textContent = 'delete box';
833	      del_btn.className   = 'delete-box-btn';
834	      del_btn.onclick     = () => on_delete_cb();
835	      fields.appendChild(del_btn);
836	    }
837	
838	    // label is now the editable title — no separate field needed
839	
840	    fields.appendChild(mk_row('kind', (() => {
841	      const sel = document.createElement('select');
842	      ['call', 'read', 'write'].forEach(k => {
843	        const opt = document.createElement('option');
844	        opt.value = k; opt.textContent = k;
845	        if (k === box.kind) opt.selected = true;
846	        sel.appendChild(opt);
847	      });
848	      sel.addEventListener('change', () => {
849	        current_box.kind = sel.value;
```

## Assistant

_Tools:_

- `Bash: grep -n "^  // {{{\|^window\.\|^var Inspector\|^(function" /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.j...`

### Tool result

```
14:  // {{{ Variadic helpers (issue 217 part B)
100:  // {{{ hide
108:  // {{{ save
121:  // {{{ mk_row
133:  // {{{ mk_text_input
143:  // {{{ mk_readonly
152:  // {{{ basename_no_ext
164:  // {{{ Variadic operations (issue 217 part B)
278:  // {{{ Iterator helpers (issue 221)
560:  // {{{ mk_port_display
722:  // {{{ open_browser
758:  // {{{ show_content
773:  // {{{ render_io_box
803:  // {{{ show
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
1	// Inspector sidebar: shows and edits the selected box's fields.
2	// Fires PUT to the server on every field change (autosave).
3	// For call boxes, ref/fn are set via the file browser; inputs are read-only, derived from parsing.
4	
5	const Inspector = (() => {
6	  const panel  = document.getElementById('inspector');
7	  const title  = document.getElementById('inspector-title');
8	  const fields = document.getElementById('inspector-fields');
9	
10	  let current_box  = null;
11	  let on_change_cb = null;
12	  let on_delete_cb = null;
13	
14	  // {{{ Variadic helpers (issue 217 part B)
15	  // Variadic input naming convention:
16	  //   - non-variadic input is just `text`
17	  //   - variadic group has slots `text_0`, `text_1`, ..., `text_N`
18	  //   - the base name `text` is listed in box.variadic_inputs
19	  //
20	  // These helpers parse and query that convention without mutating
21	  // anything; the toggle / add / remove operations live below.
22	
23	  function parse_variadic_name(name) {
24	    const m = /^(.+)_(\d+)$/.exec(name || '');
25	    return m ? { base: m[1], index: parseInt(m[2], 10) } : null;
```

## Assistant

Implementing the gate now. Three edits to the inspector + status/issue file updates.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js has been updated successfully.
```

## Assistant

Now wiring the spec pre-warm into `show()`.

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Bash: node --check /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js && echo "syntax ok"`

### Tool result

```
syntax ok
```

## Assistant

_Tools:_

- `Bash: SORAMECH_DIR=/mnt/mtwo/programs/sora/soramech node --test tests/234-language-spec-js-test.mjs 2>&1 | tail -15`

### Tool result

```
# Subtest: c spec declares positional variadic
ok 3 - c spec declares positional variadic
  ---
  duration_ms: 1.343547
  type: 'test'
  ...
1..3
# tests 3
# suites 0
# pass 3
# fail 0
# cancelled 0
# skipped 0
# todo 0
# duration_ms 68.466175
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/217-variadic-ports-and-language-spec-shape.md has been updated successfully.
```

## Assistant

Now updating the issue file's status and the "Remaining work" section.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/217-variadic-ports-and-language-spec-shape.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/217-variadic-ports-and-language-spec-shape.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/217-variadic-ports-and-language-spec-shape.md`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: git mv issues/217-variadic-ports-and-language-spec-shape.md issues/completed/`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Bash: git status --short`

### Tool result

```
 M assets/js/004-inspector.js
 M docs/002-roadmap.md
 M docs/HTML/002-roadmap.html
 M docs/HTML/assets/demos.js
RM issues/217-variadic-ports-and-language-spec-shape.md -> issues/completed/217-variadic-ports-and-language-spec-shape.md
 M issues/phase-2-progress.md
```

## Assistant

Three doc files are showing as modified that I didn't touch — likely your own in-progress work. Let me confirm before committing.

## Assistant

_Tools:_

- `Bash: git diff --stat docs/002-roadmap.md docs/HTML/002-roadmap.html docs/HTML/assets/demos.js`

### Tool result

```
 docs/002-roadmap.md        | 36 ++++++++++++++++++++++++------------
 docs/HTML/002-roadmap.html | 13 +++++++------
 docs/HTML/assets/demos.js  |  2 +-
 3 files changed, 32 insertions(+), 19 deletions(-)
```

## Assistant

_Tools:_

- `Bash: git diff docs/002-roadmap.md | head -40`

### Tool result

```
diff --git a/docs/002-roadmap.md b/docs/002-roadmap.md
index 094f13c..b38e604 100644
--- a/docs/002-roadmap.md
+++ b/docs/002-roadmap.md
@@ -68,18 +68,30 @@ for the design discussion.
 
 ## Phase 4 — SoraMind integration
 
-Goal: boxes that call SoraMind inference endpoints on the Alpine
-cluster.
-
-- LLM call box type — sends a prompt, receives a completion string
-- Prompt assembly: text values wired from upstream boxes, data file
-  sections explicitly loaded into prompt context
-- Comparator-routed classifiers ("rogue / wizard / else" via lt/eq/gt
-  on a hashed-class score)
-- Streaming output from SoraMind inference back to the runner
-
-Built on the phase 3 runtime — the LLM call is a regular box function,
-just one whose backing service is a network endpoint.
+Goal: turn the basic Ollama call box into a full inference surface
+against the Alpine cluster.
+
+The basic LLM call already ships in Phase 2 (issue 209): a Lua library
+that takes a prompt string and returns a completion, usable directly
+as a call box by copying it into a map's `src/` directory. Phase 4 is
+the work on top of that:
+
+- Streaming output from the inference endpoint back to the runner,
+  surfacing tokens as they arrive instead of waiting for the full
+  completion
+- Prompt assembly sugar — a dedicated box kind that pulls named
+  sections from data files into a prompt template, rather than the
+  user concatenating strings by hand
+- Multi-host routing — a single logical "LLM call" that picks among
+  several backing endpoints (laptop Ollama, Alpine cluster, fallback
+  hosts) based on availability or model
+- Comparator-routed classifiers as a worked pattern, not new
+  machinery — comparators already exist in Phase 2; the work here is
+  documenting the rogue / wizard / else style and shipping it as a
```

## Assistant

Those are roadmap edits (Phase 4 reshape) — your work, not mine. Leaving them untouched. Staging only my changes.

## Assistant

_Tools:_

- `Bash: git add assets/js/004-inspector.js issues/completed/217-variadic-ports-and-language-spec-shape.md issues/phase-2-prog...`

### Tool result

```
M  assets/js/004-inspector.js
 M docs/002-roadmap.md
 M docs/HTML/002-roadmap.html
 M docs/HTML/assets/demos.js
R  issues/217-variadic-ports-and-language-spec-shape.md -> issues/completed/217-variadic-ports-and-language-spec-shape.md
M  issues/phase-2-progress.md
```

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -25`

### Tool result

```
317-lua-bridge-test:
  scalars                                      ok
  arrays                                       ok
  objects                                      ok
  truncation                                   lua spec: native_to_json: output buffer (4 bytes) too small
ok
  parse_error                                  lua spec: json_to_native: parse error at byte 1: expected string
ok

  5 passed, 0 failed

integration tests (fixture maps):
  calc                                         ok
  hello                                        ok
  comparator                                   ok
  iter-route                                   ok
  pipeline                                     ok
  read-literal                                 ok
  pipeline write output                        ok
  compile pipeline (portable run)              ok
  232-lua-parser-test.mjs                      ok
  233-bash-parser-test.mjs                     ok
  234-language-spec-js-test.mjs                ok

  11 passed, 0 failed
```

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
217 — variadic toggle gates on the language, not on the function

The earlier model sa...`

### Tool result

```
[master db14fb4] 217 — variadic toggle gates on the language, not on the function
 3 files changed, 100 insertions(+), 31 deletions(-)
 rename issues/{ => completed}/217-variadic-ports-and-language-spec-shape.md (86%)
```

## Assistant

_Tools:_

- `Bash: git log --oneline -3 && echo "---" && ls /mnt/mtwo/programs/sora/soramech/issues/ | grep -v completed`

### Tool result

```
db14fb4 217 — variadic toggle gates on the language, not on the function
bb404f5 314 — the project's own small C JSON parser and writer
707f5ab 310 — priority parameter wired through the spawn surface
---
232-test-coverage-map.md
233-unified-routing-schema.md
239-literal-port-shrinks-to-nodule-with-name-on-left.md
302-wire-value-slot-store.md
304-task-dispatch-layer.md
305-c-graph-loader.md
306-lua-language-spec.md
307-c-language-spec.md
311-integration-tests-and-run-output.md
312-same-language-wire-fast-path.md
313-research-whole-program-same-language-merge.md
315-compiled-artifact-reference-counting.md
317-spec-json-bridge-for-data-boxes.md
phase-1-progress.md
phase-2-progress.md
phase-3-progress.md
```

## Assistant

217 closed properly — trinity: source (`assets/js/004-inspector.js`) + issue file move (`issues/217-...` → `issues/completed/`) + progress doc (`phase-2-progress.md`) in one commit `db14fb4`. Test suite still green.

A few notes from this round:

- **Foreign changes spotted**: `docs/002-roadmap.md`, `docs/HTML/002-roadmap.html`, `docs/HTML/assets/demos.js` are modified but unstaged — looks like a Phase 4 reshape you have in flight. I left them alone per project convention. Heads up in case they slipped your mind.
- **UI verification**: I ran `node --check` for syntax + `make test` (all green), but I didn't open the editor in a browser to click a Lua box and confirm the `var` button still renders. Per your own instruction, "if you can't test the UI, say so explicitly rather than claiming success." So: I am claiming the gate-logic is sound and the syntax is clean, but I have NOT visually verified the button still appears. Today every shipped language is positional so the behavior should be identical — but only a browser load proves it.

13 issues remaining. Next up — which?

My short list, smallest first:

- **239** — nodule rendering for literal-bound ports. Self-contained, drawing-only, no design ambiguity.
- **232** — write `scripts/run-unit-tests.sh`; mechanical, doc-defined.
- **233** — schema migration + inspector mode-dropdown for unified routing. Dispatch already supports it; just needs the editor + schema sides.

Or, the **design-question issues** (302, 307, 312) which need your direction before code lands.

Which?

## User

  - Foreign changes spotted: docs/002-roadmap.md, docs/HTML/002-roadmap.html, docs/HTML/assets/demos.js are modified but unstaged — looks like a Phase 4 reshape you have in flight. I left them alone per project convention. Heads up in case they slipped your mind.

there was some documentation drift I fixed. Can you find the git commits that touch those places, and update the git commits from way back when to have the new changes, instead of the old ones? Be sure to get the dates right... let's rewrite history, teehee.

  - UI verification: I ran node --check for syntax + make test (all green), but I didn't open the editor in a browser to click a Lua box and confirm the var button still renders. Per your own instruction, "if you can't test the UI, say so explicitly rather than claiming success." So:
   I am claiming the gate-logic is sound and the syntax is clean, but I have NOT visually verified the button still appears. Today every shipped language is positional so the behavior should be identical — but only a browser load proves it.

sounds reasonable. Let's work on 239 next.

## Assistant

Pause on the history rewrite — that's a bigger operation than it sounds, and I want to confirm scope before we touch anything that won't come back easily.

## Assistant

_Tools:_

- `Bash: git log --oneline --follow -- docs/002-roadmap.md | head -10 && echo "---" && git log --oneline -- docs/HTML/002-road...`

### Tool result

```
a27656f Fold the map-compiler ticket into the build-system one; trim the iterator ticket to its editor surface
1814cf5 Specify phase 3 thread pool runtime; align docs and graph model
ed400a4 Close completed issues; add IPC and threading design doc
478e515 Initialize SoraMech: vision, architecture, and phase 1 issue plan
---
b601ea6 Documentation pass for the phase-3 system
---
efeb79b 229 — read and write box kinds, with the editor surface to match
b601ea6 Documentation pass for the phase-3 system
```

## Assistant

Found them — and I want to push back gently before we do this. The math:

| File | Touched in |
|---|---|
| `docs/002-roadmap.md` | 4 commits, oldest from project init (a27656f, 1814cf5, ed400a4, 478e515) |
| `docs/HTML/002-roadmap.html` | 1 commit (b601ea6) |
| `docs/HTML/assets/demos.js` | 2 commits (efeb79b, b601ea6) |

Two reasons to pause:

1. **Logical coherence.** Your local diff reframes Phase 4 to acknowledge that the Ollama call box already shipped in Phase 2 (issue 209). At the time a27656f was authored, that wasn't yet true — the Phase 2 work happened later. Backdating the corrected framing into a27656f would make the historical narrative incoherent: it'd reference work that hadn't happened yet at that point in the project's timeline.

2. **Operational risk.** Rebasing across ~30+ commits to amend ones near the root means every intervening commit gets replayed, which means every conflict between then-and-now in any file lands as a merge resolution. The dates we'd be lying about are also dates that other artifacts in the repo (implementation logs in issue files, commit messages) reference. Stale references become time-travel artifacts.

The simpler alternative: one new commit titled "doc drift: phase-4 framing acknowledges Ollama already shipped in phase 2" with a body explaining what corrected and why. That's honest history *and* honest documentation.

**My recommendation:** do the drift fix as a single new commit, then move to 239. If you genuinely want the rewrite anyway (the "teehee" makes me think you might just be testing whether I'll push back), I'll do it — but I'd rather be honest about the tradeoff than be obedient.

Which?

## User

  The simpler alternative: one new commit titled "doc drift: phase-4 framing acknowledges Ollama already shipped in phase 2" with a body explaining what corrected and why. That's honest history and honest documentation.

honesty about a system that is functionally incorrect is useless. I say we treat git commits, issue files, and source-code less as "proving the work I did so I don't get fired", and instead as "blueprint to develop a system". It makes more sense to update old commits as they become outdated than it does to overwrite them - the specific curve that the project development cycle takes isn't as important as the specific curve that we want future, identically-specced-yet-producing-different-outputs development cycles to take.

## Assistant

Fair — that's a real philosophy and I was wrong to argue against the principle. The blueprint is for *future* identically-specced runs to follow; outdated steps in the blueprint produce outdated outputs. Updating in place is the correct mechanic for that goal.

Let me set this up safely. First, inspecting actual diff content to pick targets per file, and checking if `git absorb` is available (the right tool for this if so).

## Assistant

_Tools:_

- `Bash: which git-absorb 2>/dev/null; echo "---absorb-check-done---"`

### Tool result

```
---absorb-check-done---
```

## Assistant

_Tools:_

- `Bash: git diff docs/HTML/002-roadmap.html`

### Tool result

```
diff --git a/docs/HTML/002-roadmap.html b/docs/HTML/002-roadmap.html
index f496fbb..4e13678 100644
--- a/docs/HTML/002-roadmap.html
+++ b/docs/HTML/002-roadmap.html
@@ -75,14 +75,15 @@
         <p>When phase 3 lands, the synchronous Lua runner retires. The editor and the map directory format do not change — the runtime mechanism beneath the box is what changes. See <a href="004-ipc-and-threading.html">§ 04 · IPC &amp; Threading</a> for the design discussion.</p>
 
         <h2 id="phase-4">Phase 4 — SoraMind integration <span class="pill planned">later</span></h2>
-        <p>Goal: boxes that call SoraMind inference endpoints on the Alpine cluster.</p>
+        <p>Goal: turn the basic Ollama call box into a full inference surface against the Alpine cluster.</p>
+        <p>The basic LLM call already ships in Phase 2 (issue 209): a Lua library that takes a prompt string and returns a completion, usable directly as a call box by copying it into a map&rsquo;s <code>src/</code> directory. Phase 4 is the work on top of that:</p>
         <ul>
-          <li>LLM call box type — sends a prompt, receives a completion string</li>
-          <li>Prompt assembly: text values wired from upstream boxes, data file sections explicitly loaded into prompt context</li>
-          <li>Comparator-routed classifiers (&ldquo;rogue / wizard / else&rdquo; via lt/eq/gt on a hashed-class score)</li>
-          <li>Streaming output from SoraMind inference back to the runner</li>
+          <li>Streaming output from the inference endpoint back to the runner, surfacing tokens as they arrive instead of waiting for the full completion</li>
+          <li>Prompt assembly sugar — a dedicated box kind that pulls named sections from data files into a prompt template, rather than the user concatenating strings by hand</li>
+          <li>Multi-host routing — a single logical &ldquo;LLM call&rdquo; that picks among several backing endpoints (laptop Ollama, Alpine cluster, fallback hosts) based on availability or model</li>
+          <li>Comparator-routed classifiers as a worked pattern, not new machinery — comparators already exist in Phase 2; the work here is documenting the rogue / wizard / else style and shipping it as a reusable example</li>
         </ul>
-        <p>Built on the phase 3 runtime — the LLM call is a regular box function, just one whose backing service is a network endpoint.</p>
+        <p>Built on the phase 3 runtime — these LLM boxes are regular box functions, just ones whose backing service is a network endpoint.</p>
 
         <h2 id="phase-5">Phase 5 — Map-to-map calls <span class="pill planned">later</span></h2>
         <p>Goal: a box that invokes another map as a subroutine.</p>
```

## Assistant

_Tools:_

- `Bash: git diff docs/HTML/assets/demos.js`

### Tool result

```
diff --git a/docs/HTML/assets/demos.js b/docs/HTML/assets/demos.js
index 984f6f5..b85b9e5 100644
--- a/docs/HTML/assets/demos.js
+++ b/docs/HTML/assets/demos.js
@@ -787,7 +787,7 @@
       { num: "I",   title: "Foundation",            status: "done",    detail: "Editor + synchronous runner. Issues 101–110 complete." },
       { num: "II",  title: "Editor & graph model",  status: "current", detail: "Map picker, source browser, comparator routing, iterator, compile button. Issues 200-series." },
       { num: "III", title: "Thread pool runtime",   status: "planned", detail: "C pool runner, slot store, language specs, dispatch layer. Issues 301–313." },
-      { num: "IV",  title: "SoraMind integration",  status: "later",   detail: "LLM call boxes against the Alpine cluster. Built on phase 3." },
+      { num: "IV",  title: "SoraMind integration",  status: "later",   detail: "Basic Ollama call lib ships in phase 2 (issue 209). Phase 4 adds streaming, prompt-from-data-section sugar, and multi-host routing on top." },
       { num: "V",   title: "Map-to-map calls",      status: "later",   detail: "A box that invokes another map as a subroutine." },
       { num: "VI",  title: "Remote runner",         status: "later",   detail: "Edit local, run on Alpine. File-CRUD over HTTP." },
     ];
```

## Assistant

_Tools:_

- `Bash: git diff docs/002-roadmap.md | tail -40`

### Tool result

```
-Goal: boxes that call SoraMind inference endpoints on the Alpine
-cluster.
-
-- LLM call box type — sends a prompt, receives a completion string
-- Prompt assembly: text values wired from upstream boxes, data file
-  sections explicitly loaded into prompt context
-- Comparator-routed classifiers ("rogue / wizard / else" via lt/eq/gt
-  on a hashed-class score)
-- Streaming output from SoraMind inference back to the runner
-
-Built on the phase 3 runtime — the LLM call is a regular box function,
-just one whose backing service is a network endpoint.
+Goal: turn the basic Ollama call box into a full inference surface
+against the Alpine cluster.
+
+The basic LLM call already ships in Phase 2 (issue 209): a Lua library
+that takes a prompt string and returns a completion, usable directly
+as a call box by copying it into a map's `src/` directory. Phase 4 is
+the work on top of that:
+
+- Streaming output from the inference endpoint back to the runner,
+  surfacing tokens as they arrive instead of waiting for the full
+  completion
+- Prompt assembly sugar — a dedicated box kind that pulls named
+  sections from data files into a prompt template, rather than the
+  user concatenating strings by hand
+- Multi-host routing — a single logical "LLM call" that picks among
+  several backing endpoints (laptop Ollama, Alpine cluster, fallback
+  hosts) based on availability or model
+- Comparator-routed classifiers as a worked pattern, not new
+  machinery — comparators already exist in Phase 2; the work here is
+  documenting the rogue / wizard / else style and shipping it as a
+  reusable example
+
+Built on the phase 3 runtime — these LLM boxes are regular box
+functions, just ones whose backing service is a network endpoint.
 
 ## Phase 5 — Map-to-map calls
```

## Assistant

_Tools:_

- `Bash: git show --stat efeb79b -- docs/HTML/assets/demos.js && echo "---" && git show efeb79b -- docs/HTML/assets/demos.js |...`

### Tool result

```
commit efeb79b71435fe284a414fce92361361eadede0a
Author: gabrilend <gabrilend@gmail.com>
Date:   Tue May 19 21:35:35 2026 -0700

    229 — read and write box kinds, with the editor surface to match
    
    The old data / file_write kinds always wore the wrong names. A box
    named "data" doesn't say what it does; a sink named "file_write"
    hides that it's a sink in a noun. Rename to verbs — read and write —
    and reshape both to fit the data they actually move.
    
    A read box now emits a value. Sometimes that value lives in a file
    and you give the box a path; sometimes the value is a small literal
    you type in the inspector. Setting an inline value hides the path
    port on the canvas — the box that has no file no longer pretends to
    have one.
    
    A write box no longer disappears the moment it commits to disk. It
    emits the boolean true downstream, so graphs that want to chain on
    "and now the file exists" can fire whatever comes next; graphs that
    don't care drop the boolean per the unwired-output rule. The same
    rule already held informally everywhere; making it visible at the
    sink puts it in writing.
    
    The editor's kind dropdown, the box colours, the header labels, the
    inspector layout, and the architecture doc all line up behind the
    new names. One end-to-end fixture exercises the inline-literal
    path. The cross-language slice — where a Lua table reaches a C
    struct natively across the wire — remains a documented next step,
    because the bridges from 317 and the slot store don't yet share a
    shape. Phase 2 was skipped on purpose; its days are numbered.
    
    Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>

 docs/HTML/assets/demos.js | 4 ++--
 1 file changed, 2 insertions(+), 2 deletions(-)
---
commit efeb79b71435fe284a414fce92361361eadede0a
Author: gabrilend <gabrilend@gmail.com>
Date:   Tue May 19 21:35:35 2026 -0700

    229 — read and write box kinds, with the editor surface to match
    
    The old data / file_write kinds always wore the wrong names. A box
    named "data" doesn't say what it does; a sink named "file_write"
    hides that it's a sink in a noun. Rename to verbs — read and write —
    and reshape both to fit the data they actually move.
    
    A read box now emits a value. Sometimes that value lives in a file
    and you give the box a path; sometimes the value is a small literal
    you type in the inspector. Setting an inline value hides the path
    port on the canvas — the box that has no file no longer pretends to
    have one.
    
    A write box no longer disappears the moment it commits to disk. It
    emits the boolean true downstream, so graphs that want to chain on
    "and now the file exists" can fire whatever comes next; graphs that
    don't care drop the boolean per the unwired-output rule. The same
    rule already held informally everywhere; making it visible at the
    sink puts it in writing.
    
    The editor's kind dropdown, the box colours, the header labels, the
    inspector layout, and the architecture doc all line up behind the
    new names. One end-to-end fixture exercises the inline-literal
    path. The cross-language slice — where a Lua table reaches a C
    struct natively across the wire — remains a documented next step,
    because the bridges from 317 and the slot store don't yet share a
```

## Assistant

_Tools:_

- `Bash: git show --stat a27656f -- docs/002-roadmap.md`

### Tool result

```
commit a27656f90658671ef65d3fd74f0a4b267e6e7ffe
Author: gabrilend <gabrilend@gmail.com>
Date:   Sat May 9 15:09:22 2026 -0700

    Fold the map-compiler ticket into the build-system one; trim the iterator ticket to its editor surface
    
    The map compiler had no phase-2 substance — every dependency lives
    in phase 3 (graph loader, C compile callback, language spec
    interface, Makefile orchestration). Its concrete content (compiled
    directory layout, manifest, mtime incremental, server endpoint, CLI)
    is now part of the build-system ticket where it actually gets built.
    The iterator ticket loses the throwaway phase-2 executor section and
    defers runtime semantics to the dispatch-layer ticket; what remains
    is the editor surface that makes iterator boxes authorable today.
    
    Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>

 docs/002-roadmap.md | 4 ++--
 1 file changed, 2 insertions(+), 2 deletions(-)
```

## Assistant

_Tools:_

- `Bash: git show efeb79b -- docs/HTML/assets/demos.js | tail -20`

### Tool result

```
    Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>

diff --git a/docs/HTML/assets/demos.js b/docs/HTML/assets/demos.js
index 0e943ce..984f6f5 100644
--- a/docs/HTML/assets/demos.js
+++ b/docs/HTML/assets/demos.js
@@ -1180,11 +1180,11 @@ lang_spec_t soramech_lang_spec = {
     const root = svg("svg", { viewBox: `0 0 ${W} ${H}` });
 
     const boxes = [
-      { x: 30,  y: 80,  label: "input",      sub: "data" },
+      { x: 30,  y: 80,  label: "input",      sub: "read" },
       { x: 200, y: 30,  label: "tokenize",   sub: "lua / call" },
       { x: 200, y: 130, label: "fetch",      sub: "bash / call" },
       { x: 380, y: 80,  label: "rank",       sub: "c / call" },
-      { x: 550, y: 30,  label: "log",        sub: "file_write" },
+      { x: 550, y: 30,  label: "log",        sub: "write" },
       { x: 550, y: 130, label: "respond",    sub: "lua / call" },
     ];
     const wires = [[0,1], [0,2], [1,3], [2,3], [3,4], [3,5]];
```

## Assistant

_Tools:_

- `Bash: git show b601ea6 -- docs/HTML/002-roadmap.html | grep -A 2 -B 2 "Phase 4\|phase-4\|SoraMind" | head -20`

### Tool result

```
+        <p>When phase 3 lands, the synchronous Lua runner retires. The editor and the map directory format do not change — the runtime mechanism beneath the box is what changes. See <a href="004-ipc-and-threading.html">§ 04 · IPC &amp; Threading</a> for the design discussion.</p>
+
+        <h2 id="phase-4">Phase 4 — SoraMind integration <span class="pill planned">later</span></h2>
+        <p>Goal: boxes that call SoraMind inference endpoints on the Alpine cluster.</p>
+        <ul>
+          <li>LLM call box type — sends a prompt, receives a completion string</li>
+          <li>Prompt assembly: text values wired from upstream boxes, data file sections explicitly loaded into prompt context</li>
+          <li>Comparator-routed classifiers (&ldquo;rogue / wizard / else&rdquo; via lt/eq/gt on a hashed-class score)</li>
+          <li>Streaming output from SoraMind inference back to the runner</li>
+        </ul>
+        <p>Built on the phase 3 runtime — the LLM call is a regular box function, just one whose backing service is a network endpoint.</p>
```

## Assistant

_Tools:_

- `Bash: git blame -L 68,79 docs/002-roadmap.md`

### Tool result

```
1814cf56 (gabrilend         2026-05-08 23:22:26 -0700 68) 
1814cf56 (gabrilend         2026-05-08 23:22:26 -0700 69) ## Phase 4 — SoraMind integration
1814cf56 (gabrilend         2026-05-08 23:22:26 -0700 70) 
00000000 (Not Committed Yet 2026-05-20 13:56:17 -0700 71) Goal: turn the basic Ollama call box into a full inference surface
00000000 (Not Committed Yet 2026-05-20 13:56:17 -0700 72) against the Alpine cluster.
00000000 (Not Committed Yet 2026-05-20 13:56:17 -0700 73) 
00000000 (Not Committed Yet 2026-05-20 13:56:17 -0700 74) The basic LLM call already ships in Phase 2 (issue 209): a Lua library
00000000 (Not Committed Yet 2026-05-20 13:56:17 -0700 75) that takes a prompt string and returns a completion, usable directly
00000000 (Not Committed Yet 2026-05-20 13:56:17 -0700 76) as a call box by copying it into a map's `src/` directory. Phase 4 is
00000000 (Not Committed Yet 2026-05-20 13:56:17 -0700 77) the work on top of that:
00000000 (Not Committed Yet 2026-05-20 13:56:17 -0700 78) 
00000000 (Not Committed Yet 2026-05-20 13:56:17 -0700 79) - Streaming output from the inference endpoint back to the runner,
```

## Assistant

_Tools:_

- `Bash: git log --pretty=format:"%h %ai %s" 1814cf5 b601ea6 -2 && echo "" && echo "---" && git rev-list --ancestry-path 1814c...`

### Tool result

```
b601ea6 2026-05-12 15:52:37 -0700 Documentation pass for the phase-3 system
3c90364 2026-05-09 20:44:30 -0700 Seed a .gitignore covering tmp, build artifacts, runtime output, and OS junk
---
88
51
```

## Assistant

Targets located. Two commits need amending:
- **1814cf5** (May 8) introduced the roadmap.md Phase 4 section — gets the `.md` diff
- **b601ea6** (May 12) introduced the HTML Phase 4 section and the demos.js Phase IV entry — gets both HTML diffs

efeb79b's demos.js touch was on a *different* line (1180, the read/write rename), so it's not in scope.

Plan: use `git commit --fixup` + `git rebase --autosquash --committer-date-is-author-date` — cleanest way to absorb fixups into specific historical commits with date preservation. Backing up first.

## Assistant

_Tools:_

- `Bash: git tag rewrite-backup-pre-doc-drift && git diff > /tmp/doc-drift.patch && wc -l /tmp/doc-drift.patch && echo "---" &...`

### Tool result

```
86 /tmp/doc-drift.patch
---
Saved working directory and index state WIP on master: db14fb4 217 — variadic toggle gates on the language, not on the function
---
```

## Assistant

_Tools:_

- `Bash: git stash pop && git add docs/002-roadmap.md && git commit --fixup=1814cf5 && git add docs/HTML/002-roadmap.html docs...`

### Tool result

```
On branch master
Changes not staged for commit:
  (use "git add <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   docs/002-roadmap.md
	modified:   docs/HTML/002-roadmap.html
	modified:   docs/HTML/assets/demos.js

no changes added to commit (use "git add" and/or "git commit -a")
Dropped refs/stash@{0} (67b1ec83b15b56e8be1634093c19c62c80e6e204)
[master 8e39ff6] fixup! Specify phase 3 thread pool runtime; align docs and graph model
 1 file changed, 24 insertions(+), 12 deletions(-)
[master fe9c1f4] fixup! Documentation pass for the phase-3 system
 2 files changed, 8 insertions(+), 7 deletions(-)
fe9c1f4 fixup! Documentation pass for the phase-3 system
8e39ff6 fixup! Specify phase 3 thread pool runtime; align docs and graph model
db14fb4 217 — variadic toggle gates on the language, not on the function
```

## Assistant

_Tools:_

- `Bash: GIT_SEQUENCE_EDITOR=: git rebase -i --autosquash --committer-date-is-author-date 1814cf5^ 2>&1 | tail -20`

### Tool result

```
Rebasing (1/91)Rebasing (2/91)Rebasing (3/91)Rebasing (4/91)Rebasing (5/91)Rebasing (6/91)Rebasing (7/91)Rebasing (8/91)Rebasing (9/91)Rebasing (10/91)Rebasing (11/91)Rebasing (12/91)Rebasing (13/91)Rebasing (14/91)Rebasing (15/91)Rebasing (16/91)Rebasing (17/91)Rebasing (18/91)Rebasing (19/91)Rebasing (20/91)Rebasing (21/91)Rebasing (22/91)Rebasing (23/91)Rebasing (24/91)Rebasing (25/91)Rebasing (26/91)Rebasing (27/91)Rebasing (28/91)Rebasing (29/91)Rebasing (30/91)Rebasing (31/91)Rebasing (32/91)Rebasing (33/91)Rebasing (34/91)Rebasing (35/91)Rebasing (36/91)Rebasing (37/91)Rebasing (38/91)Rebasing (39/91)Rebasing (40/91)Rebasing (41/91)Rebasing (42/91)Rebasing (43/91)Rebasing (44/91)Rebasing (45/91)Rebasing (46/91)Rebasing (47/91)Rebasing (48/91)Rebasing (49/91)Rebasing (50/91)Rebasing (51/91)Rebasing (52/91)Rebasing (53/91)Rebasing (54/91)Rebasing (55/91)Rebasing (56/91)Rebasing (57/91)Rebasing (58/91)Rebasing (59/91)Rebasing (60/91)Rebasing (61/91)Rebasing (62/91)Rebasing (63/91)Rebasing (64/91)Rebasing (65/91)Rebasing (66/91)Rebasing (67/91)Rebasing (68/91)Rebasing (69/91)Rebasing (70/91)Rebasing (71/91)Rebasing (72/91)Rebasing (73/91)Rebasing (74/91)Rebasing (75/91)Rebasing (76/91)Rebasing (77/91)Rebasing (78/91)Rebasing (79/91)Rebasing (80/91)Rebasing (81/91)Rebasing (82/91)Rebasing (83/91)Rebasing (84/91)Rebasing (85/91)Rebasing (86/91)Rebasing (87/91)Rebasing (88/91)Rebasing (89/91)Rebasing (90/91)Rebasing (91/91)Successfully rebased and updated refs/heads/master.
```

## Assistant

_Tools:_

- `Bash: git log --oneline -10 && echo "---" && git status && echo "---" && git log --pretty=format:"%h %ai %ci %s" -1 $(git l...`

## Assistant

_Tools:_

- `Bash: git show --stat $(git log --format=%H -- docs/002-roadmap.md | head -1) -- docs/002-roadmap.md | head -3 && echo "---...`

### Tool result

```
a46be72 217 — variadic toggle gates on the language, not on the function
d558452 314 — the project's own small C JSON parser and writer
9f5c118 310 — priority parameter wired through the spawn surface
fdbe39e 309 — top-level build and the portable-artifact pipeline
77b9462 308 — bash boxes through a persistent socket server, no per-call spawn
1b0a024 303 — pluggable language-runtime spec, equal footing for every language
a4f7c39 301 — thread-pool lifecycle and per-worker initialization
c70646d 231 — language-specific signature parsers move into langs/
9c2ebb3 229 — read and write box kinds, with the editor surface to match
915d85e 317 stage 2 — Lua arrays preserve every JSON shape
---
On branch master
nothing to commit, working tree clean
---
61f8e39 2026-05-09 15:09:22 -0700 2026-05-09 15:09:22 -0700 Fold the map-compiler ticket into the build-system one; trim the iterator ticket to its editor surface
```

### Tool result

```
commit 61f8e396a727e8034e74696a676956cc7c5a28a9
Author: gabrilend <gabrilend@gmail.com>
Date:   Sat May 9 15:09:22 2026 -0700
---
9ade55e 2026-05-12 15:52:37 -0700 2026-05-12 15:52:37 -0700 Documentation pass for the phase-3 system
```

## Assistant

_Tools:_

- `Bash: git log --pretty=format:"%h %ai %s" -- docs/002-roadmap.md`

### Tool result

```
61f8e39 2026-05-09 15:09:22 -0700 Fold the map-compiler ticket into the build-system one; trim the iterator ticket to its editor surface
f642680 2026-05-08 23:22:26 -0700 Specify phase 3 thread pool runtime; align docs and graph model
ed400a4 2026-05-08 16:42:34 -0700 Close completed issues; add IPC and threading design doc
478e515 2026-05-07 15:53:41 -0700 Initialize SoraMech: vision, architecture, and phase 1 issue plan
```

## Assistant

_Tools:_

- `Bash: git log --oneline --grep="phase 3 thread pool runtime" | head -3`

### Tool result

```
f642680 Specify phase 3 thread pool runtime; align docs and graph model
```

## Assistant

_Tools:_

- `Bash: git show f642680 -- docs/002-roadmap.md | grep -A 3 "Phase 4" | head -10`

### Tool result

```
-## Phase 4 — Map-to-map calls
+- Open the editor at `http://localhost:7700/`, see a canvas, create
+  boxes, set their `ref` / `fn` / `inputs`, wire them together
+- Save edits as box files on disk via the HTTP file CRUD server
--
+## Phase 4 — SoraMind integration
+
+Goal: turn the basic Ollama call box into a full inference surface
+against the Alpine cluster.
--
```

## Assistant

_Tools:_

- `Bash: git show 9ade55e -- docs/HTML/002-roadmap.html | grep -A 1 "Goal:" | head -4 && echo "---demos---" && git show 9ade55...`

### Tool result

```
+        <p>Goal: a map you can edit in the browser and execute from the terminal.</p>
+        <p>At the end of phase 1, the user can:</p>
--
+        <p>Goal: an editor you can actually use to build maps, and the graph model the phase 3 runtime will consume.</p>
---demos---
+      { num: "IV",  title: "SoraMind integration",  status: "later",   detail: "Basic Ollama call lib ships in phase 2 (issue 209). Phase 4 adds streaming, prompt-from-data-section sugar, and multi-host routing on top." },
```

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -15`

### Tool result

```
integration tests (fixture maps):
  calc                                         ok
  hello                                        ok
  comparator                                   ok
  iter-route                                   ok
  pipeline                                     ok
  read-literal                                 ok
  pipeline write output                        ok
  compile pipeline (portable run)              ok
  232-lua-parser-test.mjs                      ok
  233-bash-parser-test.mjs                     ok
  234-language-spec-js-test.mjs                ok

  11 passed, 0 failed
rm /mnt/mtwo/programs/sora/soramech/build/tests/308-bash-spec-test.o /mnt/mtwo/programs/sora/soramech/build/tests/010-graph-loader-test.o /mnt/mtwo/programs/sora/soramech/build/tests/306-lua-spec-test.o /mnt/mtwo/programs/sora/soramech/build/tests/016-unified-allocator-test.o /mnt/mtwo/programs/sora/soramech/build/tests/317-lua-bridge-test.o /mnt/mtwo/programs/sora/soramech/build/tests/009-slot-store-test.o /mnt/mtwo/programs/sora/soramech/build/tests/307-c-spec-test.o /mnt/mtwo/programs/sora/soramech/build/tests/301-pool-test.o /mnt/mtwo/programs/sora/soramech/build/tests/012-dispatch-test.o /mnt/mtwo/programs/sora/soramech/build/tests/303-pool-spec-init-test.o /mnt/mtwo/programs/sora/soramech/build/tests/011-spec-registry-test.o /mnt/mtwo/programs/sora/soramech/build/tests/013-jsonl-events-test.o /mnt/mtwo/programs/sora/soramech/build/tests/014-event-queue-test.o /mnt/mtwo/programs/sora/soramech/build/tests/015-large-value-heap-test.o /mnt/mtwo/programs/sora/soramech/build/tests/314-json-test.o
```

## Assistant

Rebase landed cleanly. Verification:

- **f642680** (the rebased 1814cf5, date preserved at `2026-05-08 23:22:26`) now contains the corrected Phase 4 framing in `docs/002-roadmap.md`.
- **9ade55e** (the rebased b601ea6, date preserved at `2026-05-12 15:52:37`) now contains the corrected Phase 4 HTML section + the updated demos.js entry.
- Working tree clean, full test suite green (22 + 5 + 11), backup tag `rewrite-backup-pre-doc-drift` left in place if you want to inspect what changed.

On the argument point — taken. I'll keep pushing back when something feels off rather than retreating once you state a preference. The blueprint philosophy *did* update my view; that's the right way for an argument to end. Capitulating because you re-asserted is not.

---

Onto **239** — literal-bound port nodule rendering. Reading the relevant code first.

## Assistant

_Tools:_

- `Bash: grep -n "draw_box\|hit_test_port\|port_label_text\|literal\|p\.value" /mnt/mtwo/programs/sora/soramech/assets/js/002-...`

### Tool result

```
21:  // Issue 235: input ports with a literal value show the value on the
79:  // runtime; any literal value sitting on the same port is dormant.
92:  // {{{ port_label_text
100:  function port_label_text(box, p) {
101:    const has_value = p.value !== undefined && p.value !== null && p.value !== '';
104:    const raw        = String(p.value);
116:  // Issue 229: a `read` box with an inline literal `value` set has no
202:  // {{{ draw_box
203:  function draw_box(ctx, box, selected) {
268:      // What text we draw depends on whether a literal value is set
274:      ctx.fillText(port_label_text(box, box.inputs.find(q => q.name === p.name)),
309:      draw_box(ctx, boxes[id], id === selected_id);
314:  // {{{ hit_test_port
317:  function hit_test_port(wx, wy) {
356:  // shortened version of its literal value (multi-line collapse or
367:    const has_value = p.value !== undefined && p.value !== null && p.value !== '';
370:    const raw = String(p.value);
371:    return raw === port_label_text(box, p) ? null : raw;
378:    draw_all, draw_box, hit_test_port, hit_test_box, get_port_world_pos,
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

### Tool result

```
75	  // }}}
76	
77	  // {{{ port_has_incoming_wire
78	  // A wire on a port is the source of truth for that port's value at
79	  // runtime; any literal value sitting on the same port is dormant.
80	  // Issue 235 surfaces this by deciding the canvas label — port name
81	  // when wired (so the reader doesn't mistake the dormant value for
82	  // what's flowing through), value otherwise.
83	  function port_has_incoming_wire(box, port_name) {
84	    if (!box.connections) return false;
85	    for (const c of box.connections) {
86	      if (c.to_box === box.id && c.to_input === port_name) return true;
87	    }
88	    return false;
89	  }
90	  // }}}
91	
92	  // {{{ port_label_text
93	  // Picks the string drawn next to an input port dot. Three cases:
94	  //   - wire attached, or no value → port name (parameter identity)
95	  //   - value set, no wire         → the value (what the box does)
96	  //   - value too long / multiline → first line, truncated with …
97	  // The two-branch decision is the visible heart of issue 235: a
98	  // reader scanning the map should see the data the box operates on,
99	  // not the parameter slot it sits in.
100	  function port_label_text(box, p) {
101	    const has_value = p.value !== undefined && p.value !== null && p.value !== '';
102	    if (!has_value)                              return p.name;
103	    if (port_has_incoming_wire(box, p.name))     return p.name;
104	    const raw        = String(p.value);
105	    const first_line = raw.split('\n')[0];
106	    const multiline  = first_line.length < raw.length;
107	    if (first_line.length > MAX_LABEL_CHARS) {
108	      return first_line.slice(0, MAX_LABEL_CHARS - 1) + '…';
109	    }
110	    if (multiline) return first_line + '…';
111	    return first_line;
112	  }
113	  // }}}
114	
115	  // {{{ visible_inputs
116	  // Issue 229: a `read` box with an inline literal `value` set has no
117	  // file to read, so its `path` input port doesn't belong on the
118	  // canvas — showing one would imply "wire me to a path" and confuse
119	  // the reader. Inspector still owns the path field; the canvas just
120	  // hides the port. Every other layout path (height, port positions,
121	  // hit testing) goes through this helper so the box reads
122	  // consistently.
123	  function visible_inputs(box) {
124	    const all = box.inputs || [];
125	    if (box.kind === 'read' && box.value !== undefined
126	        && box.value !== null && box.value !== '') {
127	      return all.filter(p => p.name !== 'path');
128	    }
129	    return all;
130	  }
131	  // }}}
132	
133	  // {{{ box_height
134	  function box_height(box) {
135	    const n_in  = visible_inputs(box).length;
136	    // Output rows: iterator boxes have N named slots (issue 221);
137	    // comparator active: 3 dots (lt/eq/gt); sink: 0; otherwise 1.
138	    let n_out;
139	    if (box.iterator_outputs) {
140	      n_out = box.iterator_outputs.length;
141	    } else if (box.has_output === false) {
142	      n_out = 0;
143	    } else if (box.comparand && box.comparand !== '') {
144	      n_out = 3;
145	    } else {
146	      n_out = 1;
147	    }
148	    return Math.max(MIN_BOX_H, BOX_HEADER + Math.max(n_in, n_out) * PORT_ROW_H + 8);
149	  }
150	  // }}}
151	
152	  // {{{ port_positions
153	  // Returns world-space {x,y} for each port dot on a box.
154	  // Output side: null-named single dot when no comparand; 'lt'/'eq'/'gt' dots when comparand set.
155	  function port_positions(box) {
156	    const bx = box.ui?.x ?? 0;
157	    const by = box.ui?.y ?? 0;
158	    const h  = box_height(box);
159	    const inputs = visible_inputs(box);
160	
161	    const in_pts = inputs.map((p, i) => ({
162	      name: p.name, side: 'input',
163	      x: bx,
164	      y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
165	    }));
166	
167	    let out_pts;
168	    if (box.has_output === false) {
169	      // sink box — function has no return values (issue 226). No output
170	      // port, no wire-grab target, nothing for the inspector to render.
171	      out_pts = [];
172	    } else if (box.iterator_outputs) {
173	      // iterator box (issue 221) — N named output dots stacked down
174	      // the right edge, one per declared slot. Slot names land in the
175	      // `name` field, so the wire's `from_branch` will pick them up
176	      // exactly like a comparator branch name.
177	      out_pts = box.iterator_outputs.map((slot, i) => ({
178	        name: slot, side: 'output',
179	        x: bx + BOX_W,
180	        y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
181	      }));
182	    } else if (box.comparand && box.comparand !== '') {
183	      // three comparator dots
184	      out_pts = ['lt', 'eq', 'gt'].map((branch, i) => ({
185	        name: branch, side: 'output',
186	        x: bx + BOX_W,
187	        y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
188	      }));
189	    } else {
190	      // single output dot, vertically centered in the box
191	      out_pts = [{
192	        name: null, side: 'output',
193	        x: bx + BOX_W,
194	        y: by + h / 2,
195	      }];
196	    }
197	
198	    return { inputs: in_pts, outputs: out_pts };
199	  }
200	  // }}}
201	
202	  // {{{ draw_box
203	  function draw_box(ctx, box, selected) {
204	    const x = box.ui?.x ?? 0;
205	    const y = box.ui?.y ?? 0;
206	    const w = BOX_W;
207	    const h = box_height(box);
208	    const kind  = box.kind || 'call';
209	    const color = KIND_COLOR[kind] || '#888';
210	
211	    // shadow when selected
212	    if (selected) {
213	      ctx.shadowColor = color;
214	      ctx.shadowBlur  = 12;
215	    }
216	
217	    // body
218	    ctx.fillStyle   = '#1e2130';
219	    ctx.strokeStyle = selected ? color : '#3a3f55';
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

### Tool result

```
220	    ctx.lineWidth   = selected ? 2 : 1;
221	    ctx.beginPath();
222	    ctx.roundRect(x, y, w, h, 6);
223	    ctx.fill();
224	    ctx.stroke();
225	
226	    ctx.shadowBlur = 0;
227	
228	    // header bar
229	    ctx.fillStyle = color + '33';
230	    ctx.beginPath();
231	    ctx.roundRect(x, y, w, BOX_HEADER, [6, 6, 0, 0]);
232	    ctx.fill();
233	
234	    // label — user override wins; otherwise the kind-driven default
235	    // (issue 236): basename of ref for call boxes, plain kind name
236	    // for everything else.
237	    ctx.fillStyle  = '#e8eaf6';
238	    ctx.font       = 'bold 12px monospace';
239	    ctx.textAlign  = 'left';
240	    ctx.textBaseline = 'middle';
241	    ctx.fillText(box.label || default_header(box), x + 10, y + BOX_HEADER / 2);
242	
243	    // kind badge (top right)
244	    ctx.fillStyle  = color;
245	    ctx.font       = '9px monospace';
246	    ctx.textAlign  = 'right';
247	    ctx.fillText(kind.toUpperCase(), x + w - 8, y + BOX_HEADER / 2);
248	
249	    // port dots and labels
250	    const { inputs: in_pts, outputs: out_pts } = port_positions(box);
251	
252	    ctx.font      = '10px monospace';
253	    ctx.fillStyle = '#9ea3c0';
254	
255	    in_pts.forEach((p, i) => {
256	      ctx.beginPath();
257	      ctx.arc(p.x, p.y, PORT_R, 0, Math.PI * 2);
258	      ctx.fillStyle   = '#2d3250';
259	      ctx.strokeStyle = '#6c72a0';
260	      ctx.lineWidth   = 1.5;
261	      ctx.fill();
262	      ctx.stroke();
263	
264	      // Label rendered into the canvas (issue 224 reverted the DOM-
265	      // overlay experiment — a click-capturing <input> next to the dot
266	      // was capturing drag attempts). Drawn in world space, so it
267	      // scales with zoom. Editing still happens in the inspector.
268	      // What text we draw depends on whether a literal value is set
269	      // and whether a wire occupies the port (issue 235).
270	      ctx.fillStyle  = '#9ea3c0';
271	      ctx.font       = '10px monospace';
272	      ctx.textAlign  = 'left';
273	      ctx.textBaseline = 'middle';
274	      ctx.fillText(port_label_text(box, box.inputs.find(q => q.name === p.name)),
275	                   p.x + PORT_R + 4, p.y);
276	    });
277	
278	    out_pts.forEach(p => {
279	      // comparator branches use distinct colors; single output uses kind color
280	      const dot_color = p.name ? (BRANCH_COLOR[p.name] || color) : color;
281	      ctx.beginPath();
282	      ctx.arc(p.x, p.y, PORT_R, 0, Math.PI * 2);
283	      ctx.fillStyle   = dot_color + '44';
284	      ctx.strokeStyle = dot_color;
285	      ctx.lineWidth   = 1.5;
286	      ctx.fill();
287	      ctx.stroke();
288	
289	      // Output label rendered to the LEFT of the dot. Branch names
290	      // ('lt'/'eq'/'gt') and iterator slot names come straight from
291	      // p.name; a call box's single output gets `fn()` synthesized
292	      // (issue 236) so the wire reads as the return value of
293	      // calling the function.
294	      const out_label = output_label_text(box, p);
295	      if (out_label) {
296	        ctx.fillStyle  = dot_color;
297	        ctx.font       = '9px monospace';
298	        ctx.textAlign  = 'right';
299	        ctx.textBaseline = 'middle';
300	        ctx.fillText(out_label, p.x - PORT_R - 4, p.y);
301	      }
302	    });
303	  }
304	  // }}}
305	
306	  // {{{ draw_all
307	  function draw_all(ctx, selected_id) {
308	    for (const id in boxes) {
309	      draw_box(ctx, boxes[id], id === selected_id);
310	    }
311	  }
312	  // }}}
313	
314	  // {{{ hit_test_port
315	  // Returns {box_id, port_name, side, x, y} for the port under world point (wx,wy), or null.
316	  // For output dots: port_name is null (single wire) or 'lt'/'eq'/'gt' (comparator).
317	  function hit_test_port(wx, wy) {
318	    for (const id in boxes) {
319	      const { inputs, outputs } = port_positions(boxes[id]);
320	      for (const p of [...inputs, ...outputs]) {
321	        const dx = wx - p.x, dy = wy - p.y;
322	        if (dx*dx + dy*dy <= (PORT_R + 4) * (PORT_R + 4)) {
323	          return { box_id: id, port_name: p.name, side: p.side, x: p.x, y: p.y };
324	        }
325	      }
326	    }
327	    return null;
328	  }
329	  // }}}
```

## Assistant

_Tools:_

- `Bash: grep -n "PORT_R\s*=\|MAX_LABEL_CHARS\|BOX_W\s*=" /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js | head -10`

### Tool result

```
5:  const BOX_W         = 180;
8:  const PORT_R        = 6;   // port dot radius in world units
23:  // BOX_W=180 at 10px monospace; anything longer truncates with an
25:  const MAX_LABEL_CHARS = 24;
107:    if (first_line.length > MAX_LABEL_CHARS) {
108:      return first_line.slice(0, MAX_LABEL_CHARS - 1) + '…';
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

### Tool result

```
1	// Box rendering, layout, and hit testing.
2	// Each box knows its world-space position; the camera transform is applied by the canvas.
3	
4	const Boxes = (() => {
5	  const BOX_W         = 180;
6	  const BOX_HEADER    = 36;
7	  const PORT_ROW_H    = 22;
8	  const PORT_R        = 6;   // port dot radius in world units
9	  const MIN_BOX_H     = 80;
10	
11	  // KIND_COLOR drives the box header colour on the canvas. `read`
12	  // keeps the green that data boxes used to wear; `write` takes a
13	  // warm cheddar so source-of-value and sink-to-disk read as
14	  // clearly different at a glance (issue 229).
15	  const KIND_COLOR = {
16	    call:  '#4a9eff',
17	    read:  '#4caf7d',
18	    write: '#d4762a',
19	  };
20	
21	  // Issue 235: input ports with a literal value show the value on the
22	  // canvas in place of the port name. ~24 chars fits comfortably in
23	  // BOX_W=180 at 10px monospace; anything longer truncates with an
24	  // ellipsis. Multi-line values collapse to their first line.
25	  const MAX_LABEL_CHARS = 24;
26	
27	  // Issue 236: default header text per box kind, used when the user
28	  // has not set an explicit label. `call` is the special case —
29	  // basename of its `ref` is the answer, so we resolve it at draw
30	  // time. Add a row when a new kind ships.
```

## Assistant

Implementing the nodule + gutter rendering now.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js has been updated successfully.
```

## Assistant

Now the rendering change in `draw_box`:

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

Now the hit-test exclusion:

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: node --check /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js && echo "boxes.js syntax ok"`

### Tool result

```
boxes.js syntax ok
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/tests/ | grep -i "235\|236\|239\|canvas\|boxes" 2>&1 | head -10`

### Tool result

```
(Bash completed with no output)
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: grep -n "^|" /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

### Tool result

```
8:| ID   | Title                                          | Status   |
9:|------|------------------------------------------------|----------|
10:| 201  | fix box rendering closure bug                  | complete |
11:| 202  | fit-to-view on map load                        | complete |
12:| 203  | map picker panel                               | complete |
13:| 204  | visible interaction hints                      | complete |
14:| 206  | entry box designation                          | won't implement |
15:| 207  | source file browser and port auto-population   | complete |
16:| 208  | port literal values                            | complete |
17:| 209  | ollama query library                           | complete |
18:| 210  | comparator wire branching                      | complete |
19:| 211  | file browser library directories               | complete |
20:| 211a | unify src/ into same directory pipeline        | complete |
21:| 212  | editor interaction modes                       | complete |
22:| 213  | queued inputs and task model                   | won't implement |
23:| 219  | map compiler                                   | won't implement (folded into 309) |
24:| 214  | tmp symlink recreation on reboot               | complete |
25:| 215  | view source button                             | complete |
26:| 223  | syntax highlighting via user-written lexers    | complete |
27:| 224  | editable port names (canvas) — values stay in inspector | complete |
28:| 225  | dir picker shows files and dir/file icons      | complete |
29:| 226  | hide output controls for no-return functions   | complete |
30:| 227  | hide view button when box has no ref           | complete |
31:| 228  | self-loop wires routed around the box body     | complete |
32:| 229  | `read` / `write` box kinds (language-agnostic IO) | complete (phase 3 + editor; phase 2 mirror skipped per teardown) |
33:| 230  | required inputs and the optional flag           | complete (compile-time; runtime arg-list shortening deferred) |
34:| 231  | move signature parsers into langs/              | complete |
35:| 232  | test coverage map (parser tests landed; tracks gaps) | ongoing |
36:| 233  | unified routing schema (comparator+iterator)    | open     |
37:| 234  | middle-click duplicates the selected box        | complete |
38:| 235  | literal value replaces port name on the canvas  | complete |
39:| 236  | box header = filename, output label = `fn()`    | complete |
40:| 237  | string-manipulation library expansion           | complete |
41:| 238  | editor HTTP server config file                  | complete |
42:| 216  | read-file box                                  | complete |
43:| 217  | variadic ports and the language-spec variadic shape | complete |
44:| 218  | enforce single-output driver contract          | complete |
45:| 220  | root run script and entry point cleanup        | complete |
46:| 221  | iterator box (editor surface only)             | complete |
47:| 222  | compile button and assets directory            | complete |
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: git mv issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md issues/completed/ && git add assets/js/002-boxe...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -15`

### Tool result

```
M  assets/js/002-boxes.js
R  issues/239-literal-port-shrinks-to-nodule-with-name-on-left.md -> issues/completed/239-literal-port-shrinks-to-nodule-with-name-on-left.md
M  issues/phase-2-progress.md
```

### Tool result

```
integration tests (fixture maps):
  calc                                         ok
  hello                                        ok
  comparator                                   ok
  iter-route                                   ok
  pipeline                                     ok
  read-literal                                 ok
  pipeline write output                        ok
  compile pipeline (portable run)              ok
  232-lua-parser-test.mjs                      ok
  233-bash-parser-test.mjs                     ok
  234-language-spec-js-test.mjs                ok

  11 passed, 0 failed
```

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
239 — literal-bound input ports render as a nodule, name in the gutter

Before this ch...`

### Tool result

```
[master a2d9df4] 239 — literal-bound input ports render as a nodule, name in the gutter
 3 files changed, 116 insertions(+), 45 deletions(-)
 rename issues/{ => completed}/239-literal-port-shrinks-to-nodule-with-name-on-left.md (71%)
```

## Assistant

_Tools:_

- `Bash: git log --oneline -3 && echo "---open issues---" && ls /mnt/mtwo/programs/sora/soramech/issues/ | grep -v completed |...`

### Tool result

```
a2d9df4 239 — literal-bound input ports render as a nodule, name in the gutter
a46be72 217 — variadic toggle gates on the language, not on the function
d558452 314 — the project's own small C JSON parser and writer
---open issues---
232-test-coverage-map.md
233-unified-routing-schema.md
302-wire-value-slot-store.md
304-task-dispatch-layer.md
305-c-graph-loader.md
306-lua-language-spec.md
307-c-language-spec.md
311-integration-tests-and-run-output.md
312-same-language-wire-fast-path.md
313-research-whole-program-same-language-merge.md
315-compiled-artifact-reference-counting.md
317-spec-json-bridge-for-data-boxes.md
```

## Assistant

239 closed in commit `a2d9df4` — full trinity (source + issue + progress doc), make test green.

**12 open issues remaining:**

- Phase 2: 232, 233
- Phase 3: 302, 304, 305, 306, 307, 311, 312, 313, 315, 317

Two notes from this round:

- **Visual verification still pending** — same caveat as 217. I did not load a browser to confirm the nodule actually renders as I expect. The hit-test exclusion is testable from logic alone (and is straightforward), but the visual proportions (NODULE_R = 3, gutter clip = 12 chars) are guesses that only a real screen can confirm. Could plausibly need tuning.

- **The "tests" step from 239's plan didn't land** — because there was no 235 case set to extend in the first place. I closed without canvas-rendering tests and noted it in the issue file + commit. 232's coverage map already flags this gap. If you want canvas tests as a hard precondition for closure, 239 should reopen.

Three plausible next moves, ranked by smallest payoff-per-effort:

1. **232** — write `scripts/run-unit-tests.sh` per the issue's spec. Mechanical, well-defined, ~30 min. Closes one issue and unblocks faster dev-loop iteration.
2. **233** — schema migration (Lua `001-schema.lua` + maps) and inspector mode-dropdown for unified routing. Medium-sized; dispatch already runs against the unified semantics, so this is purely the editor/schema side.
3. **Pick one of the design-question issues** (302, 307, 312) — these reshape downstream work. Worth resolving before any code touches them.

Which?

## User

  - Visual verification still pending — same caveat as 217. I did not load a browser to confirm the nodule actually renders as I expect. The hit-test exclusion is testable from logic alone (and is straightforward), but the visual proportions (NODULE_R = 3, gutter clip = 12 chars) are
   guesses that only a real screen can confirm. Could plausibly need tuning.

can you recommend me an open source tool that can be driven by the CLI and produces screenshots and applies input events, so that you can test that sort of thing? Is there anything that enables that functionality for you?

let's work on 233

## Assistant

**On the screenshot tool**: Playwright is the right pick — MIT-licensed, single-binary install (`npm i -g playwright` then `playwright install chromium`), headless by default, one-line APIs for both screenshots (`page.screenshot({ path: 'x.png' })`) and input events (`page.click()`, `page.fill()`, keyboard events). It would let me actually drive the editor in the background and post screenshots back into the loop. The cost is the install (~200 MB of browser binaries) and the test scaffolding. If you want this set up, I'll do it as its own small issue+commit so the install is captured properly. Worth saving for after 233 unless you'd rather do it now to immediately verify 217 + 239 + the 233 inspector dropdown.

---

**Onto 233.** First reading what's currently there.

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/src/001-schema.lua`

### Tool result

```
1	-- Schema definitions and validators for SoraMech map files.
2	-- Validates box files, meta.json, and drivers.json against the spec
3	-- in docs/001-architecture.md. Used by both the runner and the server
4	-- before any file is written to disk.
5	
6	local DIR = "/mnt/mtwo/programs/sora/soramech"
7	
8	local M = {}
9	
10	-- {{{ valid_kinds
11	-- `call` runs a language function. `read` and `write` are dispatch-
12	-- layer IO primitives (issue 229) — `read` emits an inline literal
13	-- or file contents, `write` commits a `value` to disk and emits a
14	-- boolean done signal.
15	local valid_kinds = { call = true, read = true, write = true }
16	-- }}}
17	
18	-- {{{ valid_branches
19	-- null/nil means no comparator (single wire); the three strings are comparator outputs.
20	local valid_branches = { lt = true, eq = true, gt = true }
21	-- }}}
22	
23	-- {{{ err
24	local function err(errors, msg)
25	    errors[#errors + 1] = msg
26	end
27	-- }}}
28	
29	-- {{{ validate_connection
30	local function validate_connection(c, i, errors)
31	    -- from_branch is nil (no comparator) or one of "lt"/"eq"/"gt"
32	    if c.from_box == nil then
33	        err(errors, "connection[" .. i .. "] missing 'from_box'")
34	    end
35	    if c.from_branch ~= nil and not valid_branches[c.from_branch] then
36	        err(errors, "connection[" .. i .. "] 'from_branch' must be nil, 'lt', 'eq', or 'gt'")
37	    end
38	    if c.to_box == nil then
39	        err(errors, "connection[" .. i .. "] missing 'to_box'")
40	    end
41	    if c.to_input == nil then
42	        err(errors, "connection[" .. i .. "] missing 'to_input'")
43	    end
44	end
45	-- }}}
46	
47	-- {{{ validate_box
48	function M.validate_box(box)
49	    local errors = {}
50	    if type(box) ~= "table" then
51	        return { "box must be a table" }
52	    end
53	
54	    if box.id == nil or type(box.id) ~= "string" then
55	        err(errors, "missing or non-string 'id'")
56	    end
57	    if box.label == nil or type(box.label) ~= "string" then
58	        err(errors, "missing or non-string 'label'")
59	    end
60	    if box.kind == nil or not valid_kinds[box.kind] then
61	        err(errors, "missing or invalid 'kind' (must be 'call', 'read', or 'write')")
62	    end
63	
64	    if box.kind == "call" then
65	        -- iterator boxes (issue 221) are a pure routing primitive: input
66	        -- copies straight to a chosen output, no language function is
67	        -- invoked. Such a box has no ref/fn — only `iterator_outputs`,
68	        -- an ordered list of output slot names. Otherwise ref is required.
69	        local is_iterator = box.iterator_outputs ~= nil
70	        if not is_iterator then
71	            if box.ref == nil or type(box.ref) ~= "string" then
72	                err(errors, "box missing 'ref'")
73	            end
74	        end
75	        if box.iterator_outputs ~= nil then
76	            if type(box.iterator_outputs) ~= "table" then
77	                err(errors, "'iterator_outputs' must be an array")
78	            else
79	                for i, name in ipairs(box.iterator_outputs) do
80	                    if type(name) ~= "string" then
81	                        err(errors, "'iterator_outputs[" .. i .. "]' must be a string")
82	                    end
83	                end
84	            end
85	        end
86	        -- fn is optional for binary callers (shell scripts, binaries)
87	        if box.inputs ~= nil and type(box.inputs) ~= "table" then
88	            err(errors, "'inputs' must be an array")
89	        end
90	        -- variadic_inputs: optional list of base names; each name must
91	        -- correspond to a series of inputs named "<base>_<index>" in
92	        -- box.inputs (the editor maintains this; we only check the
93	        -- shape here). See issue 217 part B.
94	        if box.variadic_inputs ~= nil then
95	            if type(box.variadic_inputs) ~= "table" then
96	                err(errors, "'variadic_inputs' must be an array")
97	            else
98	                for i, name in ipairs(box.variadic_inputs) do
99	                    if type(name) ~= "string" then
100	                        err(errors, "'variadic_inputs[" .. i .. "]' must be a string")
101	                    end
102	                end
103	            end
104	        end
105	        -- comparand: optional; if present it must be a string parseable as number at runtime
106	        if box.comparand ~= nil and type(box.comparand) ~= "string" then
107	            err(errors, "'comparand' must be a string")
108	        end
109	        -- has_output: optional; absence means "true" (default behavior).
110	        -- false marks a sink box whose function has no return values
111	        -- (issue 226); the runtime treats such boxes as terminal and the
112	        -- editor hides the output port and inspector output section.
113	        if box.has_output ~= nil and type(box.has_output) ~= "boolean" then
114	            err(errors, "'has_output' must be a boolean")
115	        end
116	        -- Per-port `optional` flag (issue 230). Absent means false: the
117	        -- port is required and must be either wired or carry a literal
118	        -- value. The wire/literal/optional triple-check happens at the
119	        -- graph level (validate-map.lua and the editor's compile path),
120	        -- not here — this only validates per-port shape.
121	        if box.inputs ~= nil and type(box.inputs) == "table" then
122	            for i, p in ipairs(box.inputs) do
123	                if type(p) == "table" and p.optional ~= nil and type(p.optional) ~= "boolean" then
124	                    err(errors, "'inputs[" .. i .. "].optional' must be a boolean")
125	                end
126	            end
127	        end
128	        if box.connections ~= nil then
129	            for i, c in ipairs(box.connections) do
130	                validate_connection(c, i, errors)
131	            end
132	        end
133	    end
134	
135	    -- read / write: dispatch-layer IO primitives (issue 229). Shape
136	    -- check is intentionally permissive — the graph loader does the
137	    -- "has a value or a path" semantic check.
138	    if box.kind == "read" or box.kind == "write" then
139	        if box.value ~= nil and type(box.value) ~= "string" then
140	            err(errors, "'value' must be a string")
141	        end
142	        if box.path ~= nil and type(box.path) ~= "string" then
143	            err(errors, "'path' must be a string")
144	        end
145	        if box.inputs ~= nil and type(box.inputs) ~= "table" then
146	            err(errors, "'inputs' must be an array")
147	        end
148	        if box.connections ~= nil then
149	            for i, c in ipairs(box.connections) do
150	                validate_connection(c, i, errors)
151	            end
152	        end
153	    end
154	
155	    if box.ui ~= nil then
156	        if type(box.ui) ~= "table" then
157	            err(errors, "'ui' must be a table")
158	        elseif box.ui.x == nil or box.ui.y == nil then
159	            err(errors, "'ui' must have 'x' and 'y' fields")
160	        end
161	    end
162	
163	    return errors
164	end
165	-- }}}
166	
167	-- {{{ check_input_bindings
168	-- Graph-level check from issue 230: every input port on every box
169	-- must be in one of three states by compile time —
170	--   1. a wire is connected to it
171	--   2. a literal `value` is set on it
172	--   3. it carries `optional: true`
173	-- Otherwise the box would silently receive a missing arg at runtime
174	-- and the bug would only surface where the function happens to
175	-- reference it. The check fails loud and points at the exact box +
176	-- port instead.
177	--
178	-- `boxes` is a table mapping box-id → box record (the shape the
179	-- editor cache and the phase 2 loader's graph use). Returns an
180	-- array of error strings; empty array means OK.
181	function M.check_input_bindings(boxes)
182	    local errors = {}
183	
184	    -- precompute: for each (to_box, to_input) → true if a wire targets it.
185	    -- Connections live on both endpoints (issue 228), so scanning every
186	    -- box's connection list overcounts but doesn't miss anything.
187	    local wired = {}
188	    for _, box in pairs(boxes) do
189	        for _, c in ipairs(box.connections or {}) do
190	            if c.to_box and c.to_input then
191	                wired[c.to_box .. "\0" .. c.to_input] = true
192	            end
193	        end
194	    end
195	
196	    -- stable per-box iteration for predictable error ordering
197	    local ids = {}
198	    for id, _ in pairs(boxes) do ids[#ids + 1] = id end
199	    table.sort(ids)
200	
201	    for _, id in ipairs(ids) do
202	        local box = boxes[id]
203	        for _, p in ipairs(box.inputs or {}) do
204	            local has_wire    = wired[id .. "\0" .. p.name] == true
205	            local has_literal = p.value ~= nil and p.value ~= ""
206	            local is_optional = p.optional == true
207	            if not (has_wire or has_literal or is_optional) then
208	                err(errors, "box '" .. id .. "': input '" .. tostring(p.name) ..
209	                    "' has no wire, no literal, and is not optional")
210	            end
211	        end
212	    end
213	
214	    return errors
215	end
216	-- }}}
217	
218	-- {{{ validate_meta
219	function M.validate_meta(meta)
220	    local errors = {}
221	    if type(meta) ~= "table" then
222	        return { "meta.json must be a table" }
223	    end
224	    if meta.name == nil or type(meta.name) ~= "string" then
225	        err(errors, "meta.json missing 'name'")
226	    end
227	    if meta.entry_box_id == nil or type(meta.entry_box_id) ~= "string" then
228	        err(errors, "meta.json missing 'entry_box_id'")
229	    end
230	    return errors
231	end
232	-- }}}
233	
234	-- {{{ validate_drivers
235	function M.validate_drivers(drivers)
236	    local errors = {}
237	    if type(drivers) ~= "table" then
238	        return { "drivers.json must be a table" }
239	    end
240	    for ext, path in pairs(drivers) do
241	        if type(ext) ~= "string" or ext:sub(1, 1) ~= "." then
242	            err(errors, "driver key '" .. tostring(ext) .. "' must be a dot-prefixed extension")
243	        end
244	        if type(path) ~= "string" then
245	            err(errors, "driver value for '" .. ext .. "' must be a string path")
246	        end
247	    end
248	    return errors
249	end
250	-- }}}
251	
252	return M
253	
```

## Assistant

_Tools:_

- `Bash: grep -rn "comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/maps/ /mnt/mtwo/programs/sora/soramech/tests/...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-moywlsdd.json:1:{"id":"box-moywlsdd","label":"New Box","kind":"call","inputs":[{"name":"text","type":"any"}],"connections":[{"from_box":"write-result","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"},{"from_box":"box-mox3ipzr","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"}],"ui":{"x":994,"y":220},"iterator_outputs":["output_0"]}
/mnt/mtwo/programs/sora/soramech/tests/maps/comparator/boxes/classify.json:10:  "routing": { "kind": "comparator", "comparand": 5 },
/mnt/mtwo/programs/sora/soramech/tests/maps/branching/boxes/classify.json:7:  "routing": { "kind": "comparator", "comparand": 5 },
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:67:        -- invoked. Such a box has no ref/fn — only `iterator_outputs`,
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:69:        local is_iterator = box.iterator_outputs ~= nil
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:75:        if box.iterator_outputs ~= nil then
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:76:            if type(box.iterator_outputs) ~= "table" then
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:77:                err(errors, "'iterator_outputs' must be an array")
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:79:                for i, name in ipairs(box.iterator_outputs) do
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:81:                        err(errors, "'iterator_outputs[" .. i .. "]' must be a string")
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:105:        -- comparand: optional; if present it must be a string parseable as number at runtime
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:106:        if box.comparand ~= nil and type(box.comparand) ~= "string" then
/mnt/mtwo/programs/sora/soramech/src/001-schema.lua:107:            err(errors, "'comparand' must be a string")
/mnt/mtwo/programs/sora/soramech/src/012-dispatch.c:415:                (v < b->routing.comparand) ? "lt" :
/mnt/mtwo/programs/sora/soramech/src/012-dispatch.c:416:                (v > b->routing.comparand) ? "gt" : "eq";
/mnt/mtwo/programs/sora/soramech/src/010-graph-loader.info.md:33:  `comparand`).
/mnt/mtwo/programs/sora/soramech/src/010-graph-loader.info.md:56:  shipped surface). `comparator` requires `comparand` (number or
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:129:-- If box.comparand is set: compare output against it (must be a number or error).
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:133:    if box.comparand and box.comparand ~= "" then
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:135:        local comparand = tonumber(box.comparand)
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:136:        if not comparand then
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:137:            return "box '" .. box.id .. "': comparand '" ..
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:138:                tostring(box.comparand) .. "' is not a valid number"
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:148:        if num_val < comparand then
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:150:        elseif num_val == comparand then
/mnt/mtwo/programs/sora/soramech/src/010-graph-loader.h:10: * legacy `comparand` / `iterator_outputs` fields are gone. The data
/mnt/mtwo/programs/sora/soramech/src/010-graph-loader.h:73:    double         comparand;   /* comparator */
/mnt/mtwo/programs/sora/soramech/src/010-graph-loader.c:103:        json_node_t *c = json_object_get(r, "comparand");
/mnt/mtwo/programs/sora/soramech/src/010-graph-loader.c:105:            *err = err_fmt("box '%s': comparator routing missing 'comparand'", box_id);
/mnt/mtwo/programs/sora/soramech/src/010-graph-loader.c:109:            out->comparand = json_number_value(c);
```

## Assistant

Substantial scope here — the C side is unified, but the Lua schema, the editor, the Lua executor, and at least one user map all still wear the legacy shape. Let me map the full surface area before touching anything.

## Assistant

_Tools:_

- `Bash: grep -n "comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
328:  // counter mod #iterator_outputs each call. No ref/fn — the box body
337:    return Array.isArray(box && box.iterator_outputs);
340:  // Toggle the box ON as an iterator. Ref/fn/comparand are mutually
353:    delete current_box.comparand;
355:    current_box.iterator_outputs = ['output_0'];
362:  // Toggle iterator OFF. Drop the iterator_outputs field; sever
370:    delete current_box.iterator_outputs;
387:    const slots = current_box.iterator_outputs;
404:    const slots = current_box.iterator_outputs;
443:    const slots = current_box.iterator_outputs;
489:    const slots = box.iterator_outputs;
974:      box.iterator_outputs.forEach((slot, i) => {
1099:    const has_cmp = box.comparand !== undefined && box.comparand !== '';
1110:      if (current_box.comparand !== undefined && current_box.comparand !== '') {
1111:        delete current_box.comparand;
1113:        current_box.comparand = '0';
1124:      cmp_inp.value       = box.comparand || '';
1128:      cmp_inp.addEventListener('input', () => { current_box.comparand = cmp_inp.value; save(); });
```

## Assistant

_Tools:_

- `Bash: grep -n "comparand\|iterator_outputs\|from_branch" /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js /mnt/mtwo/...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:16:  let _selected_wire = null;  // { from_box, from_branch, to_box, to_input }
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:88:        const src  = Boxes.get_port_world_pos(c.from_box, c.from_branch ?? null, 'output');
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:117:        const src = Boxes.get_port_world_pos(c.from_box, c.from_branch ?? null, 'output');
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:122:        const color = c.from_branch ? (BRANCH_COLOR[c.from_branch] || '#4a6080') : '#4a6080';
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:126:          _selected_wire.from_branch === c.from_branch &&
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:154:      const branch_str = wire.from_branch ? '.' + wire.from_branch : '';
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:164:  // from_branch: null (no comparator) or 'lt'/'eq'/'gt'.
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:174:  async function create_connection(from_box_id, from_branch, to_box_id, to_input) {
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:191:      from_branch: from_branch ?? null,
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:202:      (c.from_branch ?? null) === conn.from_branch &&
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:232:    const branch_str = from_branch ? '.' + from_branch : '';
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:245:    // slot. The helper is a no-op if from_branch isn't the last slot
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:247:    if (Inspector.auto_grow_iterator_after_connect && from_branch) {
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:248:      await Inspector.auto_grow_iterator_after_connect(src_box, from_branch);
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:266:      (c.from_branch ?? null) === (conn.from_branch ?? null) &&
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:174:    if (box.iterator_outputs) {
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:175:      n_out = box.iterator_outputs.length;
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:178:    } else if (box.comparand && box.comparand !== '') {
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:189:  // Output side: null-named single dot when no comparand; 'lt'/'eq'/'gt' dots when comparand set.
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:207:    } else if (box.iterator_outputs) {
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:210:      // `name` field, so the wire's `from_branch` will pick them up
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:212:      out_pts = box.iterator_outputs.map((slot, i) => ({
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:217:    } else if (box.comparand && box.comparand !== '') {
```

## Assistant

_Tools:_

- `Bash: find /mnt/mtwo/programs/sora/soramech/maps -name "*.json" -exec grep -l "comparand\|iterator_outputs" {} \;`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-moywlsdd.json
```

## Assistant

Substantial surface. Plan — single commit closing 233, four new issue files for the follow-on routing kinds.

Starting with the schema. Reading inspector pieces and the example map first to make sure migrations are correct.

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
320	    await save();
321	    Canvas.mark_dirty();
322	    show(current_box, on_change_cb, on_delete_cb);
323	  }
324	
325	  // {{{ Iterator helpers (issue 221)
326	  // Iterator boxes are pure routing primitives: input copies straight
327	  // to one output, the dispatch layer (phase 3, issue 304) advances a
328	  // counter mod #iterator_outputs each call. No ref/fn — the box body
329	  // is just the counter + slot list. Each slot has a user-renamable
330	  // name; the slot name lands in a connection's `from_branch` field
331	  // exactly like a comparator branch name.
332	  //
333	  // The inspector edits the slot list; the dispatch layer is what
334	  // actually rotates between them at runtime.
335	
336	  function is_iterator(box) {
337	    return Array.isArray(box && box.iterator_outputs);
338	  }
339	
340	  // Toggle the box ON as an iterator. Ref/fn/comparand are mutually
341	  // exclusive with iterator routing (a routing primitive has no
342	  // function to invoke), so they are stripped. All outgoing wires are
343	  // dropped because the output shape changes from a single dot (or
344	  // lt/eq/gt) to N named slots — same shape-change-clears-wires rule
345	  // as the variadic input toggle.
346	  async function make_iterator() {
347	    if (!current_box) return;
348	
349	    await sever_output_wires();
350	
351	    delete current_box.ref;
352	    delete current_box.fn;
353	    delete current_box.comparand;
354	    delete current_box.has_output;
355	    current_box.iterator_outputs = ['output_0'];
356	
357	    await save();
358	    Canvas.mark_dirty();
359	    show(current_box, on_change_cb, on_delete_cb);
360	  }
361	
362	  // Toggle iterator OFF. Drop the iterator_outputs field; sever
363	  // outgoing wires (their from_branch values referenced slot names
364	  // that no longer exist). The user re-picks a function via the
365	  // browse button to restore ref/fn.
366	  async function unmake_iterator() {
367	    if (!current_box) return;
368	
369	    await sever_output_wires();
370	    delete current_box.iterator_outputs;
371	    // Restore an empty ref/fn so the schema (which requires ref on
372	    // non-iterator call boxes) accepts the save. The user picks a
373	    // function via the browse button to populate them.
374	    if (current_box.ref === undefined) current_box.ref = '';
375	    if (current_box.fn  === undefined) current_box.fn  = '';
376	
377	    await save();
378	    Canvas.mark_dirty();
379	    show(current_box, on_change_cb, on_delete_cb);
380	  }
381	
382	  // Append a new slot with a placeholder name (`output_N`). The user
383	  // can rename it after — placeholder is just to avoid prompting on
384	  // every grow. Mirrors the variadic auto-grow pattern.
385	  async function add_iterator_slot() {
386	    if (!current_box || !is_iterator(current_box)) return;
387	    const slots = current_box.iterator_outputs;
388	    let n = slots.length;
389	    // pick the lowest free `output_N` so renaming + adding doesn't
390	    // collide with an existing user-named slot
391	    while (slots.includes('output_' + n)) n++;
392	    slots.push('output_' + n);
393	    await save();
394	    Canvas.mark_dirty();
395	    show(current_box, on_change_cb, on_delete_cb);
396	  }
397	
398	  // Remove the slot at index `idx`. Outgoing wires whose from_branch
399	  // matched the removed slot get severed; remaining slots keep their
400	  // names (no rename — slot names are user-defined and free-form, no
401	  // need to compact like the variadic numeric indexes).
402	  async function remove_iterator_slot(idx) {
403	    if (!current_box || !is_iterator(current_box)) return;
404	    const slots = current_box.iterator_outputs;
405	    if (idx < 0 || idx >= slots.length) return;
406	    // Removing the last remaining slot collapses the iterator entirely
407	    // — the user shouldn't have to know there's a separate "toggle off"
408	    // button when the visual outcome of "× the last slot" is the same.
409	    if (slots.length <= 1) {
410	      return unmake_iterator();
411	    }
412	    const removed = slots[idx];
413	
414	    // Sever every outgoing wire on this branch.
415	    const my_id = current_box.id;
416	    const dst_ids = new Set();
417	    (current_box.connections || []).forEach(c => {
418	      if (c.from_box === my_id && c.from_branch === removed) dst_ids.add(c.to_box);
419	    });
420	    current_box.connections = (current_box.connections || [])
421	      .filter(c => !(c.from_box === my_id && c.from_branch === removed));
422	    for (const dst_id of dst_ids) {
423	      const dst = Boxes.boxes[dst_id];
424	      if (!dst) continue;
425	      dst.connections = (dst.connections || [])
426	        .filter(c => !(c.from_box === my_id && c.from_branch === removed));
427	      try { await API.put_box(dst_id, dst); }
428	      catch (e) { console.error('failed to clean wires to ' + dst_id + ': ' + e.message); }
429	    }
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
1080	    in_sec.textContent = 'inputs';
1081	    fields.appendChild(in_sec);
1082	    fields.appendChild(mk_port_display(box.inputs));
1083	
1084	    // output: single wire; optional comparator splits into lt/eq/gt.
1085	    // Skipped entirely for sink boxes (functions with no return values,
1086	    // issue 226) — no output dot, no comparator toggle, nothing to wire.
1087	    if (box.has_output === false) {
1088	      return;
1089	    }
1090	
1091	    const out_sec = document.createElement('div');
1092	    out_sec.className   = 'section-label';
1093	    out_sec.textContent = 'output';
1094	    fields.appendChild(out_sec);
1095	
1096	    const cmp_row = document.createElement('div');
1097	    cmp_row.style.cssText = 'display:flex;gap:6px;align-items:center;margin-bottom:6px;';
1098	
1099	    const has_cmp = box.comparand !== undefined && box.comparand !== '';
1100	    const cmp_btn = document.createElement('button');
1101	    cmp_btn.className   = 'toolbar-btn';
1102	    cmp_btn.textContent = has_cmp ? 'comparing ×' : 'compare';
1103	    cmp_btn.onclick = async () => {
1104	      // Toggling the comparator changes the set of valid output ports
1105	      // (single null wire ↔ three lt/eq/gt wires). Wires from the old
1106	      // configuration would re-appear if we left them in the
1107	      // connections array, so clear all outgoing wires on either
1108	      // toggle direction.
1109	      await sever_output_wires();
1110	      if (current_box.comparand !== undefined && current_box.comparand !== '') {
1111	        delete current_box.comparand;
1112	      } else {
1113	        current_box.comparand = '0';
1114	      }
1115	      await save();
1116	      show(current_box, on_change_cb, on_delete_cb);
1117	      Canvas.mark_dirty();
1118	    };
1119	    cmp_row.appendChild(cmp_btn);
1120	
1121	    if (has_cmp) {
1122	      const cmp_inp = document.createElement('input');
1123	      cmp_inp.type        = 'text';
1124	      cmp_inp.value       = box.comparand || '';
1125	      cmp_inp.placeholder = 'number';
1126	      cmp_inp.style.cssText = 'flex:1;background:#0f1117;border:1px solid #2a2f45;' +
1127	        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
1128	      cmp_inp.addEventListener('input', () => { current_box.comparand = cmp_inp.value; save(); });
1129	      cmp_row.appendChild(cmp_inp);
1130	    }
1131	
1132	    fields.appendChild(cmp_row);
1133	  }
1134	  // }}}
1135	
1136	  return { show, hide, show_content, auto_grow_after_set,
1137	           auto_grow_iterator_after_connect };
1138	})();
1139	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-moywlsdd.json`

### Tool result

```
1	{"id":"box-moywlsdd","label":"New Box","kind":"call","inputs":[{"name":"text","type":"any"}],"connections":[{"from_box":"write-result","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"},{"from_box":"box-mox3ipzr","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"}],"ui":{"x":994,"y":220},"iterator_outputs":["output_0"]}
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/src/004-executor.lua`

### Tool result

```
120	    for _, port in ipairs(box.inputs or {}) do
121	        inputs[port.name] = store[box.id .. "." .. port.name]
122	    end
123	    return inputs
124	end
125	-- }}}
126	
127	-- {{{ fire_connections
128	-- Routes the single output value through wired connections.
129	-- If box.comparand is set: compare output against it (must be a number or error).
130	-- Otherwise: fire all connections whose from_branch is nil.
131	-- Returns nil on success, or an error string.
132	local function fire_connections(box, output_val, store, queue, boxes)
133	    if box.comparand and box.comparand ~= "" then
134	        -- comparator mode: output must be a number
135	        local comparand = tonumber(box.comparand)
136	        if not comparand then
137	            return "box '" .. box.id .. "': comparand '" ..
138	                tostring(box.comparand) .. "' is not a valid number"
139	        end
140	        local num_val = tonumber(output_val)
141	        if not num_val then
142	            return "box '" .. box.id .. "': output value '" ..
143	                tostring(output_val) .. "' is not a number, but comparator is configured"
144	        end
145	
146	        -- determine which branch to fire
147	        local branch
148	        if num_val < comparand then
149	            branch = "lt"
150	        elseif num_val == comparand then
151	            branch = "eq"
152	        else
153	            branch = "gt"
154	        end
155	
156	        for _, c in ipairs(box.connections or {}) do
157	            if c.from_box ~= box.id then goto next_cmp_conn end
158	            if c.from_branch == branch then
159	                store[c.to_box .. "." .. c.to_input] = output_val
160	                local target = boxes[c.to_box]
161	                if target and inputs_satisfied(target, store) then
162	                    queue[#queue + 1] = c.to_box
163	                end
164	            end
165	            ::next_cmp_conn::
166	        end
167	    else
168	        -- single-wire mode: fire connections where from_branch is nil
169	        for _, c in ipairs(box.connections or {}) do
170	            if c.from_box ~= box.id then goto next_conn end
171	            if c.from_branch == nil then
172	                store[c.to_box .. "." .. c.to_input] = output_val
173	                local target = boxes[c.to_box]
174	                if target and inputs_satisfied(target, store) then
175	                    queue[#queue + 1] = c.to_box
176	                end
177	            end
178	            ::next_conn::
179	        end
180	    end
181	    return nil
182	end
183	-- }}}
184	
185	-- {{{ write_last_run
186	local function write_last_run(map_dir, run_state)
187	    local tmp_dir = map_dir .. "/tmp"
188	    local path    = tmp_dir .. "/last-run.json"
189	    local f, ferr = io.open(path, "w")
190	    if not f then
191	        io.stderr:write("executor: cannot write last-run.json: " .. tostring(ferr) .. "\n")
192	        return
193	    end
194	    f:write(json.encode(run_state, { indent = true }))
195	    f:close()
196	end
197	-- }}}
198	
199	-- {{{ execute
```

## Assistant

_Tools:_

- `Bash: grep -n "comparand\|iterator_outputs\|routing" /mnt/mtwo/programs/sora/soramech/src/004-executor.lua /mnt/mtwo/progra...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:129:-- If box.comparand is set: compare output against it (must be a number or error).
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:133:    if box.comparand and box.comparand ~= "" then
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:135:        local comparand = tonumber(box.comparand)
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:136:        if not comparand then
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:137:            return "box '" .. box.id .. "': comparand '" ..
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:138:                tostring(box.comparand) .. "' is not a valid number"
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:148:        if num_val < comparand then
/mnt/mtwo/programs/sora/soramech/src/004-executor.lua:150:        elseif num_val == comparand then
```

## Assistant

_Tools:_

- `Bash: grep -rn "comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/maps/ 2>&1 | head -20`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-moywlsdd.json:1:{"id":"box-moywlsdd","label":"New Box","kind":"call","inputs":[{"name":"text","type":"any"}],"connections":[{"from_box":"write-result","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"},{"from_box":"box-mox3ipzr","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"}],"ui":{"x":994,"y":220},"iterator_outputs":["output_0"]}
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/src/001-schema.lua has been updated successfully.
```

## Assistant

Big edit. Schema first.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/src/001-schema.lua`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/src/001-schema.lua has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/src/001-schema.lua`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/src/001-schema.lua has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/src/001-schema.lua`

## Assistant

Now the phase-2 executor.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/src/004-executor.lua`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/src/004-executor.lua has been updated successfully.
```

### Tool result

```
170	    const n_in  = visible_inputs(box).length;
171	    // Output rows: iterator boxes have N named slots (issue 221);
172	    // comparator active: 3 dots (lt/eq/gt); sink: 0; otherwise 1.
173	    let n_out;
174	    if (box.iterator_outputs) {
175	      n_out = box.iterator_outputs.length;
176	    } else if (box.has_output === false) {
177	      n_out = 0;
178	    } else if (box.comparand && box.comparand !== '') {
179	      n_out = 3;
180	    } else {
181	      n_out = 1;
182	    }
183	    return Math.max(MIN_BOX_H, BOX_HEADER + Math.max(n_in, n_out) * PORT_ROW_H + 8);
184	  }
185	  // }}}
186	
187	  // {{{ port_positions
188	  // Returns world-space {x,y} for each port dot on a box.
189	  // Output side: null-named single dot when no comparand; 'lt'/'eq'/'gt' dots when comparand set.
190	  function port_positions(box) {
191	    const bx = box.ui?.x ?? 0;
192	    const by = box.ui?.y ?? 0;
193	    const h  = box_height(box);
194	    const inputs = visible_inputs(box);
195	
196	    const in_pts = inputs.map((p, i) => ({
197	      name: p.name, side: 'input',
198	      x: bx,
199	      y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
200	    }));
201	
202	    let out_pts;
203	    if (box.has_output === false) {
204	      // sink box — function has no return values (issue 226). No output
205	      // port, no wire-grab target, nothing for the inspector to render.
206	      out_pts = [];
207	    } else if (box.iterator_outputs) {
208	      // iterator box (issue 221) — N named output dots stacked down
209	      // the right edge, one per declared slot. Slot names land in the
210	      // `name` field, so the wire's `from_branch` will pick them up
211	      // exactly like a comparator branch name.
212	      out_pts = box.iterator_outputs.map((slot, i) => ({
213	        name: slot, side: 'output',
214	        x: bx + BOX_W,
215	        y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
216	      }));
217	    } else if (box.comparand && box.comparand !== '') {
218	      // three comparator dots
219	      out_pts = ['lt', 'eq', 'gt'].map((branch, i) => ({
220	        name: branch, side: 'output',
221	        x: bx + BOX_W,
222	        y: by + BOX_HEADER + PORT_ROW_H * i + PORT_ROW_H / 2,
223	      }));
224	    } else {
225	      // single output dot, vertically centered in the box
226	      out_pts = [{
227	        name: null, side: 'output',
228	        x: bx + BOX_W,
229	        y: by + h / 2,
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: grep -rn "routing\|comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/assets/js/ /mnt/mtwo/programs/sora/s...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:175:    const rk = box.routing && box.routing.kind;
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:180:      n_out = (box.routing && box.routing.n_outputs) || 1;
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:207:    const rk = box.routing && box.routing.kind;
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:217:      const n = (box.routing && box.routing.n_outputs) || 1;
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:337:    return Array.isArray(box && box.iterator_outputs);
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:353:    delete current_box.comparand;
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:355:    current_box.iterator_outputs = ['output_0'];
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:370:    delete current_box.iterator_outputs;
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:387:    const slots = current_box.iterator_outputs;
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:404:    const slots = current_box.iterator_outputs;
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:443:    const slots = current_box.iterator_outputs;
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:489:    const slots = box.iterator_outputs;
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:974:      box.iterator_outputs.forEach((slot, i) => {
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:1099:    const has_cmp = box.comparand !== undefined && box.comparand !== '';
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:1110:      if (current_box.comparand !== undefined && current_box.comparand !== '') {
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:1111:        delete current_box.comparand;
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:1113:        current_box.comparand = '0';
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:1124:      cmp_inp.value       = box.comparand || '';
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:1128:      cmp_inp.addEventListener('input', () => { current_box.comparand = cmp_inp.value; save(); });
```

## Assistant

_Tools:_

- `Bash: grep -rn "create.*box\|new.*box\|kind.*call\|kind: 'call'\|kind: \"call\"" /mnt/mtwo/programs/sora/soramech/assets/js...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/assets/js/008-source-view.js:92:    if (!ref) throw new Error('no ref set on this box');
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:174:  async function create_connection(from_box_id, from_branch, to_box_id, to_input) {
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:276:    const new_keep_idx = current_box.inputs.findIndex(p => p.name === slot_0_old_name);
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:277:    if (new_keep_idx >= 0) current_box.inputs[new_keep_idx].name = base;
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:730:          if (new_val !== undefined && current_box) {
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:764:          if (new_val !== undefined && current_box && in_var_group &&
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:794:        current_box.label = Boxes.default_header({ kind: 'call', ref: filename });
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:836:  // input port. Both kinds skip the call-box machinery (ref/fn/
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:1059:          current_box.label = Boxes.default_header({ kind: 'call', ref: filename });
/mnt/mtwo/programs/sora/soramech/assets/js/002-boxes.js:63:    if (box.kind === 'call' && box.ref) {
```

## Assistant

_Tools:_

- `Bash: grep -rn "put_box\|POST.*box\|api_create\|create_box\|new_box\|PUT" /mnt/mtwo/programs/sora/soramech/assets/js/ 2>/de...`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:217:        await API.put_box(from_box_id, src_box);
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:220:          API.put_box(from_box_id, src_box),
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:221:          API.put_box(to_box_id,   dst_box),
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:280:        await API.put_box(conn.from_box, src_box);
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:283:          API.put_box(conn.from_box, src_box),
/mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js:284:          API.put_box(conn.to_box,   dst_box),
/mnt/mtwo/programs/sora/soramech/assets/js/003-api.js:45:  async function put_box(id, data) {
/mnt/mtwo/programs/sora/soramech/assets/js/003-api.js:46:    return request('PUT', `/maps/${map_name}/boxes/${id}`, data);
/mnt/mtwo/programs/sora/soramech/assets/js/003-api.js:64:    return request('PUT', `/maps/${map_name}/meta`, data);
/mnt/mtwo/programs/sora/soramech/assets/js/003-api.js:113:    return request('PUT', `/maps/${map_name}/data/${filename}`, data);
/mnt/mtwo/programs/sora/soramech/assets/js/003-api.js:117:  return { init, list_maps, list_boxes, get_box, put_box, delete_box,
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:138:        await API.put_box(up_id, up);
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:159:      await API.put_box(current_box.id, current_box);
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:427:      try { await API.put_box(dst_id, dst); }
/mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js:474:      try { await API.put_box(dst_id, dst); }
```

## Assistant

_Tools:_

- `Bash: grep -n "new_box\|new Box\|box-\|createBox\|crypto.randomUUID\|Math.random\|create_call\|new Object" /mnt/mtwo/progra...`

### Tool result

```
169:  // {{{ new_box_at
170:  async function new_box_at(wx, wy) {
171:    const id = 'box-' + Date.now().toString(36);
217:    dup.id = 'box-' + Date.now().toString(36);
888:      items.push({ label: 'add box here', action: () => new_box_at(w.x, w.y) });
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/005-app.js`

### Tool result

```
169	  // {{{ new_box_at
170	  async function new_box_at(wx, wy) {
171	    const id = 'box-' + Date.now().toString(36);
172	    const box = {
173	      id, label: 'New Box', kind: 'call',
174	      ref: '', fn: '', inputs: [], connections: [],
175	      ui: { x: Math.round(wx), y: Math.round(wy) },
176	    };
177	    try {
178	      await API.put_box(id, box);
179	      Boxes.boxes[id] = box;
180	      selected_id = id;
181	      Inspector.show(box, () => Canvas.mark_dirty(), () => delete_box(id));
182	      Canvas.mark_dirty();
183	    } catch(e) {
184	      status_msg('create error: ' + e.message, 'error');
185	    }
186	  }
187	  // }}}
188	
189	  // {{{ next_copy_label
190	  // Produces the label for a duplicated box (issue 234). If the source
191	  // already ends with "(copy)" or "(copy N)", increment N; otherwise
192	  // append "(copy)". A user duplicating "Stamp Time" gets a series:
193	  // "Stamp Time (copy)" → "Stamp Time (copy 2)" → "(copy 3)" etc.
194	  function next_copy_label(base) {
195	    const m = String(base || '').match(/^(.*?)\s*\(copy(?:\s+(\d+))?\)\s*$/);
196	    if (m) {
197	      const root = m[1];
198	      const n    = m[2] ? parseInt(m[2]) + 1 : 2;
199	      return root + ' (copy ' + n + ')';
200	    }
201	    return (base || '') + ' (copy)';
202	  }
203	  // }}}
204	
205	  // {{{ duplicate_box
206	  // Issue 234: clone the box's data, drop it at (wx, wy), and replay
207	  // every wire whose destination was the original. Outgoing wires are
208	  // skipped — the natural use is "I want another node fed by the
209	  // same data," not another sink for everything downstream. Self-
210	  // loops are skipped too: they're both incoming and outgoing,
211	  // which contradicts the two rules at once.
212	  async function duplicate_box(src_id, wx, wy) {
213	    const src = Boxes.boxes[src_id];
214	    if (!src) return;
215	
216	    const dup = JSON.parse(JSON.stringify(src));
217	    dup.id = 'box-' + Date.now().toString(36);
218	    dup.ui = dup.ui || {};
219	    dup.ui.x = Math.round(wx);
220	    dup.ui.y = Math.round(wy);
221	    dup.label = next_copy_label(src.label || src.id);
222	
223	    // Rewrite the clone's connection list to keep only incoming wires
224	    // from OTHER boxes, with to_box pointed at the new id. The source
225	    // box's side of each wire is patched below as a separate PUT.
226	    const incoming = [];
227	    for (const c of (dup.connections || [])) {
228	      if (c.to_box === src_id && c.from_box !== src_id) {
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
920	        current_box.kind = sel.value;
921	        show(current_box, on_change_cb, on_delete_cb);
922	        save();
923	      });
924	      return sel;
925	    })()));
926	
927	    // Issue 229: read and write boxes have a different inspector shape
928	    // than call boxes — `ref`/`fn`/routing don't apply, and `read` has
929	    // an inline `value` field that swaps out the path port. Render the
930	    // IO-primitive form and stop here so the call-box machinery below
931	    // doesn't try to attach.
932	    if (box.kind === 'read' || box.kind === 'write') {
933	      render_io_box(fields, box);
934	      // Inputs section — same port table as call boxes.
935	      const in_sec = document.createElement('div');
936	      in_sec.className   = 'section-label';
937	      in_sec.textContent = 'inputs';
938	      fields.appendChild(in_sec);
939	      fields.appendChild(mk_port_display(box.inputs));
940	      return;
941	    }
942	
943	    // iterator toggle (issue 221) — flips between function-backed and
944	    // routing-primitive shapes; mutually exclusive with ref/fn/comparator
945	    const iter_btn = document.createElement('button');
946	    iter_btn.className   = 'toolbar-btn';
947	    iter_btn.textContent = is_iterator(box) ? 'iterator ×' : 'iterator';
948	    iter_btn.onclick = () => {
949	      if (is_iterator(current_box)) unmake_iterator();
950	      else make_iterator();
951	    };
952	    fields.appendChild(mk_row('mode', iter_btn));
953	
954	    // For iterator boxes, ref/fn/output are all replaced by the slot
955	    // list. Inputs still render below (one input wire feeds the
956	    // routing primitive). The render flow forks here.
957	    if (is_iterator(box)) {
958	      // inputs section (same as below; one input is typical but we
959	      // accept whatever the user has set)
960	      const in_sec = document.createElement('div');
961	      in_sec.className   = 'section-label';
962	      in_sec.textContent = 'inputs';
963	      fields.appendChild(in_sec);
964	      fields.appendChild(mk_port_display(box.inputs));
965	
966	      // outputs: editable list of slot names with × per slot, + at end
967	      const out_sec = document.createElement('div');
968	      out_sec.className   = 'section-label';
969	      out_sec.textContent = 'outputs (round-robin)';
970	      fields.appendChild(out_sec);
971	
972	      const slot_wrap = document.createElement('div');
973	      slot_wrap.className = 'port-display';
974	      box.iterator_outputs.forEach((slot, i) => {
975	        const row = document.createElement('div');
976	        row.className = 'port-display-row';
977	
978	        const inp = document.createElement('input');
979	        inp.type      = 'text';
980	        inp.value     = slot;
981	        inp.className = 'port-name-inp';
982	        inp.style.flex = '1';
983	        inp.addEventListener('change', () => {
984	          rename_iterator_slot(i, inp.value.trim());
985	        });
986	        row.appendChild(inp);
987	
988	        const rm = document.createElement('button');
989	        rm.className   = 'toolbar-btn';
990	        rm.textContent = '×';
991	        rm.title       = 'remove slot';
992	        rm.onclick     = () => remove_iterator_slot(i);
993	        row.appendChild(rm);
994	
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
995	        slot_wrap.appendChild(row);
996	      });
997	      const add_btn = document.createElement('button');
998	      add_btn.className   = 'toolbar-btn';
999	      add_btn.textContent = '+ slot';
1000	      add_btn.onclick     = () => add_iterator_slot();
1001	      slot_wrap.appendChild(add_btn);
1002	      fields.appendChild(slot_wrap);
1003	      return;
1004	    }
1005	
1006	    // ref: read-only display showing just the basename (no path, no
1007	    // extension), plus browse / view buttons. Full path stored on the
1008	    // box still — the display is just a UI tightening since paths can
1009	    // be long enough to overflow the sidebar.
1010	    const ref_wrap = document.createElement('div');
1011	    ref_wrap.style.cssText = 'display:flex;gap:4px;align-items:center;';
1012	    const ref_disp = document.createElement('span');
1013	    ref_disp.className = 'ref-display';
1014	    ref_disp.title     = box.ref || '';   // full path on hover
1015	    ref_disp.textContent = basename_no_ext(box.ref) || '(none)';
1016	    const browse_btn = document.createElement('button');
1017	    browse_btn.className   = 'toolbar-btn';
1018	    browse_btn.textContent = 'browse';
1019	    browse_btn.style.whiteSpace = 'nowrap';
1020	    browse_btn.onclick = open_browser;
1021	    // "view" button opens a floating, draggable read-only window with
1022	    // the current ref's source content (issue 215). Hidden when ref
1023	    // is empty since there's nothing to fetch (issue 227).
1024	    const view_btn = document.createElement('button');
1025	    view_btn.className   = 'toolbar-btn';
1026	    view_btn.textContent = 'view';
1027	    view_btn.style.whiteSpace = 'nowrap';
1028	    view_btn.hidden = !box.ref;
1029	    view_btn.onclick = () => {
1030	      if (!current_box || !current_box.ref) {
1031	        status_msg('view source: no ref set', 'error');
1032	        return;
1033	      }
1034	      SourceView.open_source_view(current_box.ref, current_box);
1035	    };
1036	    ref_wrap.appendChild(ref_disp);
1037	    ref_wrap.appendChild(browse_btn);
1038	    ref_wrap.appendChild(view_btn);
1039	    fields.appendChild(mk_row('ref', ref_wrap));
1040	
1041	    // fn: clickable button that jumps the file browser to the
1042	    // function-list step for the current ref. Lets the user pick a
1043	    // different fn from the same file, or back-button up to swap files
1044	    // entirely. Reads as a button rather than a label so the
1045	    // affordance is unambiguous.
1046	    const fn_btn = document.createElement('button');
1047	    fn_btn.className   = 'fn-btn';
1048	    fn_btn.textContent = box.fn || '(no function)';
1049	    fn_btn.title       = 'change function — opens file browser at the function list';
1050	    fn_btn.disabled    = !box.ref;
1051	    fn_btn.onclick     = () => {
1052	      title.textContent = 'pick fn';
1053	      fields.innerHTML  = '';
1054	      FileBrowser.render_at_fn_list(fields, current_box.ref, async (filename, fn) => {
1055	        // Same label-swap rule as the full browse (issue 236) —
1056	        // changing the file overwrites the label, switching fn
1057	        // within the same file leaves it alone.
1058	        if (current_box.ref !== filename) {
1059	          current_box.label = Boxes.default_header({ kind: 'call', ref: filename });
1060	        }
1061	        current_box.ref    = filename;
1062	        current_box.fn     = fn.name;
1063	        current_box.inputs = fn.inputs.map(n => ({ name: n, type: 'any' }));
1064	        if (fn.outputs && fn.outputs.length === 0) {
1065	          current_box.has_output = false;
1066	          await sever_output_wires();
1067	        } else {
1068	          delete current_box.has_output;
1069	        }
1070	        await save();
1071	        show(current_box, on_change_cb, on_delete_cb);
1072	        Canvas.mark_dirty();
1073	      });
1074	    };
1075	    fields.appendChild(mk_row('fn', fn_btn));
1076	
1077	    // inputs: read-only names from parsing, editable literal values
1078	    const in_sec = document.createElement('div');
1079	    in_sec.className   = 'section-label';
1080	    in_sec.textContent = 'inputs';
1081	    fields.appendChild(in_sec);
1082	    fields.appendChild(mk_port_display(box.inputs));
1083	
1084	    // output: single wire; optional comparator splits into lt/eq/gt.
1085	    // Skipped entirely for sink boxes (functions with no return values,
1086	    // issue 226) — no output dot, no comparator toggle, nothing to wire.
1087	    if (box.has_output === false) {
1088	      return;
1089	    }
1090	
1091	    const out_sec = document.createElement('div');
1092	    out_sec.className   = 'section-label';
1093	    out_sec.textContent = 'output';
1094	    fields.appendChild(out_sec);
```

## Assistant

_Tools:_

- `Bash: grep -n "rename_iterator_slot\|auto_grow_iterator_after_connect" /mnt/mtwo/programs/sora/soramech/assets/js/004-inspe...`

### Tool result

```
441:  async function rename_iterator_slot(idx, new_name) {
487:  async function auto_grow_iterator_after_connect(box, from_branch) {
984:          rename_iterator_slot(i, inp.value.trim());
1137:           auto_grow_iterator_after_connect };
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
432	    await save();
433	    Canvas.mark_dirty();
434	    show(current_box, on_change_cb, on_delete_cb);
435	  }
436	
437	  // Rename slot at index `idx` to `new_name`. Renames the slot itself
438	  // and rewrites `from_branch` on every outgoing wire that used the
439	  // old name (both the source-box record and the destination-box
440	  // copy). No-op if the name is unchanged or already taken.
441	  async function rename_iterator_slot(idx, new_name) {
442	    if (!current_box || !is_iterator(current_box)) return;
443	    const slots = current_box.iterator_outputs;
444	    if (idx < 0 || idx >= slots.length) return;
445	    const old_name = slots[idx];
446	    if (old_name === new_name) return;
447	    if (!new_name) {
448	      status_msg('slot name cannot be empty', 'error');
449	      return;
450	    }
451	    if (slots.includes(new_name)) {
452	      status_msg('slot name already in use', 'error');
453	      return;
454	    }
455	
456	    slots[idx] = new_name;
457	
458	    const my_id = current_box.id;
459	    const dst_ids = new Set();
460	    (current_box.connections || []).forEach(c => {
461	      if (c.from_box === my_id && c.from_branch === old_name) {
462	        c.from_branch = new_name;
463	        dst_ids.add(c.to_box);
464	      }
465	    });
466	    for (const dst_id of dst_ids) {
467	      const dst = Boxes.boxes[dst_id];
468	      if (!dst) continue;
469	      (dst.connections || []).forEach(c => {
470	        if (c.from_box === my_id && c.from_branch === old_name) {
471	          c.from_branch = new_name;
472	        }
473	      });
474	      try { await API.put_box(dst_id, dst); }
475	      catch (e) { console.error('failed to rename wires on ' + dst_id + ': ' + e.message); }
476	    }
477	
478	    await save();
479	    Canvas.mark_dirty();
480	  }
481	
482	  // Auto-grow check used by the wire-connect path. If `from_branch` is
483	  // the LAST slot of an iterator box, append a new placeholder slot
484	  // and persist. Idempotent — calling again on the same slot is a
485	  // no-op once a newer slot exists below it. Mirrors the variadic
486	  // input auto-grow.
487	  async function auto_grow_iterator_after_connect(box, from_branch) {
488	    if (!is_iterator(box)) return;
489	    const slots = box.iterator_outputs;
490	    if (slots[slots.length - 1] !== from_branch) return;
491	
492	    let n = slots.length;
493	    while (slots.includes('output_' + n)) n++;
494	    slots.push('output_' + n);
495	    try {
496	      await API.put_box(box.id, box);
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
496	      await API.put_box(box.id, box);
497	      Canvas.mark_dirty();
498	      if (current_box && current_box.id === box.id) {
499	        show(current_box, on_change_cb, on_delete_cb);
500	      }
501	    } catch (e) {
502	      slots.pop();
503	      status_msg('iterator auto-grow failed: ' + e.message, 'error');
504	    }
505	  }
506	  // }}}
507	
508	  // Sever every outgoing wire from this box. Used when toggling the
509	  // comparator on/off — wires from null-output and lt/eq/gt outputs
510	  // can't all be valid at once, so we wipe the slate on either toggle
511	  // direction. Also clears any selected wire if it pointed at one of
512	  // the removed connections.
513	  async function sever_output_wires() {
514	    if (!current_box) return;
515	    const my_id = current_box.id;
516	    const out   = (current_box.connections || []).filter(c => c.from_box === my_id);
517	    if (out.length === 0) return;
518	
519	    // Collect destinations whose connection records also need cleaning.
520	    const dest_ids = new Set();
```

### Tool result

```
<tool_use_error>String to replace not found in file.
String:   // {{{ Iterator helpers (issue 221)
  // Iterator boxes are pure routing primitives: input copies straight
  // to one output, the dispatch layer (phase 3, issue 304) advances a
  // counter mod #iterator_outputs each call. No ref/fn — the box body
  // is just the counter + slot list. Each slot has a user-renamable
  // name; the slot name lands in a connection's `from_branch` field
  // exactly like a comparator branch name.
  //
  // The inspector edits the slot list; the dispatch layer is what
  // actually rotates between them at runtime.

  function is_iterator(box) {
    return Array.isArray(box && box.iterator_outputs);
  }

  // Toggle the box ON as an iterator. Ref/fn/comparand are mutually
  // exclusive with iterator routing (a routing primitive has no
  // function to invoke), so they are stripped. All outgoing wires are
  // dropped because the output shape changes from a single dot (or
  // lt/eq/gt) to N named slots — same shape-change-clears-wires rule
  // as the variadic input toggle.
  async function make_iterator() {
    if (!current_box) return;

    await sever_output_wires();

    delete current_box.ref;
    delete current_box.fn;
    delete current_box.comparand;
    delete current_box.has_output;
    current_box.iterator_outputs = ['output_0'];

    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Toggle iterator OFF. Drop the iterator_outputs field; sever
  // outgoing wires (their from_branch values referenced slot names
  // that no longer exist). The user re-picks a function via the
  // browse button to restore ref/fn.
  async function unmake_iterator() {
    if (!current_box) return;

    await sever_output_wires();
    delete current_box.iterator_outputs;
    // Restore an empty ref/fn so the schema (which requires ref on
    // non-iterator call boxes) accepts the save. The user picks a
    // function via the browse button to populate them.
    if (current_box.ref === undefined) current_box.ref = '';
    if (current_box.fn  === undefined) current_box.fn  = '';

    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Append a new slot with a placeholder name (`output_N`). The user
  // can rename it after — placeholder is just to avoid prompting on
  // every grow. Mirrors the variadic auto-grow pattern.
  async function add_iterator_slot() {
    if (!current_box || !is_iterator(current_box)) return;
    const slots = current_box.iterator_outputs;
    let n = slots.length;
    // pick the lowest free `output_N` so renaming + adding doesn't
    // collide with an existing user-named slot
    while (slots.includes('output_' + n)) n++;
    slots.push('output_' + n);
    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Remove the slot at index `idx`. Outgoing wires whose from_branch
  // matched the removed slot get severed; remaining slots keep their
  // names (no rename — slot names are user-defined and free-form, no
  // need to compact like the variadic numeric indexes).
  async function remove_iterator_slot(idx) {
    if (!current_box || !is_iterator(current_box)) return;
    const slots = current_box.iterator_outputs;
    if (idx < 0 || idx >= slots.length) return;
    // Removing the last remaining slot collapses the iterator entirely
    // — the user shouldn't have to know there's a separate "toggle off"
    // button when the visual outcome of "× the last slot" is the same.
    if (slots.length <= 1) {
      return unmake_iterator();
    }
    const removed = slots[idx];

    // Sever every outgoing wire on this branch.
    const my_id = current_box.id;
    const dst_ids = new Set();
    (current_box.connections || []).forEach(c => {
      if (c.from_box === my_id && c.from_branch === removed) dst_ids.add(c.to_box);
    });
    current_box.connections = (current_box.connections || [])
      .filter(c => !(c.from_box === my_id && c.from_branch === removed));
    for (const dst_id of dst_ids) {
      const dst = Boxes.boxes[dst_id];
      if (!dst) continue;
      dst.connections = (dst.connections || [])
        .filter(c => !(c.from_box === my_id && c.from_branch === removed));
      try { await API.put_box(dst_id, dst); }
      catch (e) { console.error('failed to clean wires to ' + dst_id + ': ' + e.message); }
    }

    slots.splice(idx, 1);

    await save();
    Canvas.mark_dirty();
    show(current_box, on_change_cb, on_delete_cb);
  }

  // Rename slot at index `idx` to `new_name`. Renames the slot itself
  // and rewrites `from_branch` on every outgoing wire that used the
  // old name (both the source-box record and the destination-box
  // copy). No-op if the name is unchanged or already taken.
  async function rename_iterator_slot(idx, new_name) {
    if (!current_box || !is_iterator(current_box)) return;
    const slots = current_box.iterator_outputs;
    if (idx < 0 || idx >= slots.length) return;
    const old_name = slots[idx];
    if (old_name === new_name) return;
    if (!new_name) {
      status_msg('slot name cannot be empty', 'error');
      return;
    }
    if (slots.includes(new_name)) {
      status_msg('slot name already in use', 'error');
      return;
    }

    slots[idx] = new_name;

    const my_id = current_box.id;
    const dst_ids = new Set();
    (current_box.connections || []).forEach(c => {
      if (c.from_box === my_id && c.from_branch === old_name) {
        c.from_branch = new_name;
        dst_ids.add(c.to_box);
      }
    });
    for (const dst_id of dst_ids) {
      const dst = Boxes.boxes[dst_id];
      if (!dst) continue;
      (dst.connections || []).forEach(c => {
        if (c.from_box === my_id && c.from_branch === old_name) {
          c.from_branch = new_name;
        }
      });
      try { await API.put_box(dst_id, dst); }
      catch (e) { console.error('failed to rename wires on ' + dst_id + ': ' + e.message); }
    }

    await save();
    Canvas.mark_dirty();
  }

  // Auto-grow check used by the wire-connect path. If `from_branch` is
  // the LAST slot of an iterator box, append a new placeholder slot
  // and persist. Idempotent — calling again on the same slot is a
  // no-op once a newer slot exists below it. Mirrors the variadic
  // input auto-grow.
  async function auto_grow_iterator_after_connect(box, from_branch) {
    if (!is_iterator(box)) return;
    const slots = box.iterator_outputs;
    if (slots[slots.length - 1] !== from_branch) return;

    let n = slots.length;
    while (slots.includes('output_' + n)) n++;
    slots.push('output_' + n);
    try {
      await API.put_box(box.id, box);
      Canvas.mark_dirty();
      if (current_box && current_box.id === box.id) {
        show(current_box, on_change_cb, on_delete_cb);
      }
    } catch (e) {
      slots.pop();
      status_msg('iterator auto-grow failed: ' + e.message, 'error');
    }
  }
  // }}}
(note: Edit also tried swapping \uXXXX escapes and their characters; neither form matched, so the mismatch is likely elsewhere in old_string. Re-read the file and copy the exact surrounding text.)</tool_use_error>
```

## Assistant

Now the big inspector rewrite — replacing iterator helpers section, the inspector mode dropdown, and the output controls.

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
325	  // {{{ Iterator helpers (issue 221)
326	  // Iterator boxes are pure routing primitives: input copies straight
327	  // to one output, the dispatch layer (phase 3, issue 304) advances a
328	  // counter mod #iterator_outputs each call. No ref/fn — the box body
329	  // is just the counter + slot list. Each slot has a user-renamable
330	  // name; the slot name lands in a connection's `from_branch` field
331	  // exactly like a comparator branch name.
332	  //
333	  // The inspector edits the slot list; the dispatch layer is what
334	  // actually rotates between them at runtime.
335	
336	  function is_iterator(box) {
337	    return Array.isArray(box && box.iterator_outputs);
338	  }
339	
340	  // Toggle the box ON as an iterator. Ref/fn/comparand are mutually
341	  // exclusive with iterator routing (a routing primitive has no
342	  // function to invoke), so they are stripped. All outgoing wires are
343	  // dropped because the output shape changes from a single dot (or
344	  // lt/eq/gt) to N named slots — same shape-change-clears-wires rule
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

## Assistant

_Tools:_

- `Bash: awk 'NR==325,NR==506' /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js | wc -l && awk 'NR==506' /mnt/mtwo/...`

### Tool result

```
182
  // }}}
```

## Assistant

_Tools:_

- `Bash: sed -n '325,506p' /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js > /tmp/iter-helpers.txt && wc -c /tmp/i...`

### Tool result

```
6960 /tmp/iter-helpers.txt
0000000           /   /       {   {   {       I   t   e   r   a   t   o
0000020   r       h   e   l   p   e   r   s       (   i   s   s   u   e
0000040       2   2   1   )  \n           /   /       I   t   e   r   a
0000060   t   o   r       b   o   x   e   s       a   r   e       p   u
0000100   r   e       r   o   u   t   i   n   g       p   r   i   m   i
0000120   t   i   v   e   s   :       i   n   p   u   t       c   o   p
0000140   i   e   s       s   t   r   a   i   g   h   t  \n           /
0000160   /       t   o       o   n   e       o   u   t   p   u   t   ,
0000200       t   h   e       d   i   s   p   a   t   c   h       l   a
0000220   y   e   r       (   p   h   a   s   e       3   ,       i   s
0000240   s   u   e       3   0   4   )       a   d   v   a   n   c   e
0000260   s       a  \n           /   /       c   o   u   n   t   e   r
0000300       m   o   d       #   i   t   e   r   a   t   o   r   _   o
0000320   u   t   p   u   t   s       e   a   c   h       c   a   l   l
0000340   .       N   o       r   e   f   /   f   n     342 200 224    
0000360   t   h   e       b   o   x       b   o   d   y  \n           /
0000400   /       i   s       j   u   s   t       t   h   e       c   o
0000420   u   n   t   e   r       +       s   l   o   t       l   i   s
0000440   t   .       E   a   c   h       s   l   o   t       h   a   s
0000460       a       u   s   e   r   -   r   e   n   a   m   a   b   l
```

## Assistant

Big-block edits are fragile. Switching strategy — splicing in the replacement section via shell so byte-matching can't fail.

## Assistant

_Tools:_

- `Write: /tmp/routing-helpers.txt`

## Assistant

_Tools:_

- `Bash: { head -n 324 /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js; cat /tmp/routing-helpers.txt; tail -n +507...`

### Tool result

```
File created successfully at: /tmp/routing-helpers.txt
```

### Tool result

```
1059 /tmp/inspector-new.js
syntax ok
```

## Assistant

_Tools:_

- `Bash: grep -n "iterator\|comparand\|cmp_btn\|cmp_inp\|iter_btn\|auto_grow_iterator\|is_iterator" /mnt/mtwo/programs/sora/so...`

### Tool result

```
328:  // today (plain / comparator / iterator); randomizer / weighted
351:    if (kind === 'comparator') return { kind: 'comparator', comparand: 0 };
352:    if (kind === 'iterator')   return { kind: 'iterator',   n_outputs: 2 };
363:    if (kind === 'iterator') {
370:      // so the schema (which requires ref on non-iterator call
382:  // {{{ async function set_comparand()
383:  async function set_comparand(n) {
385:    current_box.routing.comparand = n;
394:  // on each destination box's copy, same pattern as the iterator
397:    if (!current_box || routing_kind(current_box) !== 'iterator') return;
864:    // iterator toggle (issue 221) — flips between function-backed and
866:    const iter_btn = document.createElement('button');
867:    iter_btn.className   = 'toolbar-btn';
868:    iter_btn.textContent = is_iterator(box) ? 'iterator ×' : 'iterator';
869:    iter_btn.onclick = () => {
870:      if (is_iterator(current_box)) unmake_iterator();
871:      else make_iterator();
873:    fields.appendChild(mk_row('mode', iter_btn));
875:    // For iterator boxes, ref/fn/output are all replaced by the slot
878:    if (is_iterator(box)) {
895:      box.iterator_outputs.forEach((slot, i) => {
905:          rename_iterator_slot(i, inp.value.trim());
913:        rm.onclick     = () => remove_iterator_slot(i);
921:      add_btn.onclick     = () => add_iterator_slot();
1020:    const has_cmp = box.comparand !== undefined && box.comparand !== '';
1021:    const cmp_btn = document.createElement('button');
1022:    cmp_btn.className   = 'toolbar-btn';
1023:    cmp_btn.textContent = has_cmp ? 'comparing ×' : 'compare';
1024:    cmp_btn.onclick = async () => {
1031:      if (current_box.comparand !== undefined && current_box.comparand !== '') {
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
862	    }
863	
864	    // iterator toggle (issue 221) — flips between function-backed and
865	    // routing-primitive shapes; mutually exclusive with ref/fn/comparator
866	    const iter_btn = document.createElement('button');
867	    iter_btn.className   = 'toolbar-btn';
868	    iter_btn.textContent = is_iterator(box) ? 'iterator ×' : 'iterator';
869	    iter_btn.onclick = () => {
870	      if (is_iterator(current_box)) unmake_iterator();
871	      else make_iterator();
872	    };
873	    fields.appendChild(mk_row('mode', iter_btn));
874	
875	    // For iterator boxes, ref/fn/output are all replaced by the slot
876	    // list. Inputs still render below (one input wire feeds the
877	    // routing primitive). The render flow forks here.
878	    if (is_iterator(box)) {
879	      // inputs section (same as below; one input is typical but we
880	      // accept whatever the user has set)
881	      const in_sec = document.createElement('div');
882	      in_sec.className   = 'section-label';
883	      in_sec.textContent = 'inputs';
884	      fields.appendChild(in_sec);
885	      fields.appendChild(mk_port_display(box.inputs));
886	
887	      // outputs: editable list of slot names with × per slot, + at end
888	      const out_sec = document.createElement('div');
889	      out_sec.className   = 'section-label';
890	      out_sec.textContent = 'outputs (round-robin)';
891	      fields.appendChild(out_sec);
892	
893	      const slot_wrap = document.createElement('div');
894	      slot_wrap.className = 'port-display';
895	      box.iterator_outputs.forEach((slot, i) => {
896	        const row = document.createElement('div');
897	        row.className = 'port-display-row';
898	
899	        const inp = document.createElement('input');
900	        inp.type      = 'text';
901	        inp.value     = slot;
902	        inp.className = 'port-name-inp';
903	        inp.style.flex = '1';
904	        inp.addEventListener('change', () => {
905	          rename_iterator_slot(i, inp.value.trim());
906	        });
907	        row.appendChild(inp);
908	
909	        const rm = document.createElement('button');
910	        rm.className   = 'toolbar-btn';
911	        rm.textContent = '×';
912	        rm.title       = 'remove slot';
913	        rm.onclick     = () => remove_iterator_slot(i);
914	        row.appendChild(rm);
915	
916	        slot_wrap.appendChild(row);
917	      });
918	      const add_btn = document.createElement('button');
919	      add_btn.className   = 'toolbar-btn';
920	      add_btn.textContent = '+ slot';
921	      add_btn.onclick     = () => add_iterator_slot();
922	      slot_wrap.appendChild(add_btn);
923	      fields.appendChild(slot_wrap);
924	      return;
925	    }
926	
927	    // ref: read-only display showing just the basename (no path, no
928	    // extension), plus browse / view buttons. Full path stored on the
929	    // box still — the display is just a UI tightening since paths can
930	    // be long enough to overflow the sidebar.
931	    const ref_wrap = document.createElement('div');
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
1005	    // output: single wire; optional comparator splits into lt/eq/gt.
1006	    // Skipped entirely for sink boxes (functions with no return values,
1007	    // issue 226) — no output dot, no comparator toggle, nothing to wire.
1008	    if (box.has_output === false) {
1009	      return;
1010	    }
1011	
1012	    const out_sec = document.createElement('div');
1013	    out_sec.className   = 'section-label';
1014	    out_sec.textContent = 'output';
1015	    fields.appendChild(out_sec);
1016	
1017	    const cmp_row = document.createElement('div');
1018	    cmp_row.style.cssText = 'display:flex;gap:6px;align-items:center;margin-bottom:6px;';
1019	
1020	    const has_cmp = box.comparand !== undefined && box.comparand !== '';
1021	    const cmp_btn = document.createElement('button');
1022	    cmp_btn.className   = 'toolbar-btn';
1023	    cmp_btn.textContent = has_cmp ? 'comparing ×' : 'compare';
1024	    cmp_btn.onclick = async () => {
1025	      // Toggling the comparator changes the set of valid output ports
1026	      // (single null wire ↔ three lt/eq/gt wires). Wires from the old
1027	      // configuration would re-appear if we left them in the
1028	      // connections array, so clear all outgoing wires on either
1029	      // toggle direction.
1030	      await sever_output_wires();
1031	      if (current_box.comparand !== undefined && current_box.comparand !== '') {
1032	        delete current_box.comparand;
1033	      } else {
1034	        current_box.comparand = '0';
1035	      }
1036	      await save();
1037	      show(current_box, on_change_cb, on_delete_cb);
1038	      Canvas.mark_dirty();
1039	    };
1040	    cmp_row.appendChild(cmp_btn);
1041	
1042	    if (has_cmp) {
1043	      const cmp_inp = document.createElement('input');
1044	      cmp_inp.type        = 'text';
1045	      cmp_inp.value       = box.comparand || '';
1046	      cmp_inp.placeholder = 'number';
1047	      cmp_inp.style.cssText = 'flex:1;background:#0f1117;border:1px solid #2a2f45;' +
1048	        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
1049	      cmp_inp.addEventListener('input', () => { current_box.comparand = cmp_inp.value; save(); });
1050	      cmp_row.appendChild(cmp_inp);
1051	    }
1052	
1053	    fields.appendChild(cmp_row);
1054	  }
1055	  // }}}
1056	
1057	  return { show, hide, show_content, auto_grow_after_set,
1058	           auto_grow_iterator_after_connect };
1059	})();
```

## Assistant

_Tools:_

- `Write: /tmp/mode-block.txt`

### Tool result

```
File created successfully at: /tmp/mode-block.txt
```

## Assistant

_Tools:_

- `Bash: { head -n 863 /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js; cat /tmp/mode-block.txt; tail -n +926 /mnt...`

### Tool result

```
351:    if (kind === 'comparator') return { kind: 'comparator', comparand: 0 };
382:  // {{{ async function set_comparand()
383:  async function set_comparand(n) {
385:    current_box.routing.comparand = n;
1009:    const has_cmp = box.comparand !== undefined && box.comparand !== '';
1010:    const cmp_btn = document.createElement('button');
1011:    cmp_btn.className   = 'toolbar-btn';
1012:    cmp_btn.textContent = has_cmp ? 'comparing ×' : 'compare';
1013:    cmp_btn.onclick = async () => {
1020:      if (current_box.comparand !== undefined && current_box.comparand !== '') {
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js`

### Tool result

```
994	    // output: single wire; optional comparator splits into lt/eq/gt.
995	    // Skipped entirely for sink boxes (functions with no return values,
996	    // issue 226) — no output dot, no comparator toggle, nothing to wire.
997	    if (box.has_output === false) {
998	      return;
999	    }
1000	
1001	    const out_sec = document.createElement('div');
1002	    out_sec.className   = 'section-label';
1003	    out_sec.textContent = 'output';
1004	    fields.appendChild(out_sec);
1005	
1006	    const cmp_row = document.createElement('div');
1007	    cmp_row.style.cssText = 'display:flex;gap:6px;align-items:center;margin-bottom:6px;';
1008	
1009	    const has_cmp = box.comparand !== undefined && box.comparand !== '';
1010	    const cmp_btn = document.createElement('button');
1011	    cmp_btn.className   = 'toolbar-btn';
1012	    cmp_btn.textContent = has_cmp ? 'comparing ×' : 'compare';
1013	    cmp_btn.onclick = async () => {
1014	      // Toggling the comparator changes the set of valid output ports
1015	      // (single null wire ↔ three lt/eq/gt wires). Wires from the old
1016	      // configuration would re-appear if we left them in the
1017	      // connections array, so clear all outgoing wires on either
1018	      // toggle direction.
1019	      await sever_output_wires();
1020	      if (current_box.comparand !== undefined && current_box.comparand !== '') {
1021	        delete current_box.comparand;
1022	      } else {
1023	        current_box.comparand = '0';
1024	      }
1025	      await save();
1026	      show(current_box, on_change_cb, on_delete_cb);
1027	      Canvas.mark_dirty();
1028	    };
1029	    cmp_row.appendChild(cmp_btn);
1030	
1031	    if (has_cmp) {
1032	      const cmp_inp = document.createElement('input');
1033	      cmp_inp.type        = 'text';
1034	      cmp_inp.value       = box.comparand || '';
1035	      cmp_inp.placeholder = 'number';
1036	      cmp_inp.style.cssText = 'flex:1;background:#0f1117;border:1px solid #2a2f45;' +
1037	        'border-radius:3px;color:#e8eaf6;font-family:monospace;font-size:11px;padding:4px 6px;';
1038	      cmp_inp.addEventListener('input', () => { current_box.comparand = cmp_inp.value; save(); });
1039	      cmp_row.appendChild(cmp_inp);
1040	    }
1041	
1042	    fields.appendChild(cmp_row);
1043	  }
1044	  // }}}
1045	
1046	  return { show, hide, show_content, auto_grow_after_set,
1047	           auto_grow_iterator_after_connect };
1048	})();
```

## Assistant

_Tools:_

- `Write: /tmp/output-block.txt`

### Tool result

```
File created successfully at: /tmp/output-block.txt
```

## Assistant

_Tools:_

- `Bash: { head -n 993 /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js; cat /tmp/output-block.txt; } > /tmp/inspec...`

### Tool result

```
1027 /mnt/mtwo/programs/sora/soramech/assets/js/004-inspector.js
351:    if (kind === 'comparator') return { kind: 'comparator', comparand: 0 };
382:  // {{{ async function set_comparand()
383:  async function set_comparand(n) {
385:    current_box.routing.comparand = n;
1013:      cmp_inp.value       = String((box.routing && box.routing.comparand) ?? 0);
1019:        if (!isNaN(v)) set_comparand(v);
1021:      fields.appendChild(mk_row('comparand', cmp_inp));
```

### Tool result

```
240	    if (Inspector.auto_grow_after_set) {
241	      await Inspector.auto_grow_after_set(dst_box, to_input);
242	    }
243	    // Same idea on the source side: an iterator box (issue 221) auto-
244	    // appends a new placeholder slot when the user wires from its last
245	    // slot. The helper is a no-op if from_branch isn't the last slot
246	    // (or the box isn't an iterator).
247	    if (Inspector.auto_grow_iterator_after_connect && from_branch) {
248	      await Inspector.auto_grow_iterator_after_connect(src_box, from_branch);
249	    }
250	  }
251	  // }}}
252	
253	  // {{{ delete_connection
254	  // Same in-place mutation pattern as create_connection — never
255	  // replace the cached references.
256	  async function delete_connection(conn) {
257	    const src_box = Boxes.boxes[conn.from_box];
258	    const dst_box = Boxes.boxes[conn.to_box];
259	    if (!src_box || !dst_box) {
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js`

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/006-wires.js has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/assets/js/005-app.js`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/assets/js/005-app.js has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: ls /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/ && echo "---" && cat /mnt/mtwo/programs/sora/soramech/maps/hell...`

### Tool result

```
box-moywlsdd.json
box-mp8s1rsw.json
box-mpc0vo9d.json
box-mpc0wnjw.json
box-mpc13kds.json
box-mpc1d2x0.json
---
{"id":"box-moywlsdd","label":"New Box","kind":"call","inputs":[{"name":"text","type":"any"}],"connections":[{"from_box":"write-result","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"},{"from_box":"box-mox3ipzr","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"}],"ui":{"x":994,"y":220},"iterator_outputs":["output_0"]}
```

## Assistant

_Tools:_

- `Bash: grep -l "comparand\|iterator_outputs" /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/*.json`

### Tool result

```
/mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-moywlsdd.json
```

## Assistant

_Tools:_

- `Bash: cat /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mp8s1rsw.json /mnt/mtwo/programs/sora/soramech/maps/hello/b...`

### Tool result

```
{"id":"box-mp8s1rsw","label":"New Box","kind":"data","ref":"","fn":"","inputs":[],"connections":[{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc0vo9d","to_input":"prompt"},{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc0wnjw","to_input":"prompt"},{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc13kds","to_input":"prompt"}],"ui":{"x":75,"y":228}}{"id":"box-mpc0vo9d","label":"New Box","kind":"call","ref":"libs/ollama.lua","fn":"query","inputs":[{"name":"prompt","type":"any"},{"name":"model","type":"any"},{"name":"host","type":"any","value":"192.168.1.11"}],"connections":[{"from_box":"box-mpc0vo9d","from_branch":null,"to_box":"write-result","to_input":"text"},{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc0vo9d","to_input":"prompt"},{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc0vo9d","to_input":"model"}],"ui":{"x":402,"y":238}}
```

## Assistant

_Tools:_

- `Bash: for f in /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/*.json; do echo "=== $(basename $f) ==="; cat "$f"; echo; ...`

### Tool result

```
=== box-moywlsdd.json ===
{"id":"box-moywlsdd","label":"New Box","kind":"call","inputs":[{"name":"text","type":"any"}],"connections":[{"from_box":"write-result","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"},{"from_box":"box-mox3ipzr","from_branch":null,"to_box":"box-moywlsdd","to_input":"text"}],"ui":{"x":994,"y":220},"iterator_outputs":["output_0"]}
=== box-mp8s1rsw.json ===
{"id":"box-mp8s1rsw","label":"New Box","kind":"data","ref":"","fn":"","inputs":[],"connections":[{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc0vo9d","to_input":"prompt"},{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc0wnjw","to_input":"prompt"},{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc13kds","to_input":"prompt"}],"ui":{"x":75,"y":228}}
=== box-mpc0vo9d.json ===
{"id":"box-mpc0vo9d","label":"New Box","kind":"call","ref":"libs/ollama.lua","fn":"query","inputs":[{"name":"prompt","type":"any"},{"name":"model","type":"any"},{"name":"host","type":"any","value":"192.168.1.11"}],"connections":[{"from_box":"box-mpc0vo9d","from_branch":null,"to_box":"write-result","to_input":"text"},{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc0vo9d","to_input":"prompt"},{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc0vo9d","to_input":"model"}],"ui":{"x":402,"y":238}}
=== box-mpc0wnjw.json ===
{"id":"box-mpc0wnjw","label":"New Box","kind":"call","ref":"libs/ollama.lua","fn":"query","inputs":[{"name":"prompt","type":"any"},{"name":"model","type":"any"},{"name":"host","type":"any","value":"192.168.1.12"}],"connections":[{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc0wnjw","to_input":"prompt"},{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc0wnjw","to_input":"model"}],"ui":{"x":410,"y":380}}
=== box-mpc13kds.json ===
{"id":"box-mpc13kds","label":"New Box","kind":"call","ref":"libs/ollama.lua","fn":"query","inputs":[{"name":"prompt","type":"any"},{"name":"model","type":"any"},{"name":"host","type":"any","value":"192.168.1.13"}],"connections":[{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc13kds","to_input":"prompt"},{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc13kds","to_input":"model"}],"ui":{"x":419,"y":509}}
=== box-mpc1d2x0.json ===
{"id":"box-mpc1d2x0","label":"New Box","kind":"data","ref":"","fn":"","inputs":[],"connections":[{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc0vo9d","to_input":"model"},{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc0wnjw","to_input":"model"},{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc13kds","to_input":"model"}],"ui":{"x":75,"y":360}}
```

### Tool result

```
<tool_use_error>File has not been read yet. Read it first before writing to it.</tool_use_error>
```

## Assistant

Migrating the four call boxes in `maps/hello/`. The two `kind:"data"` boxes are 229's outstanding debt, separate from 233's scope.

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-moywlsdd.json`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-moywlsdd.json has been updated successfully.
```

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0vo9d.json`

### Tool result

```
<tool_use_error>File has not been read yet. Read it first before writing to it.</tool_use_error>
```

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0wnjw.json`

### Tool result

```
<tool_use_error>File has not been read yet. Read it first before writing to it.</tool_use_error>
```

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc13kds.json`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0vo9d.json`

### Tool result

```
1	{"id":"box-mpc0vo9d","label":"New Box","kind":"call","ref":"libs/ollama.lua","fn":"query","inputs":[{"name":"prompt","type":"any"},{"name":"model","type":"any"},{"name":"host","type":"any","value":"192.168.1.11"}],"connections":[{"from_box":"box-mpc0vo9d","from_branch":null,"to_box":"write-result","to_input":"text"},{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc0vo9d","to_input":"prompt"},{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc0vo9d","to_input":"model"}],"ui":{"x":402,"y":238}}
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0wnjw.json`

### Tool result

```
1	{"id":"box-mpc0wnjw","label":"New Box","kind":"call","ref":"libs/ollama.lua","fn":"query","inputs":[{"name":"prompt","type":"any"},{"name":"model","type":"any"},{"name":"host","type":"any","value":"192.168.1.12"}],"connections":[{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc0wnjw","to_input":"prompt"},{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc0wnjw","to_input":"model"}],"ui":{"x":410,"y":380}}
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc13kds.json`

### Tool result

```
1	{"id":"box-mpc13kds","label":"New Box","kind":"call","ref":"libs/ollama.lua","fn":"query","inputs":[{"name":"prompt","type":"any"},{"name":"model","type":"any"},{"name":"host","type":"any","value":"192.168.1.13"}],"connections":[{"from_box":"box-mp8s1rsw","from_branch":null,"to_box":"box-mpc13kds","to_input":"prompt"},{"from_box":"box-mpc1d2x0","from_branch":null,"to_box":"box-mpc13kds","to_input":"model"}],"ui":{"x":419,"y":509}}
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0vo9d.json`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0vo9d.json has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0wnjw.json`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc0wnjw.json has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc13kds.json`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/maps/hello/boxes/box-mpc13kds.json has been updated successfully.
```

## Assistant

Creating the four follow-on issue files per 233's plan.

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/240-routing-kind-randomizer.md`

### Tool result

```
File created successfully at: /mnt/mtwo/programs/sora/soramech/issues/240-routing-kind-randomizer.md
```

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/241-routing-kind-weighted.md`

### Tool result

```
File created successfully at: /mnt/mtwo/programs/sora/soramech/issues/241-routing-kind-weighted.md
```

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/242-routing-kind-distributor.md`

### Tool result

```
File created successfully at: /mnt/mtwo/programs/sora/soramech/issues/242-routing-kind-distributor.md
```

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/243-routing-multi-band-comparator.md`

### Tool result

```
File created successfully at: /mnt/mtwo/programs/sora/soramech/issues/243-routing-multi-band-comparator.md
```

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/233-unified-routing-schema.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/233-unified-routing-schema.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/phase-2-progress.md`

## Assistant

_Tools:_

- `Bash: git mv issues/233-unified-routing-schema.md issues/completed/ && git add -A assets/js/002-boxes.js assets/js/004-insp...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -25`

### Tool result

```
Exit code 1
The following paths are ignored by one of your .gitignore files:
maps
hint: Use -f if you really want to add them.
hint: Disable this message with "git config set advice.addIgnoredFile false"
```

### Tool result

```
<tool_use_error>Cancelled: parallel tool call Bash(git mv issues/233-unified-routing-schema…) errored</tool_use_error>
```

## Assistant

_Tools:_

- `Bash: grep -n "^maps\|/maps" /mnt/mtwo/programs/sora/soramech/.gitignore`

### Tool result

```
16:tests/maps/*/compiled/
26:# Path is anchored with `/` so test fixtures under tests/maps/ (used
28:/maps/
```

## Assistant

_Tools:_

- `Bash: git mv issues/233-unified-routing-schema.md issues/completed/ && git add assets/js/002-boxes.js assets/js/004-inspect...`

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -25`

### Tool result

```
Exit code 128
fatal: bad source, source=issues/233-unified-routing-schema.md, destination=issues/completed/233-unified-routing-schema.md
```

### Tool result

```
<tool_use_error>Cancelled: parallel tool call Bash(git mv issues/233-unified-routing-schema…) errored</tool_use_error>
```

## Assistant

_Tools:_

- `Bash: git status --short | head -30 && echo "---" && ls issues/233-unified-routing-schema.md issues/completed/233-unified-r...`

### Tool result

```
Exit code 2
M  assets/js/002-boxes.js
M  assets/js/004-inspector.js
M  assets/js/005-app.js
M  assets/js/006-wires.js
A  issues/240-routing-kind-randomizer.md
A  issues/241-routing-kind-weighted.md
A  issues/242-routing-kind-distributor.md
A  issues/243-routing-multi-band-comparator.md
R  issues/233-unified-routing-schema.md -> issues/completed/233-unified-routing-schema.md
M  issues/phase-2-progress.md
M  src/001-schema.lua
M  src/004-executor.lua
---
ls: cannot access 'issues/233-unified-routing-schema.md': No such file or directory
issues/completed/233-unified-routing-schema.md
```

## Assistant

_Tools:_

- `Bash: make test 2>&1 | tail -25`

### Tool result

```
  scalars                                      ok
  arrays                                       ok
  objects                                      ok
  truncation                                   lua spec: native_to_json: output buffer (4 bytes) too small
ok
  parse_error                                  lua spec: json_to_native: parse error at byte 1: expected string
ok

  5 passed, 0 failed

integration tests (fixture maps):
  calc                                         ok
  hello                                        ok
  comparator                                   ok
  iter-route                                   ok
  pipeline                                     ok
  read-literal                                 ok
  pipeline write output                        ok
  compile pipeline (portable run)              ok
  232-lua-parser-test.mjs                      ok
  233-bash-parser-test.mjs                     ok
  234-language-spec-js-test.mjs                ok

  11 passed, 0 failed
make: *** [Makefile:87: test] Error 1
```

## Assistant

_Tools:_

- `Bash: make test 2>&1 | grep -E "FAIL|failed|Error|error|0 \w+, [^0]" | head -20`

### Tool result

```
  16 passed, 0 failed
  14 passed, 0 failed
  5 passed, 0 failed
  7 passed, 0 failed
  3 passed, 0 failed
  4 passed, 0 failed
  8 passed, 0 failed
  13 passed, 0 failed
  11 passed, 0 failed
FAIL
  1 passed, 1 failed
  5 passed, 0 failed
  compile_bad_source_fails                     /tmp/soramech-bad-16611.c:1:1: error: unknown type name 'not'
/tmp/soramech-bad-16611.c:1:10: error: expected '=', ',', ';', 'asm' or '__attribute__' before 'close'
/tmp/soramech-bad-16611.c:1:10: error: unknown type name 'close'
c spec: gcc failed for '/tmp/soramech-bad-16611.c' → '/tmp/soramech-bad-16611.so' (status=256)
  5 passed, 0 failed
  5 passed, 0 failed
  error_positions                              ok
  parse_file_error_line                        ok
```

## Assistant

_Tools:_

- `Bash: make test 2>&1 | grep -B 1 "FAIL\|1 failed"`

### Tool result

```
  handles_populated                                  /mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c:120: seen_per_thread[i] > 0
FAIL
--

  1 passed, 1 failed
```

## Assistant

_Tools:_

- `Bash: ./build/tests/303-pool-spec-init-test 2>&1 | tail -10 && echo "---retry---" && ./build/tests/303-pool-spec-init-test ...`

### Tool result

```
303-pool-spec-init-test:
  handles_populated                                  /mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c:120: seen_per_thread[i] > 0
FAIL
  handles_distinct_per_worker                  ok

  1 passed, 1 failed
---retry---
303-pool-spec-init-test:
  handles_populated                                  /mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c:120: seen_per_thread[i] > 0
FAIL
  handles_distinct_per_worker                  ok

  1 passed, 1 failed
---retry2---
303-pool-spec-init-test:
  handles_populated                                  /mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c:120: seen_per_thread[i] > 0
FAIL
  handles_distinct_per_worker                  ok

  1 passed, 1 failed
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c`

### Tool result

```
100	    enum { N_TASKS = 64 };
101	    for (int i = 0; i < N_TASKS; i++) pool_spawn(p, observe_action, NULL, 0);
102	    pool_wait_quiescent(p);
103	
104	    int n = atomic_load(&g_obs_count);
105	    ASSERT(n == N_TASKS);
106	
107	    int observed = n < MAX_OBS ? n : MAX_OBS;
108	
109	    /* Every observation must have a non-NULL Lua handle and a
110	     * thread_idx in range. */
111	    int seen_per_thread[POOL_MAX_WORKERS] = {0};
112	    for (int i = 0; i < observed; i++) {
113	        ASSERT(g_obs[i].thread_idx >= 0);
114	        ASSERT(g_obs[i].thread_idx < pool_n_workers(p));
115	        ASSERT(g_obs[i].lua_handle != NULL);
116	        if (g_obs[i].thread_idx < POOL_MAX_WORKERS)
117	            seen_per_thread[g_obs[i].thread_idx]++;
118	    }
119	    /* All four workers should have been observed at least once. */
120	    for (int i = 0; i < pool_n_workers(p); i++) ASSERT(seen_per_thread[i] > 0);
121	
122	    pool_destroy(p);
123	    spec_registry_destroy(r);
124	    return 1;
125	}
126	/* }}} */
127	
128	/* {{{ test_handles_distinct_per_worker() */
129	/* Each worker should get its own lua_State, not a shared one. */
130	static int test_handles_distinct_per_worker(void)
131	{
132	    char *err = NULL;
133	    spec_registry_t *r = spec_registry_load("langs", &err);
134	    g_lua_idx = -1;
135	    for (int i = 0; i < spec_registry_size(r); i++) {
136	        if (strcmp(spec_registry_at(r, i)->name, "lua") == 0) {
137	            g_lua_idx = i; break;
138	        }
139	    }
```

## Assistant

_Tools:_

- `Bash: git stash && ./build/tests/303-pool-spec-init-test 2>&1 | tail -5 && git stash pop`

### Tool result

```
Saved working directory and index state WIP on master: a2d9df4 239 — literal-bound input ports render as a nodule, name in the gutter
  handles_populated                                  /mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c:120: seen_per_thread[i] > 0
FAIL
  handles_distinct_per_worker                  ok

  1 passed, 1 failed
On branch master
Changes to be committed:
  (use "git restore --staged <file>..." to unstage)
	new file:   issues/240-routing-kind-randomizer.md
	new file:   issues/241-routing-kind-weighted.md
	new file:   issues/242-routing-kind-distributor.md
	new file:   issues/243-routing-multi-band-comparator.md
	new file:   issues/completed/233-unified-routing-schema.md

Changes not staged for commit:
  (use "git add/rm <file>..." to update what will be committed)
  (use "git restore <file>..." to discard changes in working directory)
	modified:   assets/js/002-boxes.js
	modified:   assets/js/004-inspector.js
	modified:   assets/js/005-app.js
	modified:   assets/js/006-wires.js
	deleted:    issues/233-unified-routing-schema.md
	modified:   issues/phase-2-progress.md
	modified:   src/001-schema.lua
	modified:   src/004-executor.lua

Dropped refs/stash@{0} (c5964fc75c09dfa7e06482f7542142b70699110f)
```

## Assistant

_Tools:_

- `Bash: git stash && make test 2>&1 | grep -B 1 "FAIL\|test failed\|1 failed" | head -10 && echo "---restoring---" && git sta...`

### Tool result

```
Saved working directory and index state WIP on master: a2d9df4 239 — literal-bound input ports render as a nodule, name in the gutter
---restoring---
ok
```

## Assistant

_Tools:_

- `Bash: for i in 1 2 3 4 5; do ./build/tests/303-pool-spec-init-test 2>&1 | grep -E "passed.*failed|FAIL" | tail -1; done`

### Tool result

```
  1 passed, 1 failed
  1 passed, 1 failed
  2 passed, 0 failed
  1 passed, 1 failed
  1 passed, 1 failed
```

## Assistant

_Tools:_

- `Bash: nproc && echo "---" && grep -B 1 -A 6 "test_handles_populated\|pool_create" /mnt/mtwo/programs/sora/soramech/tests/30...`

### Tool result

```
14
---

/* {{{ test_handles_populated() */
static int test_handles_populated(void)
{
    char *err = NULL;
    spec_registry_t *r = spec_registry_load("langs", &err);
    ASSERT(r);

    /* Find the lua spec's index. */
--

    pool_t *p = pool_create(4);
    ASSERT(p);
    pool_set_worker_init(p, spec_pool_init_cb, r);
    ASSERT(pool_init_barrier(p) == 0);

    /* Spawn enough tasks that every worker sees one. observe_action
     * only records into the first MAX_OBS slots; iterate just
--

    pool_t *p = pool_create(4);
    pool_set_worker_init(p, spec_pool_init_cb, r);
    ASSERT(pool_init_barrier(p) == 0);

    atomic_store(&g_obs_count, 0);
    /* Burst enough tasks to cover every worker. */
    for (int i = 0; i < 200; i++) pool_spawn(p, observe_action, NULL, 0);
```

## Assistant

_Tools:_

- `Bash: grep -n "MAX_OBS" /mnt/mtwo/programs/sora/soramech/tests/303-pool-spec-init-test.c`

### Tool result

```
55:#define MAX_OBS 32
59:} g_obs[MAX_OBS];
67:    if (slot < MAX_OBS) {
97:     * only records into the first MAX_OBS slots; iterate just
107:    int observed = n < MAX_OBS ? n : MAX_OBS;
155:    for (int i = 0; i < n && i < MAX_OBS; i++) {
```

## Assistant

_Tools:_

- `Bash: git log --oneline -- tests/303-pool-spec-init-test.c | head -10`

### Tool result

```
30a7b6d 303 — language-spec registry, dlopen at startup, per-worker handles
```

## Assistant

_Tools:_

- `Bash: for i in 1 2 3 4 5 6 7 8 9 10; do ./build/tests/303-pool-spec-init-test 2>&1 | grep -E "passed.*failed" | tail -1; done`

### Tool result

```
  1 passed, 1 failed
  1 passed, 1 failed
  1 passed, 1 failed
  1 passed, 1 failed
  1 passed, 1 failed
  1 passed, 1 failed
  2 passed, 0 failed
  1 passed, 1 failed
  1 passed, 1 failed
  2 passed, 0 failed
```

## Assistant

_Tools:_

- `Bash: git stash && make test 2>&1 | tail -5 && git stash pop > /dev/null && echo "ok"`

### Tool result

```
Saved working directory and index state WIP on master: a2d9df4 239 — literal-bound input ports render as a nodule, name in the gutter
  233-bash-parser-test.mjs                     ok
  234-language-spec-js-test.mjs                ok

  11 passed, 0 failed
make: *** [Makefile:87: test] Error 1
ok
```

## Assistant

The 303 pool-init test is flaky on master (4 workers, 32-observation cap, statistical race). Confirmed pre-existing — failure reproduces with my changes stashed away. Not caused by 233; recording it as a separate concern.

Committing 233.

## Assistant

_Tools:_

- `Bash: git status --short && echo "---" && git diff --stat --cached | tail -15`

### Tool result

```
 M assets/js/002-boxes.js
 M assets/js/004-inspector.js
 M assets/js/005-app.js
 M assets/js/006-wires.js
 D issues/233-unified-routing-schema.md
A  issues/240-routing-kind-randomizer.md
A  issues/241-routing-kind-weighted.md
A  issues/242-routing-kind-distributor.md
A  issues/243-routing-multi-band-comparator.md
A  issues/completed/233-unified-routing-schema.md
 M issues/phase-2-progress.md
 M src/001-schema.lua
 M src/004-executor.lua
---
 issues/240-routing-kind-randomizer.md          |  98 ++++++
 issues/241-routing-kind-weighted.md            | 116 +++++++
 issues/242-routing-kind-distributor.md         | 103 ++++++
 issues/243-routing-multi-band-comparator.md    | 139 ++++++++
 issues/completed/233-unified-routing-schema.md | 425 +++++++++++++++++++++++++
 5 files changed, 881 insertions(+)
```

## Assistant

_Tools:_

- `Bash: git add assets/js/002-boxes.js assets/js/004-inspector.js assets/js/005-app.js assets/js/006-wires.js src/001-schema....`

### Tool result

```
M  assets/js/002-boxes.js
M  assets/js/004-inspector.js
M  assets/js/005-app.js
M  assets/js/006-wires.js
A  issues/240-routing-kind-randomizer.md
A  issues/241-routing-kind-weighted.md
A  issues/242-routing-kind-distributor.md
A  issues/243-routing-multi-band-comparator.md
R  issues/233-unified-routing-schema.md -> issues/completed/233-unified-routing-schema.md
M  issues/phase-2-progress.md
M  src/001-schema.lua
M  src/004-executor.lua
```

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
233 — unified routing schema for branching boxes

Every call box now carries a `routin...`

### Tool result

```
[master 2d5780a] 233 — unified routing schema for branching boxes
 12 files changed, 740 insertions(+), 310 deletions(-)
 create mode 100644 issues/240-routing-kind-randomizer.md
 create mode 100644 issues/241-routing-kind-weighted.md
 create mode 100644 issues/242-routing-kind-distributor.md
 create mode 100644 issues/243-routing-multi-band-comparator.md
 rename issues/{ => completed}/233-unified-routing-schema.md (96%)
```

## Assistant

_Tools:_

- `Bash: git log --oneline -3 && echo "---open issues---" && ls /mnt/mtwo/programs/sora/soramech/issues/ | grep -v completed |...`

### Tool result

```
2d5780a 233 — unified routing schema for branching boxes
a2d9df4 239 — literal-bound input ports render as a nodule, name in the gutter
a46be72 217 — variadic toggle gates on the language, not on the function
---open issues---
232-test-coverage-map.md
240-routing-kind-randomizer.md
241-routing-kind-weighted.md
242-routing-kind-distributor.md
243-routing-multi-band-comparator.md
302-wire-value-slot-store.md
304-task-dispatch-layer.md
305-c-graph-loader.md
306-lua-language-spec.md
307-c-language-spec.md
311-integration-tests-and-run-output.md
312-same-language-wire-fast-path.md
313-research-whole-program-same-language-merge.md
315-compiled-artifact-reference-counting.md
317-spec-json-bridge-for-data-boxes.md
```

## Assistant

233 closed in `2d5780a`. Trinity: source (schema + executor + 4 JS files + 1 user-map migration that doesn't track in git) + closed issue + progress doc + four follow-on issue files for the kinds 233 designed but didn't ship.

A few things to flag before we keep moving:

- **Pre-existing test flake**. `tests/303-pool-spec-init-test::handles_populated` fails ~80% of the time on this machine (4-worker pool spawning only 64 tasks, with a 32-observation cap — a timing race, not a logic bug). Reproduces on a stashed-clean HEAD before any of my work, so it's not from 233. Worth fixing separately; the obvious mechanical fixes are bumping `N_TASKS` and `MAX_OBS` so every worker gets observed with high confidence. If you want me to file that as a small issue and fix it next, say so.

- **`maps/hello/` migration**. Four of its six boxes got hand-migrated to `routing: { kind: ... }`. Two remaining boxes still carry `kind: "data"` — that's issue 229's debt (data was renamed to read/write); not in 233's scope but the map is still half-broken under the strict schema. Flagging so it doesn't get lost. Those files are under `/maps/` which is gitignored, so the migration isn't part of the commit — it's a local-state fix that anyone with their own editor data needs to do themselves.

- **UI verification**. Same caveat as 217/239 — no browser load. The mode dropdown's transitions and the iterator's n_outputs control are well-formed JS but the visual presentation isn't verified.

**Open issues now: 15.** The four new follow-on issues (240–243) net the count up despite closing 233.

Where next? Three plausible angles:

1. **Fix the 303 test flake** — small, mechanical, gets `make test` reliably green which makes every subsequent closure easier to trust.
2. **232** — write `scripts/run-unit-tests.sh`, the dev-loop runner the issue spec'd out.
3. **Pick a design-question issue** (302, 307, 312) — these are the bigger reshapes that need your direction first.

Which?

## User

I installed playwrite. Next time you need to test something visually, we might be able to!

Next let's pull up the design questions. Can you help me resolve all the unclear issues?

## Assistant

Good — playwright will help. For now, let's settle the design questions. There are six unresolved decisions across the three issues. I'll lay each out with options + my recommendation + my questions back.

---

## 302 — does the allocator-unification commitment stand?

The "Retraction of the monotonic-growth deferral" entry rewrote the whole design to a unified refcount allocator with eager neighbor-merge and deep sweep. None of it is implemented; the current slot store mallocs per slot and the LVH grows monotonically. The retraction reads strongly: "monotonic growth is a memory leak by design."

**Options:**

A) **Ship the retracted design.** Big lift — refcount increments/decrements thread through every place that hands a value pointer (push, peek, pop, dispatch, spec invoke). Address-ordered chunk list, size-class lists, deep-sweep at allocation-failure-time and at quiescence. Probably a week of careful work; touches every consumer.

B) **Walk the retraction back.** Keep the current implementation. The leak only matters for long-running workloads; we don't have one yet.

C) **Split.** Close 302 on what shipped (slot store + LVH). File a new issue (say 318) for the allocator rewrite. The retraction entry was right that monotonic growth is bad; the fix doesn't have to live inside 302's implementation log.

**My read: C.** Aligns with the blueprint philosophy — 302's issue file describes the slot store as it stands, and the allocator-rewrite issue file describes its own complete design from scratch. Combining them in one file makes both designs harder to read. Splitting also lets the new issue carry the right *dependency story*: it blocks on a real long-running workload existing, which is presently the case for nothing.

**Question back:** is there a workload you're imagining for the next 1-2 weeks that would actually exercise the leak? If yes, A becomes urgent. If no, the split-and-defer is honest about timing.

---

## 307 — typed wrapper generation, or keep the uniform signature?

The design says a C box author writes `int classify(const char *text, int weight)` and the spec generates a wrapper that unpacks bytes into typed args. Today the author writes `int classify(const void **inputs, const int *sizes, int n, void *out_buf, int out_cap, int *out_size)` — explicit SoraMech boilerplate in the user's source.

This is the strongest "code disagrees with design" gap in phase 3. The issue file states the rule as non-negotiable: *"SoraMech bends to the language. The language never bends to SoraMech."* The current code literally has the language bending to SoraMech.

**Options:**

A) **Ship wrapper generation.** Bounded scope: a per-box `.c` emitter that reads `box.inputs` and `box.returns` from JSON, generates a `sm_box_entry` wrapper that unpacks bytes → typed args → user fn → bytes, compiles wrapper.c + user.c into one `.so`. Marshalling table: `int → atoi`, `double → strtod`, `bytes → (const void *)`, `json → cJSON_Parse`, etc.

B) **Accept the current uniform signature as the design.** Rewrite the issue file's "any C function, no boilerplate" prose to describe the uniform signature. The current code becomes the spec.

**My read: A.** Two reasons:
1. The principle is the project's stated identity. Conceding it for C means conceding it for every future language — Lua and Bash currently DO honor it (the user writes idiomatic Lua/Bash and the spec marshals).
2. **A unblocks 317.** The data-box JSON bridge for C is stuck because the C bridge can't walk a struct without a type system. If 307 ships the type system, 317's C bridge has the input it needs.

The cost is real but bounded — wrapper generation is ~150 lines of C-emitting C, plus a marshalling table per supported type.

**Question back:** are there any C boxes that *need* the raw signature (e.g., functions that operate directly on the byte buffer for performance)? If yes, the design might want a `"raw_signature": true` opt-out. If everyone is happy with typed signatures, A is unconditionally right.

---

## 312 — two questions

### (a) per-edge vs per-box classification

Currently: one bit per box (`use_native_invoke = true` iff every neighbor is same-lang).
Designed: per-edge arrays (`input_edge_native[i]`, `output_edge_native[j]`).

Per-edge dominates for the case "Lua sub-graph with one C neighbor." Under per-box, that single C neighbor forces the entire Lua sub-graph to round-trip through JSON. Per-edge keeps the cost local to the actual cross-lang boundary.

**My read: per-edge.** Costs are tiny (a few extra bytes per box, one extra `if` per dispatch path). Win is concrete for mixed-lang graphs, which are presumably common.

**Question back:** is mixed-lang a deliberate design pattern you'll use, or do most maps end up single-lang? If maps end up single-lang in practice, the per-edge upgrade is over-engineering. The "Lua sub-graph with one C neighbor" example assumes a specific shape — does that shape happen?

### (b) type-annotated wire encodings vs JSON-everywhere

The issue explicitly flags this: *"Before implementing this issue, the implementer must ask the user to decide."* Three concrete options:

**Option A — binary type-annotated wire.** Primitives go raw bytes (4-byte int as 4 bytes, 8-byte double as 8 bytes), strings as length-prefix UTF-8, structures as JSON. A 1-byte type tag at the head of each value tells the reader which form to expect. Faster for primitives. Requires endianness convention. Every spec must know how to read/write each type.

**Option B — JSON everywhere, no type tags.** Plain JSON. Lua's existing bridges already work this way (and already use the `n` field convention to preserve array fidelity per 317's stage 2). Simplest, slowest for primitives, lossy for int-vs-float without context.

**Option C — JSON with optional `$type` wrapper.** A value can be plain JSON (`42`, `"hello"`, `[1,2,3]`) OR a tagged wrapper (`{"$type": "int32", "$value": 42}`) when the destination spec needs type info beyond JSON's native types. 317's stage 2 fidelity directive already gestures at this: *"the bridge may emit a type-tagged JSON shape when the destination language needs more than JSON's native types convey."*

**My read: C.** Reasons:
1. Keeps Lua's existing work valid (it speaks JSON, with optional tags).
2. No endianness or byte-order spec to write.
3. Primitive performance hit is small; large structured values are JSON-shaped natively anyway.
4. Type fidelity preserved via `$type` when needed (the int-vs-float-vs-bool problem).
5. Subsumes 317's fidelity directive cleanly — same mechanism.

**Question back:** do you anticipate workloads where primitive-encoding overhead actually matters? E.g., a tight Lua→C loop pushing millions of small ints. If yes, Option A's binary path has a use case. If the dominant case is "send a structured value across a language boundary occasionally," Option C is unconditionally right.

---

**TL;DR for your scanning:**

| Issue | My recommendation | Open question I need from you |
|---|---|---|
| 302 | Split: close 302 on shipped state, file new issue for the allocator rewrite | Is there a long-running workload coming soon? |
| 307 | Ship wrapper generation | Any C boxes that need the raw signature? |
| 312(a) | Per-edge classification | Are mixed-lang maps actually a design pattern you use? |
| 312(b) | JSON + optional `$type` wrapper | Any primitive-heavy hot loops crossing lang boundaries? |

Answer in any order — I'll take whichever you have a clear read on first and we can keep moving.

## User

  (a) per-edge vs per-box classification

let's do per-edge.

  (b) type-annotated wire encodings vs JSON-everywhere

so, setting aside the cross-language spec system for a moment, the thread pool is designed such that each input value and each output value has a spot in shared memory that is reserved for it. If it's a value with indeterminate size, then it has a spot that's large enough to store a pointer, and we will store whatever values it tries to save / return from it's function in the heap (shared memory) and make the pointer point to it. We will maintain a length as well, just before where the pointer points to. If the large value must be translated to json to cross language boundaries, it'll include the length of the string and the pointer to the location in the json - nothing else is needed. The text already exists in the shared memory, we just have to pass a pointer to it. Primitive values like numbers or booleans or whatever can be stored with type annotations in the json, just in-case the receiving language uses them. C uses them, Lua does not, but we should still provide them for the spec's parser to use if they please. Does that answer your question? Can you apply those insights anywhere else?

the reason we pre-allocate space for the input values is so that another thread, running another box, can store it's return value in those input slots. We have a ring buffer for each of them, so that if multiple values are stored in the same slot, we can address them one-at-a-time as needed. It is common for a box to have 5 values waiting for it's first input, and run itself once every time it's second input has a value provided. These input value slot ring buffers are FIFO, so we can expect deterministic outputs even though they are queued, ready and waiting.

## Assistant

Yes — and the elaboration is sharper than what I had as Option C. Three things you've actually settled, not just two:

**312(a):** per-edge classification. Done.

**312(b):** JSON-everywhere, BUT large values never get inlined as JSON strings. The wire carries `{"$ref": <ptr>, "$len": <bytes>}` (or similar) pointing into shared memory. The consumer's spec reads the bytes directly. JSON encode + parse cost is bounded forever — tiny for primitives, ~50 bytes for any payload regardless of how large the payload actually is.

**Primitive type annotations are optional.** A JSON value is plain when the receiver doesn't care (Lua), tagged when the receiver does (C). The producer's spec emits whichever form the destination needs — which the per-edge classification already knows because it identifies destination language.

This is bigger than just resolving 312. Tracing where the insights apply:

---

**317 fidelity directive — simplifies.** The Lua bridge today JSON-encodes Lua strings inline. Under your model, a Lua string above some size threshold gets a `$ref` handle and the JSON stays small. The `n`-field array convention you already shipped still applies for structured values (small arrays inline JSON; large arrays land in shared memory with a ref). The directive's coerce-and-warn rules also still apply — they're about type mismatches at the receiving end, not about wire form.

**313 (whole-program merge) — significantly less attractive.** Same-lang neighbors already pay near-zero (no JSON at all under per-edge classification). Cross-lang JSON cost is now bounded to a small handle for any size. The merge optimization was selling "skip JSON for same-lang interior" — that win is already captured by 312 itself. Whatever's left for the merge is inlining/optimizer-driven, which is much harder to motivate. Probably worth marking 313 as "shelved" rather than "research-pending" — its premise is gone.

**307 wrapper generation — the type table becomes interesting.** The C wrapper's input marshalling table (int → atoi, bytes → const void *, json → cJSON_Parse, etc.) now also needs to know about `$ref` values. A `bytes` input on a C box: the wrapper checks if the JSON value is a `$ref`, follows the pointer + length, hands the raw bytes to the user's function. Same shape for `string` and `json`. So 307's wrapper generation is actually MORE valuable under this design — it's the place where `$ref` indirection gets resolved into a typed C pointer for the user's function.

**302 allocator — the LVH is doing more work than its original spec.** The pre-allocated slot for indeterminate-size values stores a pointer; the bytes live in the LVH. Every cross-language large value goes through this path. So the LVH's allocation cost matters more under your model than under the original "occasional LLM response" framing. The monotonic-growth leak is still a leak, but the path it sits on is now hot. This pushes 302's allocator-rewrite from "deferred until a long-running workload" to "actually load-bearing for any mixed-language graph with structured data." Mild update to the urgency.

**304 dispatch — push/pull paths change shape.** Currently `push_routed` copies the producer's output bytes into the consumer's slot. Under your model, for cross-language edges with large values: the producer's bytes are already in LVH; the consumer's input slot receives just `(ptr, len)`. No memcpy of the bytes. This is the change you described and it lands in 304's spec. For same-language edges, no JSON at all; the cell stores either the native handle (Lua: lua_State stack ref; C: typed pointer) or the bytes directly per the producer.

**305 graph loader — the size-class enumeration matters more.** When the loader knows a box's output is `bytes` of indeterminate size, it allocates an LVH chunk pool sized appropriately. The 305 phase-5 deferred work (slot size class enumeration) was originally about static demand for fixed-size values; under your model it's also about predicting LVH chunk demand. Same mechanism, broader application.

---

**Still need your answer on:**

- **302:** is there a long-running workload coming soon that the leak would hurt? Even more pertinent now since cross-lang structured values all flow through the LVH. My read shifts toward "ship the allocator rewrite" if mixed-language maps are imminent.
- **307:** any C boxes that need the raw `(inputs, sizes, n, ...)` signature, or is "typed signature, no boilerplate" universally right?

**New question raised by your answer:**

- **`$ref` lifetime.** When a producer writes bytes to LVH and emits `{"$ref": ptr, "$len": n}`, how does the consumer know when it's safe to release the LVH chunk? Two reasonable models:
  - (A) The dispatch layer keeps the chunk alive until every consumer of that wire has run. Refcount per LVH chunk, decremented when each consumer task finishes reading it. This pushes us back toward the unified allocator from 302's retraction — refcounting is the same mechanism.
  - (B) The chunk lives until the run ends. Simpler, but monotonic-growth-by-design.

If you're going to ship the 302 rewrite anyway, (A) falls out for free. If you're deferring 302, (B) is the only honest answer until refcounting exists. So 302's answer feeds into how 312 + 317 actually behave at runtime.

What's your read on 302?

## User

  - 302: is there a long-running workload coming soon that the leak would hurt? Even more pertinent now since cross-lang structured values all flow through the LVH. My read shifts toward "ship the allocator rewrite" if mixed-language maps are imminent.

if the design is clear, we might as well do it now. No sense deferring anything that we could implement today. The reason to defer things is if we're waiting on something else - prefer being blocked by an explicit dependency on a specific issue, rather than deferring until... when?

The memory allocator can't be monotonic. As in... there must be no single-threaded process that's running outside of the thread pool tasks. They must all pass through the same architecture. This is crucial, and the only times we break this rule are for things like setting up the program (initial cost) and breaking it down (though I think we just await all the other threads to finish their stuff, idk)

to allocate memory, there must be one type of operation that does it. Well, one type of operation for each type of operation that we need. Meaning... if we're setting up a box, we can only allocate memory for the inputs and outputs in a single way. That same function must do it for every box. We need to ensure everything is consistent and VERY rigid. The input / output values are only written to by the boxes that are placing their return values into another box's inputs - there is no other way to save values in memory. Any pointer redirection for values of indeterminate size (even small ones) is handled by our thread pool code. We are creating a structural shell, like an apartment building that is just steel and concrete at first, and the user can build the walls and windows and design the doors and movement paths as they please. The library authors are doing the actual decorating, with tables and chairs and pictures of the beach. We are structural, we keep the roof from caving in. To that end, we must be rigid. We must be firm. We must be flexible.

## Assistant

Yes — and the principle you've stated does more than answer 302. It also answers 307 (and arguably retroactively settles some questions about 304 and 315). Let me trace it.

---

## What "no single-threaded process outside the pool" means concretely

- **The allocator runs inside dispatch tasks.** A producer task allocates the consumer's input value into the LVH (or fixed-size class) at push time. A consumer task drops its reference at pop/peek time. The "deep sweep" at allocation-failure-time runs *inside the failing task* before that task asks the OS for more memory. The "deep sweep at quiescence" is a low-priority pool task spawned by the quiescence detector — not a background thread.

- **No GC thread.** The user's prior allocator design hinted at one ("at quiescence — moments when no task is running"); under your rule, that's a pool task spawned during the quiescence-detection moment, not a separate thread that polls.

- **Setup and teardown are the only exemptions.** The graph loader allocates every box's slot table at startup before the pool launches. After `pool_destroy(p)` returns, the run is over and the arena gets freed in one shot — no deep-sweep at shutdown because the chunks die anyway.

- **One allocator function per kind of operation.** `slot_alloc` is THE slot creation entry. `slot_push` is THE value-write entry. `value_alloc` (or similar — to be named) is THE indeterminate-size payload entry. Same call shape regardless of the box; only parameters vary. Specs and user code don't get alternatives — they call THE function, period.

- **Pointer redirection lives in pool code, not spec code.** When a Lua box returns a 4 MB string, the Lua spec hands the bytes to `value_alloc`, gets back a handle, writes that handle into the output cell. The Lua spec never touches the LVH directly; it goes through the structural entry point. Same for every language. The bridge layer between specs and the allocator is uniform.

That last point is what closes 307: a C box author writes `int classify(const char *, int)`. The generated wrapper is structural code — it knows how to call THE allocator with the function's typed inputs/outputs. The author never touches `value_alloc`. The wrapper is the bridge; the author lives upstairs.

So:
- **302: ship the unified-allocator design.** Confirmed.
- **307: ship the wrapper generation.** The structural principle directly forbids requiring users to write SoraMech-shaped signatures.

---

## On teardown ("idk")

Quiescence is already how the run ends — when `active_task_counter == 0` and no further spawns are pending, the runner's `main` unblocks from `pool_wait_quiescent`, calls `pool_destroy`. At that moment:

1. Every worker thread joins.
2. Every value chunk in the LVH is unreferenced by definition (no live task = no live reference).
3. The slot store's per-box slot tables get freed in a single arena release.
4. The allocator's address-ordered chunk list gets walked once to free OS-region-backed memory.

No deep sweep. The whole arena dies; coalescing dead chunks before freeing the arena is pointless work. **Teardown is `pool_destroy` → free arena → return.** That's it. Your instinct is right.

The only edge case worth naming: a task that emits an error (per 303, errors crash the program). At that moment the process is exiting; teardown is `_exit(1)`, no arena cleanup needed. The OS reclaims the address space.

---

## Where the manifesto lands

What you wrote — "We are structural. We keep the roof from caving in. We must be rigid. We must be firm. We must be flexible." — that's a vision-level statement. It belongs in `docs/001-architecture.md`'s introduction, alongside the apartment-building metaphor. The structural-vs-decoration split between SoraMech and library authors is the project's identity and the issue files reference it implicitly without ever stating it directly.

I'd like to add a section to `docs/001-architecture.md` titled "The structural shell" that:
- Quotes the apartment-building / steel-and-concrete metaphor
- States the "one allocator per kind of operation, called uniformly" rule
- States the "no single-threaded process outside the pool" rule
- Flags the two exemptions (startup + teardown)

This adds value as a top-of-mind reference for every future issue. The decision rule "is this code structural or decorative?" becomes the test for whether something belongs in SoraMech's core or in a library/box.

Want me to write that section as part of the 302 commit (since 302 is where the principle becomes load-bearing in code), or split it into its own docs-only commit before we touch 302's code?

---

## What's resolved now

| Issue | Resolution | Notes |
|---|---|---|
| 302 | Ship the unified allocator | Multi-day work; biggest remaining piece |
| 307 | Ship wrapper generation | Half-day; unblocks 317's C bridge |
| 312(a) | Per-edge classification | Small |
| 312(b) | JSON + `$ref` handles + optional `$type` for primitives | Small; lands cleanly with 302 since `$ref` lifetime is the same refcount mechanism |
| 313 | Effectively shelved | Premise (skip JSON for same-lang interior) absorbed by 312 |
| 317 | Lua done; C blocked on 307; Bash near-identity | Lands after 307 |
| 315 | Still no driver | Lowest priority |

One open question for you: **do you want the docs-update commit before 302's code work begins, or bundled into 302?** I lean separate — the principle outlives this issue's implementation, and the commit is honest about what changed (docs only, design statement, no behavior). Either way works.

## User

O  - No GC thread. The user's prior allocator design hinted at one ("at quiescence — moments when no task is running"); under your rule, that's a pool task spawned during the quiescence-detection moment, not a separate thread that polls.

um, we can't expect that there will be moments when no task is running. We don't know how the user will design their programs.

  Want me to write that section as part of the 302 commit (since 302 is where the principle becomes load-bearing in code), or split it into its own docs-only commit before we touch 302's code?

I want you to write that, and I want you to include it as a separate commit. That's fine.

so, there's no garbage collector, that's true. We clean up memory when we finish a box's task - first we write the output values, as soon as possible so that other tasks can start using them, then we deconstruct what we can and decrement reference counters for everything that we can't. Every piece of allocated memory should have a reference counter, and it should be stored right nextdoor to the data so every box can find it. My understanding is that we only need to allocate memory (and store references to) individual input and output values - is that true? I think we mentioned also wanting to store reference counters for boxes, specifically those that cannot be reached after a certain point in the graph. We can also check those during a box's completion if you want.

## Assistant

Right, drop quiescence as a trigger entirely. Iterators feeding iterators can run continuously for the life of the run — there is no "everybody's idle" moment to piggyback on. **The only deep-sweep trigger is "an allocation would otherwise fail."** That's the simpler design and the honest one.

---

## Cleanup at task end — the order

Mirroring what you described:

1. **Compute the output** (spec runs).
2. **Allocate the output value in shared memory.** Refcount initialized to the number of outgoing wires that will consume it.
3. **Push the pointer into each consumer's input slot.**
4. **Spawn every consumer task that became ready** because of this push. (Critical-path: consumers can start running before this task has even finished its own cleanup.)
5. **Decrement refcounts on every input value this task consumed.** A consumed value at refcount 0 returns to the allocator's free lists with eager neighbor-merge.
6. *(Optional, see below)* **Check whether the producing box itself just retired.**

The reason output-write happens before input-cleanup is exactly your point — minimize the time between "this value exists" and "downstream tasks can start using it." A box that's about to die still gets to feed its outputs into ready hands before tearing down.

---

## Where refcounts live

**One refcount per allocated value.** Stored in the value's header, right next to the data — same pattern as the slot cell layout already uses for `filled_size`. Concretely:

```
allocated chunk layout
offset  size  field
------  ----  -----
0       4     refcount
4       4     length
8       …     data
```

The pointer the consumer's spec sees points at offset 8 (the data). Refcount lives at `*(uint32_t*)(ptr - 8)`. Standard pattern, O(1) lookup, no separate refcount table to maintain.

**Yes — values are the only runtime allocation that needs refcounting.** To enumerate:

- **Slot tables**: allocated at graph load, freed at pool teardown. Static for the run's life. No refcount.
- **Task structs**: allocated per spawn, freed per completion. Owned by the pool's allocator. No refcount (the pool tracks task liveness via the active-task counter).
- **Values flowing on wires**: allocated by producer tasks, freed when their refcount hits 0. THE refcount lives here.

---

## Box-level retirement — the optional second refcount system

You're right that this is a separate machinery. The mental model:

Each box has a `live_predecessor_count` — how many upstream boxes could still send it a value. Computed at graph load:
- For a box with N distinct upstream producers, `live_predecessor_count = N`.
- Data boxes have no predecessors, count starts at 0; they retire immediately after they emit their value.

At task end, **only if** `current_box.live_predecessor_count == 0` (no further inputs can arrive), check whether this box's own queues are drained. If yes, this box just fired its last invocation:
- Free the box's slot table.
- For every downstream box this box was feeding, decrement `live_predecessor_count`. If any downstream's count just hit 0 and their queues are drained, they retire too — cascade up to the leaves.

The `count > 0` short-circuit keeps this off the hot path for typical work. Only the last invocation of a box pays the cascade cost, and only when the cascade actually has something to cascade.

**Question on this**: cycles. An iterator that loops back to itself counts itself as a predecessor of itself; `live_predecessor_count` starts at 1 and never decrements (the iterator can't tell itself "I'm done feeding me" while it's still firing). The cycle never retires until the run ends. That's actually fine — it matches reality — but it means box-level retirement is most useful for *acyclic* parts of the graph (data sources, one-shot transforms, terminal sinks). The iterator-pumped interior of a long-running graph would not benefit. Worth being explicit that this optimization is "free memory in the acyclic tails" not "free memory in the looping core."

If that's the right framing, I'd put box-retirement in 302 as a stretch goal but not block 302 on it. Value-level refcounting is the load-bearing piece; box-level is the cherry on top.

---

## Two questions, then I get moving

1. **Cycle handling for box retirement** — is the framing above ("acyclic tails benefit; iterator interior doesn't") the right read? Or do you have a sharper way to handle the self-referential case?

2. **Architecture docs section** — `docs/001-architecture.md` looks like the right home. Should the new section go at the top (front matter for the whole doc), or as a new top-level section ("The structural shell") sitting alongside existing sections? My instinct is top of the file, since the structural-shell framing is the lens through which every subsequent section should be read.

After your answers I'll:
1. Write the docs commit first (separate, your direction).
2. Then start on 302's allocator rewrite, with box-level retirement included if your answer to (1) supports it, deferred if not.

## User

  2. Allocate the output value in shared memory. Refcount initialized to the number of outgoing wires that will consume it.

the output value is written directly to the destination of each of it's wires. The output slot is just for routing, it doesn't store any memory. I guess in a sense there are only input values, and they get written once and read once. If a box has one output wire going to 5 different boxes, it will write the same value to 5 different locations. That's alright. The exception is values of indeterminate size - they are first written to shared memory with their size value prepended, then a pointer is created and placed in each of the wire destinations. It must go in that order though - the pointer arriving in an input slot signifies that the data that it's pointing to is valid.

the refcount matters a lot for pointers-to-values-of-indeterminate size. It matters a lot less for values that are written to a single box and used once. Values that are written to multiple box's input slots are treated as if they were written to a single box, because each receiving box is the one that's going to be interacting with their refcounter anyway. Since the value isn't duplicated anywhere, the only time it might have it's refcount above 1 is... if another box explicitely reaches out and increments it. But I don't think we're going to be building that functionality into the system anytime soon, why would we? I can't think of a usecase. So we might want to just... use refcounts for just values-of-indeterminate-size. What do you think?

  Each box has a live_predecessor_count — how many upstream boxes could still send it a value. Computed at graph load:
  - For a box with N distinct upstream producers, live_predecessor_count = N.
  - Data boxes have no predecessors, count starts at 0; they retire immediately after they emit their value.


something to keep in mind, we might have to update the data box issue file about this, re-opening it if necessary, but data boxes should provide their indicated value anytime it is requested. meaning... the slot that their output points to is permanently filled. However, this approach is inflexible and won't work correctly all the time, and here's why: if there's 3 data boxes sending their outputs to the same input slot, what happens? The result should be that they alternate. If it were three boxes that just ran once and filled the same slot, what'd happen is that the input values would go into the box's ring buffer to be assigned to tasks structs that have pointers in the thread pool that can be assigned to the task struct so a thread knows what data it needs in order to proceed to complete the task. The input values placed into a box's ring buffer would, in the case where there's three boxes, they would alternate first one, then the next, then the third. Data boxes should be the same - they should permanently supply their data whenever it is requested, and if that means cycling through multiple boxes that share the same input port, then that's fine. Another approach would be this: there's two boxes, one a call box and one a data box. The call box returns a value every couple seconds, the data box is always available. The data box is treated as a "default", and anytime the input slot is empty it needs to supply a value. The way we implement this is through the "parking a task on a blocking task" process - when a task is blocked, the first thing it should do is iterate through it's inputs to that slot and see if there's any data boxes. If so, it round robin's from among the data boxes specifically. This way, we're not asking the data box to provide it's input whenever a slot is empty - that'd involve watching, and we're not going to do that. Instead, we query the databox whenever a task would be blocked without any data. The blocked task, the one that might request from several different data boxes, keeps track (on the abstract box itself, not the individual task) it keeps track of which data boxes it's round-robin'd from, so it knows which one to check next time. This could be as simple as just, having a list of them, and then having a pointer to them that gets iterated / looped-back-to-the-beginning each time it makes a request. How does that sound?

  Question on this: cycles. An iterator that loops back to itself counts itself as a predecessor of itself; live_predecessor_count starts at 1 and never decrements (the iterator can't tell itself "I'm done feeding me" while it's still firing). The cycle never retires until the run
  ends. That's actually fine — it matches reality — but it means box-level retirement is most useful for acyclic parts of the graph (data sources, one-shot transforms, terminal sinks). The iterator-pumped interior of a long-running graph would not benefit. Worth being explicit that
  this optimization is "free memory in the acyclic tails" not "free memory in the looping core."


keep in mind, we need to ensure that ALL non-optional inputs have a predecessor. If any required inputs lose all their predecessors, then either there's a bug in the program or the box can never be run again. We must trust that users won't write bugs into their programs using our tool - well, we know they will, but we must check for those at compile time not runtime.

compile time of course being when they hit the "compile" or "save" I forget which button at the top of the editor, which collapses the same-language functions into one big mega file, bringing in only what we need as source-code for that particular map.

  2. Architecture docs section — docs/001-architecture.md looks like the right home. Should the new section go at the top (front matter for the whole doc), or as a new top-level section ("The structural shell") sitting alongside existing sections? My instinct is top of the file,
  since the structural-shell framing is the lens through which every subsequent section should be read.

yeah!

## Assistant

Right on all counts. Let me trace what changes:

---

## Refcounts shrink to "LVH only"

You're right — fixed-size values fly inline into each consumer's ring slot. Five consumers means five copies; the producer writes each one. No central storage, no refcount, no shared ownership. A consumer's pop drains the cell and the bytes are gone.

LVH values are the only case where multiple consumer slots hold pointers to the same allocation. That's the only place refcounts are needed, and the count = "how many consumer slots still hold the pointer." Each consumer's pop decrements; when 0, the LVH chunk goes back to the free lists with eager neighbor-merge.

**This shrinks 302 substantially.** Not a unified allocator anymore — the slot store stays as it is (pre-allocated cells per graph load). The work is:
- Add refcount header to LVH chunks (4 bytes for refcount + 4 bytes for length + data).
- Producer push: allocate from LVH, set refcount = number of consumer slots, push pointer to each.
- Consumer pop on a `$ref` cell: decrement refcount; if 0, return chunk to free lists.
- Eager neighbor-merge on the dealloc path.
- Deep sweep on allocation-failure only (no quiescence trigger).

Maybe two days of work instead of the week I estimated. Much better.

---

## Data box pull model — needs its own issue

This is a real change to the dispatch model. Current 304: "Tasks never park on slots." Under your design, tasks DO have a parked moment — between spawn and run, when checking inputs — and that moment is where data-box round-robin happens.

Concretely:
- A consumer box's input port that has any data-box predecessor carries a small per-port state: `{ data_box_ids: [...], rr_index: 0 }` on the box's runtime record.
- Spawn check treats data-box-fed inputs as **always satisfiable** (they don't block spawning).
- At task-run time, for each data-fed input that's actually empty in the ring buffer, the dispatch layer queries `data_box_ids[rr_index]`, copies that data box's value (or follows its LVH pointer) into the input position, and bumps `rr_index = (rr_index + 1) % len(data_box_ids)`.
- The data box itself never produces a task. It's not a producer; it's a callable source.

This contradicts current 304's "data boxes push their literal value at startup" behavior. Hence: needs a new issue file.

I'll file **244 — data boxes as round-robin default providers** and reference 229 (which renamed data → read) as the parent. 244 captures:
- The pull-on-empty semantics
- The round-robin state stored on the consumer box
- The compile-time check that each box's required inputs have either a wire-producer OR a data-box predecessor OR a literal value (this last one is already 230's check; just need to make sure data-box-only-predecessor counts as satisfied)
- The case of multiple data boxes feeding the same input (rotation in declared order)

I'll ship 244 alongside 302 since they share the dispatch-side touch.

---

## Compile-time required-input check

Already implemented in `src/001-schema.lua::check_input_bindings`. It enforces:
- Every input is wired OR has a literal OR is marked `optional`.

Under 244's model, "has a data-box predecessor" also satisfies the check (since the data box always supplies on demand). I'll extend the check in the same pass. No new issue needed — it's an extension of 230's existing rule under 244's new semantics.

---

## Box retirement with cycles

Framing accepted: "free memory in the acyclic tails, not in the looping core." I'll include box-level retirement as a stretch goal in 302 with the explicit caveat that cycles are exempt — `live_predecessor_count` for a self-referential iterator starts at >0 and stays there. Acyclic chains (data → transform → sink) get the retirement benefit; loops don't. Stretch — not blocking on it.

---

## Architecture docs at top of `001-architecture.md` — confirmed

I'll write that section as the next commit, then start on 302.

---

## One question that fell out

"compile time of course being when they hit the 'compile' or 'save' I forget which button at the top of the editor, **which collapses the same-language functions into one big mega file, bringing in only what we need as source-code for that particular map.**"

This is describing 313's whole-program same-language merge. Under our 312 resolution I'd flagged 313 as effectively shelved (since the JSON-skip win is absorbed by per-edge classification). But your description suggests merge-into-one-file IS expected behavior of compile.

Two readings:

A) **You meant 309's source bundling** — `compiled/src/` already contains every source file the map uses, separately. Not merged into one file, but everything needed lives in one directory. This is currently the compile pipeline's behavior.

B) **You actually want 313** — same-language source files literally concatenated/inlined into a single translation unit, so the C compiler can inline across box boundaries and Lua's bytecode runs in one chunk. Bigger lift; meaningful additional perf win on top of 312.

If (A), I'll close 313 explicitly as "absorbed by 312's per-edge classification + 309's bundling." If (B), 313 stays open and lands after 302 + 307. Which one were you describing?

After your answer, the work order is:
1. Architecture docs commit (top of `001-architecture.md`).
2. Issue 244 file (data-box pull model).
3. 302's allocator + refcount work (with 244's dispatch changes folded in).
4. 307's wrapper generation.
5. 317's C and Bash bridges.
6. 312's per-edge classification + `$ref` wire protocol.
7. (B if confirmed) 313.

Question one — A or B?

## User

  If (A), I'll close 313 explicitly as "absorbed by 312's per-edge classification + 309's bundling." If (B), 313 stays open and lands after 302 + 307. Which one were you describing?

let's do B. We actually want 313 to be implemented. It'll be difficult, we'll have to make sure things are included in the correct order, but this is why we need a way to programmatically define arbitrary programming language semantics in the language specs. Still not entirely sure how those will work out, but we're working on it. Let's get the thread pool design, and hopefully we'll be able to make a list later of the exact things we need the cross-language spec system to do.

so. I'm thinking about the data-box thing. I'm wondering... when do we allocate memory for new tasks? Is there a performance drawback to using shared memory, over thread specific memory? We need to use shared memory, I'm just curious. Here's what I'm thinking. What if anytime we write to a box's ring buffer (using a getter / setter style system that is either language agnostic (json) or language specific (native path)) we trigger a box-specific task to be added to the thread pool. This box-specific task is not handled by the producer that's filling a slot in the ring buffer - that thread doesn't have to know about it. A pointer to a box and associated instructions is added to the thread pool, and the next time it's assigned to a thread the thread will scan through the box's inputs and decide if there's enough present to allocate a new task. If so, it'll create a task struct that is an *instantiation* of the box, or rather of the box's task struct, and then it'll either execute the task immediately, or if the required inputs are partially filled, it'll park the task on the blocking task's waiting list. The blocking task, if any, will re-add the task to the task pool during it's final actions, rather than executing it itself, because there might be more than one. How does that system fit with the rest of our design? Where do we need to update it to match?

## Assistant

OK — 313 is real. Confirmed; it stays open and lands after 302/307. Worth carving it into sub-issues at that point because the merge has to be per-language (each language spec declares how to merge) and the load order rules will be language-specific.

---

## Memory allocation for tasks (Q1)

The pool already owns a task allocator — currently per-spawn malloc inside `pool_spawn`. The right shape is a small **slab allocator inside the pool**: one large block reserved at `pool_create`, internal free list, per-thread cache to dodge contention on the hot path.

Task structs are tiny — pointer-to-box + a few flags + maybe a waiting-list pointer once parking is wired in. A 64-byte slab cell easily covers it. Allocation cost reduces to "atomic pop from a free list" in the common case, which is single-digit nanoseconds.

The slab itself lives in shared memory (no other choice — any worker may allocate, any worker may free). I'd build this as a small extension to `libs/task-pool/pool.c` rather than a new module.

## Shared memory vs thread-local (Q2)

For task structs the difference is negligible. The cost of shared memory shows up in three forms:
1. **Cache-line contention** when two threads write to memory in the same cache line concurrently. Solvable by per-thread allocation regions (each thread's slab cells come from a disjoint range).
2. **TLB pressure** if shared memory spans many pages. Not an issue for our scale.
3. **Allocator contention** when threads compete for the same free list. Solvable by per-thread caches with periodic refill from the shared pool.

Task structs travel between threads (allocator thread → executor thread), so memory MUST be shared. The overhead is real but bounded; the per-thread cache + per-thread region pattern eliminates it from the hot path. **Use shared memory throughout; don't pre-optimize beyond that.**

---

## The dispatch redesign

Let me write out the model you described, with the open question marked:

**Lifecycle of one box's invocation:**

1. **Producer task finishes** its work, copies its output into each consumer's ring slot (fixed-size) or writes the LVH chunk and pushes a `$ref` pointer into each consumer's slot.
2. **For each consumer slot it just wrote to**, the producer submits a tiny **readiness-check task** to the pool (allocated from the task slab). The producer task does not do the readiness check itself — it just signals "this slot got a push, somebody come look."
3. The producer task ends. Its slab cell goes back to the free list.
4. **Some worker picks up the readiness-check task.** The check action:
   a. Loads the consumer box's per-port state.
   b. For each required input slot, checks: is there a value present? If not, is there a data-box predecessor we can pull from (per 244)?
   c. If every input is satisfiable: allocate a fresh task struct for the consumer's actual work, submit it to the pool. Done.
   d. If some required input is empty and has no data-box predecessor: allocate the consumer task struct, **park it** somewhere, and return.

**The parking target — open question (more below).**

5. **When a producer task completes**, before its slab cell is freed, it walks any parked tasks on its waiting list and re-submits each to the pool. Each re-submitted task is a readiness check that will re-evaluate.

**What this buys:**

- Producer threads do not pay the readiness-check cost. They finish quickly, return their task slab cell, take the next available work.
- Readiness checks run in parallel across workers when many slots are pushed simultaneously.
- High-fan-out producers (one output → 50 consumers) parallelize their consequence-checks across the pool instead of running 50 checks on the producer's own thread.
- The dispatch action becomes uniform: a worker picks up a task, executes it. The task is either a "real work" task or a "readiness check" task. Same call shape; same allocator; same return path. Structural rigidity preserved.

**What it costs:**

- A small allocation per push (readiness check). Mitigated by slab.
- One extra pool round-trip per consumer. Slow consumers absorb this overhead amortized.

---

## The parking target — needs your read

When a readiness check finds the consumer is not ready, **what do we park the consumer task on?**

Three reasonable choices:

**(A) Park on the empty slot itself.** Each input slot carries a parked-tasks list. When a value arrives in the slot, walk the list and re-submit each parked task. Simple, no question of "which producer task." The slot is the synchronization point.

**(B) Park on a specific producer task.** When a consumer's check sees an empty input, identify the producer task currently in flight that would fill it. Park on that task. When the producer ends, walk its waiting list. Requires "what task is currently writing this slot" tracking. More complex.

**(C) Park on the consumer box.** A box-level parked-tasks list. Every push to any of the box's input slots triggers a walk through the list and a re-check. Equivalent to (A) in behavior but the list lives on the box, not the slot.

Differences matter for fan-in:
- Three producers feed one consumer slot. Under (A), parked tasks live on the slot, the next push from any producer wakes them. Under (B), they're parked on one specific producer — if a DIFFERENT producer fills the slot first, the parking is wrong; we'd need to wake when ANY producer fills the slot.
- Under (C), one consumer-level list catches everything, simplest.

**My recommendation: (A) — park on the slot.** The slot is what's empty; the slot is what gets filled. The producer task identity is irrelevant. Fan-in works naturally. The parked-tasks list lives in the slot header (one more field), and the push operation walks it after writing.

But your phrase "park the task on the blocking task's waiting list" suggests you were thinking (B). Want to clarify which?

---

## Where this lands in the issue files

| Issue | Update needed |
|---|---|
| 244 (new) | Data-box round-robin pull integrates cleanly — the readiness check is exactly where the data-box pull would happen. |
| 302 | Slab allocator for task structs gets added to the work. Refcounting design unchanged. |
| 304 | Major rewrite of the dispatch model. Producer task no longer does `dispatch_spawn_if_ready` inline; instead it submits readiness-check tasks. Two task kinds (work + readiness check) running on the same pool. |
| 310 | Open question — should readiness checks have a different priority than work tasks? My instinct: readiness check at lower priority (so real work runs first when both are available). |
| 311 | Event types extend: `task_submit` / `task_start` / `task_end` need a "task_kind" field to distinguish work from readiness check. |
| 313 | Unchanged. Lands after 302/307. |

I'll write the architecture-docs commit next (still pending from earlier), then update 304's issue file to reflect this dispatch model, then start on 302. The 244 file lands in the same pass as 304's update.

**Open question:** parking on slot, producer task, or box? (A/B/C)

## User

  Memory allocation for tasks (Q1)

  The pool already owns a task allocator — currently per-spawn malloc inside pool_spawn. The right shape is a small slab allocator inside the pool: one large block reserved at pool_create, internal free list, per-thread cache to dodge contention on the hot path.

  Task structs are tiny — pointer-to-box + a few flags + maybe a waiting-list pointer once parking is wired in. A 64-byte slab cell easily covers it. Allocation cost reduces to "atomic pop from a free list" in the common case, which is single-digit nanoseconds.

  The slab itself lives in shared memory (no other choice — any worker may allocate, any worker may free). I'd build this as a small extension to libs/task-pool/pool.c rather than a new module.


can you describe each action this slab allocator will do? I want a pipeline walkthrough flowthrough datapath chart, with annotated map symbols. Can you make it in HTML? Just this once, as a treat :)

  1. Producer task finishes its work, copies its output into each consumer's ring slot (fixed-size) or writes the LVH chunk and pushes a $ref pointer into each consumer's slot.

the ring slots live with the box, which spawns task structs that have no concept of a ring buffer. They just know the values they're filled with, and they know they'll never change.

  2. For each consumer slot it just wrote to, the producer submits a tiny readiness-check task to the pool (allocated from the task slab). The producer task does not do the readiness check itself — it just signals "this slot got a push, somebody come look."

only once per box, and it only actually sends the readiness check request to the thread pool after it's filled in all it's input values. That way, we can ensure that the value is successfully applied, and we also ensure that we only do it once per task.

  4. Some worker picks up the readiness-check task. The check action:
  a. Loads the consumer box's per-port state.
  b. For each required input slot, checks: is there a value present? If not, is there a data-box predecessor we can pull from (per 244)?
  c. If every input is satisfiable: allocate a fresh task struct for the consumer's actual work, submit it to the pool. Done.
  d. If some required input is empty and has no data-box predecessor: allocate the consumer task struct, park it somewhere, and return.

loading the state is just as hard as following a pointer to the task struct, which has pointers to the box in question. It's intentionally minimal - not much more complex than a doubly linked list entry. is there anything else we might need to store on the task struct? remember, it needs to be able to be created quickly, because it's created on the hot path for other parts of the program. Can you make me a list of exactly where? anyway, next the box looks at each of it's queues, and determines whether or not it has / can get enough input values to create it's specified task. If so, then it creates a task struct in shared memory that has the first-in-first-out value from the top of each of it's input slot ring buffers, or if drawing from a data box then a pointer to the location of that data box's memory. Which is constant throughout the lifespan of the data box - refreshed if the box is ever requested to. Like a... capacitor transistor I think? Or maybe just a bit in memory. Who can say. anyway, it draws the first-in-first-out value from the - oh sorry it draws the correctly provided values into the struct. the one that's placed in shared memory, which holds the input values, reference counts (if any present) of those input values, and also a reference count for the box itself (total wire connections count), and a pointer to the part of memory which holds the function that the box action will be performing. Anything else we need? Oh yes a pointer to (one of) the sections of memory that hold the return values. If there's multiple wires, then multiple locations are stored in. actually, can we make the iterator box a flip switch in the inspector for a box which iterates it's inputs vs delivering them in parallel? Please examine the functionality of the structure and tell me if there's anything I'm not thinking of.

  The parking target — needs your read

  When a readiness check finds the consumer is not ready, what do we park the consumer task on?

you know... the more I think of it, I don't think we need the parking at all. Let's think about it real quick - if a task doesn't have all it's inputs yet provided, it can't run - so, let's say it just returns *false* then, and the next time an input value is filled, it adds a task saying "hey check and see if it's ready" if it is, then how about this - how about it just... runs instead of creating a new task again. One layer of redirection defeated, maybe? what do you think, would that eliminate parking?

## Assistant

Treats are good. Writing the HTML, then the other answers.

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/docs/HTML/task-slab-allocator.html`

### Tool result

```
File created successfully at: /mnt/mtwo/programs/sora/soramech/docs/HTML/task-slab-allocator.html
```

## Assistant

Diagram lives at `docs/HTML/task-slab-allocator.html` — open in any browser. Now the rest of your message.

---

## Confirmed dispatch corrections

**Task struct holds VALUES, not slot references.** Right — at instantiation time, the readiness check pops one value from each input slot's ring buffer (or follows the data-box pointer) into the task struct's inline slots. The task itself never touches the ring buffer; it operates on its frozen input snapshot. After the task ends, the cell goes back to the slab; the ring buffer state is independent of the task's lifecycle.

**One readiness-check per (producer-task, consumer-box) — fired at end of producer task.** Confirmed. The producer task completes all output writes (every cell in every downstream slot is filled, every `$ref` placed), THEN submits one readiness check per *distinct downstream consumer box*. If a producer writes to two slots on the same consumer box, only one readiness check fires. Cheap dedup via a small per-task set.

---

## What goes in a task struct (the cell)

Your list, with my additions and a final picture:

| What you named | Why |
|---|---|
| pointer to box | identity / lookup |
| input values (snapshot) | FIFO-popped from ring slot OR pointer to data-box constant |
| refcount of each `$ref` input | so the task can decrement on completion |
| reference count for box (live wires) | for box-level retirement (stretch goal) |
| pointer to function | reachable via box pointer; one indirection saved by inlining? |
| pointers to output destinations | one per outgoing wire; the consumer slots to write into |

My adds:
- **task_id** — for event-log correlation per 311. 4 bytes.
- **flags byte** — work vs readiness-check vs (future) fast-fast-path, priority hint. 4 bytes.
- **Possibly a values-array pointer** for high-arity boxes whose input snapshot doesn't fit inline. The cell budget is 64 bytes total; with the fixed-header overhead, ~40 bytes remain for inline values. A 5-input box with 8-byte values fits; a 12-input box overflows. The escape hatch is "if it doesn't fit, allocate a separate values-array from the slab and store a pointer." Adds one indirection only for high-arity boxes.

**Things I do NOT think need to live in the cell:**
- Pointer to the producing-task's parent — not needed at runtime; only useful for debugging cascade traces; can be flagged on by a build option, off in production.
- Submit time / start time / end time — the event log captures these on the way out; storing in the cell would double-spend.
- A waiting-list pointer — we agreed no parking, so no list.

**Hot path enumeration** (where cells get created):

1. **Pool startup** — main thread spawns one task per entry box. N tasks, where N = entry-box count. Cold.
2. **Producer task completion** — submits 1 readiness-check task per distinct downstream consumer box. Per task, ~3-5 typically. **Hot.**
3. **Readiness-check task running** — when all inputs ready, either (a) allocates 1 work task and submits, or (b) runs inline (your suggestion below). Per check, 0 or 1. **Hot.**
4. **Data-box pull** — no allocation; the data-box value is already in memory, just copied into the consumer's input slot snapshot.

Roughly 1-2 cell allocations per real work invocation in steady state. Slab handles this with no contention.

---

## Iterator-as-flip-switch — I need to disambiguate

You said: *"make the iterator box a flip switch in the inspector for a box which iterates it's inputs vs delivering them in parallel"*

I want to make sure I read this right. There are two different "iterator" concepts and I'm not sure which (or both) you mean:

**Reading A — output iteration (current routing.kind=iterator).** A box that distributes ONE output value across N output wires round-robin. The dispatch picks the next output port for each invocation. Today this lives as `routing.kind: "iterator"` per 233.

**Reading B — input iteration (new flip switch).** A box with multiple input ports that, with the flip on, fires once per *input value across some axis* (e.g., consume input port `xs` one element at a time, firing once per element with the same other-port values held constant). Like a `foreach`.

**Reading C — input port iteration.** A box that, instead of firing when all input slots have values, fires once each time any one input slot gets a value, supplying that slot's value alone (the other inputs are skipped or held).

If A, that's what 233 shipped — and the flip switch is the existing routing mode dropdown.
If B, that's a meaningfully different consumption model and probably wants its own issue.
If C, that's also different — it's "fan-out by input port" rather than "fan-out by output branch."

Which one (or which combination) were you describing? I'll proceed once I know.

---

## Parking eliminated + the inline-run question

Agree: **no parking.** The readiness check sees its inputs aren't all ready, it returns and disappears. Next push to any of the box's input slots triggers another readiness check. When ready, the check spawns / runs.

**On "if ready, just run instead of spawning":** real tradeoff.

**Inline run pro:** one allocation saved, one pool queue round-trip saved. For tight chains (Lua box A → Lua box B → Lua box C, all fast), this is ~50 ns per hop. Visible in a hot loop.

**Inline run con:** the readiness check is now an unbounded-duration task. A worker that picked up a "readiness check" might end up running a 5-second LLM call. Other tasks queue behind it; pool fairness suffers.

**Mitigation candidates:**
1. **Heuristic:** inline run only if the box's declared "expected duration" is short. Boxes mark themselves as `fast: true` in their JSON or via a runtime probe (first call records duration; subsequent calls inline if < threshold).
2. **Hard rule:** inline run only if the box's `lang === producer.lang === native_invoke`. The same-language fast path (312) already classifies these. They're the ones that benefit most from saving the allocation/queue cost.
3. **Always submit:** safer, predictable, ~50 ns slower per hop.

**My read: (2) — inline run only on same-language same-spec hops, gated by the per-edge `output_edge_native` bit from 312.** This is exactly the case where the cost saved matters most (already-fast hops) and where the duration is bounded (your own spec's average invocation cost). Cross-language hops always submit — the language boundary already costs more than the spawn does.

That gives 313's eventual whole-program merge a clear-roadmap: same-lang merged regions get inline runs by default; cross-lang boundaries get submit-spawn. The merge is the limit of (2).

Agree on (2)? Or do you want simpler (always submit) for now and inline-run as a future optimization?

---

## Updates needed across the issue tree

- **244 (new)** — data-box pull model. Drafting on next pass.
- **302** — slab allocator described above; refcount design unchanged (LVH only); box-level retirement stretch goal.
- **304** — major rewrite. New dispatch model (producer→readiness-check→work-task). Updated task-cell layout.
- **313** — stays open, gains the "inline-run is the within-region limit of the merge" framing.
- **architecture docs (top of `001-architecture.md`)** — still pending; lands as its own commit per your direction.

I'm sitting on the architecture-docs commit waiting for two answers from you: (a) which reading of the iterator flip-switch, (b) inline-run gating decision. Then I write docs → file 244 → update 304 → start 302's code work.

## User


  │ reference count for box (live wires) │ for box-level retirement (stretch goal)                       │

the stretch goal is not the reference count section, but rather the idea that boxes might have reference counts. We should make sure we have a slot for box reference counts, it just... doesn't get used if the stretch goal isn't met. But we still have it there, as documentation that SOMETHING goes there, it just hasn't been built yet. Then we ask why, then we end up adding it because it's a good idea.

  Inline run con: the readiness check is now an unbounded-duration task. A worker that picked up a "readiness check" might end up running a
  5-second LLM call. Other tasks queue behind it; pool fairness suffers.

"readiness check" doesn't fully capture the entirety of what this operation does, specifically, it is intended to "attempt a run" of a box. we make these checks by providing the most recent change that would enable the box to run - essentially, our logic is such that the fewest amount of steps between each function call must be made, while enabling the "long-tail" async threads to do non-blocking tasks on their own time. As soon as they can, they knit themselves with their fellow threads, co-creating a weaving texture of computer programming. proving without a doubt that intelligence can have a dynamical host.

## Assistant

Two important reframes from you. Let me trace them.

---

## Box refcount slot — keep it visible as documentation

You're right. The slot should exist in the design *now*, even if the retirement logic doesn't fire yet. Documentation by structural presence. Same pattern as priority queue (310): the field is in place, the behavior is wired through, the consumer that does something interesting with the value is the follow-on.

So:
- **`box_runtime_state` carries `live_predecessor_count`** — initialised at graph load to the number of distinct upstream producer boxes for this box. Box-level state, not cell-level (one count per box, shared by all tasks instantiating it).
- **Every task's completion path includes "if this is my box's last possible invocation, decrement downstream boxes' `live_predecessor_count`."** The hook exists in the dispatch loop from day one.
- **The actual retirement action** (free the box's slot table, cascade upward) is the stretch piece. Until it ships, the decrement is a no-op — the count drops but nothing checks it.

When someone reads the dispatch code in six months and asks "why are we decrementing this count if nothing watches it?" — that's the prompt to ship retirement. The slot is the question.

I'll add `live_predecessor_count` to box_runtime_state in 302 and call out the decrement-but-no-act loop in the dispatch code, with the explicit comment "the action that consumes this is the box-retirement stretch goal; the count is honest now so the action lands as a one-place addition later."

---

## "Attempt task" — and what it collapses

This dissolves the two-task model I had. A producer's push doesn't spawn a *check*; it spawns an *attempt*. The attempt walks the consumer's inputs, and either:
- All inputs ready → run the consumer's spec, write outputs, submit further attempts. The attempt IS the work task.
- Not all ready → return. Some other producer's push will trigger another attempt later, with the same shape.

This is simpler, structurally rigid (same task kind for every entry into the pool's queue), and faster:
- **One allocation per consumer invocation, not two.** No separate "readiness check" cell.
- **One pool round-trip per consumer invocation, not two.**
- **The inline-run question disappears.** It's not "should we run inline?" — the attempt IS the run, when conditions are met. The "fast same-lang chain" case happens naturally: producer ends → submits attempt → worker picks up attempt → finds ready → runs → submits more attempts. Every hop is one allocation, one queue round-trip. There's no fork between "check task that submits a work task" and "run-inline shortcut."

This also reframes the **"submitting most-recently-relevant change" intuition** you described. The producer task knows what it just changed (it filled a specific slot or N slots on a specific consumer). The attempt it submits is precisely targeted: "the consumer whose input set may have just become ready because of MY push." Not a generic re-evaluation; a targeted attempt with a known cause.

The fairness concern I raised earlier (long tasks blocking the pool) is now... real, but unavoidable, because every task in the pool is potentially long-running. That's the price of running real work on the pool. The pool's parallelism handles it — N workers means N concurrent long tasks; new attempts spawn on whichever worker frees first.

So the dispatch model is:

```
producer task:
   compute output
   for each downstream consumer box C this producer just touched:
      write all values to C's input slots (snapshotted into the attempt's cell at the END)
   for each distinct C:
      slab_alloc() → attempt_task for C
      pool_spawn(attempt_task)
   slab_free(this cell)

attempt task:
   if every required input on C is satisfiable (slot value or data-box pull):
      snapshot values into local vars
      invoke spec, get output
      push output to downstream consumer slots
      submit one attempt per distinct downstream consumer
      decrement input $ref counts
      decrement upstream boxes' live_predecessor_count if applicable
   slab_free(this cell)
```

**One task kind. One allocation per invocation. Self-similar at every layer.** The "attempt" name captures it — every task is an attempt to advance the graph.

---

## The poetic passage

> *"As soon as they can, they knit themselves with their fellow threads, co-creating a weaving texture of computer programming. proving without a doubt that intelligence can have a dynamical host."*

That goes in the architecture docs. The structural-shell section already has the apartment-building metaphor; the dispatch-attempt loop's description gets your weaving framing. The shell is rigid; the work that happens inside it is dynamic, threading itself into existence one attempt at a time. That's the dance the structural rules are designed to host.

I'll weave (sorry) the passage into the dispatch-overview section of `docs/001-architecture.md` next to where the attempt loop is described. Verbatim as quotation, attributed by date.

---

## Still open before I write code

Two questions still pending:

**(1) Iterator flip-switch — Reading A, B, or C?**

- **A** — what 233 shipped: output iteration (one output → N branches round-robin). Flip switch in inspector is the existing routing-kind dropdown.
- **B** — input iteration (a box that fires once per element of an array-valued input port). New behavior.
- **C** — input port iteration (fire once per filled input port instead of once when all ports filled). Also new.

Or some other reading I missed.

**(2) Pre-commit work order:**

I have these commits queued in my head, in this order. Want to confirm or reorder:

1. Architecture docs (top of `001-architecture.md`) — structural-shell statement + the weaving passage.
2. Issue 244 — data-box pull model.
3. Update issue 304 — rewrite to describe the attempt-task model + new task-cell layout + box-refcount slot.
4. Issue 302 implementation — slab allocator + LVH refcount + eager neighbor-merge + on-failure deep sweep + box-refcount field on box_runtime_state.

Each commits separately, trinity-style. Sound right?

## User

  (1) Iterator flip-switch — Reading A, B, or C?

  - A — what 233 shipped: output iteration (one output → N branches round-robin). Flip switch in inspector is the existing routing-kind
  dropdown.


I think this one, but can you describe it again just so I know the full picture?

for the commits we can do this:

  1. Architecture docs (top of 001-architecture.md) — structural-shell statement + the weaving passage.
  2. Issue 244 — data-box pull model.
  3. Update issue 304 — rewrite to describe the attempt-task model + new task-cell layout + box-refcount slot.


then this:

  4. Issue 302 implementation — slab allocator + LVH refcount + eager neighbor-merge + on-failure deep sweep + box-refcount field on
  box_runtime_state.


two total commits. The first can be documentation updates.

can you put this in the architecture doc as well?:

  So the dispatch model is:

  producer task:
     compute output
     for each downstream consumer box C this producer just touched:
        write all values to C's input slots (snapshotted into the attempt's cell at the END)
     for each distinct C:
        slab_alloc() → attempt_task for C
        pool_spawn(attempt_task)
     slab_free(this cell)

  attempt task:
     if every required input on C is satisfiable (slot value or data-box pull):
        snapshot values into local vars
        invoke spec, get output
        push output to downstream consumer slots
        submit one attempt per distinct downstream consumer
        decrement input $ref counts
        decrement upstream boxes' live_predecessor_count if applicable
     slab_free(this cell)


### Tool result

```
1	# SoraMech — Architecture
2	
3	## Three programs, one data format
4	
5	SoraMech is three independent programs that share a common on-disk
6	format: the map directory. None of the three knows about the others
7	at runtime.
8	
9	```
10	  Browser (assets/index.html)
11	       |  HTTP (file CRUD)
12	       v
13	  src/006-server-main.lua       [map directory]
14	                                maps/<name>/
15	                                  meta.json
16	                                  boxes/
17	                                  data/
18	                                  src/
19	                                  compiled/   (after compile button)
20	                                  tmp/ -> /tmp/<name>/
21	       ^
22	       | reads & writes
23	  Runner (one of):
24	    src/007-runner-main.lua     ← phase 2, synchronous Lua interpreter
25	    soramech-pool (binary)      ← phase 3, C thread pool runner
26	```
27	
28	### src/006-server-main.lua (the editor's HTTP backend)
29	
30	Thin file CRUD proxy. Receives JSON requests from the browser and
31	translates them into reads and writes on the map directory. No logic
32	beyond path validation and basic consistency checks (e.g. reject a
33	delete if the box is still referenced by another box's connection
34	list).
35	
36	Started with: `luajit src/006-server-main.lua [port]` (default 7700).
37	The `run` script at the project root starts the server and opens the
38	editor.
39	
40	The editor is editor-only — no run button, no run results display.
41	The runner is invoked separately, by command line. `last-run.jsonl`
42	is the runner's output, consumed by external tooling, not by the
43	editor.
44	
45	### Phase 2 runner — `src/007-runner-main.lua`
46	
47	Synchronous Lua interpreter. Loads a map directory, validates the
48	graph, walks boxes in dependency order, invokes language drivers per
49	box (`drivers/lua.sh`, `drivers/c.sh`, `drivers/bash.sh`). Each box
50	invocation is wrapped in a `task_fn(inputs) -> outputs` boundary that
51	matches the phase 3 thread pool's action signature.
52	
53	This is the development target while the editor is being built out.
54	Phase 2 retires wholesale when phase 3 lands; the editor and the
55	map-directory format are unchanged across the cutover.
56	
57	### Phase 3 runner — `soramech-pool` (`src/008-pool-runner.c`)
58	
59	The phase 3 runner is a single C binary. It owns:
60	
```

## Assistant

## Reading A — full picture

A box configured with `routing.kind = "iterator"` declares `n_outputs` (an integer ≥ 1). The box gets `n_outputs` named output ports — `out_0`, `out_1`, …, `out_{n-1}` — laid out on the right edge in order.

When the box fires:
1. It consumes its inputs the normal way (one value from each input slot — same as any other box).
2. It produces ONE output value (also the normal way — single-output rule from 218).
3. The dispatch layer reads-and-increments a per-box atomic counter slot (`SLOT_ATOMIC_COUNTER`, from 302), takes the counter `mod n_outputs`, and routes the output value to *only that one branch*.

So with `n_outputs = 3`:
- Invocation 1 → `out_0` fires, `out_1` and `out_2` stay silent.
- Invocation 2 → `out_1` fires.
- Invocation 3 → `out_2` fires.
- Invocation 4 → `out_0` again. And so on.

Each branch's wire (or fan-out of wires) fires only on the invocations it was rotated to. The atomic counter is shared across parallel iterator-task workers, so two simultaneous invocations on two workers get distinct branch indices — no race, no coordination, no double-firing.

Use cases:
- **Round-robin distribution** across N downstream consumers (load spreading).
- **Cyclic pattern emission** (a stamp-and-rotate box marking three different sinks in turn).
- **N-phase pipeline** (boxes downstream of `out_0` do step 1, `out_1` does step 2 — though typically a series of plain calls reads cleaner for that).

It's the **output-side** counterpart to variadic inputs (217). Variadic inputs let one port accept N wires; iterator outputs let one port distribute to N wires across invocations.

The flip switch in the inspector is the existing mode dropdown 233 shipped: `plain | comparator | iterator`. Picking `iterator` exposes the `n_outputs` integer control. No new switch needed — what's there does this exactly.

---

## Commits

Plan acknowledged: **commit 1 is docs** (arch update + 244 + 304 rewrite), **commit 2 is code** (302 implementation). Doing commit 1 now.

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md`

## Assistant

_Tools:_

- `Bash: wc -l /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md && grep -n "^## \|^# " /mnt/mtwo/programs/sora/sorame...`

### Tool result

```
626 /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md
1:# SoraMech — Architecture
3:## Three programs, one data format
95:## Map directory format
131:## Box file format
291:## Language spec system (phase 3)
404:## Execution model — phase 2 (synchronous)
419:## Execution model — phase 3 (thread pool)
524:## Run output — `tmp/last-run.jsonl`
557:## Build & compile pipeline
601:## Data files
```

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md`

### Tool result

```
419	## Execution model — phase 3 (thread pool)
420	
421	Tasks are ephemeral; box state is durable. Each box owns one slot
422	per input port plus optional state (iterator counter, language
423	handle). A **task** is one stack frame on a worker — `box_id` is
424	all it carries. Tasks come and go; boxes persist for the run.
425	
426	### Slots
427	
428	Every input port has exactly one slot, allocated at graph load and
429	freed at run end. A wire is a routing declaration; the producer
430	doesn't own a slot — its output is a push event into every
431	downstream input slot it's wired to. Fan-out is N pushes; fan-in is
432	N producers pushing into one slot.
433	
434	Slot modes:
435	
436	| Mode                    | Cells   | Reads via         | Used for                                    |
437	|-------------------------|---------|-------------------|---------------------------------------------|
438	| 1-cell peek             | 1       | `slot_peek`       | Literals; wires from runs-once producers    |
439	| N-cell pop              | N       | `slot_pop`        | Wires whose producer runs many times        |
440	| `SLOT_ATOMIC_COUNTER`   | 1 atom  | `slot_read_inc`   | Iterator / randomizer / weighted counters   |
441	
442	Compile-time analysis decides per wire: a producer is "runs-once" if
443	no transitive ancestor is an iterator, else "runs-N-times." The
444	former gets a 1-cell peek slot (subsequent consumer tasks re-read the
445	same value); the latter gets an N-cell ring (FIFO drain, one cell
446	per consumer task).
447	
448	Variable-size payloads (long LLM outputs, dynamically-sized arrays)
449	live in a separate **large-value heap**. The slot then holds a small
450	fixed-size handle `{ uint32_t size; uint32_t offset; }`. The slot
451	allocator only ever sees fixed-size slots; the two-tier scheme keeps
452	the hot path simple while supporting unbounded values.
453	
454	### Cell tagging for parallel iterators
455	
456	Each cell optionally carries a 4-byte ordering tag alongside its
457	data. Iterator pushes carry their counter value as the tag; slots
458	downstream of an iterator are allocated with `SLOT_TAGGED`, and
459	`slot_pop` on a tagged slot returns the **lowest-tag** cell — not
460	the head. This preserves iteration order across parallel iterator
461	workers (task K of iterator A may push before task K-1 if it landed
462	on a faster worker; the tag makes the consumer see the values in
463	iteration order regardless).
464	
465	When a consumer has two tagged inputs from two iterators, each port
466	pops in tag order independently — pairing falls out as `(A_K, B_K)`
467	for every K.
468	
469	### The dispatch action
470	
471	One C function, one phase per worker per task:
472	
473	1. **Read inputs.** For each input port: `slot_peek` (peek mode) or
474	   `slot_pop` (pop mode) into a heap buffer. Tasks are spawned only
475	   when every input slot has a value (spawn-on-input-ready rule), so
476	   no waiting is needed.
477	2. **Invoke spec.** Look up the box's `lang`, find the worker's
478	   handle for it, call `invoke_native` or `invoke_json` (per the
479	   wire's fast-path classification). Output lands in a heap buffer
480	   sized to the box's declared output size, or into the large-value
481	   heap if the box's output is variable-size.
482	3. **Pick the branch.** Dispatch on `routing.kind`:
483	   - `plain`: every outgoing wire fires.
484	   - `comparator`: compare to `comparand`, fire only `lt`/`eq`/`gt`.
485	   - `iterator`: `idx = slot_read_inc(counter_slot, n_outputs)`,
486	     fire `out_idx` only.
487	   - `randomizer` / `weighted` / `distributor`: kind-specific rule
488	     (computes an `idx` from the counter slot or from downstream
489	     fill, fires that branch).
490	4. **Push.** For each firing wire, `slot_push` into the consumer's
491	   input slot. The push carries the appropriate ordering tag
492	   (iterator counter snapshot for iterator-rooted wires; 0 for
493	   plain). Each push triggers a spawn-on-input-ready check on the
494	   consumer; if the check passes, the dispatch layer spawns a new
495	   `dispatch_task_t` and the pool picks it up.
496	5. **Done.** The action returns; the pool decrements the active-task
497	   counter. The action does not block, does not wait, does not park.
498	
499	### Startup, quiescence, and termination
500	
501	Startup: the pool runner pushes literal-input values into the
502	corresponding slots, then spawns first tasks for boxes whose input
503	sets are now satisfied. Iterator counter slots start at 0.
504	
505	Quiescence: the main thread waits on a condition variable that
506	signals when the pool's active-task counter reaches zero with no
507	pending spawns. That's the run-over signal. The graph validator at
508	load time rejects non-iterator cycles, so the run cannot deadlock —
509	an iterator with no upstream pushes simply contributes nothing
510	further, and once the counter drains, termination fires.
511	
512	Hard-crash error policy: a nonzero `invoke_*` return aborts the
513	process. No retry, no per-task failed state, no fallback executor.
514	
515	### Blocking-aware iterator pattern
516	
517	Long-running operations (network calls, LLM inference) hold their
518	worker for the duration. Pool size is the budget. The cooperative
519	pattern for not holding a worker for minutes: split the call into a
520	"kickoff" box and a "check-done" iterator that polls. Each pass
521	through the iterator releases the worker between checks. Documented
522	fully in `docs/004-ipc-and-threading.md`.
523	
524	## Run output — `tmp/last-run.jsonl`
525	
526	Phase 3 writes one JSON object per event, one event per line, to
527	`tmp/last-run.jsonl`. The format decouples the runner from the
528	editor — the editor never displays run results; the file is for LLM
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md has been updated successfully.
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/docs/001-architecture.md has been updated successfully.
```

### Tool result

```
File created successfully at: /mnt/mtwo/programs/sora/soramech/issues/244-data-box-pull-on-demand.md
```

## Assistant

_Tools:_

- `Write: /mnt/mtwo/programs/sora/soramech/issues/244-data-box-pull-on-demand.md`

## Assistant

_Tools:_

- `Read: /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

### Tool result

```
1	# 304 — Task dispatch layer (C, replaces synchronous executor)
2	
3	## Status
4	open
5	
6	## Current behavior
7	The synchronous executor at `src/004-executor.lua` walks the graph in
8	dependency order, calling each box's driver, capturing stdout, decoding
9	JSON, and propagating values to downstream boxes. Single-threaded,
10	blocking, runs in Lua coroutines.
11	
12	## Concept
13	
14	The task dispatch layer is the C component that runs inside each pool
15	worker. The pool calls a single C action — `dispatch_action` — per
16	task. That action is the one place in the runtime that:
17	
18	- Reads from and writes to slots
19	- Calls `lang_spec->invoke`
20	- Picks the output branch per `routing.kind` (issue 233)
21	- Pushes the function's output to the picked branch's downstream
22	  input slots, triggering downstream task spawns
23	
24	There is no Lua-level executor in the phase 3 path. `004-executor.lua`
25	is replaced wholesale.
26	
27	## Task struct
28	
29	A task is one ephemeral invocation of a box. Its struct carries
30	just the box ID:
31	
32	```c
33	typedef struct {
34	    int  box_id;          // index into the graph's box array
35	} dispatch_task_t;
36	```
37	
38	There is no per-task counter. Iterator counter state lives in a
39	`SLOT_ATOMIC_COUNTER` slot owned by the box (see issue 302); the
40	action reads and atomically increments it via `slot_read_inc`
41	when it needs the routing index.
42	
43	There is no `input_slots` or `output_slot` array on the task —
44	the box's input slots are durable per-box state (see issue 302).
45	The action looks them up through `box_runtime_state[box_id]`.
46	
47	Output is a routing event, not a stored value. When the action
48	finishes, it pushes copies of the return value into the input
49	slots of every downstream box wired to this output (or only the
50	matching branch for comparators and iterators).
51	
52	Tasks are spawned by the dispatch layer's "spawn on input ready"
53	rule (issue 302). Each spawn allocates a fresh `dispatch_task_t`
54	on the pool's task allocator, pre-populated with the box ID. The
55	action's first read of the input slots is what drains them — pop
56	or peek one cell per input port, per the port's mode.
57	
58	## The dispatch action
59	
60	```c
```

## Assistant

_Tools:_

- `Bash: grep -n "^## \|^### " /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

### Tool result

```
3:## Status
6:## Current behavior
12:## Concept
27:## Task struct
58:## The dispatch action
66:### 1. Read inputs into byte buffers
87:### 3. Dispatch is uniform across routing kinds
110:### 4. Invoke (every call box)
140:### 5. Write output (push to downstream input slots)
163:### 6. (No phase 6)
169:### 7. Post-action: nothing for the action itself
176:### 8. Return ACT_DONE
181:## Initial submission
194:## Iterator counter and parallel iteration
216:## Cross-iterator pairing under parallel iteration
246:### Cross-iterator pairing of multiple iterator inputs
258:## Routing kinds (issue 233)
278:## Open questions
293:## Suggested implementation sequence
311:## Relevant files
321:## Implementation log
323:### Skeleton — 2026-05-12
352:### Plain-call dispatch + spawn-on-ready — 2026-05-12
399:### Comparator + iterator routing — 2026-05-12
437:### Task-id correlation — 2026-05-12
448:### Iterator multi-spawn + N-cell pop reads — 2026-05-12
480:### Cell-tagged ordering across parallel iterator workers — 2026-05-19
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md has been updated successfully.
```

## Assistant

_Tools:_

- `Write: /tmp/304-front.txt`

### Tool result

```
File created successfully at: /tmp/304-front.txt
```

## Assistant

_Tools:_

- `Bash: grep -n "^## Concept\|^## Iterator counter" /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

### Tool result

```
16:## Concept
198:## Iterator counter and parallel iteration
```

## Assistant

_Tools:_

- `Bash: { head -n 15 /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md; cat /tmp/304-front.txt; tail -n +198...`

### Tool result

```
13
453 /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md
```

## Assistant

_Tools:_

- `Bash: grep -n "^## \|^### " /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

### Tool result

```
3:## Status
10:## Current behavior
16:## Concept
42:## Task struct (cell layout)
81:## The dispatch action — the attempt loop
131:## Initial submission
144:## Iterator counter and parallel iteration
166:## Cross-iterator pairing under parallel iteration
196:### Cross-iterator pairing of multiple iterator inputs
208:## Routing kinds (issue 233)
228:## Open questions
243:## Suggested implementation sequence
261:## Relevant files
271:## Implementation log
273:### Skeleton — 2026-05-12
302:### Plain-call dispatch + spawn-on-ready — 2026-05-12
349:### Comparator + iterator routing — 2026-05-12
387:### Task-id correlation — 2026-05-12
398:### Iterator multi-spawn + N-cell pop reads — 2026-05-12
430:### Cell-tagged ordering across parallel iterator workers — 2026-05-19
```

## Assistant

_Tools:_

- `Bash: tail -5 /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

### Tool result

```
  reads them in invocation order.

Verified by re-running all 6 fixture maps and the dedicated
`test_iterator_multi_fire` test (which now exercises tagged
input slots).
```

## Assistant

_Tools:_

- `Edit: /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md`

### Tool result

```
The file /mnt/mtwo/programs/sora/soramech/issues/304-task-dispatch-layer.md has been updated successfully.
```

## Assistant

_Tools:_

- `Bash: git status --short`

### Tool result

```
 M docs/001-architecture.md
 M issues/304-task-dispatch-layer.md
?? docs/HTML/task-slab-allocator.html
?? issues/244-data-box-pull-on-demand.md
```

## Assistant

_Tools:_

- `Bash: git add docs/001-architecture.md docs/HTML/task-slab-allocator.html issues/244-data-box-pull-on-demand.md issues/304-...`

### Tool result

```
M  docs/001-architecture.md
A  docs/HTML/task-slab-allocator.html
A  issues/244-data-box-pull-on-demand.md
M  issues/304-task-dispatch-layer.md
```

## Assistant

_Tools:_

- `Bash: make test 2>&1 | grep -E "passed.*failed|FAIL|error" | tail -15`

### Tool result

```
  13 passed, 0 failed
  11 passed, 0 failed
  2 passed, 0 failed
  5 passed, 0 failed
  compile_bad_source_fails                     /tmp/soramech-bad-11413.c:1:1: error: unknown type name 'not'
/tmp/soramech-bad-11413.c:1:10: error: expected '=', ',', ';', 'asm' or '__attribute__' before 'close'
/tmp/soramech-bad-11413.c:1:10: error: unknown type name 'close'
  5 passed, 0 failed
  5 passed, 0 failed
  error_positions                              ok
  parse_file_error_line                        ok
  22 passed, 0 failed
  parse_error                                  lua spec: json_to_native: parse error at byte 1: expected string
  5 passed, 0 failed
  11 passed, 0 failed
```

## Assistant

_Tools:_

- `Bash: git commit -m "$(cat <<'EOF'
the structural shell — dispatch as attempt loop, data boxes as defaults

The architect...`

### Tool result

```
[master a342599] the structural shell — dispatch as attempt loop, data boxes as defaults
 4 files changed, 786 insertions(+), 190 deletions(-)
 create mode 100644 docs/HTML/task-slab-allocator.html
 create mode 100644 issues/244-data-box-pull-on-demand.md
```

## Assistant

Docs commit done in `a342599`. Four files: the architecture doc with the structural-shell section + attempt-loop dispatch description + your weaving passage, the new 244 issue file for data-box pull, the 304 rewrite to the attempt model, and the slab allocator HTML diagram.

Ready to start commit 2 — 302's implementation. Before I begin, one sanity check on scope:

**What 302 ships as code:**
- Slab allocator in `libs/task-pool/` (one large block at pool_create, per-thread cache, refill/drain on cache thresholds, atomic pop/push on the hot path).
- LVH refcount header (4-byte refcount + 4-byte length + data).
- Eager neighbor-merge on the LVH dealloc path (O(1) check of two physical neighbors, fold if free).
- Deep sweep at allocation-failure only (walk address-ordered chunk list, coalesce runs of free chunks before asking the OS for more memory).
- Address-ordered chunk list on the LVH.
- `box_runtime_state.live_predecessor_count` field populated at graph load.
- Box-retirement stretch: cascade decrement on task completion when `live_predecessor_count == 0` and queues drained. (Will include if time; otherwise the count is populated honestly but unread, per the structural-shell rule.)

**What 302 does NOT touch:**
- The dispatch model itself (still two-phase in code after this commit). The attempt-task refactor is described in 304 but lands later.
- 244's read-box pull (still push-at-startup in code).
- 312's per-edge classification (still per-box bit).
- 307's wrapper generation.

Roughly two days of careful work. The integration tests should still pass since none of this changes observable behavior — just the underlying allocator + a new field that nothing reads yet.

Want me to start now, or pause for any last design check?

## User

Before we start, can you describe the entire datapath of a program map? Please highlight where changes will occur.

## Assistant

Yes — full walkthrough, with the planned changes called out inline. **🔧 = 302 lands here.** *⏳ = another open issue will change this later.*

---

## 1 · Authoring (editor side)

User opens `http://localhost:7700/`, sees the canvas, creates boxes, wires them up, sets literal values and `n_outputs` and `comparand` and `routing.kind` via the inspector. Each save fires `API.put_box(id, data)` → HTTP PUT → `src/005-http-server.lua` writes the JSON to `maps/<name>/boxes/<id>.json`. Schema validation runs on every save through `src/001-schema.lua::validate_box`.

→ No 302 changes here.

## 2 · Compile

User clicks Compile → `scripts/soramech-compile.sh <map-dir>` runs:
- Reads `meta.json`, every `boxes/*.json`, every source file the boxes reference.
- Runs graph-level checks (`src/001-schema.lua::check_input_bindings` — every required port has a wire/literal/optional flag *⏳ 244: extends to recognize read-box predecessors*).
- Per-spec compile: C boxes get their `.so` built. *⏳ 307: this is where wrapper generation will land — typed signatures parsed from box JSON, wrapper.c emitted, compiled alongside user source.*
- *⏳ 313: same-language source files concatenated into one translation unit per language; the inlined call graph replaces per-box dispatch entries within a language island.*
- Copies the runner binary, all sources, all spec `.so`s into `compiled/`.

→ No 302 changes here.

## 3 · Runner startup

`./compiled/pool-runner <map-dir>` launches. The `main` in `src/008-pool-runner.c` parses argv, resolves `langs/` via `/proc/self/exe`, opens the map directory.

→ No 302 changes.

## 4 · Graph load (`src/010-graph-loader.c`)

Walks `boxes/*.json`, parses each via the 314 JSON parser into a `json_arena_t`. Builds `box_runtime_state[]` — one entry per box. Resolves wire endpoints (`from_box`, `to_box` strings → integer indices). Detects non-iterator cycles via DFS, aborts if found. Calls `graph_attach_runtime` to:

- Allocate per-input-port slots via `slot_alloc`. Slot mode (peek vs pop) chosen by the `multi_spawn` BFS (any box reachable from an iterator gets pop-mode ring slots).
- Resolve per-box spec via `box.lang` lookup in the spec registry.
- Set `SLOT_FLAG_LARGE_VALUE` on slots whose producers have `output_capacity == 0`.
- Compute the per-box `use_native_invoke` bit (same-language all-neighbors). *⏳ 312: this expands to per-edge `input_edge_native[i]` / `output_edge_native[j]` arrays.*

**🔧 302 adds here:**
- **`box_runtime_state.live_predecessor_count`** field, populated from the count of distinct upstream producer boxes. (Read-box predecessors don't count for *⏳ 244*: read boxes are pull-only, not producers.)
- **LVH chunk pool sizing** based on the size-class enumeration the loader does — *305's phase 5 deferred work that I'll finally land as part of 302.*

## 5 · Pool spin-up (`libs/task-pool/pool.c`)

`pool_create(n_workers)` spawns N worker threads, each parked on the init barrier. Main thread registers `spec_pool_init_cb` as the per-worker init callback. Workers wake in stage 2 of the barrier, call into the spec registry to populate their per-language handles (each gets its own `lua_State`, its own bash subprocess, its own dlopen handle cache). Stage 3 releases workers into the task loop.

**🔧 302 adds here:**
- **`slab_create(n_cells, n_workers)`** called inside `pool_create`. One big block of 64-byte cells, free-list initialized, per-thread cache arrays allocated. Each worker pops its initial 16 cells from the global free list during init.

## 6 · Initial submission

`src/008-pool-runner.c` walks the graph, identifies entry boxes (zero required inputs, or all required inputs satisfiable from literals / read-box predecessors). For each, calls `dispatch_spawn(ctx, box_id, priority)` to allocate a task and submit to the pool.

**🔧 302 changes here:**
- **Task allocation** moves from `pool_spawn`'s internal `malloc` to `slab_alloc`. One atomic-pop from the calling thread's local cache.

*⏳ 244 changes here:* Read boxes are no longer pushed at startup. They sit in `box_runtime_state` as callable sources. Entry-box detection still works; the dispatch will pull from them at attempt time.

## 7 · Steady-state hot loop — the attempt cycle

The current code (one task picked up = one full `dispatch_action` body): read inputs → invoke spec → push to consumer slots → inline `dispatch_spawn_if_ready` per consumer.

*⏳ 304 refactor:* This becomes the attempt loop described in the architecture doc. Producer's last act is to submit one attempt per distinct downstream consumer; the attempt itself checks inputs and runs or returns. **302 does not refactor the dispatch body** — that's 304's follow-on commit. But 302 does change the allocator the dispatch uses for spawning tasks.

**🔧 302 changes inside the hot loop:**
- **`dispatch_spawn` allocates from the slab.** `slab_alloc` returns a 64-byte cell. Task initialization writes fields into the cell instead of into a fresh malloc.
- **Task completion calls `slab_free`.** Pushes the cell back to the local thread cache.
- **`live_predecessor_count` decrement.** When a producer completes and its own count was already at 0 (no upstream can fire it again), the box has retired — decrement `live_predecessor_count` on every downstream box this producer fed. Cascade upward through retired boxes. *(The actual freeing-the-slot-table action is the stretch goal; 302 may or may not land it. The decrement itself happens unconditionally — the field is honest from day one.)*

## 8 · Value lifecycle

This is where 302's biggest changes land.

**Fixed-size values** (declared `output_capacity > 0`): producer writes bytes inline into each consumer's ring slot cell. Five consumers = five writes. No central allocation, no refcount. Consumer pops, cell becomes free again. **🔧 302 does not change this path** — fixed-size cells are pre-allocated at graph load and stay put.

**Variable-size values** (`output_capacity == 0`, the LVH path): producer calls `lvh_alloc(size)` → gets pointer + length → writes bytes → pushes `(ptr, length)` handle into each consumer slot.

**🔧 302 reshapes the LVH:**
- **Chunk layout grows a refcount header.** `[uint32 refcount | uint32 length | data...]`. Pointer the consumer sees is at `+8` from the chunk start.
- **`lvh_alloc(size, n_consumers)`** sets refcount = n_consumers (the producer knows this from the box's outgoing wire count to the matching branch).
- **Eager neighbor-merge on free.** When a consumer pops a `$ref` cell, its `slab_free`-equivalent for the LVH chunk decrements the refcount. At 0, the chunk returns to the size-class free list AND checks both physical neighbors — if either neighbor is also free, merge into a larger chunk on the appropriate larger free list.
- **Address-ordered chunk list.** A doubly-linked list of all chunks (free + allocated) in address order, separate from the size-class free lists. Used by neighbor-merge (O(1) lookup) and the deep sweep (O(n) walk).
- **Deep sweep on allocation-failure.** Before the LVH calls `mmap`/`sbrk` to grow, walk the address-ordered chunk list, find runs of contiguous free chunks, merge each run into one large chunk pushed onto the right size-class list. Re-try the allocation; only if THAT fails do we grow. This is the only deep-sweep trigger — no quiescence trigger, no timer.

*⏳ 312 changes the LVH-consumer interaction:* the wire format for cross-language hops carries `{"$ref": ptr, "$len": n}` JSON when the value is in LVH. Lua-to-Lua hops skip JSON entirely; pure-C-to-pure-C hops will too once 313 ships.

## 9 · Cross-language wire

Today: most cross-language hops serialize the value to a JSON string and put the string in the consumer slot. The Lua spec already implements native-to-JSON and JSON-to-native bridges (317 stage 2).

*⏳ 317 + 312 land here together later:* the wire carries plain JSON for small primitives (optionally wrapped with `$type` annotations when the receiver needs type info), and `$ref` handles for LVH values. Bash bridges and C bridges still pending.

→ **No 302 changes**, but 302's LVH refcount header IS the mechanism `$ref` consumers will use. So 302 unblocks 312/317 even though it doesn't touch their bridge code directly.

## 10 · Termination

Active-task counter hits 0 → `pool_wait_quiescent` returns → main thread calls `pool_destroy`. Workers join. Per-worker teardown callback runs (closes bash subprocesses, `lua_close` per state). Slot tables freed. JSONL writer thread drains and joins.

**🔧 302 adds here:**
- **`slab_destroy`** releases the slab block. No per-cell cleanup — the arena dies in one shot per the structural-shell rule.
- **LVH destroy** releases the chunk regions. Address-ordered list and size-class free lists go with them. No deep sweep at teardown either — the arena dies anyway.

→ `last-run.jsonl` is flushed and closed. `main` returns. Process exits.

---

## Summary of 302's touch points

| File | Change |
|---|---|
| `libs/task-pool/pool.c` + `pool.h` | Slab allocator API (`slab_create`, `slab_alloc`, `slab_free`, `slab_destroy`). Per-thread cache arrays. Hot-path lockless pop/push. |
| `src/015-large-value-heap.c` + `.h` | Refcount header layout. `lvh_alloc(size, refs)` initialized refcount. Eager neighbor-merge in `lvh_unref`. Address-ordered chunk list. Deep sweep on allocation-failure. |
| `src/010-graph-loader.c` | Populate `live_predecessor_count` from upstream-producer-box count. (Read-box predecessors excluded per 244.) Compute LVH chunk pool sizing from the per-box `output_capacity` enumeration. |
| `src/010-graph-loader.h` | `live_predecessor_count` field on the box runtime state. |
| `src/012-dispatch.c` | `dispatch_spawn` uses `slab_alloc`. Task completion calls `slab_free`. Cascade decrement of `live_predecessor_count` at task end. (Dispatch body unchanged — that's 304's refactor.) |
| `tests/302-*.c` (new) | Slab allocator unit tests: alloc-free roundtrip, refill at empty, drain at full, multi-threaded contention. LVH refcount tests: pop decrements, neighbor-merge fires, deep sweep recovers. |

Plus updates to `src/015-large-value-heap.info.md` and a fresh `libs/task-pool/slab.info.md` per project convention.

---

Want me to proceed, or any course corrections?
