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
const FLIGHT_MS = 520;       /* how long a value takes to cross a wire */
const FRAME_MS = 33;
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

/* {{{ Picture */
const Picture = {
    stations: [],
    byName: new Map(),
    placed: new Map(),        /* index -> {x, y, pinned} */
    lastRun: new Map(),       /* index -> timestamp */
    flights: [],              /* values crossing a wire right now */
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
const flightLayer = document.getElementById("flights");
const stationsLayer = document.getElementById("stations");
const stateChip = document.getElementById("state");
const lostChip = document.getElementById("lost");
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
    return { d: `M ${a.x} ${a.y} C ${a.x + bend} ${a.y}, ${b.x - bend} ${b.y}, ${b.x} ${b.y}`,
             p0: a, p1: { x: a.x + bend, y: a.y },
             p2: { x: b.x - bend, y: b.y }, p3: b };
}
/* }}} */

/* {{{ alongCurve(c, t) */
/* A point on the cubic, worked out directly rather than by asking the
 * browser to measure the path: a value in flight has to move every
 * frame and a measurement per frame per value is the one thing that
 * would make this page cost something. */
function alongCurve(c, t) {
    const u = 1 - t, a = u * u * u, b = 3 * u * u * t, d = 3 * u * t * t, e = t * t * t;
    return {
        x: a * c.p0.x + b * c.p1.x + d * c.p2.x + e * c.p3.x,
        y: a * c.p0.y + b * c.p1.y + d * c.p2.y + e * c.p3.y,
    };
}
/* }}} */

/* {{{ drawGrid(w, h) */
function drawGrid(w, h) {
    gridLayer.replaceChildren();
    for (let x = 0; x < w; x += 40)
        gridLayer.appendChild(el("line", { x1: x, y1: 0, x2: x, y2: h }));
    for (let y = 0; y < h; y += 40)
        gridLayer.appendChild(el("line", { x1: 0, y1: y, x2: w, y2: y }));
}
/* }}} */

/* {{{ draw() */
function draw() {
    const now = performance.now();

    let widest = 600, tallest = 400;
    for (const s of Picture.stations) {
        const p = Picture.placed.get(s.index);
        if (!p) continue;
        widest = Math.max(widest, p.x + BOX_W + MARGIN);
        tallest = Math.max(tallest, p.y + boxHeight(s) + MARGIN);
    }
    svg.setAttribute("viewBox", `0 0 ${widest} ${tallest}`);
    drawGrid(widest, tallest);

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
            const busy = Picture.flights.some((f) => f.key === key);
            wiresLayer.appendChild(el("path", { d: c.d, class: busy ? "wire busy" : "wire" }));
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

    /* --- values in flight --- */
    flightLayer.replaceChildren();
    Picture.flights = Picture.flights.filter((f) => now - f.t0 < FLIGHT_MS);
    for (const f of Picture.flights) {
        const c = curves.get(f.key);
        if (!c) continue;
        const t = (now - f.t0) / FLIGHT_MS;
        const at = alongCurve(c, t);
        flightLayer.appendChild(el("circle", { class: "value-halo", cx: at.x, cy: at.y, r: 9 }));
        flightLayer.appendChild(el("circle", { class: "value", cx: at.x, cy: at.y, r: 4 }));
    }

    tallyText.textContent =
        `${Picture.stations.length} stations · ${Picture.tally.ran} runs · ` +
        `${Picture.tally.moved} values moved · ${Picture.tally.due} tasks due · ` +
        `${Picture.flights.length} in flight`;
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
        Picture.flights.push({ key: `${e.a}.${e.b}>${e.c}.${e.d}`, t0: now });
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
        stateChip.className = "state";
        stateChip.textContent = "no trail to read yet";
    });

    /* Redrawn on a timer rather than per event: a value in flight has
     * to move whether or not anything arrived, and a program moving
     * fast would otherwise spend every frame on layout. */
    setInterval(draw, FRAME_MS);
}
/* }}} */

start();
