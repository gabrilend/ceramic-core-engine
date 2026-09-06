/*
 * 122-viewer.js — draws the graph, and lights it as it runs.
 *
 * Three jobs, in order. Read the map file and work out the shape. Lay
 * that shape out on a grid as close to square as the count allows. Then
 * listen, and let events change how it looks and nothing else.
 *
 * **Nothing here can reach the program.** The only channel is the event
 * stream, which arrives; there is no request this page makes that
 * changes anything anywhere. Saving a layout is a download the browser
 * performs, not something written on the far end.
 */

/* {{{ parseMap(text) */
/*
 * The map file's grammar, which is small on purpose.
 *
 *   station NAME BOX KIND [entry|result] [@N]
 *     in  P = VALUE | $N | - | [a, b] | xN
 *     out P - TARGET.PORT
 *
 * Only three things matter for a picture: what the stations are called,
 * what they place, and which port feeds which. Everything else is
 * carried through untouched so nothing is silently lost.
 */
function parseMap(text) {
    const stations = [];
    const byName = new Map();
    let current = null;

    for (const raw of text.split("\n")) {
        const line = raw.replace(/#.*$/, "").trimEnd();
        if (!line.trim()) continue;

        const station = line.match(/^station\s+(\S+)\s+(\S+)\s+(\S+)(.*)$/);
        if (station) {
            current = {
                index: stations.length,
                name: station[1],
                box: station[2],
                kind: station[3],
                door: /\bentry\b/.test(station[4]) ? "in"
                    : /\bresult\b/.test(station[4]) ? "out" : null,
                inPorts: 0,
                outWires: [],
            };
            stations.push(current);
            byName.set(current.name, current);
            continue;
        }
        if (!current) continue;

        const out = line.match(/^\s+out\s+(\d+)\s+-\s+(\S+)\.(\d+)/);
        if (out) {
            current.outWires.push({
                fromPort: +out[1], toName: out[2], toPort: +out[3],
            });
            continue;
        }
        const inp = line.match(/^\s+in\s+(\d+)/);
        if (inp) current.inPorts = Math.max(current.inPorts, +inp[1] + 1);
    }

    /* A port nothing mentions still exists if a wire lands on it. */
    for (const s of stations)
        for (const w of s.outWires) {
            const target = byName.get(w.toName);
            if (target) target.inPorts = Math.max(target.inPorts, w.toPort + 1);
        }
    for (const s of stations)
        if (s.inPorts === 0) s.inPorts = 1;

    return { stations, byName };
}
/* }}} */

/* {{{ gridFor(count) */
/*
 * As close to square as the count allows, preferring an extra column to
 * an extra row: a screen is wider than it is tall, so the spare space
 * is horizontal.
 */
function gridFor(count) {
    const cols = Math.ceil(Math.sqrt(count));
    const rows = Math.ceil(count / cols);
    return { cols, rows };
}
/* }}} */

/* {{{ the drawing's constants */
const BOX_W = 150, BOX_H = 58, PAD_X = 70, PAD_Y = 60, MARGIN = 40;
const HEAT_MS = 900;          /* how long a station stays lit after running */
/* }}} */

/* {{{ Picture */
const Picture = {
    stations: [],
    byName: new Map(),
    placed: new Map(),        /* index -> {x, y, pinned} */
    depths: new Map(),        /* "station.port" -> slots */
    lastRun: new Map(),       /* index -> timestamp */
    busyWire: new Map(),      /* "a.b>c.d" -> timestamp */
    lostTotal: 0,
    tally: { ran: 0, moved: 0, due: 0 },
};
/* }}} */

/* {{{ layOut() */
/* Every station that nobody has dragged gets a cell. A dragged station
 * is pinned and is never moved by anything here again. */
function layOut() {
    const { cols } = gridFor(Picture.stations.length);
    let cell = 0;
    for (const s of Picture.stations) {
        const at = Picture.placed.get(s.index);
        if (at && at.pinned) continue;
        const col = cell % cols, row = Math.floor(cell / cols);
        Picture.placed.set(s.index, {
            x: MARGIN + col * (BOX_W + PAD_X),
            y: MARGIN + row * (BOX_H + PAD_Y),
            pinned: false,
        });
        cell++;
    }
}
/* }}} */

/* {{{ placeNear(index, neighbours) */
/*
 * Where a station that appeared mid-run should sit once its wires
 * arrive: roughly equidistant from what it connects to, then nudged
 * until it overlaps nothing.
 *
 * Creating a station and wiring it are separate events, so this cannot
 * run when the box first appears — at that moment it has no wires at
 * all. It is called again as each wire lands, and stops mattering the
 * moment somebody drags the box.
 */
function placeNear(index, neighbours) {
    const at = Picture.placed.get(index);
    if (!at || at.pinned || neighbours.length === 0) return;

    let x = 0, y = 0;
    for (const n of neighbours) {
        const p = Picture.placed.get(n);
        if (!p) return;
        x += p.x; y += p.y;
    }
    x /= neighbours.length;
    y /= neighbours.length;
    y += BOX_H + PAD_Y;            /* below its neighbours, not on top of them */

    for (let tries = 0; tries < 200; tries++) {
        let clash = false;
        for (const [other, p] of Picture.placed) {
            if (other === index) continue;
            if (Math.abs(p.x - x) < BOX_W + 16 && Math.abs(p.y - y) < BOX_H + 16) {
                clash = true; break;
            }
        }
        if (!clash) break;
        x += BOX_W / 2 + 12;
        if (x > MARGIN + 8 * (BOX_W + PAD_X)) { x = MARGIN; y += BOX_H + PAD_Y; }
    }
    at.x = Math.max(MARGIN, x);
    at.y = Math.max(MARGIN, y);
}
/* }}} */

/* {{{ the elements */
const svg = document.getElementById("canvas");
const wiresLayer = document.getElementById("wires");
const stationsLayer = document.getElementById("stations");
const stateChip = document.getElementById("state");
const lostChip = document.getElementById("lost");
const tallyText = document.getElementById("tally");
const mapName = document.getElementById("mapname");
const svgNS = "http://www.w3.org/2000/svg";
function el(name, attrs) {
    const node = document.createElementNS(svgNS, name);
    for (const k in attrs) node.setAttribute(k, attrs[k]);
    return node;
}
/* }}} */

/* {{{ draw() */
function draw() {
    wiresLayer.replaceChildren();
    stationsLayer.replaceChildren();

    let widest = 0, tallest = 0;
    for (const p of Picture.placed.values()) {
        widest = Math.max(widest, p.x + BOX_W + MARGIN);
        tallest = Math.max(tallest, p.y + BOX_H + MARGIN);
    }
    svg.setAttribute("viewBox", `0 0 ${Math.max(widest, 600)} ${Math.max(tallest, 400)}`);

    const now = performance.now();

    for (const s of Picture.stations) {
        const from = Picture.placed.get(s.index);
        if (!from) continue;
        for (const w of s.outWires) {
            const target = Picture.byName.get(w.toName);
            if (!target) continue;
            const to = Picture.placed.get(target.index);
            if (!to) continue;

            const x1 = from.x + BOX_W, y1 = from.y + BOX_H / 2;
            const x2 = to.x, y2 = to.y + BOX_H / 2;
            const bend = Math.max(30, Math.abs(x2 - x1) / 2);
            const key = `${s.index}.${w.fromPort}>${target.index}.${w.toPort}`;
            const hot = now - (Picture.busyWire.get(key) || -1e9) < HEAT_MS;
            wiresLayer.appendChild(el("path", {
                d: `M ${x1} ${y1} C ${x1 + bend} ${y1}, ${x2 - bend} ${y2}, ${x2} ${y2}`,
                class: hot ? "wire busy" : "wire",
            }));
        }
    }

    for (const s of Picture.stations) {
        const at = Picture.placed.get(s.index);
        if (!at) continue;

        const g = el("g", {
            class: "station" + (at.pinned ? " pinned" : ""),
            transform: `translate(${at.x} ${at.y})`,
            "data-index": s.index,
        });

        /* Heat: full at the instant it ran, gone by HEAT_MS. */
        const since = now - (Picture.lastRun.get(s.index) || -1e9);
        if (since < HEAT_MS) {
            g.appendChild(el("rect", {
                class: "heat", x: 0, y: 0, width: BOX_W, height: BOX_H,
                opacity: (1 - since / HEAT_MS) * 0.35,
            }));
        }
        g.appendChild(el("rect", { x: 0, y: 0, width: BOX_W, height: BOX_H }));

        const label = el("text", { x: 10, y: 22 });
        label.textContent = s.name + (s.door === "in" ? "  ⇥"
                                     : s.door === "out" ? "  ⇤" : "");
        g.appendChild(label);

        const box = el("text", { x: 10, y: 38, class: "box" });
        box.textContent = s.box + (s.kind !== "p" ? "  [" + s.kind + "]" : "");
        g.appendChild(box);

        /* One pip per input port, filled by how deep its backlog is. */
        const pipW = 14, pipH = 7;
        for (let p = 0; p < s.inPorts; p++) {
            const px = 10 + p * (pipW + 4), py = BOX_H - 13;
            const deep = Picture.depths.get(`${s.index}.${p}`) || 0;
            const pip = el("g", { class: deep > 64 ? "port deep" : "port" });
            pip.appendChild(el("rect", { x: px, y: py, width: pipW, height: pipH, rx: 2 }));
            if (deep > 0) {
                const frac = Math.min(1, Math.log2(deep + 1) / 10);
                pip.appendChild(el("rect", {
                    class: "fill", x: px, y: py,
                    width: Math.max(2, pipW * frac), height: pipH, rx: 2,
                }));
            }
            g.appendChild(pip);
        }

        stationsLayer.appendChild(g);
    }

    tallyText.textContent =
        `${Picture.stations.length} stations · ${Picture.tally.ran} runs · ` +
        `${Picture.tally.moved} values moved · ${Picture.tally.due} tasks due`;
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
function pointAt(event) {
    const box = svg.getBoundingClientRect();
    const view = svg.viewBox.baseVal;
    return {
        x: (event.clientX - box.left) / box.width * view.width,
        y: (event.clientY - box.top) / box.height * view.height,
    };
}
svg.addEventListener("mousedown", (event) => {
    const g = event.target.closest(".station");
    if (!g) return;
    const index = +g.dataset.index;
    const at = Picture.placed.get(index);
    const p = pointAt(event);
    dragging = { index, dx: p.x - at.x, dy: p.y - at.y };
    g.classList.add("dragging");
    event.preventDefault();
});
window.addEventListener("mousemove", (event) => {
    if (!dragging) return;
    const p = pointAt(event);
    const at = Picture.placed.get(dragging.index);
    at.x = p.x - dragging.dx;
    at.y = p.y - dragging.dy;
    at.pinned = true;
    draw();
});
window.addEventListener("mouseup", () => { dragging = null; });
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

/* {{{ receive(event) */
/* What each kind of event does to the picture, and nothing else. */
function receive(e) {
    if (e.lost) {
        Picture.lostTotal += e.lost;
        lostChip.hidden = false;
        lostChip.textContent = `${Picture.lostTotal} events lost — this view is behind`;
    }

    const now = performance.now();
    switch (e.kind) {
    case "came up":
        stateChip.className = "state live";
        stateChip.textContent = "running";
        break;
    case "ran":
        Picture.lastRun.set(e.a, now);
        Picture.tally.ran++;
        break;
    case "moved":
        Picture.busyWire.set(`${e.a}.${e.b}>${e.c}.${e.d}`, now);
        Picture.tally.moved++;
        break;
    case "due":
        Picture.tally.due++;
        break;
    case "grew":
        Picture.depths.set(`${e.a}.${e.b}`, e.c);
        break;
    case "added": {
        /* A station nobody has seen before. It has no wires yet, so it
         * simply goes on the end of the grid; when its wires arrive it
         * moves once, unless somebody has dragged it by then. */
        if (!Picture.byName.has("#" + e.a)) {
            const s = { index: e.a, name: "#" + e.a, box: "(added while running)",
                        kind: "p", door: null, inPorts: Math.max(1, e.c),
                        outWires: [] };
            Picture.stations.push(s);
            Picture.byName.set(s.name, s);
            layOut();
        }
        break;
    }
    case "wired": {
        const from = Picture.stations.find((s) => s.index === e.a);
        const to = Picture.stations.find((s) => s.index === e.c);
        if (from && to) {
            from.outWires.push({ fromPort: e.b, toName: to.name, toPort: e.d });
            to.inPorts = Math.max(to.inPorts, e.d + 1);
            placeNear(to.index, [from.index]);
            placeNear(from.index, [to.index]);
        }
        break;
    }
    case "unwired": {
        const from = Picture.stations.find((s) => s.index === e.a);
        const to = Picture.stations.find((s) => s.index === e.c);
        if (from && to)
            from.outWires = from.outWires.filter(
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

/* {{{ start() */
async function start() {
    try {
        mapName.textContent = await (await fetch("/mapname")).text();
    } catch (ignored) { mapName.textContent = "a program"; }

    const text = await (await fetch("/map")).text();
    const parsed = parseMap(text);
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

    draw();

    const stream = new EventSource("/events");
    stream.onmessage = (m) => receive(JSON.parse(m.data));
    stream.addEventListener("ended", () => {
        if (!stateChip.classList.contains("ended")) {
            stateChip.className = "state ended";
            stateChip.textContent = "the program has ended";
        }
    });
    stream.addEventListener("notrail", () => {
        stateChip.className = "state waiting";
        stateChip.textContent = "no trail to read yet";
    });

    /* Redrawn on a timer rather than per event: a program moving fast
     * would otherwise spend the whole frame budget on layout, and the
     * heat needs to fade on its own anyway. */
    setInterval(draw, 90);
}
/* }}} */

start();
