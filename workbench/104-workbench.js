/*
 * 104-workbench.js — placing, naming, and wiring, and nothing else.
 *
 * What this is: step one of issue 801. A drawing of a map lives here
 * as two lists — stations and wires — and everything on screen is a
 * rendering of those two lists. No validation, no download, no
 * simulation; those are later steps and each of them reads this model
 * rather than replacing it.
 *
 * How it does it, in general terms: the model is plain data, the
 * screen is redrawn from it, and every interaction edits the model and
 * asks for a redraw. Nothing reads the screen to find out what the
 * drawing is, because a screen that is the source of truth is a screen
 * somebody has to keep in step by hand.
 *
 * **A station carries a fact the map file does not.** How many input
 * ports it has comes from the C — the box function's parameter list —
 * and the format deliberately never says. The page holds no C, so the
 * author says instead, and that number is layout rather than program:
 * it belongs in the companion file beside the map, never in the map.
 */

// {{{ the shelf, and the drawing that is one of its entries
/*
 * **The map on the canvas is one of the shelf's entries, not something
 * beside them.** That is the whole design of this part: switching away
 * cannot be a decision about whether the current work survives,
 * because there is no current work in a different place from the rest.
 *
 * A drawing is two lists and a counter. A station's identity is a
 * number that is never reused, so a wire can name its ends without
 * caring where anything sits in an array — the same reason a wire in
 * the engine is a pair of indices rather than a pair of pointers.
 */
function emptyDrawing() {
  return { stations: [], wires: [], nextId: 1 };
}

const shelf = { maps: [], at: 0 };
let drawing = emptyDrawing();

/*
 * Written to the browser's own storage as the work happens, so a tab
 * closed by accident costs nothing. **Nothing is sent anywhere**, which
 * is what lets "nothing on the server" stay true as written.
 *
 * Saved on every edit rather than on a timer: a drawing is small, and
 * a timer is a window in which work is not saved yet — which is the
 * window somebody's browser crashes in.
 */
const SHELF_KEY = 'soramech.workbench.shelf';

function remember() {
  shelf.maps[shelf.at].drawing = drawing;
  try {
    localStorage.setItem(SHELF_KEY, JSON.stringify(shelf));
  } catch (e) {
    /* Storage can be full or forbidden. Said once, in the place a
     * person is looking, rather than swallowed — a page that quietly
     * stopped saving is a page somebody trusts until they lose a
     * day. */
    const tally = document.getElementById('tally');
    if (tally) tally.textContent = 'not saving: ' + e.name;
  }
}

function recall() {
  let saved = null;
  try {
    saved = JSON.parse(localStorage.getItem(SHELF_KEY));
  } catch (e) {
    saved = null;
  }
  if (saved && Array.isArray(saved.maps) && saved.maps.length) {
    shelf.maps = saved.maps;
    shelf.at = Math.min(saved.at || 0, saved.maps.length - 1);
  } else {
    shelf.maps = [{ name: 'first', drawing: emptyDrawing() }];
    shelf.at = 0;
  }
  drawing = shelf.maps[shelf.at].drawing;
  /* An older shelf may predate a field this version expects. Filled in
   * rather than refused: somebody's saved work is not the place to be
   * strict about a schema. */
  if (!drawing.stations) drawing.stations = [];
  if (!drawing.wires) drawing.wires = [];
  if (!drawing.nextId) drawing.nextId = 1;
}

function switchTo(index) {
  remember();
  shelf.at = index;
  drawing = shelf.maps[index].drawing;
  render();
}

function newMap() {
  remember();
  shelf.maps.push({ name: 'map ' + (shelf.maps.length + 1),
                    drawing: emptyDrawing() });
  shelf.at = shelf.maps.length - 1;
  drawing = shelf.maps[shelf.at].drawing;
  render();
}

function dropMap(index) {
  shelf.maps.splice(index, 1);

  /* The shelf is never empty: discarding the last one leaves a fresh
   * blank rather than a page with nothing to draw on. */
  if (!shelf.maps.length)
    shelf.maps.push({ name: 'first', drawing: emptyDrawing() });

  /* **Which map is being drawn is followed, not clamped.** Dropping
   * something to the left of it shifts every index after that point
   * down by one, so keeping the same number would quietly move
   * somebody onto a different map — the one thing a shelf must never
   * do. Clamping was the first thing written here and it was wrong for
   * exactly that case. */
  if (index < shelf.at)
    shelf.at -= 1;
  shelf.at = Math.max(0, Math.min(shelf.at, shelf.maps.length - 1));

  drawing = shelf.maps[shelf.at].drawing;
  render();
}
// }}}

/* How many exits a kind has. A plain station has one; a comparator has
 * three, which is the whole of what makes it a comparator; an iterator
 * has as many as the author gives it and takes them in turn. */
function exitsOf(station) {
  if (station.kind === 'c') return 3;
  if (station.kind === 'i') return station.exits;
  return 1;
}

function addStation(x, y) {
  const id = drawing.nextId++;
  const s = {
    id,
    /* Named for its own number rather than the next one, which is what
     * reading `nextId` after incrementing it gave — the first station
     * called itself `station2`. */
    name: 'station' + id,
    box: '',
    kind: 'p',
    inPorts: 1,
    exits: 2,
    x: Math.round(x), y: Math.round(y),
  };
  drawing.stations.push(s);
  return s;
}

function stationById(id) {
  return drawing.stations.find(s => s.id === id);
}

/* A wire is drawn once. Drawing the same one twice is not an error
 * worth a message — it is somebody repeating themselves — so it is
 * quietly the same wire. */
function addWire(from, fromPort, to, toPort) {
  const already = drawing.wires.some(w =>
    w.from === from && w.fromPort === fromPort &&
    w.to === to && w.toPort === toPort);
  if (already) return;
  drawing.wires.push({ from, fromPort, to, toPort });
}
// }}}

// {{{ drawing the stations
const stationsLayer = document.getElementById('stations');
const wiresLayer = document.getElementById('wires');
const tally = document.getElementById('tally');

/*
 * Rebuilt from the model rather than patched in place. A canvas this
 * size redraws faster than anybody can notice, and patching is where a
 * screen and a model start disagreeing.
 */
function render() {
  stationsLayer.replaceChildren();
  for (const s of drawing.stations) stationsLayer.appendChild(draw(s));
  renderWires();
  renderShelf();
  tally.textContent =
    `${drawing.stations.length} station${drawing.stations.length === 1 ? '' : 's'}, ` +
    `${drawing.wires.length} wire${drawing.wires.length === 1 ? '' : 's'}`;
  remember();
}

/*
 * The shelf, redrawn with everything else. A tab is a name you can
 * type into and a way to leave; the one being drawn is marked rather
 * than removed from the list, because it is one of them.
 */
const shelfList = document.getElementById('shelf-list');

function renderShelf() {
  shelfList.replaceChildren();
  shelf.maps.forEach((entry, index) => {
    const tab = document.createElement('span');
    tab.className = 'tab' + (index === shelf.at ? ' here' : '');

    const name = document.createElement('input');
    name.value = entry.name;
    /* Sized to what it holds, in character widths, so a long name is
     * not hidden behind a fixed box and a short one does not sit in
     * a field of space. The `size` attribute would do this and cannot,
     * because the stylesheet gives the field a width. */
    name.style.width = Math.max(4, entry.name.length + 1) + 'ch';
    name.addEventListener('input', () => {
      entry.name = name.value;
      name.style.width = Math.max(4, name.value.length + 1) + 'ch';
      remember();
    });
    /* Clicking a tab you are not on switches to it; clicking the one
     * you are on puts the cursor in its name, which is what a click on
     * a name already looks like it will do. */
    name.addEventListener('focus', () => {
      if (index !== shelf.at) switchTo(index);
    });

    const drop = document.createElement('button');
    drop.className = 'drop';
    drop.textContent = '\u00d7';
    drop.title = 'discard this map';
    drop.addEventListener('click', () => dropMap(index));

    tab.append(name, drop);
    shelfList.appendChild(tab);
  });
}

function draw(s) {
  const el = document.createElement('div');
  el.className = 'station';
  el.style.left = s.x + 'px';
  el.style.top = s.y + 'px';
  el.dataset.id = s.id;

  // {{{ the grip: name, kind, and the box it places
  const grip = document.createElement('div');
  grip.className = 'grip';

  const name = document.createElement('input');
  name.value = s.name;
  name.title = 'what this station is called in the map';
  name.addEventListener('input', () => { s.name = name.value; remember(); });

  const kind = document.createElement('select');
  for (const [letter, what] of [['p', 'plain'], ['c', 'comparator'],
                                ['i', 'iterator']]) {
    const opt = document.createElement('option');
    opt.value = letter;
    opt.textContent = letter;
    opt.title = what;
    kind.appendChild(opt);
  }
  kind.value = s.kind;
  kind.title = 'plain, comparator, or iterator';
  kind.addEventListener('change', () => {
    s.kind = kind.value;
    /* Exits change with the kind, so wires leaving a port that no
     * longer exists have to go. Dropping them silently would leave the
     * drawing saying something the author cannot see. */
    const limit = exitsOf(s);
    drawing.wires = drawing.wires.filter(
      w => !(w.from === s.id && w.fromPort >= limit));
    render();
  });

  grip.append(name, kind);
  el.appendChild(grip);
  // }}}

  // {{{ the box function, typed rather than chosen
  const boxRow = document.createElement('div');
  boxRow.className = 'grip';
  const box = document.createElement('input');
  box.className = 'box-name';
  box.value = s.box;
  box.placeholder = 'file.c:function';
  box.title = 'the box this station places. the page cannot tell you ' +
              'whether it exists — that answer needs a compiler';
  /* Typing does not re-render, because rebuilding the field would
   * take the cursor out of it. It still has to be saved. */
  box.addEventListener('input', () => { s.box = box.value; remember(); });
  boxRow.appendChild(box);
  el.appendChild(boxRow);
  // }}}

  // {{{ the ports, in on the left and out on the right
  const body = document.createElement('div');
  body.className = 'body';

  const ins = document.createElement('div');
  ins.className = 'side in';
  for (let i = 0; i < s.inPorts; i++)
    ins.appendChild(port(s, i, 'in'));

  const outs = document.createElement('div');
  outs.className = 'side out';
  for (let i = 0; i < exitsOf(s); i++)
    outs.appendChild(port(s, i, 'out'));

  body.append(ins, outs);
  el.appendChild(body);
  // }}}

  // {{{ how many ports, which is layout and not program
  const count = document.createElement('div');
  count.className = 'count';
  count.appendChild(counter('in', s.inPorts, n => {
    s.inPorts = Math.max(1, n);
    drawing.wires = drawing.wires.filter(
      w => !(w.to === s.id && w.toPort >= s.inPorts));
    render();
  }));
  if (s.kind === 'i')
    count.appendChild(counter('exits', s.exits, n => {
      s.exits = Math.max(1, n);
      drawing.wires = drawing.wires.filter(
        w => !(w.from === s.id && w.fromPort >= s.exits));
      render();
    }));
  el.appendChild(count);
  // }}}

  return el;
}

function counter(label, value, set) {
  const wrap = document.createElement('span');
  const less = document.createElement('button');
  less.textContent = '−';
  less.addEventListener('click', () => set(value - 1));
  const more = document.createElement('button');
  more.textContent = '+';
  more.addEventListener('click', () => set(value + 1));
  const text = document.createElement('span');
  text.textContent = ` ${label} ${value} `;
  wrap.append(less, text, more);
  return wrap;
}

function port(s, index, side) {
  const el = document.createElement('div');
  el.className = 'port';
  el.dataset.station = s.id;
  el.dataset.port = index;
  el.dataset.side = side;
  const dot = document.createElement('span');
  dot.className = 'dot';
  const label = document.createElement('span');
  label.textContent = index;
  el.append(dot, label);
  return el;
}
// }}}

// {{{ drawing the wires
/*
 * Where a port sits is asked of the screen, because a port's position
 * is the one thing the model genuinely does not know: it depends on
 * how tall a station rendered, which depends on how many ports it has
 * and what the browser did with them.
 */
function portAt(stationId, index, side) {
  const el = stationsLayer.querySelector(
    `.port[data-station="${stationId}"][data-port="${index}"][data-side="${side}"]`);
  if (!el) return null;
  const dot = el.querySelector('.dot');
  const box = dot.getBoundingClientRect();
  const frame = stationsLayer.getBoundingClientRect();
  return {
    x: box.left - frame.left + box.width / 2,
    y: box.top - frame.top + box.height / 2,
  };
}

/* A wire leaves to the right and arrives from the left, so the curve
 * bows outward by an amount that grows with the gap — which keeps two
 * stations side by side from having a wire that looks like a straight
 * line through them. */
function curve(a, b) {
  const reach = Math.max(40, Math.abs(b.x - a.x) * 0.45);
  return `M ${a.x} ${a.y} C ${a.x + reach} ${a.y}, ` +
         `${b.x - reach} ${b.y}, ${b.x} ${b.y}`;
}

function renderWires() {
  wiresLayer.replaceChildren();
  for (const w of drawing.wires) {
    const a = portAt(w.from, w.fromPort, 'out');
    const b = portAt(w.to, w.toPort, 'in');
    if (!a || !b) continue;
    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute('class', 'wire');
    path.setAttribute('d', curve(a, b));
    wiresLayer.appendChild(path);
  }
  if (dragging.wire) {
    const path = document.createElementNS('http://www.w3.org/2000/svg', 'path');
    path.setAttribute('class', 'wire dragging');
    path.setAttribute('d', curve(dragging.wire.from, dragging.wire.at));
    wiresLayer.appendChild(path);
  }
}
// }}}

// {{{ picking things up
/*
 * One gesture handler for both things that can be dragged, because
 * they are the same gesture with different bookkeeping: press, move,
 * release. Two handlers would be two places for the release to be
 * forgotten.
 */
const canvas = document.getElementById('canvas');
const dragging = { station: null, wire: null };

function canvasPoint(event) {
  const frame = stationsLayer.getBoundingClientRect();
  return { x: event.clientX - frame.left, y: event.clientY - frame.top };
}

canvas.addEventListener('mousedown', event => {
  const portEl = event.target.closest('.port');
  if (portEl && portEl.dataset.side === 'out') {
    const from = portAt(+portEl.dataset.station, +portEl.dataset.port, 'out');
    dragging.wire = {
      station: +portEl.dataset.station,
      port: +portEl.dataset.port,
      from, at: from,
    };
    event.preventDefault();
    return;
  }

  const grip = event.target.closest('.grip');
  if (grip && event.target.tagName !== 'INPUT'
           && event.target.tagName !== 'SELECT') {
    const el = grip.closest('.station');
    const s = stationById(+el.dataset.id);
    const point = canvasPoint(event);
    dragging.station = { s, dx: point.x - s.x, dy: point.y - s.y };
    el.classList.add('holding');
    event.preventDefault();
  }
});

canvas.addEventListener('mousemove', event => {
  if (dragging.station) {
    const point = canvasPoint(event);
    dragging.station.s.x = Math.max(0, Math.round(point.x - dragging.station.dx));
    dragging.station.s.y = Math.max(0, Math.round(point.y - dragging.station.dy));
    const el = stationsLayer.querySelector(
      `.station[data-id="${dragging.station.s.id}"]`);
    el.style.left = dragging.station.s.x + 'px';
    el.style.top = dragging.station.s.y + 'px';
    renderWires();
    return;
  }
  if (dragging.wire) {
    dragging.wire.at = canvasPoint(event);
    const over = event.target.closest('.port');
    for (const p of stationsLayer.querySelectorAll('.port.aimed'))
      p.classList.remove('aimed');
    if (over && over.dataset.side === 'in')
      over.classList.add('aimed');
    renderWires();
  }
});

window.addEventListener('mouseup', event => {
  if (dragging.station) {
    const el = stationsLayer.querySelector(
      `.station[data-id="${dragging.station.s.id}"]`);
    if (el) el.classList.remove('holding');
    dragging.station = null;
    /* Moving does not re-render — the element is nudged directly, so
     * the drag stays smooth — so where it landed is saved here. */
    remember();
  }
  if (dragging.wire) {
    const over = event.target.closest && event.target.closest('.port');
    /* A wire that lands anywhere but an input port is a wire nobody
     * drew. Dropped without comment: an aimless drag is a change of
     * mind, not a mistake worth reporting. */
    if (over && over.dataset.side === 'in')
      addWire(dragging.wire.station, dragging.wire.port,
              +over.dataset.station, +over.dataset.port);
    dragging.wire = null;
    for (const p of stationsLayer.querySelectorAll('.port.aimed'))
      p.classList.remove('aimed');
    render();
  }
});

canvas.addEventListener('dblclick', event => {
  if (event.target.closest('.station')) return;
  const point = canvasPoint(event);
  addStation(point.x - 95, point.y - 30);
  render();
});
// }}}

document.getElementById('shelf-new').addEventListener('click', newMap);

recall();
render();
