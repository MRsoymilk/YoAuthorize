// Bounded snapshots, not a terminal: no cursor movement, links, or HTML interpretation.
const renderAnsi = (() => {
  const snapshots = new WeakMap();
  const palette = ['#000000', '#aa0000', '#00aa00', '#aa5500', '#0000aa', '#aa00aa', '#00aaaa', '#aaaaaa',
    '#555555', '#ff5555', '#55ff55', '#ffff55', '#5555ff', '#ff55ff', '#55ffff', '#ffffff'];
  const indexed = n => {
    if (n < 16) return palette[n];
    if (n >= 232) return `rgb(${Array(3).fill(8 + (n - 232) * 10).join(', ')})`;
    const levels = [0, 95, 135, 175, 215, 255];
    n -= 16;
    return `rgb(${[levels[Math.floor(n / 36)], levels[Math.floor(n / 6) % 6], levels[n % 6]].join(', ')})`;
  };
  return (node, snapshot, truncated = false, suffix = '') => {
    const previous = snapshots.get(node);
    if (previous?.snapshot === snapshot && previous.truncated === truncated && previous.suffix === suffix) return;
    const top = node.scrollTop, left = node.scrollLeft;
    const atBottom = node.scrollHeight - node.clientHeight - top <= 2;
    const fragment = document.createDocumentFragment();
    let style = {}, text = snapshot;
    // A byte cap can remove ESC or part of an SGR prefix. Only trim recognizable remnants.
    if (truncated) text = text.replace(/^(?:\[[\d;:]*|[\d;:]+)m/, '');
    const flush = value => {
      if (!value) return;
      const span = document.createElement('span');
      span.textContent = value;
      // CSSOM property assignments are allowed by style-src 'self'; no style attributes/cssText.
      Object.assign(span.style, style);
      fragment.append(span);
    };
    const sgr = parameters => {
      if (!/^[\d;]*$/.test(parameters)) return;
      const codes = parameters.split(';').map(Number);
      for (let i = 0; i < codes.length; i++) {
        const code = codes[i];
        if (code === 0) style = {};
        else if (code === 1) style.fontWeight = 'bold';
        else if (code === 2) style.opacity = '0.65';
        else if (code === 3) style.fontStyle = 'italic';
        else if (code === 4) style.textDecoration = 'underline';
        else if (code === 22) { delete style.fontWeight; delete style.opacity; }
        else if (code === 23) delete style.fontStyle;
        else if (code === 24) delete style.textDecoration;
        else if (code === 39) delete style.color;
        else if (code === 49) delete style.backgroundColor;
        else if (code >= 30 && code <= 37) style.color = palette[code - 30];
        else if (code >= 90 && code <= 97) style.color = palette[code - 90 + 8];
        else if (code >= 40 && code <= 47) style.backgroundColor = palette[code - 40];
        else if (code >= 100 && code <= 107) style.backgroundColor = palette[code - 100 + 8];
        else if (code === 38 || code === 48) {
          const mode = codes[++i], count = mode === 5 ? 1 : mode === 2 ? 3 : 0;
          if (!count) break;
          const values = codes.slice(i + 1, i + 1 + count); i += count;
          if (values.length === count && values.every(n => Number.isInteger(n) && n >= 0 && n <= 255)) {
            style[code === 38 ? 'color' : 'backgroundColor'] = mode === 5 ? indexed(values[0]) : `rgb(${values.join(', ')})`;
          }
        }
      }
    };
    // Complete and trailing partial CSI/OSC/string escapes are swallowed. OSC links stay text only.
    const escapes = /(?:\x1b\[|\x9b)([0-?]*[ -/]*)([@-~]|$)|(?:\x1b\]|\x9d)[\s\S]*?(?:\x07|\x1b\\|\x9c|$)|\x1b[PX^_][\s\S]*?(?:\x1b\\|\x9c|$)|\x1b[ -/]*[0-~]?|[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x9f]|\r\n?/g;
    // Parse manager diagnostics separately so an unfinished escape cannot swallow them.
    for (const source of [text, suffix]) {
      style = {};
      let start = 0;
      for (const match of source.matchAll(escapes)) {
        flush(source.slice(start, match.index));
        if (match[2] === 'm') sgr(match[1]);
        else if (match[0][0] === '\r') flush('\n');
        start = match.index + match[0].length;
      }
      flush(source.slice(start));
    }
    node.replaceChildren(fragment);
    node.scrollTop = atBottom ? node.scrollHeight : top;
    node.scrollLeft = left;
    snapshots.set(node, { snapshot, truncated, suffix });
  };
})();
