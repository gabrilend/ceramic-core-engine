// C syntax lexer for the source viewer (issue 223).
//
// Same shape as the Lua and Bash lexers: returns ordered, gap-free
// {type, start, end} tokens covering the input. Skips preprocessor
// nuance (treats #directive lines as a single span tagged keyword).

const KEYWORDS = new Set([
  'auto','break','case','char','const','continue','default','do',
  'double','else','enum','extern','float','for','goto','if','inline',
  'int','long','register','restrict','return','short','signed',
  'sizeof','static','struct','switch','typedef','union','unsigned',
  'void','volatile','while','_Bool','_Complex','_Imaginary',
  // C99/C11 stuff that shows up in headers
  'bool','true','false','NULL',
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

    // Line comments: //
    if (c === '/' && text[i + 1] === '/') {
      const start = i;
      while (i < N && text[i] !== '\n') i++;
      push('comment', start, i);
      continue;
    }

    // Block comments: /* ... */
    if (c === '/' && text[i + 1] === '*') {
      const start = i;
      i += 2;
      while (i < N && !(text[i] === '*' && text[i + 1] === '/')) i++;
      if (i < N) i += 2;
      push('comment', start, i);
      continue;
    }

    // Preprocessor: # to end of line (with backslash-continuation)
    if (c === '#') {
      const prev = i === 0 ? '\n' : text[i - 1];
      if (prev === '\n' || i === 0 || prev === ' ' || prev === '\t') {
        const start = i;
        while (i < N) {
          if (text[i] === '\n' && text[i - 1] !== '\\') { break; }
          i++;
        }
        push('keyword', start, i);
        continue;
      }
    }

    // Strings and char literals: "..." or '...'
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

    // Numbers: 0x..., decimal, suffixes (UL etc.)
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
      // suffixes: u, l, ul, ull, f, etc.
      while (i < N && /[uUlLfF]/.test(text[i])) i++;
      push('number', start, i);
      continue;
    }

    // Identifiers / keywords
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c === '_') {
      const start = i;
      while (i < N && /\w/.test(text[i])) i++;
      const word = text.slice(start, i);
      push(KEYWORDS.has(word) ? 'keyword' : 'identifier', start, i);
      continue;
    }

    // Operators / punctuation
    if (/[=<>+\-*\/%!&|^~?:;,.(){}\[\]]/.test(c)) {
      const start = i;
      while (i < N && /[=<>+\-*\/%!&|^~?:;,.(){}\[\]]/.test(text[i])) i++;
      push('operator', start, i);
      continue;
    }

    push('plain', i, i + 1);
    i++;
  }

  return out;
}
// }}}
