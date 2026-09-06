/*
 * 122-viewer.js — draws the graph, and lights it as it runs.
 *
 * Four jobs. Read the map file and work out the shape. Lay that shape
 * out on a grid as close to square as the count allows. Draw it as
 * ports and wires rather than boxes and arrows, because the ports are
 * the mechanism. Then listen, and let events change how it looks and
 * nothing else.
 *
 * **Nothing here can reach the program.** The only channel is the event
 * stream, which arrives. Saving a layout is a download the browser
 * performs, not something written on the far end.
 *
 * A note on what the trail does and does not carry. Events say a value
 * moved from one port to another and that a station ran; they never
 * carry the value itself, because a value is any size at all and
 * copying one onto the trail would be a cost paid on the delivery path.
 * So the depth of a buffer is not reported — it is **derived** here, by
 * counting arrivals against runs, exactly the way the engine itself
 * decides a station is ready.
 */

/* {{{ parseMap(text) */
/*
 * The map file's grammar, which is small on purpose.
 *
 *   station NAME BOX KIND [entry|result] [@N]
 *     in  P = VALUE | $N | - | [a, b] | xN
 *     out P - TARGET.PORT
 *
 * A port's kind matters to the picture: a ring buffer queues and can
 * back up, a static always holds one value and never gates readiness,
 * and a port with no source at all means the station can never run.
 */
function parseMap(text) {
    const stations = [];
    const byName = new Map();
    let current = null;

    for (const raw of text.split("\n")) {
        /*
         * A dump says more than a hand-written map does, in comments:
         * which index each station actually has, and how deep each
         * buffer currently is. Both are read where they are offered,
         * because a station's index is what every event names and
         * counting down the file to guess it is how a picture ends up
         * confidently wrong.
         */
        const note = raw.includes("#") ? raw.slice(raw.indexOf("#")) : "";
        const line = raw.replace(/#.*$/, "").trimEnd();

        const portNote = note.match(/#\s*port\s+(\d+):\s*(\w+)/);
        if (portNote && current) {
            const at = +portNote[1];
            while (current.ports.length <= at) current.ports.push(null);
            const slots = note.match(/(\d+)\s+slots/);
            current.ports[at] = {
                kind: portNote[2] === "buffer" ? "ring"
                    : portNote[2] === "constant" ? "static" : "none",
                held: 0,
                capacity: slots ? +slots[1] : 0,
            };
            continue;
        }
        if (!line.trim()) continue;

        const station = line.match(/^station\s+(\S+)\s+(\S+)\s+(\S+)(.*)$/);
        if (station) {
            const said = note.match(/#\s*station\s+(\d+)/);
            current = {
                index: said ? +said[1] : stations.length,
                name: station[1],
                box: station[2],
                kind: station[3],
                door: /\bentry\b/.test(station[4]) ? "in"
                    : /\bresult\b/.test(station[4]) ? "out" : null,
                ports: [],          /* input ports, by index */
                outs: 1,            /* how many exits it has */
                wires: [],
            };
            stations.push(current);
            byName.set(current.name, current);
            continue;
        }
        if (!current) continue;

        const out = line.match(/^\s+out\s+(\d+)\s+-\s+(\S+)\.(\d+)/);
        if (out) {
            const from = +out[1];
            current.outs = Math.max(current.outs, from + 1);
            current.wires.push({ fromPort: from, toName: out[2], toPort: +out[3] });
            continue;
        }
        const inp = line.match(/^\s+in\s+(\d+)\s*(.*)$/);
        if (inp) {
            const at = +inp[1], rest = inp[2].trim();
            const kind = rest === "-" ? "none"
                       : (rest.startsWith("=") || rest.startsWith("$")) ? "static"
                       : "ring";
            while (current.ports.length <= at) current.ports.push(null);
            const had = current.ports[at];
            current.ports[at] = { kind, held: 0, capacity: had ? had.capacity : 0 };
        }
    }

    /* A comparator has three exits whether or not all three are wired,
     * and a port a wire lands on is a ring buffer whatever the file
     * said about it. */
    for (const s of stations) {
        if (s.kind === "c") s.outs = Math.max(s.outs, 3);
        for (const w of s.wires) {
            const target = byName.get(w.toName);
            if (!target) continue;
            while (target.ports.length <= w.toPort) target.ports.push(null);
            if (!target.ports[w.toPort])
                target.ports[w.toPort] = { kind: "ring", held: 0, capacity: 0 };
        }
    }
    for (const s of stations) {
        if (s.ports.length === 0) s.ports.push({ kind: "ring", held: 0, capacity: 0 });
        for (let i = 0; i < s.ports.length; i++)
            if (!s.ports[i]) s.ports[i] = { kind: "none", held: 0, capacity: 0 };
    }
    return { stations, byName };
}
/* }}} */

/* {{{ the drawing's constants */
const BOX_W = 190, HEAD_H = 40, PORT_H = 20, BOX_MIN = 62;
const PAD_X = 130, PAD_Y = 80, MARGIN = 48;
const GLOW_MS = 700;         /* how long a station stays lit after running */
const FRAME_MS = 33;

/*
 * A wire shows that it is **carrying**, not what it carries.
 *
 * There were dots crossing the wires, one per delivery, and they were a
 * lie in two directions: the trail carries no values, so a dot stood
 * for nothing in particular, and it carries no durations either, so the
 * speed was invented. Worse, a dot was still crawling along the wire
 * after the station at the far end had already run and drained the
 * value it was pretending to be.
 *
 * So: an active wire brightens and its chevrons march toward the
 * destination. The marching says *this way*, the brightness says *just
 * now*, and neither claims to be a particular value in a particular
 * place. What is true is what is drawn.
 */
const WIRE_WARM_MS = 900;    /* how long a wire stays lit after carrying */
const CHEVRON_SPEED = 46;    /* world units per second the marks travel */
const CHEVRON_GAP = 26;
/* }}} */

/* {{{ boxHeight(s) */
function boxHeight(s) {
    return Math.max(BOX_MIN, HEAD_H + Math.max(s.ports.length, s.outs) * PORT_H + 10);
}
/* }}} */

/* {{{ gridFor(count) */
/* As close to square as the count allows, preferring an extra column to
 * an extra row: a screen is wider than it is tall. */
function gridFor(count) {
    const cols = Math.ceil(Math.sqrt(count));
    return { cols, rows: Math.ceil(count / cols) };
}
/* }}} */

/* {{{ View */
/*
 * Where the window is looking, in the drawing's own coordinates. The
 * paper has no edges: the grid is drawn to cover whatever is in view
 * and the picture is never resized to fit its contents, which is what
 * made the whole graph jump every time a box was dragged past the
 * old boundary.
 */
const View = { x: 0, y: 0, scale: 1 };
const ZOOM_MIN = 0.12, ZOOM_MAX = 4;

/*
 * Zoom in even steps of a fixed ratio, so every notch feels the same
 * whatever it came from. A wheel reports its movement in one of three
 * units — pixels, lines or pages — and a browser picks whichever it
 * likes, so the raw number is meaningless until it is converted. Not
 * converting it is what made a mouse barely zoom while a trackpad
 * lurched: the same gesture arrives as 3 from one and 300 from the
 * other.
 */
const ZOOM_STEP = 1.15;
const WHEEL_UNIT = [1, 16, 800];   /* pixels, lines, pages */

function wheelNotches(event) {
    const unit = WHEEL_UNIT[event.deltaMode] || 1;
    const pixels = event.deltaY * unit;
    /* Bounded, so one violent flick is one firm zoom rather than a jump
     * to the far end of the range. */
    return Math.max(-4, Math.min(4, pixels / 100));
}

/* {{{ zoomAbout(scale, sx, sy) */
/* Change the scale while holding one point of the drawing still under
 * one point of the screen. Without this a zoom drifts, which is the
 * whole of what makes zooming feel wrong. */
function zoomAbout(scale, sx, sy) {
    const box = svg.getBoundingClientRect();
    const px = sx === undefined ? box.width / 2 : sx - box.left;
    const py = sy === undefined ? box.height / 2 : sy - box.top;

    const worldX = View.x + px / View.scale;
    const worldY = View.y + py / View.scale;

    View.scale = Math.min(ZOOM_MAX, Math.max(ZOOM_MIN, scale));

    View.x = worldX - px / View.scale;
    View.y = worldY - py / View.scale;
    applyView();
    showZoom();
}
/* }}} */

/* {{{ showZoom() */
function showZoom() {
    const chip = document.getElementById("zoomlevel");
    if (chip) chip.textContent = Math.round(View.scale * 100) + "%";
}
/* }}} */

function viewport() {
    const box = svg.getBoundingClientRect();
    return { w: box.width || 1200, h: box.height || 700 };
}

function applyView() {
    const { w, h } = viewport();
    svg.setAttribute("viewBox",
        `${View.x} ${View.y} ${w / View.scale} ${h / View.scale}`);
}

/* Screen to drawing, which every pointer event needs. */
function pointAt(event) {
    const box = svg.getBoundingClientRect();
    return {
        x: View.x + (event.clientX - box.left) / View.scale,
        y: View.y + (event.clientY - box.top) / View.scale,
    };
}

/* {{{ lookAt(cx, cy, scale) */
function lookAt(cx, cy, scale) {
    const { w, h } = viewport();
    View.scale = Math.min(ZOOM_MAX, Math.max(ZOOM_MIN, scale));
    View.x = cx - w / View.scale / 2;
    View.y = cy - h / View.scale / 2;
    applyView();
    showZoom();
}
/* }}} */
/* }}} */

/* {{{ Picture */
const Picture = {
    stations: [],
    byName: new Map(),
    placed: new Map(),        /* index -> {x, y, pinned} */
    lastRun: new Map(),       /* index -> timestamp */
    wireLast: new Map(),      /* wire -> when it last carried anything */
    lostTotal: 0,
    tally: { ran: 0, moved: 0, due: 0 },
};
function stationAt(index) {
    return Picture.stations.find((s) => s.index === index);
}
/* }}} */

/* {{{ layOut() */
/* Every station nobody has dragged gets a cell. A dragged station is
 * pinned and nothing here moves it again. */
function layOut() {
    const { cols } = gridFor(Picture.stations.length);
    let cell = 0, rowTop = MARGIN, rowTall = 0;
    for (const s of Picture.stations) {
        const at = Picture.placed.get(s.index);
        const col = cell % cols;
        if (col === 0 && cell > 0) { rowTop += rowTall + PAD_Y; rowTall = 0; }
        rowTall = Math.max(rowTall, boxHeight(s));
        if (!at || !at.pinned)
            Picture.placed.set(s.index,
                { x: MARGIN + col * (BOX_W + PAD_X), y: rowTop, pinned: false });
        cell++;
    }
}
/* }}} */

/* {{{ placeNear(index, neighbours) */
/*
 * Where a station that appeared mid-run should sit once its wires
 * arrive: roughly equidistant from what it connects to, nudged until it
 * overlaps nothing.
 *
 * This cannot run when the box first appears, because creating a
 * station and wiring it are separate events and at that moment it has
 * no wires at all. It is called again as each wire lands, and stops
 * mattering the moment somebody drags the box.
 */
function placeNear(index, neighbours) {
    const at = Picture.placed.get(index);
    if (!at || at.pinned || neighbours.length === 0) return;

    let x = 0, y = 0, seen = 0;
    for (const n of neighbours) {
        const p = Picture.placed.get(n);
        if (!p) continue;
        x += p.x; y += p.y; seen++;
    }
    if (!seen) return;
    x = x / seen + BOX_W + PAD_X;
    y = y / seen;

    for (let tries = 0; tries < 300; tries++) {
        let clash = false;
        for (const [other, p] of Picture.placed) {
            if (other === index) continue;
            if (Math.abs(p.x - x) < BOX_W + 24 && Math.abs(p.y - y) < BOX_MIN + 24) {
                clash = true; break;
            }
        }
        if (!clash) break;
        y += PORT_H;
    }
    at.x = Math.max(MARGIN, x);
    at.y = Math.max(MARGIN, y);
}
/* }}} */

/* {{{ the elements */
const svg = document.getElementById("canvas");
const gridLayer = document.getElementById("grid");
const wiresLayer = document.getElementById("wires");
const stationsLayer = document.getElementById("stations");
const stateChip = document.getElementById("state");
const lostChip = document.getElementById("lost");
const joinedChip = document.getElementById("joined");
const tallyText = document.getElementById("tally");
const mapName = document.getElementById("mapname");
const svgNS = "http://www.w3.org/2000/svg";

function el(name, attrs, text) {
    const node = document.createElementNS(svgNS, name);
    for (const k in attrs) node.setAttribute(k, attrs[k]);
    if (text !== undefined) node.textContent = text;
    return node;
}
/* }}} */

/* {{{ portAnchor(s, side, index) */
/* Where a wire meets a port: the left edge for an input, the right for
 * an exit. Wires join ports rather than boxes, because which port is
 * the whole of what a wire says. */
function portAnchor(s, side, index) {
    const at = Picture.placed.get(s.index);
    if (!at) return null;
    const y = at.y + HEAD_H + index * PORT_H + PORT_H / 2;
    return { x: side === "in" ? at.x : at.x + BOX_W, y };
}
/* }}} */

/* {{{ curveThrough(a, b) */
function curveThrough(a, b) {
    const bend = Math.max(40, Math.abs(b.x - a.x) / 2);
    const c = { d: `M ${a.x} ${a.y} C ${a.x + bend} ${a.y}, ${b.x - bend} ${b.y}, ${b.x} ${b.y}`,
                p0: a, p1: { x: a.x + bend, y: a.y },
                p2: { x: b.x - bend, y: b.y }, p3: b };
    return c;
}
/* }}} */

/* {{{ drawGrid(w, h) */
function drawGrid() {
    const { w, h } = viewport();
    const wide = w / View.scale, tall = h / View.scale;

    /* The ruling coarsens as you pull back, so it never becomes a
     * solid wash of lines at low zoom. */
    let step = 40;
    while (step * View.scale < 18) step *= 4;

    const x0 = Math.floor(View.x / step) * step;
    const y0 = Math.floor(View.y / step) * step;

    gridLayer.replaceChildren();
    for (let x = x0; x < View.x + wide + step; x += step)
        gridLayer.appendChild(el("line",
            { x1: x, y1: y0, x2: x, y2: View.y + tall + step }));
    for (let y = y0; y < View.y + tall + step; y += step)
        gridLayer.appendChild(el("line",
            { x1: x0, y1: y, x2: View.x + wide + step, y2: y }));
}
/* }}} */

/* {{{ draw() */
function draw() {
    const now = performance.now();
    applyView();
    drawGrid();

    /* --- wires, port to port --- */
    wiresLayer.replaceChildren();
    const curves = new Map();
    for (const s of Picture.stations) {
        for (const w of s.wires) {
            const target = Picture.byName.get(w.toName);
            if (!target) continue;
            const a = portAnchor(s, "out", w.fromPort);
            const b = portAnchor(target, "in", w.toPort);
            if (!a || !b) continue;
            const c = curveThrough(a, b);
            const key = `${s.index}.${w.fromPort}>${target.index}.${w.toPort}`;
            curves.set(key, c);

            const since = now - (Picture.wireLast.get(key) || -1e9);
            const warmth = Math.max(0, 1 - since / WIRE_WARM_MS);

            wiresLayer.appendChild(el("path", {
                d: c.d, class: warmth > 0 ? "wire live" : "wire",
                "marker-end": warmth > 0 ? "url(#arrow-live)" : "url(#arrow)",
            }));

            /* The chevrons, marching toward the destination. Their
             * offset is a function of the clock alone, so every active
             * wire flows at the same rate and none of them pretends to
             * be tracking a particular delivery. */
            if (warmth > 0)
                wiresLayer.appendChild(el("path", {
                    d: c.d, class: "flow",
                    "stroke-dasharray": `6 ${CHEVRON_GAP - 6}`,
                    "stroke-dashoffset": -((now / 1000) * CHEVRON_SPEED) % CHEVRON_GAP,
                    opacity: 0.25 + 0.75 * warmth,
                }));
        }
    }

    /* --- stations --- */
    stationsLayer.replaceChildren();
    for (const s of Picture.stations) {
        const at = Picture.placed.get(s.index);
        if (!at) continue;
        const H = boxHeight(s);

        const g = el("g", {
            class: "station" + (at.pinned ? " pinned" : ""),
            transform: `translate(${at.x} ${at.y})`,
            "data-index": s.index,
        });

        const since = now - (Picture.lastRun.get(s.index) || -1e9);
        if (since < GLOW_MS)
            g.appendChild(el("rect", {
                class: "glow", x: -4, y: -4, width: BOX_W + 8, height: H + 8,
                opacity: (1 - since / GLOW_MS) * 0.4,
            }));

        g.appendChild(el("rect", { class: "body", x: 0, y: 0, width: BOX_W, height: H }));
        g.appendChild(el("text", { class: "name", x: 12, y: 20 }, s.name));
        g.appendChild(el("text", { class: "fn", x: 12, y: 34 }, s.box + "()"));
        if (s.door)
            g.appendChild(el("text", { class: "door", x: BOX_W - 12, y: 20,
                                       "text-anchor": "end" },
                             s.door === "in" ? "ENTRANCE" : "RESULT"));

        /* Input ports down the left edge: what kind it is, how many
         * values are waiting in it, and out of what capacity. */
        s.ports.forEach((port, i) => {
            const y = HEAD_H + i * PORT_H;
            const p = el("g", { class: "port " + port.kind +
                                       (port.held > 32 ? " deep" : "") });
            p.appendChild(el("rect", { class: "slot", x: -7, y: y + 3, width: 14, height: 13 }));
            if (port.kind === "ring" && port.held > 0) {
                const cap = Math.max(port.capacity, 10);
                const frac = Math.min(1, port.held / cap);
                p.appendChild(el("rect", {
                    class: "level", x: -6, y: y + 4 + 11 * (1 - frac),
                    width: 12, height: Math.max(2, 11 * frac),
                }));
            }
            p.appendChild(el("text", { x: 14, y: y + 14 }, "in " + i));
            const said = port.kind === "static" ? "constant"
                       : port.kind === "none" ? "no source"
                       : port.held + (port.capacity ? " / " + port.capacity : "");
            p.appendChild(el("text", { class: "count", x: 52, y: y + 14 }, said));
            g.appendChild(p);
        });

        /* Exits down the right edge. */
        for (let j = 0; j < s.outs; j++) {
            const y = HEAD_H + j * PORT_H;
            const p = el("g", { class: "port" });
            p.appendChild(el("rect", { class: "slot", x: BOX_W - 7, y: y + 3,
                                       width: 14, height: 13 }));
            p.appendChild(el("text", { x: BOX_W - 14, y: y + 14,
                                       "text-anchor": "end" }, "out " + j));
            g.appendChild(p);
        }

        stationsLayer.appendChild(g);
    }

    let live = 0;
    for (const when of Picture.wireLast.values())
        if (now - when < WIRE_WARM_MS) live++;

    tallyText.textContent =
        `${Picture.stations.length} stations · ${Picture.tally.ran} runs · ` +
        `${Picture.tally.moved} values moved · ${Picture.tally.due} tasks due · ` +
        `${live} wires carrying`;
}
/* }}} */

/* {{{ dragging */
/*
 * Moving a box changes the picture and nothing else. A box that has
 * been dragged is pinned, and the automatic layout never touches it
 * again — a picture that rearranges itself under a hand is worse than
 * one in the wrong order.
 */
let dragging = null;
let panning = null;

svg.addEventListener("mousedown", (event) => {
    const g = event.target.closest(".station");
    const p = pointAt(event);
    if (g) {
        const index = +g.dataset.index;
        const at = Picture.placed.get(index);
        dragging = { index, dx: p.x - at.x, dy: p.y - at.y };
        g.classList.add("dragging");
    } else {
        /* Anywhere else is the paper, and dragging the paper moves the
         * window over it. */
        panning = { fromX: event.clientX, fromY: event.clientY,
                    atX: View.x, atY: View.y };
        svg.style.cursor = "grabbing";
    }
    event.preventDefault();
});

window.addEventListener("mousemove", (event) => {
    if (dragging) {
        const p = pointAt(event);
        const at = Picture.placed.get(dragging.index);
        at.x = p.x - dragging.dx;
        at.y = p.y - dragging.dy;
        at.pinned = true;
        return;
    }
    if (panning) {
        View.x = panning.atX - (event.clientX - panning.fromX) / View.scale;
        View.y = panning.atY - (event.clientY - panning.fromY) / View.scale;
    }
});

window.addEventListener("mouseup", () => {
    dragging = null;
    panning = null;
    svg.style.cursor = "";
});

/* Zoom about the pointer, so whatever is under it stays under it. */
svg.addEventListener("wheel", (event) => {
    event.preventDefault();
    zoomAbout(View.scale * Math.pow(ZOOM_STEP, -wheelNotches(event)),
              event.clientX, event.clientY);
}, { passive: false });

/* Double-click frames the whole graph again, which is the way back from
 * having zoomed into a corner and lost the rest of it. */
svg.addEventListener("dblclick", (event) => {
    event.preventDefault();
    frameTheGraph();
});

/* The same steps from the keyboard, for anybody without a wheel. */
window.addEventListener("keydown", (event) => {
    if (event.target !== document.body) return;
    if (event.key === "+" || event.key === "=")
        zoomAbout(View.scale * ZOOM_STEP);
    else if (event.key === "-" || event.key === "_")
        zoomAbout(View.scale / ZOOM_STEP);
    else if (event.key === "0")
        zoomAbout(1);
    else if (event.key === "f")
        frameTheGraph();
    else return;
    event.preventDefault();
});

/* And the buttons, which are the only way somebody discovers any of
 * this without being told. */
document.getElementById("zoomin").addEventListener("click",
    () => zoomAbout(View.scale * ZOOM_STEP));
document.getElementById("zoomout").addEventListener("click",
    () => zoomAbout(View.scale / ZOOM_STEP));
document.getElementById("zoomfit").addEventListener("click", () => frameTheGraph());
/* }}} */

/* {{{ saving a layout */
/*
 * A download the browser performs. The viewer writes nothing anywhere:
 * dropping the file beside the map is the person's own act, and picking
 * it up again is a read like any other.
 */
document.getElementById("save").addEventListener("click", () => {
    const out = {};
    for (const s of Picture.stations) {
        const at = Picture.placed.get(s.index);
        if (at) out[s.name] = { x: Math.round(at.x), y: Math.round(at.y) };
    }
    const blob = new Blob([JSON.stringify(out, null, 2)], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = (mapName.textContent || "layout") + ".lay";
    a.click();
    URL.revokeObjectURL(a.href);
});
/* }}} */

/* {{{ receive(e) */
/*
 * What each kind of event does to the picture, and nothing else.
 *
 * The depths are counted here rather than reported: a value arriving at
 * a ring port is one more waiting, and a station running takes one from
 * each of its ring ports. That is the readiness rule the engine itself
 * follows, applied to the same events the engine emitted.
 */
function receive(e) {
    /*
     * Loss here means events overwritten **while this page was already
     * watching**: the view really is behind and really is incomplete.
     * Arriving after a run had started is a different thing entirely
     * and is said elsewhere, quietly.
     */
    if (e.lost) {
        Picture.lostTotal += e.lost;
        lostChip.hidden = false;
        lostChip.textContent =
            `${Picture.lostTotal.toLocaleString()} events lost — this view is behind`;
    }

    const now = performance.now();
    switch (e.kind) {
    case "came up":
        stateChip.className = "state live";
        stateChip.textContent = "running";
        break;

    case "ran": {
        Picture.lastRun.set(e.a, now);
        Picture.tally.ran++;
        const s = stationAt(e.a);
        if (s) for (const p of s.ports)
            if (p.kind === "ring" && p.held > 0) p.held--;
        break;
    }

    case "moved": {
        Picture.tally.moved++;
        Picture.wireLast.set(`${e.a}.${e.b}>${e.c}.${e.d}`, now);
        const to = stationAt(e.c);
        if (to && to.ports[e.d] && to.ports[e.d].kind === "ring")
            to.ports[e.d].held++;
        break;
    }

    case "due":
        Picture.tally.due++;
        break;

    case "grew": {
        const s = stationAt(e.a);
        if (s && s.ports[e.b]) s.ports[e.b].capacity = e.c;
        break;
    }

    case "added": {
        if (!stationAt(e.a)) {
            const s = { index: e.a, name: "#" + e.a, box: "added while running",
                        kind: "p", door: null, outs: 1, wires: [],
                        ports: [] };
            for (let i = 0; i < Math.max(1, e.c); i++)
                s.ports.push({ kind: "ring", held: 0, capacity: 0 });
            Picture.stations.push(s);
            Picture.byName.set(s.name, s);
            layOut();
        }
        break;
    }

    case "wired": {
        const from = stationAt(e.a), to = stationAt(e.c);
        if (from && to) {
            from.outs = Math.max(from.outs, e.b + 1);
            while (to.ports.length <= e.d) to.ports.push({ kind: "none", held: 0, capacity: 0 });
            to.ports[e.d].kind = "ring";
            from.wires.push({ fromPort: e.b, toName: to.name, toPort: e.d });
            placeNear(to.index, [from.index]);
        }
        break;
    }

    case "unwired": {
        const from = stationAt(e.a), to = stationAt(e.c);
        if (from && to)
            from.wires = from.wires.filter(
                (w) => !(w.fromPort === e.b && w.toName === to.name && w.toPort === e.d));
        break;
    }

    case "removed":
        Picture.stations = Picture.stations.filter((s) => s.index !== e.a);
        Picture.placed.delete(e.a);
        break;

    case "finished":
        stateChip.className = "state ended";
        stateChip.textContent = `finished — ${e.a} tasks in all`;
        break;
    }
}
/* }}} */

/* {{{ frameTheGraph() */
function frameTheGraph() {
    if (Picture.stations.length === 0) return;

    let left = Infinity, top = Infinity, right = -Infinity, bottom = -Infinity;
    for (const s of Picture.stations) {
        const p = Picture.placed.get(s.index);
        if (!p) continue;
        left = Math.min(left, p.x);
        top = Math.min(top, p.y);
        right = Math.max(right, p.x + BOX_W);
        bottom = Math.max(bottom, p.y + boxHeight(s));
    }
    if (!isFinite(left)) return;

    const { w, h } = viewport();
    const fits = Math.min(w / (right - left + 2 * MARGIN),
                          h / (bottom - top + 2 * MARGIN));

    /* The entrance if there is one, otherwise the middle of everything. */
    const door = Picture.stations.find((s) => s.door === "in");
    const at = door ? Picture.placed.get(door.index) : null;
    const cx = at ? at.x + BOX_W / 2 : (left + right) / 2;
    const cy = at ? at.y + BOX_MIN / 2 : (top + bottom) / 2;

    /*
     * If the graph fits, frame the whole of it centred on the entrance.
     * If it does not, come in to a readable size and let the rest be
     * found by panning — which is the point of the paper being endless.
     */
    lookAt(cx, cy, Math.min(1.1, Math.max(0.45, fits)));

    /* Centred on the entrance can push everything else off one side, so
     * pull back toward the middle when the whole thing would fit. */
    if (fits >= 0.45 && at) {
        const midX = (left + right) / 2, midY = (top + bottom) / 2;
        View.x += (midX - cx) * 0.6;
        View.y += (midY - cy) * 0.6;
        applyView();
    }
    showZoom();
}
/* }}} */

/* {{{ start() */
async function start() {
    try {
        mapName.textContent = (await (await fetch("/mapname")).text()).trim();
    } catch (ignored) { mapName.textContent = "a program"; }

    const parsed = parseMap(await (await fetch("/map")).text());
    Picture.stations = parsed.stations;
    Picture.byName = parsed.byName;
    layOut();

    /* A layout saved beside the map, if there is one. Read only. */
    try {
        const saved = await fetch("/layout");
        if (saved.ok) {
            const at = await saved.json();
            for (const s of Picture.stations)
                if (at[s.name])
                    Picture.placed.set(s.index,
                        { x: at[s.name].x, y: at[s.name].y, pinned: true });
        }
    } catch (ignored) { /* no layout is the ordinary case */ }

    /*
     * Open looking at the way in. A program is read from its entrance
     * outward, so that is where a first glance belongs — and the scale
     * is chosen to fit the whole graph if it will fit comfortably,
     * because a picture nobody can read is not an improvement on no
     * picture.
     */
    frameTheGraph();
    draw();

    const stream = new EventSource("/events");
    stream.onmessage = (m) => receive(JSON.parse(m.data));
    stream.addEventListener("joined", (m) => {
        const at = JSON.parse(m.data);
        joinedChip.hidden = at.before === 0;
        joinedChip.textContent =
            `joined after ${at.before.toLocaleString()} events`;
    });
    stream.addEventListener("ended", () => {
        if (!stateChip.classList.contains("ended")) {
            stateChip.className = "state ended";
            stateChip.textContent = "the program has ended";
        }
    });
    stream.addEventListener("notrail", () => {
        stateChip.className = "state";
        stateChip.textContent = "no trail to read yet";
    });

    /* Redrawn on a timer rather than per event: a value in flight has
     * to move whether or not anything arrived, and a program moving
     * fast would otherwise spend every frame on layout. */
    setInterval(draw, FRAME_MS);
    window.addEventListener("resize", applyView);
}
/* }}} */

start();
