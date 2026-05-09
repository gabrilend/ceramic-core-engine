// Bash syntax lexer for the source viewer (issue 223).
//
// Same shape as the Lua lexer: returns an ordered, gap-free list of
// {type, start, end} tokens covering the entire input. Bash is a
// loose, context-sensitive language; this lexer aims for "looks
// right at a glance" rather than total accuracy.

const KEYWORDS = new Set([
  'if','then','elif','else','fi','for','while','until','do','done',
  'case','esac','in','function','select','time','break','continue',
  'return','exit','local','readonly','export','declare','typeset',
  'set','unset','shift','source',
]);

// {{{ tokenize
export function tokenize(text) {
  const out = [];
  let i = 0;
  const N = text.length;

  function push(type, start, end) {
    if (end > start) out.push({ type, start, end });
  }

  while (i < N) {
    const c = text[i];

    // Whitespace
    if (c === ' ' || c === '\t' || c === '\n' || c === '\r') {
      const start = i;
      while (i < N && (text[i] === ' ' || text[i] === '\t' ||
                       text[i] === '\n' || text[i] === '\r')) i++;
      push('plain', start, i);
      continue;
    }

    // Comments: # to end of line. Skipped if # appears inside a word
    // (e.g. `foo#bar` is one token), so we only treat it as comment
    // start at line-start or after whitespace — which we approximate
    // by checking the previous emitted char.
    if (c === '#') {
      const prev = i === 0 ? '\n' : text[i - 1];
      if (prev === ' ' || prev === '\t' || prev === '\n' || prev === '\r' || i === 0) {
        const start = i;
        while (i < N && text[i] !== '\n') i++;
        push('comment', start, i);
        continue;
      }
    }

    // Strings: "..." with escapes, '...' literal, $'...' ANSI-C
    if (c === '"' || c === "'") {
      const quote = c;
      const start = i;
      i++;
      while (i < N) {
        if (quote === '"' && text[i] === '\\' && i + 1 < N) { i += 2; continue; }
        if (text[i] === quote) { i++; break; }
        i++;
      }
      push('string', start, i);
      continue;
    }
    if (c === '$' && (text[i + 1] === "'" || text[i + 1] === '"')) {
      const start = i;
      const quote = text[i + 1];
      i += 2;
      while (i < N) {
        if (text[i] === '\\' && i + 1 < N) { i += 2; continue; }
        if (text[i] === quote) { i++; break; }
        i++;
      }
      push('string', start, i);
      continue;
    }

    // Variable expansions: $foo $1 ${...} $(...)
    if (c === '$') {
      const start = i;
      i++;
      if (text[i] === '{') {
        i++;
        let depth = 1;
        while (i < N && depth > 0) {
          if (text[i] === '{') depth++;
          else if (text[i] === '}') depth--;
          i++;
        }
      } else if (text[i] === '(') {
        // command substitution — treat the whole $(...) as identifier
        // for color purposes; the contents could be re-lexed but
        // approximating is fine for a viewer
        i++;
        let depth = 1;
        while (i < N && depth > 0) {
          if (text[i] === '(') depth++;
          else if (text[i] === ')') depth--;
          i++;
        }
      } else {
        while (i < N && /\w/.test(text[i])) i++;
      }
      push('identifier', start, i);
      continue;
    }

    // Numbers
    if (c >= '0' && c <= '9') {
      const start = i;
      while (i < N && text[i] >= '0' && text[i] <= '9') i++;
      push('number', start, i);
      continue;
    }

    // Identifiers / keywords
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c === '_') {
      const start = i;
      while (i < N && /[\w-]/.test(text[i])) i++;
      const word = text.slice(start, i);
      push(KEYWORDS.has(word) ? 'keyword' : 'identifier', start, i);
      continue;
    }

    // Operators / punctuation
    if (/[=<>+\-*\/%!&|;:(){}\[\],.?@]/.test(c)) {
      const start = i;
      while (i < N && /[=<>+\-*\/%!&|;:(){}\[\],.?@]/.test(text[i])) i++;
      push('operator', start, i);
      continue;
    }

    push('plain', i, i + 1);
    i++;
  }

  return out;
}
// }}}
