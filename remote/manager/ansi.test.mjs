import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFile } from 'node:fs/promises';
import { runInNewContext } from 'node:vm';

const script = await readFile(new URL('./public/ansi.js', import.meta.url), 'utf8');
function renderer() {
  const element = () => ({ style: {}, children: [], scrollTop: 20, scrollLeft: 8, scrollHeight: 200, clientHeight: 100,
    append(node) { this.children.push(node); },
    replaceChildren(fragment) { this.children = fragment.children; this.replacements = (this.replacements || 0) + 1; },
    set innerHTML(_) { assert.fail('HTML must never be parsed'); },
  });
  const render = runInNewContext(script + '\nrenderAnsi', {
    document: { createElement(tag) { assert.equal(tag, 'span'); return element(); }, createDocumentFragment: element },
  });
  const node = element();
  return { node, render: (text, truncated, suffix) => render(node, text, truncated, suffix), text: () => node.children.map(n => n.textContent).join('') };
}

test('base and bright foreground/background and all supported attribute resets', () => {
  const r = renderer();
  r.render('\x1b[1;2;3;4;31;104mstyled\x1b[22;23;24;39;49mplain\x1b[92;45mbright\x1b[0mreset');
  assert.deepEqual({ ...r.node.children[0].style }, { fontWeight: 'bold', opacity: '0.65', fontStyle: 'italic', textDecoration: 'underline', color: '#aa0000', backgroundColor: '#5555ff' });
  assert.deepEqual({ ...r.node.children[1].style }, {});
  assert.deepEqual({ ...r.node.children[2].style }, { color: '#55ff55', backgroundColor: '#aa00aa' });
  assert.deepEqual({ ...r.node.children[3].style }, {});
  for (let i = 0; i < 8; i++) {
    r.render(`\x1b[${30 + i};${40 + i}ma\x1b[${90 + i};${100 + i}mb`);
    assert.equal(r.node.children[0].style.color, r.node.children[0].style.backgroundColor);
    assert.equal(r.node.children[1].style.color, r.node.children[1].style.backgroundColor);
    assert.notEqual(r.node.children[0].style.color, r.node.children[1].style.color);
  }
});

test('256 colors and truecolor accept only bounded numeric channels', () => {
  const r = renderer();
  r.render('\x1b[38;5;196;48;2;1;20;255ma\x1b[38;5;232mb\x1b[38;5;255mc\x1b[38;5;9md');
  assert.equal(r.node.children[0].style.color, 'rgb(255, 0, 0)');
  assert.equal(r.node.children[0].style.backgroundColor, 'rgb(1, 20, 255)');
  assert.equal(r.node.children[1].style.color, 'rgb(8, 8, 8)');
  assert.equal(r.node.children[2].style.color, 'rgb(238, 238, 238)');
  assert.equal(r.node.children[3].style.color, '#ff5555');
  for (const code of ['38;2;256;0;0', '48;5;999', '38;2;1;2', '38;6;31']) {
    r.render(`\x1b[${code}mplain`);
    assert.deepEqual({ ...r.node.children[0].style }, {});
  }
});

test('HTML stays literal, OSC hyperlinks/titles and terminal controls are discarded', () => {
  const r = renderer();
  const html = '<img src=x onerror="alert(1)">&<script>bad()</script>';
  r.render(`${html}\x1b]8;;javascript:alert(1)\x1b\\label\x1b]8;;\x07\x1b]0;title\x07\x1b[2J\x1b[1A\x1b[?25l\x1bPignored\x1b\\\x00\x08\x07\rnext\r\nline\tend`);
  assert.equal(r.text(), html + 'label\nnext\nline\tend');
  assert.ok(r.node.children.every(n => Object.keys(n.style).length === 0));
});

test('partial escapes, truncated prefixes, and fresh bounded snapshots do not leak state', () => {
  const r = renderer();
  for (const partial of ['\x1b', '\x1b[', '\x1b[38;2;1', '\x1b]8;;https://example.test', '\x1bPunfinished']) {
    r.render('before' + partial); assert.equal(r.text(), 'before');
  }
  r.render('before\x1b[31mred'); assert.equal(r.text(), 'beforered');
  assert.equal(r.node.children.at(-1).style.color, '#aa0000');
  for (const prefix of ['[31m', '31m', ';2;255;0;0m', '[m']) {
    r.render(prefix + 'remaining\x1b[32mgreen', true);
    assert.equal(r.text(), 'remaininggreen');
    assert.deepEqual({ ...r.node.children[0].style }, {});
  }
  r.render('31m literal', false); assert.equal(r.text(), '31m literal');
  r.render('migration', true); assert.equal(r.text(), 'migration');
  const bounded = 'x'.repeat(256 * 1024 - 5) + '\x1b[31';
  r.render(bounded, true); assert.equal(r.text(), 'x'.repeat(256 * 1024 - 5));
  assert.equal(r.node.children.length, 1);
  r.render('plain'); assert.deepEqual({ ...r.node.children[0].style }, {});
  r.render('output\x1b]unfinished', false, '\nCommand completed.');
  assert.equal(r.text(), 'output\nCommand completed.');
  r.render('output\x1b]unfinished', false, '\nERROR: failed');
  assert.equal(r.text(), 'output\nERROR: failed');
});

test('unchanged snapshots keep nodes; updates preserve scroll or follow the bottom', () => {
  const r = renderer();
  r.render('first'); const child = r.node.children[0];
  r.render('first'); assert.equal(r.node.children[0], child); assert.equal(r.node.replacements, 1);
  r.render('second'); assert.equal(r.node.scrollTop, 20); assert.equal(r.node.scrollLeft, 8);
  r.node.scrollTop = 100; r.render('third'); assert.equal(r.node.scrollTop, 200);
});
