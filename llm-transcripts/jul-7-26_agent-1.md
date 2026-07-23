# Conversation Summary: e7329deb-41a8-4d1e-929c-16407d4d0fd6

Generated on: 2026-08-03 14:19:28

--------------------------------------------------------------------------------

### User Request 1

Hi, this program currently uses Ollama. I'd like to switch to using llama.cpp,
because I've heard it's faster and more customizable. Can you investigate the
requirements of such a task, and then write issue files for such a future
development?


--------------------------------------------------------------------------------

### Assistant Response 1

I'll investigate how this program currently uses Ollama and understand the issue
file conventions before writing anything. Let me explore in parallel.


Let me dig into the actual Ollama integration and the conventions used in the
completed issue files.


Now let me understand the surrounding conventions — the info.md, docs
structure, phase organization, and any other references to Ollama's defaults.


Let me check the docs structure, phase themes, and verify the current llama.cpp
server API (it changes fast, so I want accuracy for the blueprints).


Let me verify the current `llama-server` API details from the official source so
the blueprints are accurate.


I now have a clear picture. Let me confirm a couple of last details — the free
issue numbers in the phase-2 range and whether the companion `info.md` exists.


I've finished the investigation. Here's what I found before I draft the
blueprints.


## What uses Ollama today


The integration is remarkably small — a single file, `libs/ollama.lua`, ~80
lines. It exposes two functions that shell out to `curl`:


- **`M.query(prompt, model, host)`** → POSTs to Ollama's `/api/generate`,
  reads back the `response` field.

- **`M.chat(prompt, system, model, host)`** → POSTs to `/api/chat`, reads back
  `message.content`.


Defaults are hard-coded: host `http://localhost:11434`, model `llama3.2`. It's
consumed as a **box source** — copied into a map's `src/`, the file browser
auto-detects the two functions and their ports. Created by completed issue
**209**; the old `llm-route` box that also called Ollama was already removed in
issue **210**.


## What changes for llama.cpp (verified against the current `llama-server` docs)


| Concern | Ollama (now) | llama.cpp `llama-server` |

|---|---|---|

| Text endpoint | `POST /api/generate` → `{response}` | `POST /completion` → `{content}` |

| Chat endpoint | `POST /api/chat` → `{message.content}` | `POST /v1/chat/completions` → `{choices[0].message.content}` (OpenAI shape) |

| Default port | 11434 | 8080 |

| Model selection | by name (`llama3.2`), auto-pulled | a **GGUF file path** passed to `-m` at startup; one model per process (or "router mode") |

| Lifecycle | background daemon, usually already running | **you launch it** with a model, GPU layers, ctx size |

| Customization | limited | rich: `n_predict`, `top_k/top_p/min_p`, `repeat_penalty`, and **GBNF grammar** for guaranteed-shape output |


`★ Insight
─────────────────────────────────────`

The real work isn't the HTTP rewrite (that's ~20 lines). It's that **Ollama
hides model management and llama.cpp doesn't**. Ollama is a name-addressable
daemon that pulls models for you; llama-server is a single-model process you
point at a `.gguf` file and launch yourself. So a faithful migration has to
*replace the daemon's convenience*, not just swap two URLs.

`─────────────────────────────────────────────────`


Two things I noticed in passing (won't touch without your say-so):
`libs/ollama.lua.info.md` doesn't actually exist even though issue 209 claims it
does, and the next free phase-2 issue numbers are **254+**.


Before I write the blueprints, three forks genuinely change their shape and are
yours to call:


**[Asked the user]**


*Q1 — Backend:* How should the llama.cpp client relate to the existing Ollama
one?

- Swappable backend — Keep libs/ollama.lua, add libs/llamacpp.lua with the
  same query/chat API, and a config value picks which one boxes call. Existing
  maps keep working; you can A/B the two and fall back if llama-server isn't up.

- Clean replacement — Retire the Ollama path. libs/llamacpp.lua becomes the
  only LLM client. Less code and no dual maintenance, but any existing ollama
  box needs rewiring and there's no fallback.

→ **Selected:** Clean replacement


*Q2 — Server:* Should the project own the llama-server process, or assume it's
already running?

- Project owns the server — A scripts/ launcher reads a config (GGUF path, ctx
  size, GPU layers, host/port), starts llama-server, and polls /health before
  boxes fire. Recreates the 'it just runs' feel Ollama gave you.

- Client-only — The library just points host/port at a llama-server you
  started by hand. Simplest to build, but nothing recreates Ollama's
  auto-running daemon — you manage the process yourself.

→ **Answered:** we should have the capability for both.


*Q3 — Capabilities:* Which llama.cpp capabilities are worth building into
boxes? (This scopes the 'more customizable' issue.)

- Sampling control — Expose temperature, top_k, top_p, min_p, repeat_penalty
  as optional port values so each LLM box can be tuned independently.

- Constrained output (GBNF) — A grammar field that forces the model to emit a
  guaranteed shape — a number, an enum, valid JSON. Pairs naturally with the
  comparator/routing boxes that need a clean value on the wire.

- Embeddings endpoint — Wrap /v1/embeddings so a box can turn text into a
  vector — useful for similarity/routing without a full generation call.

- Just speed for now — Keep the API minimal (query/chat only) and treat the
  extra knobs as a later issue; the win is llama.cpp being faster.

→ **Answered:** Sampling control, Constrained output (GBNF), Embeddings
endpoint, How does GBNF work?


Great answers — and good question. Let me answer **"How does GBNF work?"**
directly, because it's the crux of the constrained-output issue.


`★ Insight ─ How GBNF constrains a model ────────`

GBNF ("GGML BNF") is a grammar you hand to `llama-server` as a `grammar` string.
Mechanically:


1. At every generation step the model emits a probability over the **whole
   vocabulary** — every possible next token. Normally it samples freely from
   that.

2. With a grammar active, llama.cpp keeps a **parser state**: "given what's been
   generated so far, which tokens could legally come next?" Before sampling, it
   **zeroes out (−∞) the probability of every token that would break the
   grammar**. The model can only pick a token that keeps the output on a valid
   path.

3. So the output is *provably* valid against the grammar — not "usually valid
   JSON," but impossible-to-be-otherwise, because illegal tokens were never
   selectable.


A grammar is named rules; `root` is the entry point. Examples:

```gbnf

root ::= "yes" | "no"                    # a strict enum

root ::= "-"? [0-9]+ ("." [0-9]+)?       # a decimal number, nothing else

```

`─────────────────────────────────────────────────`


This is why it pairs *perfectly* with your comparator boxes (issue 210): the
executor **halts** if a box's output isn't a strict number. A
grammar-constrained LLM box can **guarantee** it emits a number — no parsing,
no halts. That synergy will be the centerpiece of the constrained-output issue.


Let me confirm the `config/` and `drivers/` layout so issue 255 references real
paths, then I'll write the four blueprints.


Everything checks out — `drivers/lua.sh` already puts `libs/?.lua` on
`LUA_PATH`, so a new `require("llamacpp")` will resolve once the file exists.
Now I'll write the four blueprints. Here's the shape, ordered foundational →
capstone (lower number = fewer blockers, per your conventions):


- **254** — the client library itself (clean replacement of `ollama.lua`)

- **255** — server lifecycle, supporting *both* a project-managed launcher and
  pointing at an external server

- **256** — sampling knobs + GBNF constrained output (the "more customizable"
  payoff)

- **257** — an embeddings box (new capability)


Now let me add these four to the phase-2 progress table so the phase index stays
coherent. Let me see where the table ends.


The table ends at issue 252. I'll insert the four new rows as `planned` right
before the phase checklist section.


Done. Four blueprints written, ordered so the foundational piece has the lowest
number and the capabilities stack on top:


| Issue | Scope | Depends on |

|---|---|---|

| **254** | `libs/llamacpp.lua` — clean replacement of the Ollama client. Same `query`/`chat` signatures (so existing boxes' ports don't change), new endpoints, port 8080, retire `ollama.lua`. | — |

| **255** | Server lifecycle — `config/llamacpp.conf` + `scripts/llama-server.sh`, supporting **both** a project-managed launcher *and* pointing at an external server, with a `/health` gate. | 254 |

| **256** | Sampling knobs (temp/top_k/top_p/min_p/repeat_penalty/n_predict) + GBNF grammars, with number/enum/JSON grammar builders. The number-grammar → comparator synergy is the centerpiece. | 254 |

| **257** | `M.embed` + `M.similarity` embeddings box; the similarity number drops onto a comparator for similarity-based routing. | 254, 255 |


I also added four `planned` rows to the phase-2 progress table so the phase
index stays complete.


Two things I did **not** do, deliberately:

- **No implementation and no `git` commit** — these are planning docs, not
  completed issues, and the tree already has other people's uncommitted work
  (transcripts, issues 247/249/251) that isn't mine to stage.

- **I flagged, but didn't fix, a doc bug**: issue 209 claims
  `libs/ollama.lua.info.md` exists — it doesn't. I captured that inside 254's
  plan (the replacement must ship a *real* `.info.md`) rather than silently
  patching a completed, immutable issue.


`★ Insight ─ Why "clean replacement" still keeps the signatures ─`

You chose to retire Ollama entirely, but 254 still preserves `M.query(prompt,
model, host)` / `M.chat(...)` verbatim. That's the leverage point: in soramech a
box's *ports* are derived from the function signature (the file browser
auto-populates them). Keep the signature identical and every existing LLM box in
every map keeps its exact port layout — the swap is invisible above the wire.
Change the signature and you'd be re-wiring maps by hand. The one semantic shift
you can't hide: `model` stops meaning "a name Ollama pulls" and becomes "ignored
unless the server is in router mode" — which is why 254 drops the `llama3.2`
default rather than lying about a model the server may not have loaded.

`─────────────────────────────────────────────────`


One thing worth your thought before any implementation begins — the biggest
unknown across these four is buried in 255: **who watches the watcher?** If a
managed `llama-server` crashes mid-run, do you want fire-and-forget (you restart
it) or supervised auto-restart? I left it as an open question in the issue
rather than deciding for you. What's your instinct — is a crashing inference
server a real failure mode on your hardware, or rare enough to not build for
yet?


Want me to start on 254, or refine any of the blueprints first?


--------------------------------------------------------------------------------

### User Request 2

  One thing worth your thought before any implementation begins — the biggest
  unknown across these four is buried in 255: who watches the

  watcher? If a managed llama-server crashes mid-run, do you want
  fire-and-forget (you restart it) or supervised auto-restart? I left it as

  an open question in the issue rather than deciding for you. What's your
  instinct — is a crashing inference server a real failure mode on

  your hardware, or rare enough to not build for yet?



the "startup" command should be an HTTP server that runs on the remote (or
local) LLM server. If multiple "start" requests are received, the subsequent
ones are silently dropped, and the requesting clients are added to a list of
clients to notify when the server has successfully started.


  ★ Insight ─ Why "clean replacement" still keeps the signatures ─

  You chose to retire Ollama entirely, but 254 still preserves M.query(prompt,
  model, host) / M.chat(...) verbatim. That's the leverage

  point: in soramech a box's ports are derived from the function signature (the
  file browser auto-populates them). Keep the signature

  identical and every existing LLM box in every map keeps its exact port layout
  — the swap is invisible above the wire. Change the signature

  and you'd be re-wiring maps by hand. The one semantic shift you can't hide:
  model stops meaning "a name Ollama pulls" and becomes "ignored

  unless the server is in router mode" — which is why 254 drops the llama3.2
  default rather than lying about a model the server may not have

  loaded.

  ─────────────────────────────────────────────────



this confuses me. Don't we need to provide a model name? Or is it one llama.cpp
server per model? If so, then our http manager server (Ollama, re-implemented)
should route requests to each server depending on the model requested.
Otherwise, we should process using the correct model as declared in the function
call arguments (model parameter)


  - I flagged, but didn't fix, a doc bug: issue 209 claims
    libs/ollama.lua.info.md exists — it doesn't. I captured that inside 254's
    plan

  (the replacement must ship a real .info.md) rather than silently patching a
  completed, immutable issue.


we are removing Ollama so this is not a bug, but rather a pre-done cleanup
opportunity. Silly robot, by adding such a flag you are simply undo-ing the
pre-done cleanup opportunity! You made a mess! That's okay, we are right back
where we started, which makes sense because we are at the beginning. Good job
for keeping things congruent! Can you provide useful continuation of the
requested process?


--------------------------------------------------------------------------------

### Assistant Response 2

You've clarified the architecture, and it's a better shape than what I drafted.
Let me feed both corrections back:


1. **On the "watcher" question** — your answer dissolves it. The start
   operation is an HTTP manager that dedupes concurrent starts (subsequent ones
   dropped) and holds a notify-list of waiting clients. That same mechanism *is*
   the crash-recovery answer: a dead backend just fails its health check, so the
   next request re-runs the start-rendezvous. No separate supervisor loop —
   one path handles cold-start, concurrent-start, and re-spawn-after-crash.


2. **On the model confusion — you're right and I was wrong.** Here's the
   mechanical truth:


`★ Insight ─ One model per process, so the manager IS the router ─`

A bare `llama-server -m modelA.gguf` serves *only* modelA on its port; the
request's `model` field is ignored. That's why, with no manager, `model` looked
meaningless. But you don't want a bare server — you want the
re-implemented-Ollama manager. So the manager keeps a **registry**: `model name
→ a llama-server instance` (spawn one per model, each on its own port, lazily
on first use). Now `model` is meaningful again — it's the **routing key** the
manager resolves to pick (or spawn) the right backend. So `M.query(prompt,
model, host)` keeps `model`, and `host` points at the *manager*, not a raw
server. Exactly your "route requests to each server depending on the model
requested."

`─────────────────────────────────────────────────`


3. **On the info.md — you got me.** Removing Ollama means its missing
   `.info.md` was cleanup already done for free; my "flag" re-created the mess.
   Reverting that framing now.


Let me rewrite 254 (client now talks to the manager, `model` is the routing key)
and 255 (now the manager daemon), and lightly touch 256/257.


Now remove the old 255 file (it was an untracked draft from this session,
superseded by the manager reframe):


Now light touches to 256 and 257 so they align with the manager reframe.


Now sync the phase-2 progress rows for 254 and 255 to the revised design.


Let me verify the cross-references are consistent — no dangling pointers to
the old 255 filename.


The blueprints now reflect your architecture. Cross-references are clean, the
old 255 draft is gone, and the phase-2 index is synced.


## Where the four issues landed


| Issue | Now says | Depends on |

|---|---|---|

| **254** — client library | `libs/llamacpp.lua` replaces `libs/ollama.lua`, same `query`/`chat` signatures, but points at the **manager**; `model` is the routing key, not a dead argument | 255 (for real routing) |

| **255** — manager daemon | *"Ollama, re-implemented on llama.cpp"*: name→backend **registry**, lazy one-`llama-server`-per-model spawn, **idempotent start + notify-list rendezvous**, request routing, managed *and* external per model | 254 (shared contract) |

| **256** — sampling + GBNF | knobs + grammar ride through the manager verbatim to the backend | 254 |

| **257** — embeddings | `embedding=true` is a per-model registry flag; `M.embed`/`M.similarity` route by model like generation | 254, 255 |


## The two design threads you tightened


**The rendezvous is one mechanism doing three jobs.** In 255 I wrote it so
single-flight spawn, duplicate-drop, and post-crash re-spawn are literally the
same code path — a health-check failure just flips a model back to `not
started`, and the next request re-enters the same start-rendezvous. That's why
there's no separate supervisor: "who watches the watcher" collapses into "the
next request does."


`★ Insight ─ Long-poll IS the notify-list ────────`

Your "list of clients to notify when the server has started" has a clean HTTP
shape: each waiting client's request is simply *held open*, and the waiter list
is the set of those pending responses. When `/health` flips to ok, the manager
flushes them all at once. No callback URLs, no client-side server — a soramech
box is just `curl`, and `curl` blocking until ready is exactly the notification.
The one thing to get right is the long-poll timeout, so a genuinely wedged model
load surfaces as an error instead of an infinite hang.

`─────────────────────────────────────────────────`


**`model` came back to life.** Because one `llama-server` = one model, the
manager's registry is what makes `model` meaningful again — it's the key that
selects (or spawns) a backend. So 254 keeps the signature *and* the argument
does real work, which it couldn't against a bare server.


One open sub-decision I deliberately left for implementation time, in 255:
**port allocation for spawned backends** — let the OS hand out ephemeral ports
and record them in the live registry, or carve a fixed range in
`config/llamacpp.conf`. Ephemeral is simpler and I'd default to it, but a fixed
range is easier to firewall on a remote host. Worth your call when we build it
— does the manager ever run on a box where you'd want the backend ports
pinned?


Want me to start implementing **254** (the client is the foundation and testable
against a single hand-started `llama-server` before the manager exists), or keep
refining the manager blueprint first?


--------------------------------------------------------------------------------
