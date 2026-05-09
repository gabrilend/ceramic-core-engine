// Lua syntax lexer for the source viewer (issue 223).
//
// Produces an ordered, gap-free list of typed tokens covering
// [0, text.length). Token types are convention strings; the viewer
// applies a CSS class `syntax-<type>` per span. Unknown types fall
// back to `syntax-default`.
//
// Hand-rolled — no regex backtracking quirks, no external dependency.
// Designed to be cheap enough to run on every viewer-open.

const KEYWORDS = new Set([
  'and','break','do','else','elseif','end','false','for','function',
  'goto','if','in','local','nil','not','or','repeat','return','then',
  'true','until','while',
]);

// {{{ tokenize
export function tokenize(text) {
  const out = [];
  let i = 0;
  const N = text.length;

  // {{{ push
  // Append a token of `type` covering [start, end). Empty tokens
  // skipped so we don't pollute the output with zero-width spans.
  function push(type, start, end) {
    if (end > start) out.push({ type, start, end });
  }
  // }}}

  while (i < N) {
    const c = text[i];

    // Whitespace runs
    if (c === ' ' || c === '\t' || c === '\n' || c === '\r') {
      const start = i;
      while (i < N && (text[i] === ' ' || text[i] === '\t' ||
                       text[i] === '\n' || text[i] === '\r')) i++;
      push('plain', start, i);
      continue;
    }

    // Comments: -- (line) or --[[ ]] (block, with optional levels)
    if (c === '-' && text[i + 1] === '-') {
      const start = i;
      i += 2;
      // long-bracket comment? --[=*[ ... ]=*]
      if (text[i] === '[') {
        let level = 0;
        let j = i + 1;
        while (j < N && text[j] === '=') { level++; j++; }
        if (text[j] === '[') {
          // long-bracket form: scan for matching ]=*]
          i = j + 1;
          const close = ']' + '='.repeat(level) + ']';
          const end_idx = text.indexOf(close, i);
          i = end_idx === -1 ? N : end_idx + close.length;
          push('comment', start, i);
          continue;
        }
      }
      // line comment: scan to newline
      while (i < N && text[i] !== '\n') i++;
      push('comment', start, i);
      continue;
    }

    // Strings: "..." '...' and long brackets [[...]] / [=*[...]=*]
    if (c === '"' || c === "'") {
      const quote = c;
      const start = i;
      i++;
      while (i < N) {
        if (text[i] === '\\' && i + 1 < N) { i += 2; continue; }
        if (text[i] === quote) { i++; break; }
        if (text[i] === '\n') break;
        i++;
      }
      push('string', start, i);
      continue;
    }
    if (c === '[') {
      let j = i + 1;
      let level = 0;
      while (j < N && text[j] === '=') { level++; j++; }
      if (text[j] === '[') {
        const start = i;
        i = j + 1;
        const close = ']' + '='.repeat(level) + ']';
        const end_idx = text.indexOf(close, i);
        i = end_idx === -1 ? N : end_idx + close.length;
        push('string', start, i);
        continue;
      }
    }

    // Numbers: 0x... hex, decimal with optional fraction and exponent
    if ((c >= '0' && c <= '9') ||
        (c === '.' && text[i + 1] >= '0' && text[i + 1] <= '9')) {
      const start = i;
      if (c === '0' && (text[i + 1] === 'x' || text[i + 1] === 'X')) {
        i += 2;
        while (i < N && /[0-9A-Fa-f]/.test(text[i])) i++;
      } else {
        while (i < N && text[i] >= '0' && text[i] <= '9') i++;
        if (text[i] === '.') {
          i++;
          while (i < N && text[i] >= '0' && text[i] <= '9') i++;
        }
        if (text[i] === 'e' || text[i] === 'E') {
          i++;
          if (text[i] === '+' || text[i] === '-') i++;
          while (i < N && text[i] >= '0' && text[i] <= '9') i++;
        }
      }
      push('number', start, i);
      continue;
    }

    // Identifiers / keywords: [A-Za-z_][\w]*
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c === '_') {
      const start = i;
      while (i < N && /\w/.test(text[i])) i++;
      const word = text.slice(start, i);
      push(KEYWORDS.has(word) ? 'keyword' : 'identifier', start, i);
      continue;
    }

    // Operators / punctuation: gather a run of non-word, non-space
    // chars. Multi-char operators (==, ~=, <=, >=, .., ..., ::) come
    // out as a single span, which matches how a reader scans them.
    if (/[=~<>+\-*\/%^#.,;:(){}\[\]&|]/.test(c)) {
      const start = i;
      while (i < N && /[=~<>+\-*\/%^#.,;:(){}\[\]&|]/.test(text[i])) i++;
      push('operator', start, i);
      continue;
    }

    // Anything else: a single plain char, so the cover-the-input
    // invariant holds even for unexpected characters.
    push('plain', i, i + 1);
    i++;
  }

  return out;
}
// }}}
