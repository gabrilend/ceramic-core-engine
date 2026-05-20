/* SoraMech interactive demos.
   Drop a <div data-demo="<name>"></div> into a page; this script
   initializes it on DOMContentLoaded. Each demo is self-contained,
   uses inline SVG where it makes sense, and shares the design tokens
   from style.css via CSS variables. */

(function () {

  const NS = "http://www.w3.org/2000/svg";
  const svg = (tag, attrs = {}, children = []) => {
    const el = document.createElementNS(NS, tag);
    for (const [k, v] of Object.entries(attrs)) el.setAttribute(k, v);
    (Array.isArray(children) ? children : [children]).forEach(c => {
      if (c == null) return;
      el.appendChild(typeof c === "string" ? document.createTextNode(c) : c);
    });
    return el;
  };
  const el = (tag, attrs = {}, children = []) => {
    const e = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs)) {
      if (k === "class") e.className = v;
      else if (k === "html") e.innerHTML = v;
      else if (k.startsWith("on") && typeof v === "function") e.addEventListener(k.slice(2), v);
      else e.setAttribute(k, v);
    }
    (Array.isArray(children) ? children : [children]).forEach(c => {
      if (c == null) return;
      e.appendChild(typeof c === "string" ? document.createTextNode(c) : c);
    });
    return e;
  };
  const frame = (label, hint, body, foot) => {
    const wrap = el("div", { class: "demo" });
    const head = el("div", { class: "demo-head" }, [
      el("span", { class: "label" }, label),
      hint ? el("span", { class: "hint" }, hint) : null,
    ]);
    const b = el("div", { class: "demo-body" }, body);
    const f = foot ? el("div", { class: "demo-foot" }, foot) : null;
    wrap.append(head, b);
    if (f) wrap.appendChild(f);
    return wrap;
  };

  const demos = {};

  /* ─── three-program architecture diagram ─────────────────── */
  demos.threeProgram = (host) => {
    const W = 720, H = 360;
    const root = svg("svg", { viewBox: `0 0 ${W} ${H}` });

    // Lamp glow
    root.appendChild(svg("defs", {}, [
      (() => {
        const r = svg("radialGradient", { id: "tp-glow", cx: "20%", cy: "0%", r: "70%" });
        r.appendChild(svg("stop", { offset: "0%", "stop-color": "var(--amber)", "stop-opacity": "0.12" }));
        r.appendChild(svg("stop", { offset: "100%", "stop-color": "var(--amber)", "stop-opacity": "0" }));
        return r;
      })()
    ]));
    root.appendChild(svg("rect", { x: 0, y: 0, width: W, height: H, fill: "url(#tp-glow)" }));

    // Nodes
    const nodes = [
      { id: "browser",  x: 100, y: 60,  w: 180, h: 70, title: "Browser",            sub: "assets/index.html", role: "Editor — canvas, inspector, file browser. Talks HTTP to the server." },
      { id: "server",   x: 100, y: 200, w: 180, h: 70, title: "src/006-server-main.lua", sub: "HTTP file CRUD",     role: "Thin file proxy. No logic beyond path validation. No runner knowledge." },
      { id: "map",      x: 340, y: 130, w: 200, h: 110, title: "[ map directory ]",  sub: "maps/<name>/",       role: "boxes/  data/  src/  compiled/  tmp/.  The only shared contract." },
      { id: "runner",   x: 590, y: 60,  w: 130, h: 100, title: "Phase 2",            sub: "007-runner-main.lua", role: "Synchronous Lua interpreter. Drivers via shell-out." },
      { id: "pool",     x: 590, y: 220, w: 130, h: 100, title: "Phase 3",            sub: "soramech-pool (C)",   role: "C thread pool. Specs via dlopen. Per-input-port slot store." },
    ];

    const nodeEls = {};

    nodes.forEach(n => {
      const g = svg("g", { class: "tp-node", "data-id": n.id, style: "cursor:pointer" });
      const r = svg("rect", {
        x: n.x, y: n.y, width: n.w, height: n.h,
        rx: 2, ry: 2,
        fill: "var(--ink-card)",
        stroke: "var(--ink-rim)",
        "stroke-width": 1
      });
      const t = svg("text", { x: n.x + 14, y: n.y + 26, class: "label-serif", "font-size": 15, fill: "var(--cream)" }, n.title);
      const s = svg("text", { x: n.x + 14, y: n.y + 44, class: "label-mono" }, n.sub);
      g.append(r, t, s);
      root.appendChild(g);
      nodeEls[n.id] = { g, r, n };
    });

    // Wires
    const wires = [
      { from: "browser", to: "server",  curve: "down",   label: "HTTP" },
      { from: "server",  to: "map",     curve: "right",  label: "reads / writes" },
      { from: "map",     to: "runner",  curve: "right",  label: "map dir" },
      { from: "map",     to: "pool",    curve: "right",  label: "map dir" },
      { from: "pool",    to: "map",     curve: "left",   label: "tmp/last-run.jsonl", dashed: true },
    ];

    const wireGroup = svg("g", {});
    root.appendChild(wireGroup);

    const wirePath = (a, b, kind) => {
      const sx = a.x + a.w, sy = a.y + a.h / 2;
      const tx = b.x,        ty = b.y + b.h / 2;
      // simple S curve
      const dx = (tx - sx) / 2;
      return `M ${sx} ${sy} C ${sx + dx} ${sy}, ${tx - dx} ${ty}, ${tx} ${ty}`;
    };

    wires.forEach(w => {
      const a = nodeEls[w.from].n, b = nodeEls[w.to].n;
      let d;
      if (w.from === "browser" && w.to === "server") {
        d = `M ${a.x + a.w / 2} ${a.y + a.h} L ${b.x + b.w / 2} ${b.y}`;
      } else if (w.from === "pool" && w.to === "map") {
        const sx = b.x + b.w, sy = a.y + a.h / 2;
        const tx = a.x,        ty = sy;
        d = `M ${a.x} ${a.y + a.h / 2} C ${a.x - 40} ${a.y + a.h / 2}, ${b.x + b.w + 60} ${b.y + b.h + 30}, ${b.x + b.w / 2 + 20} ${b.y + b.h}`;
      } else {
        d = wirePath(a, b);
      }
      const p = svg("path", {
        d,
        stroke: w.dashed ? "var(--copper)" : "var(--amber)",
        "stroke-width": 1.4,
        "stroke-dasharray": w.dashed ? "4 3" : "none",
        fill: "none",
        opacity: 0.55,
        "data-wire": `${w.from}-${w.to}`
      });
      wireGroup.appendChild(p);
      if (w.label) {
        const m = p.getPointAtLength ? null : null; // skip — set after mount
      }
    });

    // Side panel detail
    const detail = el("div", { class: "tp-detail" }, [
      el("div", { class: "kvline" }, [
        el("span", { class: "k" }, "selected"), el("span", { class: "v amber" }, "hover a node ↗")
      ]),
      el("p", { class: "tiny dim", style: "margin:0.6rem 0 0; min-height: 4.5em; line-height:1.5" }, "Three independent programs share the map directory. None knows about the others at runtime — the directory format is the only shared contract."),
    ]);

    Object.values(nodeEls).forEach(({ g, r, n }) => {
      g.addEventListener("mouseenter", () => {
        r.setAttribute("stroke", "var(--amber)");
        r.setAttribute("fill", "var(--ink-paper)");
        wireGroup.querySelectorAll("path").forEach(p => {
          const wid = p.getAttribute("data-wire");
          if (wid && (wid.startsWith(n.id + "-") || wid.endsWith("-" + n.id))) {
            p.setAttribute("opacity", "1");
            p.setAttribute("stroke-width", "2.2");
          } else {
            p.setAttribute("opacity", "0.18");
          }
        });
        detail.querySelector(".kvline .v").textContent = n.title;
        detail.querySelector("p").textContent = n.role;
      });
      g.addEventListener("mouseleave", () => {
        r.setAttribute("stroke", "var(--ink-rim)");
        r.setAttribute("fill", "var(--ink-card)");
        wireGroup.querySelectorAll("path").forEach(p => {
          p.setAttribute("opacity", "0.55");
          p.setAttribute("stroke-width", "1.4");
        });
      });
    });

    host.appendChild(frame(
      "FIG · three-program architecture",
      "hover the parts ↘",
      [
        el("div", { style: "display:grid;grid-template-columns:1fr 220px;gap:1.3rem" }, [
          el("div", {}, root),
          detail,
        ])
      ],
      el("span", {}, [
        document.createTextNode("Server never knows about runner. Runner never knows about server. "),
        el("span", { class: "amber" }, "Phase 2"),
        document.createTextNode(" retires when "),
        el("span", { class: "amber" }, "Phase 3"),
        document.createTextNode(" lands; box JSON unchanged."),
      ])
    ));
  };

  /* ─── routing kinds explorer ─────────────────────────────── */
  demos.routingExplorer = (host) => {
    const state = {
      kind: "plain",
      counter: 0,
      lastValue: 4,
      lastBranch: -1,
      log: [],
    };

    const kinds = [
      { id: "plain",       label: "plain",       n: 1 },
      { id: "comparator",  label: "comparator",  n: 3, comparand: 4 },
      { id: "iterator",    label: "iterator",    n: 4 },
      { id: "randomizer",  label: "randomizer",  n: 4 },
      { id: "weighted",    label: "weighted",    n: 3, weights: [0.5, 0.3, 0.2] },
      { id: "distributor", label: "distributor", n: 3, fills: [2, 0, 1] },
    ];

    const branchNames = (k) => {
      if (k.id === "plain") return ["out"];
      if (k.id === "comparator") return ["lt", "eq", "gt"];
      return Array.from({ length: k.n }, (_, i) => `out_${i}`);
    };

    const seg = el("div", { class: "seg" }, kinds.map(k => {
      const b = el("button", { type: "button" }, k.label);
      if (k.id === state.kind) b.classList.add("active");
      b.addEventListener("click", () => {
        state.kind = k.id;
        state.counter = 0;
        state.log = [];
        seg.querySelectorAll("button").forEach(x => x.classList.remove("active"));
        b.classList.add("active");
        redraw();
      });
      return b;
    }));

    const stage = el("div", {});
    const sidebar = el("div", {});

    const fire = () => {
      const k = kinds.find(x => x.id === state.kind);
      const branches = branchNames(k);
      let idx;
      if (k.id === "plain") {
        idx = "*"; // all
      } else if (k.id === "comparator") {
        const v = state.lastValue;
        idx = v < k.comparand ? 0 : (v === k.comparand ? 1 : 2);
      } else if (k.id === "iterator") {
        idx = state.counter % k.n;
        state.counter++;
      } else if (k.id === "randomizer") {
        const c = state.counter++;
        // small mixing hash
        let h = (c ^ 0x9e3779b9) >>> 0;
        h = ((h ^ (h >>> 16)) * 0x85ebca6b) >>> 0;
        idx = h % k.n;
      } else if (k.id === "weighted") {
        const c = state.counter++;
        const PR = 1000;
        const r = c % PR;
        let acc = 0;
        idx = 0;
        for (let i = 0; i < k.n; i++) {
          acc += k.weights[i] * PR;
          if (r < acc) { idx = i; break; }
        }
      } else if (k.id === "distributor") {
        let min = Infinity;
        idx = 0;
        for (let i = 0; i < k.n; i++) if (k.fills[i] < min) { min = k.fills[i]; idx = i; }
        k.fills[idx]++;
      }
      state.lastBranch = idx;
      state.log.unshift({ step: state.log.length + 1, k: k.id, idx, v: state.lastValue });
      if (state.log.length > 5) state.log.pop();
      redraw();
    };

    const redraw = () => {
      const k = kinds.find(x => x.id === state.kind);
      const branches = branchNames(k);

      // Stage SVG
      const W = 460, H = 230;
      const root = svg("svg", { viewBox: `0 0 ${W} ${H}` });

      // Producer box
      const px = 30, py = H/2 - 30, pw = 110, ph = 60;
      const pg = svg("g", {});
      pg.appendChild(svg("rect", { x: px, y: py, width: pw, height: ph, fill: "var(--ink-card)", stroke: "var(--amber)", "stroke-width": 1.4, rx: 2 }));
      pg.appendChild(svg("text", { x: px + 12, y: py + 24, class: "label-serif", "font-size": 13, fill: "var(--cream)" }, "score()"));
      pg.appendChild(svg("text", { x: px + 12, y: py + 42, class: "label-mono" }, `routing: ${k.id}`));
      root.appendChild(pg);

      // Output dot(s)
      const outX = px + pw;
      const branchY = (i) => 40 + (i + 0.5) * (H - 80) / branches.length;

      branches.forEach((bn, i) => {
        const cy = branchY(i);
        const cx = outX + 8;
        const isFired = state.lastBranch === i || (state.lastBranch === "*" && k.id === "plain");
        const colour = isFired ? "var(--amber)" : "var(--cream-faint)";

        // wire
        root.appendChild(svg("path", {
          d: `M ${outX} ${py + ph/2} C ${outX + 40} ${py + ph/2}, ${cx + 50} ${cy}, ${cx + 120} ${cy}`,
          stroke: colour,
          "stroke-width": isFired ? 2.2 : 1,
          fill: "none",
          opacity: isFired ? 0.95 : 0.4
        }));

        // branch port
        root.appendChild(svg("circle", { cx: cx, cy: py + ph/2, r: 4, fill: colour }));

        // consumer box
        const conX = cx + 120, conY = cy - 16;
        root.appendChild(svg("rect", { x: conX, y: conY, width: 96, height: 32, fill: "var(--ink-card)", stroke: isFired ? "var(--amber)" : "var(--ink-rim)", rx: 2 }));
        root.appendChild(svg("text", { x: conX + 10, y: conY + 20, class: "label-mono", fill: isFired ? "var(--amber)" : "var(--cream-faint)" }, `→ ${bn}`));

        // animated pulse along wire when fired
        if (isFired) {
          const pulse = svg("circle", { r: 3.5, fill: "var(--gold)" });
          pulse.appendChild(svg("animateMotion", {
            dur: "0.6s",
            repeatCount: "1",
            path: `M ${outX} ${py + ph/2} C ${outX + 40} ${py + ph/2}, ${cx + 50} ${cy}, ${cx + 120} ${cy}`
          }));
          root.appendChild(pulse);
        }
      });

      stage.replaceChildren(root);

      // Sidebar
      const lines = [
        el("div", { class: "kvline" }, [el("span", { class: "k" }, "kind"),     el("span", { class: "v amber" }, k.id)]),
        el("div", { class: "kvline" }, [el("span", { class: "k" }, "branches"), el("span", { class: "v" }, branches.join(" / "))]),
      ];
      if (k.id === "comparator") {
        lines.push(el("div", { class: "kvline" }, [el("span", { class: "k" }, "comparand"), el("span", { class: "v" }, String(k.comparand))]));
        lines.push(el("div", { class: "kvline" }, [el("span", { class: "k" }, "f(x)"), el("span", { class: "v copper" }, String(state.lastValue))]));
      }
      if (k.id === "iterator" || k.id === "randomizer" || k.id === "weighted") {
        lines.push(el("div", { class: "kvline" }, [el("span", { class: "k" }, "counter"), el("span", { class: "v amber" }, String(state.counter))]));
      }
      if (k.id === "weighted") {
        k.weights.forEach((w, i) => lines.push(
          el("div", { class: "kvline" }, [el("span", { class: "k" }, `out_${i}`), el("span", { class: "v" }, `${(w*100)|0}%`)])
        ));
      }
      if (k.id === "distributor") {
        k.fills.forEach((v, i) => lines.push(
          el("div", { class: "kvline" }, [el("span", { class: "k" }, `out_${i} fill`), el("span", { class: "v copper" }, String(v))])
        ));
      }
      sidebar.replaceChildren(...lines);
    };

    // Comparator input
    const cmpInput = el("div", { style: "display:flex; gap:0.8rem; align-items:center; margin-top:0.7rem;" }, [
      el("span", { class: "mono dim tiny" }, "f(x) returns:"),
      (() => {
        const inp = el("input", { type: "number", value: "4", style: "width:64px; background:var(--ink-card); color:var(--cream); border:1px solid var(--ink-rim); padding:0.3rem 0.5rem; font-family:var(--font-mono); font-size:0.85rem;" });
        inp.addEventListener("input", () => { state.lastValue = Number(inp.value) || 0; redraw(); });
        return inp;
      })(),
    ]);

    const controls = el("div", { class: "row", style: "margin-top:0.6rem" }, [
      el("button", { class: "btn solid", onclick: fire }, "↺ spawn task"),
      el("button", { class: "btn ghost", onclick: () => { state.counter = 0; state.log = []; redraw(); } }, "reset"),
    ]);

    const body = el("div", { style: "display:grid; grid-template-columns: 1fr 200px; gap:1.4rem" }, [
      el("div", {}, [
        el("p", { class: "tiny dim", style: "margin-bottom:0.8rem" }, "Pick a routing kind, then fire the box. The same function runs every time — only the routing decision changes."),
        seg,
        stage,
        cmpInput,
        controls,
      ]),
      el("div", {}, [
        el("h4", {}, "live state"),
        sidebar,
      ])
    ]);

    host.appendChild(frame(
      "WORKBENCH · routing kinds",
      "fire the box ↘",
      [body],
      "Issue 233 unified the schema. Every call box carries routing.kind explicitly."
    ));

    redraw();
  };

  /* ─── weighted slider ────────────────────────────────────── */
  demos.weighted = (host) => {
    let weights = [0.55, 0.25, 0.20]; // initial
    const N = 3;
    const COLOURS = ["var(--amber)", "var(--copper)", "var(--gold)"];

    const draw = () => {
      const total = weights.reduce((a, b) => a + b, 0);
      const norm = weights.map(w => w / total);
      bar.replaceChildren();
      let acc = 0;
      norm.forEach((w, i) => {
        const seg = el("div", {
          style: `width:${w*100}%; background:${COLOURS[i]}; height:100%; display:flex; align-items:center; justify-content:center; color:var(--ink-deep); font-family:var(--font-mono); font-size:0.74rem; font-weight:600; transition:width 200ms;`
        }, w > 0.06 ? `out_${i}` : "");
        bar.appendChild(seg);
        acc += w;
      });

      // breakdown
      rows.replaceChildren();
      norm.forEach((w, i) => {
        const row = el("div", { style: "display:grid; grid-template-columns: 70px 1fr 60px; gap:0.6rem; align-items:center; margin: 0.4rem 0;" });
        row.append(
          el("span", { class: "mono tiny", style: `color:${COLOURS[i]}` }, `out_${i}`),
          (() => {
            const r = el("input", { type: "range", min: "0", max: "100", value: String((w*100)|0) });
            r.addEventListener("input", () => {
              const v = Number(r.value);
              const others = weights.reduce((a, b, j) => a + (j === i ? 0 : b), 0);
              if (others > 0) {
                const remain = 1 - v/100;
                weights = weights.map((w0, j) => j === i ? v/100 : (w0 / others) * remain);
              } else {
                weights[i] = v/100;
              }
              draw();
            });
            return r;
          })(),
          el("span", { class: "mono tiny amber", style: "text-align:right" }, `${(w*100).toFixed(1)}%`)
        );
        rows.appendChild(row);
      });

      // simulate 1000 invocations, count the histogram
      const counts = new Array(N).fill(0);
      const PR = 1000;
      for (let c = 0; c < 1000; c++) {
        const r = c % PR;
        let acc2 = 0, idx = 0;
        for (let i = 0; i < N; i++) {
          acc2 += norm[i] * PR;
          if (r < acc2) { idx = i; break; }
        }
        counts[idx]++;
      }
      hist.replaceChildren();
      counts.forEach((c, i) => {
        hist.appendChild(el("div", { class: "kvline" }, [
          el("span", { class: "k" }, `out_${i} (1000 runs)`),
          el("span", { class: "v amber" }, `${c} hits`)
        ]));
      });
    };

    const bar = el("div", { style: "display:flex; width:100%; height:34px; border:1px solid var(--ink-rim); border-radius:2px; overflow:hidden;" });
    const rows = el("div", { style: "margin-top:1rem;" });
    const hist = el("div", { style: "margin-top:1rem; padding-top:0.8rem; border-top:1px dashed var(--ink-rule);" });

    host.appendChild(frame(
      "INSPECTOR · weighted routing",
      "drag the knobs ↘",
      [
        el("p", { class: "tiny dim" }, "Each output port takes a fraction of invocations. The dispatch layer reads a counter slot, scales it to PRECISION (1000), and looks up the band. The histogram below is a deterministic simulation of 1000 runs."),
        bar,
        rows,
        hist,
      ]
    ));

    draw();
  };

  /* ─── iterator counter animation ─────────────────────────── */
  demos.iterator = (host) => {
    const N = 4;
    let counter = 0;
    let history = [];
    let auto = false;
    let autoT = null;

    const renderRing = () => {
      const W = 360, H = 220;
      const root = svg("svg", { viewBox: `0 0 ${W} ${H}` });
      const cx = W/2, cy = H/2, R = 70;

      // center counter
      root.appendChild(svg("circle", { cx, cy, r: 36, fill: "var(--ink-card)", stroke: "var(--amber)", "stroke-width": 1.5 }));
      root.appendChild(svg("text", { x: cx, y: cy - 4, "text-anchor": "middle", class: "label-mono", fill: "var(--amber)", "font-size": 9 }, "COUNTER"));
      root.appendChild(svg("text", { x: cx, y: cy + 16, "text-anchor": "middle", "font-family": "var(--font-display)", "font-size": 22, fill: "var(--cream)" }, String(counter)));

      // branches
      for (let i = 0; i < N; i++) {
        const angle = (-Math.PI/2) + (i * 2 * Math.PI / N);
        const bx = cx + Math.cos(angle) * R;
        const by = cy + Math.sin(angle) * R;
        const fired = (counter - 1) % N === i && counter > 0;

        root.appendChild(svg("line", {
          x1: cx + Math.cos(angle) * 36,
          y1: cy + Math.sin(angle) * 36,
          x2: bx, y2: by,
          stroke: fired ? "var(--amber)" : "var(--cream-faint)",
          "stroke-width": fired ? 2 : 1,
          opacity: fired ? 1 : 0.4
        }));
        root.appendChild(svg("circle", {
          cx: bx, cy: by, r: 14,
          fill: fired ? "var(--amber)" : "var(--ink-card)",
          stroke: fired ? "var(--amber)" : "var(--ink-rim)"
        }));
        root.appendChild(svg("text", {
          x: bx, y: by + 4, "text-anchor": "middle",
          "font-family": "var(--font-mono)",
          "font-size": 10,
          fill: fired ? "var(--ink-deep)" : "var(--cream-faint)"
        }, `${i}`));
      }
      return root;
    };

    const ring = el("div", {});
    const log = el("div", { style: "font-family:var(--font-mono); font-size:0.78rem; max-height:200px; overflow-y:auto" });

    const renderLog = () => {
      log.replaceChildren();
      history.slice(0, 8).forEach((h, i) => {
        log.appendChild(el("div", { class: "kvline" }, [
          el("span", { class: "k" }, `task #${h.t}`),
          el("span", { class: "v" }, [
            el("span", { class: "dim" }, "counter "),
            el("span", { class: "amber" }, String(h.c)),
            el("span", { class: "dim" }, " → "),
            el("span", { class: "copper" }, `out_${h.b}`),
          ])
        ]));
      });
    };

    const draw = () => { ring.replaceChildren(renderRing()); renderLog(); };

    const spawn = () => {
      const branch = counter % N;
      counter++;
      history.unshift({ t: history.length + 1, c: counter - 1, b: branch });
      if (history.length > 32) history.pop();
      draw();
    };

    const reset = () => { counter = 0; history = []; if (autoT) clearInterval(autoT); auto = false; toggle.textContent = "↻ run"; draw(); };

    const toggle = el("button", { class: "btn", onclick: () => {
      auto = !auto;
      if (auto) {
        toggle.textContent = "■ stop";
        autoT = setInterval(() => spawn(), 600);
      } else {
        toggle.textContent = "↻ run";
        if (autoT) clearInterval(autoT);
      }
    }}, "↻ run");

    host.appendChild(frame(
      "ANIMATION · iterator counter",
      "watch the index walk ↘",
      [
        el("div", { style: "display:grid; grid-template-columns: 1fr 1fr; gap:1.5rem" }, [
          el("div", {}, [
            ring,
            el("div", { class: "row", style: "justify-content:center; margin-top:0.8rem" }, [
              el("button", { class: "btn solid", onclick: spawn }, "▸ spawn task"),
              toggle,
              el("button", { class: "btn ghost", onclick: reset }, "reset"),
            ]),
          ]),
          el("div", {}, [
            el("h4", {}, "spawn log"),
            log,
          ]),
        ])
      ],
      "Counter lives in an atomic-counter slot. Concurrent tasks each get a distinct value — iterators parallelize across workers without coordination."
    ));

    draw();
  };

  /* ─── slot store push/pop ────────────────────────────────── */
  demos.slotStore = (host) => {
    const state = { mode: "pop", cells: [], head: 0, tail: 0, n: 5, pushSeq: 1 };

    const draw = () => {
      const W = 460, H = 130;
      const root = svg("svg", { viewBox: `0 0 ${W} ${H}` });
      const cellW = 70, cellH = 60;
      const margin = 12;
      const y = H/2 - cellH/2;

      // header label
      root.appendChild(svg("text", { x: margin, y: 18, class: "label-mono", fill: "var(--amber)" }, `mode = ${state.mode}    n_cells = ${state.n}    head=${state.head}    tail=${state.tail}`));

      for (let i = 0; i < state.n; i++) {
        const x = margin + i * (cellW + 6);
        const filled = state.cells[i];
        const isHead = i === state.head % state.n;
        root.appendChild(svg("rect", {
          x, y, width: cellW, height: cellH,
          fill: filled ? "var(--ink-card)" : "var(--ink-paper)",
          stroke: isHead ? "var(--amber)" : "var(--ink-rim)",
          "stroke-width": isHead ? 2 : 1,
          rx: 2
        }));
        if (filled) {
          root.appendChild(svg("text", { x: x + cellW/2, y: y + 28, "text-anchor": "middle", "font-family": "var(--font-display)", "font-size": 18, fill: "var(--cream)" }, `"${filled.value}"`));
          root.appendChild(svg("text", { x: x + cellW/2, y: y + 46, "text-anchor": "middle", class: "label-mono", fill: "var(--cream-faint)" }, `tag ${filled.tag}`));
        }
        if (isHead) {
          root.appendChild(svg("text", { x: x + cellW/2, y: y - 6, "text-anchor": "middle", class: "label-mono", fill: "var(--amber)" }, "HEAD"));
        }
      }
      stage.replaceChildren(root);
    };

    const stage = el("div", {});

    const push = (value) => {
      // produce a value into next tail slot
      const slot = state.tail % state.n;
      if (state.cells[slot] && state.mode === "pop") {
        // overrun? show a small warning
        warn.textContent = `! slot ${slot} occupied — pop first`;
        return;
      }
      state.cells[slot] = { value, tag: state.pushSeq++ };
      if (state.mode === "pop") state.tail++;
      warn.textContent = "";
      draw();
    };

    const pop = () => {
      if (state.mode === "peek") {
        // peek doesn't drain
        const slot = state.head % state.n;
        const cell = state.cells[slot];
        if (cell) flash(`peek → "${cell.value}"`);
        else warn.textContent = "! slot empty";
        return;
      }
      const slot = state.head % state.n;
      const cell = state.cells[slot];
      if (!cell) { warn.textContent = "! empty"; return; }
      flash(`pop → "${cell.value}"`);
      state.cells[slot] = null;
      state.head++;
      warn.textContent = "";
      draw();
    };

    const flash = (msg) => { last.textContent = msg; last.style.color = "var(--amber)"; };

    const reset = () => {
      state.cells = []; state.head = 0; state.tail = 0; state.pushSeq = 1; warn.textContent = ""; last.textContent = "—";
      draw();
    };

    const warn = el("span", { class: "mono tiny", style: "color: var(--rust); margin-left:0.6rem" });
    const last = el("span", { class: "mono tiny amber" }, "—");

    const modeSeg = el("div", { class: "seg" }, [
      (() => { const b = el("button", { type: "button" }, "peek (1-cell)"); b.addEventListener("click", () => { state.mode = "peek"; state.n = 1; reset(); modeSeg.querySelectorAll("button").forEach(x => x.classList.remove("active")); b.classList.add("active"); }); return b; })(),
      (() => { const b = el("button", { type: "button", class: "active" }, "pop (N-cell ring)"); b.addEventListener("click", () => { state.mode = "pop"; state.n = 5; reset(); modeSeg.querySelectorAll("button").forEach(x => x.classList.remove("active")); b.classList.add("active"); }); return b; })(),
    ]);

    const samples = ["alpha", "beta", "gamma", "delta", "epsilon", "zeta", "eta"];
    let nextSample = 0;
    const pushBtn = el("button", { class: "btn solid", onclick: () => { push(samples[nextSample % samples.length]); nextSample++; } }, "↑ push value");
    const popBtn  = el("button", { class: "btn", onclick: pop }, "↓ pop (or peek)");
    const resetBtn= el("button", { class: "btn ghost", onclick: reset }, "reset");

    host.appendChild(frame(
      "SCHEMATIC · slot store",
      "push / pop / peek ↘",
      [
        el("p", { class: "tiny dim" }, "Every input port has one slot, allocated at graph load. 1-cell slots are peek-mode (literals, runs-once wires); N-cell rings are pop-mode (queued / multi-push)."),
        modeSeg,
        stage,
        el("div", { class: "row", style: "margin-top:0.6rem" }, [pushBtn, popBtn, resetBtn, warn]),
        el("div", { class: "mono tiny", style: "margin-top:0.4rem" }, [
          el("span", { class: "k dim" }, "last read: "), last,
        ]),
      ],
      "The slot allocator pre-sizes from compile-time enumeration; rings grow on demand."
    ));

    draw();
  };

  /* ─── cell tagging across parallel iterators ─────────────── */
  demos.cellTagging = (host) => {
    let pushed = []; // {tag, value, order}
    let pushOrder = 0;
    let tagSeq = 1;

    const draw = () => {
      const W = 460, H = 200;
      const root = svg("svg", { viewBox: `0 0 ${W} ${H}` });

      // Two iterator workers
      root.appendChild(svg("text", { x: 10, y: 16, class: "label-mono", fill: "var(--amber)" }, "iterator A — parallel workers"));
      // FIFO queue
      const qx = 10, qy = 40, slotW = 56, slotH = 50;

      // tagged sort
      const fifo = [...pushed].sort((a, b) => a.order - b.order);   // arrival order
      const tagged = [...pushed].sort((a, b) => a.tag - b.tag);     // tag order

      // FIFO row
      root.appendChild(svg("text", { x: qx, y: qy - 6, class: "label-mono", fill: "var(--cream-faint)" }, "fifo pop (untagged) →"));
      fifo.slice(0, 7).forEach((p, i) => {
        const x = qx + i * (slotW + 4);
        root.appendChild(svg("rect", { x, y: qy, width: slotW, height: slotH, fill: "var(--ink-card)", stroke: "var(--ink-rim)", rx: 2 }));
        root.appendChild(svg("text", { x: x + slotW/2, y: qy + 22, "text-anchor": "middle", "font-family": "var(--font-display)", "font-size": 14, fill: "var(--cream)" }, p.value));
        root.appendChild(svg("text", { x: x + slotW/2, y: qy + 40, "text-anchor": "middle", class: "label-mono", fill: "var(--cream-faint)" }, `t${p.tag}`));
      });

      // Tagged row
      const ty = qy + slotH + 24;
      root.appendChild(svg("text", { x: qx, y: ty - 6, class: "label-mono", fill: "var(--amber)" }, "tagged pop (lowest tag first) →"));
      tagged.slice(0, 7).forEach((p, i) => {
        const x = qx + i * (slotW + 4);
        const ok = p.tag === (i + 1); // would be in-order
        root.appendChild(svg("rect", { x, y: ty, width: slotW, height: slotH, fill: "var(--ink-card)", stroke: ok ? "var(--copper)" : "var(--amber)", rx: 2 }));
        root.appendChild(svg("text", { x: x + slotW/2, y: ty + 22, "text-anchor": "middle", "font-family": "var(--font-display)", "font-size": 14, fill: "var(--cream)" }, p.value));
        root.appendChild(svg("text", { x: x + slotW/2, y: ty + 40, "text-anchor": "middle", class: "label-mono", fill: "var(--amber)" }, `t${p.tag}`));
      });

      stage.replaceChildren(root);
    };

    const pushOutOfOrder = () => {
      // simulate: take next tag in sequence but with random arrival order
      const tag = tagSeq++;
      const value = String.fromCharCode(64 + tag);
      pushed.push({ tag, value, order: pushOrder + Math.random() });
      pushOrder++;
      draw();
    };

    const pushBackwards = () => {
      // produce a value with a high tag first, then a low tag — out of order
      const t1 = tagSeq++;
      const t2 = tagSeq++;
      pushed.push({ tag: t2, value: String.fromCharCode(64 + t2), order: pushOrder++ });
      pushed.push({ tag: t1, value: String.fromCharCode(64 + t1), order: pushOrder++ });
      draw();
    };

    const reset = () => { pushed = []; tagSeq = 1; pushOrder = 0; draw(); };

    const stage = el("div", {});

    host.appendChild(frame(
      "ANALYSIS · cell tagging",
      "see the difference ↘",
      [
        el("p", { class: "tiny dim" }, "When iterator A spawns parallel tasks, push K may arrive before push K-1 (faster worker). FIFO pop sees the wrong order; tagged pop returns the lowest tag first and preserves iteration sequence."),
        stage,
        el("div", { class: "row", style: "margin-top:0.6rem" }, [
          el("button", { class: "btn", onclick: pushOutOfOrder }, "↑ push (random order)"),
          el("button", { class: "btn", onclick: pushBackwards }, "↑↑ push two reversed"),
          el("button", { class: "btn ghost", onclick: reset }, "reset"),
        ]),
      ],
      "Compile-time analysis tags any slot whose feeding wire descends from an iterator."
    ));

    draw();
  };

  /* ─── phase roadmap timeline ─────────────────────────────── */
  demos.phaseTimeline = (host) => {
    const phases = [
      { num: "I",   title: "Foundation",            status: "done",    detail: "Editor + synchronous runner. Issues 101–110 complete." },
      { num: "II",  title: "Editor & graph model",  status: "current", detail: "Map picker, source browser, comparator routing, iterator, compile button. Issues 200-series." },
      { num: "III", title: "Thread pool runtime",   status: "planned", detail: "C pool runner, slot store, language specs, dispatch layer. Issues 301–313." },
      { num: "IV",  title: "SoraMind integration",  status: "later",   detail: "Basic Ollama call lib ships in phase 2 (issue 209). Phase 4 adds streaming, prompt-from-data-section sugar, and multi-host routing on top." },
      { num: "V",   title: "Map-to-map calls",      status: "later",   detail: "A box that invokes another map as a subroutine." },
      { num: "VI",  title: "Remote runner",         status: "later",   detail: "Edit local, run on Alpine. File-CRUD over HTTP." },
    ];
    const progressByStatus = { done: 1, current: 0.42, planned: 0, later: 0 };

    const wrap = el("div", { class: "phase-timeline", style: "display:flex; flex-direction:column; gap:0.65rem" });

    phases.forEach((p, i) => {
      const colour = p.status === "done" ? "var(--copper)" :
                     p.status === "current" ? "var(--amber)" :
                     p.status === "planned" ? "var(--gold)" : "var(--cream-faint)";
      const card = el("div", {
        style: "border:1px solid var(--ink-rim); border-radius:3px; padding:0.85rem 1rem; background:var(--ink-card); display:grid; grid-template-columns: 50px 1fr auto; gap:1rem; align-items:center; cursor:pointer; transition: all 150ms;"
      });
      card.addEventListener("mouseenter", () => { card.style.borderColor = colour; card.style.background = "rgba(255,157,61,0.04)"; });
      card.addEventListener("mouseleave", () => { card.style.borderColor = "var(--ink-rim)"; card.style.background = "var(--ink-card)"; });

      card.append(
        el("div", { style: `font-family:var(--font-display); font-style:italic; font-size:1.8rem; color:${colour}; font-variation-settings:'opsz' 96, 'wght' 320;` }, p.num),
        el("div", {}, [
          el("div", { style: "font-family:var(--font-display); font-size:1.05rem; color:var(--cream)" }, p.title),
          el("div", { class: "tiny dim" }, p.detail),
          el("div", { style: "margin-top:0.5rem; height:3px; background:var(--ink-rule); border-radius:2px;" }, [
            el("div", {
              style: `width:${progressByStatus[p.status]*100}%; height:100%; background:${colour}; border-radius:2px; transition:width 600ms;`
            })
          ]),
        ]),
        el("span", { class: "pill " + (p.status === "done" ? "done" : (p.status === "current" ? "open" : "planned")) }, p.status)
      );
      wrap.appendChild(card);
    });

    host.appendChild(frame(
      "CALENDAR · phase timeline",
      "click stays put — these aren't deadlines ↘",
      [wrap],
      "Phase III is fully designed across issues 301–313; implementation hasn't started."
    ));
  };

  /* ─── driver vs spec animation ───────────────────────────── */
  demos.driverVsSpec = (host) => {
    let mode = "driver";
    let running = false;

    const stage = el("div", {});

    const draw = (frameIdx) => {
      const W = 460, H = 200;
      const root = svg("svg", { viewBox: `0 0 ${W} ${H}` });
      const cy = 100;

      const callerX = 20, calleeX = 360;
      // Caller
      root.appendChild(svg("rect", { x: callerX, y: cy - 28, width: 110, height: 56, fill: "var(--ink-card)", stroke: "var(--amber)", rx: 2 }));
      root.appendChild(svg("text", { x: callerX + 12, y: cy - 8, class: "label-serif", fill: "var(--cream)" }, "runner / pool"));
      root.appendChild(svg("text", { x: callerX + 12, y: cy + 12, class: "label-mono", fill: "var(--cream-faint)" }, mode === "driver" ? "synchronous" : "worker thread"));

      // Callee
      const calleeColour = mode === "driver" ? "var(--cream-faint)" : "var(--copper)";
      root.appendChild(svg("rect", { x: calleeX, y: cy - 28, width: 110, height: 56, fill: "var(--ink-card)", stroke: calleeColour, rx: 2 }));
      root.appendChild(svg("text", { x: calleeX + 12, y: cy - 8, class: "label-serif", fill: "var(--cream)" }, mode === "driver" ? "lua.sh" : "lua_State"));
      root.appendChild(svg("text", { x: calleeX + 12, y: cy + 12, class: "label-mono", fill: "var(--cream-faint)" }, mode === "driver" ? "shell process" : "in-process"));

      // Latency arc
      const time = mode === "driver" ? 3.6 : 0.04; // ms
      root.appendChild(svg("text", { x: W/2, y: 30, "text-anchor": "middle", class: "label-mono", fill: "var(--amber)", "font-size": 11 }, `≈ ${time} ms / call`));

      // Animated dot
      if (running) {
        const dot = svg("circle", { r: 4.5, fill: "var(--gold)" });
        const pathD = `M ${callerX + 110} ${cy} L ${calleeX} ${cy}`;
        const ret = svg("path", { d: pathD, fill: "none", stroke: "var(--amber)", opacity: 0.5, "stroke-dasharray": "3 3" });
        root.appendChild(ret);
        dot.appendChild(svg("animateMotion", { dur: mode === "driver" ? "2s" : "0.15s", repeatCount: "indefinite", path: pathD }));
        root.appendChild(dot);

        if (mode === "driver") {
          // Show process spawn cost
          for (let i = 0; i < 3; i++) {
            root.appendChild(svg("rect", { x: calleeX + 20 + i*8, y: cy - 50, width: 5, height: 8, fill: "var(--cream-faint)", opacity: 0.4 - i*0.1 }));
          }
          root.appendChild(svg("text", { x: calleeX + 50, y: cy - 54, "text-anchor": "middle", class: "label-mono", fill: "var(--cream-faint)", "font-size": 9 }, "process spawn"));
        } else {
          root.appendChild(svg("text", { x: (callerX + calleeX)/2 + 55, y: cy - 38, "text-anchor": "middle", class: "label-mono", fill: "var(--copper)", "font-size": 9 }, "dlsym → call"));
        }
      }

      // bullet points below
      stage.replaceChildren(root);
    };

    const startStop = el("button", { class: "btn solid", onclick: () => { running = !running; startStop.textContent = running ? "■ stop" : "▶ animate"; draw(); } }, "▶ animate");
    const seg = el("div", { class: "seg" }, [
      (() => { const b = el("button", { type: "button", class: "active" }, "phase 2 · driver"); b.addEventListener("click", () => { mode = "driver"; seg.querySelectorAll("button").forEach(x => x.classList.remove("active")); b.classList.add("active"); draw(); }); return b; })(),
      (() => { const b = el("button", { type: "button" }, "phase 3 · spec"); b.addEventListener("click", () => { mode = "spec"; seg.querySelectorAll("button").forEach(x => x.classList.remove("active")); b.classList.add("active"); draw(); }); return b; })(),
    ]);

    host.appendChild(frame(
      "BENCHMARK · driver vs spec",
      "watch the cost ↘",
      [
        el("p", { class: "tiny dim" }, "Phase 2 spawns a new process per call. Phase 3 calls the language directly through a per-worker handle. Same box, two orders of magnitude difference."),
        seg,
        stage,
        el("div", { class: "row", style: "margin-top:0.6rem" }, [startStop]),
        el("div", { style: "display:grid; grid-template-columns: 1fr 1fr; gap:1rem; margin-top:1rem; padding-top:0.8rem; border-top:1px dashed var(--ink-rule)" }, [
          el("div", {}, [
            el("h4", {}, "driver / phase 2"),
            el("ul", { class: "tiny dim", style: "padding-left:1.1rem" }, [
              el("li", {}, "process spawn per call (1–10 ms)"),
              el("li", {}, "stdout as channel"),
              el("li", {}, "no persistent runtime"),
              el("li", {}, "drivers/*.sh"),
            ])
          ]),
          el("div", {}, [
            el("h4", {}, "spec / phase 3"),
            el("ul", { class: "tiny copper", style: "padding-left:1.1rem" }, [
              el("li", {}, "dlopen at startup"),
              el("li", {}, "persistent handle per worker"),
              el("li", {}, "direct call / Unix socket"),
              el("li", {}, "langs/<name>/spec.so"),
            ])
          ]),
        ])
      ]
    ));

    draw();
  };

  /* ─── thread pool blocking visualization ─────────────────── */
  demos.threadPool = (host) => {
    let nWorkers = 8;
    const state = {
      workers: Array.from({ length: 8 }, () => ({ task: null, kind: "idle" })),
    };
    const stage = el("div", {});
    const status = el("div", {});

    const palette = {
      idle:    "var(--ink-card)",
      working: "var(--copper)",
      blocked: "var(--amber)",
    };

    const draw = () => {
      stage.replaceChildren();
      const grid = el("div", { style: `display:grid; grid-template-columns: repeat(8, 1fr); gap:6px;` });
      state.workers.forEach((w, i) => {
        const c = el("button", {
          type: "button",
          style: `border:1px solid var(--ink-rim); background:${palette[w.kind] === "var(--ink-card)" ? "var(--ink-card)" : palette[w.kind]}; height:46px; border-radius:2px; cursor:pointer; font-family:var(--font-mono); font-size:0.65rem; color: ${w.kind === "idle" ? "var(--cream-faint)" : "var(--ink-deep)"}; transition: all 150ms;`,
          onclick: () => {
            const next = { idle: "working", working: "blocked", blocked: "idle" }[w.kind];
            w.kind = next;
            draw();
          }
        }, `w${i}`);
        grid.appendChild(c);
      });
      stage.appendChild(grid);

      const idle = state.workers.filter(w => w.kind === "idle").length;
      const blocked = state.workers.filter(w => w.kind === "blocked").length;
      const working = state.workers.filter(w => w.kind === "working").length;
      const free = idle;
      status.replaceChildren(
        el("div", { class: "kvline" }, [el("span", { class: "k" }, "idle"),    el("span", { class: "v dim" }, String(idle))]),
        el("div", { class: "kvline" }, [el("span", { class: "k" }, "working"), el("span", { class: "v copper" }, String(working))]),
        el("div", { class: "kvline" }, [el("span", { class: "k" }, "blocked"), el("span", { class: "v amber" }, String(blocked))]),
        el("div", { class: "kvline" }, [el("span", { class: "k" }, "capacity left"), el("span", { class: "v" }, `${free}/${nWorkers}`)]),
        el("div", { class: "callout copper", style: "margin-top:0.8rem; padding:0.6rem 1rem 0.6rem 2.2rem" }, [
          el("strong", {}, "rule "),
          document.createTextNode(blocked === nWorkers
            ? "every worker is parked. Quiescence will not advance until one unblocks."
            : `${free + working} workers can still pick up tasks. Pool size is the budget for long-running ops.`)
        ])
      );
    };

    const setN = (n) => {
      nWorkers = n;
      state.workers = Array.from({ length: n }, () => ({ kind: "idle" }));
      draw();
    };

    const sizes = [4, 8, 16];
    const seg = el("div", { class: "seg" }, sizes.map(n => {
      const b = el("button", { type: "button" }, `${n} workers`);
      if (n === 8) b.classList.add("active");
      b.addEventListener("click", () => {
        seg.querySelectorAll("button").forEach(x => x.classList.remove("active"));
        b.classList.add("active");
        setN(n);
      });
      return b;
    }));

    host.appendChild(frame(
      "MODEL · thread pool budget",
      "click workers to cycle states ↘",
      [
        el("p", { class: "tiny dim" }, "Each box invocation occupies one worker. Long-running ops (sleep, network) hold the worker for their full duration. Pool size is the concurrency budget."),
        seg,
        el("div", { style: "display:grid; grid-template-columns: 1fr 220px; gap:1.4rem; margin-top:0.8rem" }, [stage, status]),
      ],
      "For many concurrent waits without burning workers, use the cooperative kickoff + check-done iterator pattern."
    ));

    draw();
  };

  /* ─── IPC option comparison ──────────────────────────────── */
  demos.ipcOptions = (host) => {
    const data = [
      { id: "ffi",    label: "FFI (.so)",          ns: 0.08,   langs: "C-ABI only",   color: "var(--copper)" },
      { id: "shm",    label: "shared memory",      ns: 0.15,   langs: "any (w/ caveats)", color: "var(--gold)" },
      { id: "usock",  label: "Unix domain socket", ns: 12,     langs: "any",           color: "var(--amber)" },
      { id: "drv",    label: "phase 2 driver",     ns: 3500,   langs: "any",           color: "var(--rust)" },
    ];
    const maxLog = Math.log10(4000);

    const wrap = el("div", {});
    data.forEach(d => {
      const pct = (Math.log10(d.ns + 1) / maxLog) * 100;
      const row = el("div", { style: "margin: 0.45rem 0;" });
      row.append(
        el("div", { style: "display:flex; justify-content:space-between; margin-bottom:0.2rem;" }, [
          el("span", { class: "mono tiny", style: `color:${d.color}` }, d.label),
          el("span", { class: "mono tiny dim" }, d.ns >= 1 ? `${d.ns} μs` : `${d.ns*1000} ns`),
        ]),
        el("div", { style: "height:14px; background:var(--ink-card); border:1px solid var(--ink-rule); border-radius:1px; position:relative; overflow:hidden;" }, [
          el("div", { style: `width:${pct}%; height:100%; background:${d.color}; transition: width 600ms;` }),
          el("span", { class: "mono tiny", style: `position:absolute; right:6px; top:-1px; color:var(--cream-faint); font-size:0.6rem; line-height:14px;` }, d.langs),
        ])
      );
      wrap.appendChild(row);
    });

    host.appendChild(frame(
      "CHART · IPC speed comparison (log scale)",
      "lower is faster ↘",
      [
        el("p", { class: "tiny dim" }, "Latency per cross-language call. The bar widths are log-scaled because the spread is four orders of magnitude — phase 2's process-spawn cost dwarfs everything else."),
        wrap,
      ],
      "Unix sockets land in the microsecond range; FFI in nanoseconds. The driver path is a millisecond per call."
    ));
  };

  /* ─── lexer tokenizer demo ───────────────────────────────── */
  demos.lexerDemo = (host) => {
    const sample = `-- {{{ local function trim()
local function trim(s)
    return (s:gsub("^%s*(.-)%s*$", "%1"))
end
-- }}}`;

    const tokenize = (text) => {
      const tokens = [];
      const re = /(--\[\[[\s\S]*?\]\]|--[^\n]*|"[^"\n]*"|'[^'\n]*'|\b(?:local|function|end|return|if|then|else|elseif|for|while|do|in|nil|true|false)\b|\b\d+(?:\.\d+)?\b|[A-Za-z_]\w*|[+\-*/%=<>~^#:,.{}()\[\]]|\s+)/g;
      let m;
      let last = 0;
      while ((m = re.exec(text)) !== null) {
        if (m.index > last) tokens.push({ type: "plain", text: text.slice(last, m.index) });
        const v = m[0];
        let type = "plain";
        if (/^--/.test(v)) type = "comment";
        else if (/^["']/.test(v)) type = "string";
        else if (/^(local|function|end|return|if|then|else|elseif|for|while|do|in|nil|true|false)$/.test(v)) type = "keyword";
        else if (/^\d/.test(v)) type = "number";
        else if (/^[A-Za-z_]/.test(v)) type = "identifier";
        else if (/^\s+$/.test(v)) type = "plain";
        else type = "operator";
        tokens.push({ type, text: v });
        last = re.lastIndex;
      }
      if (last < text.length) tokens.push({ type: "plain", text: text.slice(last) });
      return tokens;
    };

    const COLOURS = {
      keyword:    "var(--amber)",
      string:     "var(--copper)",
      comment:    "var(--cream-ghost)",
      number:     "var(--gold)",
      identifier: "var(--cream)",
      operator:   "var(--indigo)",
      plain:      "var(--cream-dim)",
    };

    const input = el("textarea", { style: "width:100%; height:120px; background:var(--ink-card); color:var(--cream); border:1px solid var(--ink-rule); padding:0.7rem 0.9rem; font-family:var(--font-mono); font-size:0.84rem; line-height:1.55; resize:vertical;" }, sample);
    const output = el("pre", { class: "ascii", style: "white-space:pre-wrap;" });
    const counts = el("div", { class: "mono tiny", style: "margin-top:0.6rem" });

    const render = () => {
      const tokens = tokenize(input.value);
      output.replaceChildren();
      const tally = {};
      tokens.forEach(t => {
        tally[t.type] = (tally[t.type] || 0) + 1;
        const sp = document.createElement("span");
        sp.style.color = COLOURS[t.type] || COLOURS.plain;
        sp.textContent = t.text;
        output.appendChild(sp);
      });
      counts.replaceChildren();
      Object.entries(tally).forEach(([k, v]) => {
        counts.appendChild(el("span", {
          style: `display:inline-block; margin-right:0.8rem; color:${COLOURS[k]};`
        }, `${k}:${v}`));
      });
    };
    input.addEventListener("input", render);

    host.appendChild(frame(
      "TOOL · Lua lexer",
      "edit the text ↘",
      [
        el("p", { class: "tiny dim" }, "Each language ships an editor lexer at langs/<name>/lexer.js. Here's a small Lua tokenizer — type in the box, watch the tokens recolour."),
        input,
        el("h4", {}, "tokens"),
        output,
        counts,
      ],
      "Token types: keyword · string · comment · number · identifier · operator · plain."
    ));
    render();
  };

  /* ─── lang_spec_t interface diagram ──────────────────────── */
  demos.langSpec = (host) => {
    const callbacks = [
      { name: "init",          when: "once per worker at startup", returns: "void* handle", who: "spec author" },
      { name: "teardown",      when: "once per worker at shutdown", returns: "void", who: "spec author" },
      { name: "compile",       when: "at map build time", returns: "int 0/err", who: "spec author (or NULL)" },
      { name: "invoke_json",   when: "per cross-language call", returns: "int 0/err", who: "spec author" },
      { name: "invoke_native", when: "per same-language call", returns: "int 0/err", who: "spec author" },
      { name: "native_to_json", when: "wire crosses language boundary", returns: "int 0/err", who: "spec author" },
      { name: "json_to_native", when: "wire enters this language", returns: "int 0/err", who: "spec author" },
    ];
    let selected = 0;

    const list = el("div", { style: "display:flex; flex-direction:column; gap:0.3rem" });
    const detail = el("div", {});

    const render = () => {
      list.replaceChildren();
      callbacks.forEach((cb, i) => {
        const row = el("button", {
          type: "button",
          style: `font-family:var(--font-mono); text-align:left; padding:0.55rem 0.8rem; background: ${i === selected ? "rgba(255,157,61,0.10)" : "var(--ink-card)"}; color:${i === selected ? "var(--amber)" : "var(--cream-dim)"}; border:1px solid ${i === selected ? "var(--amber-deep)" : "var(--ink-rule)"}; border-radius:2px; cursor:pointer; font-size:0.84rem;`,
          onclick: () => { selected = i; render(); }
        }, cb.name);
        list.appendChild(row);
      });
      const cb = callbacks[selected];
      detail.replaceChildren(
        el("h4", {}, cb.name),
        el("div", { class: "kvline" }, [el("span", { class: "k" }, "when"),    el("span", { class: "v" }, cb.when)]),
        el("div", { class: "kvline" }, [el("span", { class: "k" }, "returns"), el("span", { class: "v copper" }, cb.returns)]),
        el("div", { class: "kvline" }, [el("span", { class: "k" }, "provided by"), el("span", { class: "v" }, cb.who)]),
        el("pre", { class: "ascii", style: "margin-top:1rem; font-size:0.74rem" }, `// langs/<name>/spec.c
lang_spec_t soramech_lang_spec = {
    .name     = "${cb.name === "init" ? "lua" : "..."}",
    .file_ext = ".lua",
    .${cb.name} = my_${cb.name},
    // ...
};`)
      );
    };

    host.appendChild(frame(
      "INTERFACE · lang_spec_t",
      "pick a callback ↘",
      [
        el("p", { class: "tiny dim" }, "Every language is one spec — the same callback signature. Click a callback to see when it runs and what the spec author provides."),
        el("div", { style: "display:grid; grid-template-columns: 220px 1fr; gap:1.2rem; align-items:start" }, [list, detail])
      ]
    ));
    render();
  };

  /* ─── tiny dataflow animation for the landing page ────── */
  demos.dataflow = (host) => {
    const W = 740, H = 220;
    const root = svg("svg", { viewBox: `0 0 ${W} ${H}` });

    const boxes = [
      { x: 30,  y: 80,  label: "input",      sub: "read" },
      { x: 200, y: 30,  label: "tokenize",   sub: "lua / call" },
      { x: 200, y: 130, label: "fetch",      sub: "bash / call" },
      { x: 380, y: 80,  label: "rank",       sub: "c / call" },
      { x: 550, y: 30,  label: "log",        sub: "write" },
      { x: 550, y: 130, label: "respond",    sub: "lua / call" },
    ];
    const wires = [[0,1], [0,2], [1,3], [2,3], [3,4], [3,5]];

    boxes.forEach((b, i) => {
      const w = 130, h = 50;
      const g = svg("g", {});
      g.appendChild(svg("rect", { x: b.x, y: b.y, width: w, height: h, fill: "var(--ink-card)", stroke: i === 3 ? "var(--amber)" : "var(--ink-rim)", rx: 3 }));
      g.appendChild(svg("text", { x: b.x + 12, y: b.y + 22, "font-family": "var(--font-display)", "font-size": 14, fill: "var(--cream)" }, b.label));
      g.appendChild(svg("text", { x: b.x + 12, y: b.y + 38, class: "label-mono" }, b.sub));
      root.appendChild(g);
    });

    wires.forEach(([a, b], i) => {
      const A = boxes[a], B = boxes[b];
      const sx = A.x + 130, sy = A.y + 25;
      const tx = B.x,      ty = B.y + 25;
      const d = `M ${sx} ${sy} C ${(sx+tx)/2} ${sy}, ${(sx+tx)/2} ${ty}, ${tx} ${ty}`;
      root.appendChild(svg("path", { d, stroke: "var(--amber)", "stroke-width": 1.3, fill: "none", opacity: 0.5 }));

      const dot = svg("circle", { r: 3.5, fill: "var(--gold)" });
      dot.appendChild(svg("animateMotion", {
        dur: "3.2s", repeatCount: "indefinite",
        path: d, begin: `${i * 0.4}s`
      }));
      root.appendChild(dot);
    });

    host.appendChild(root);
  };

  /* ─── init ──────────────────────────────────────────────── */
  document.addEventListener("DOMContentLoaded", () => {
    document.querySelectorAll("[data-demo]").forEach(host => {
      const name = host.dataset.demo;
      if (demos[name]) {
        try { demos[name](host); }
        catch (e) {
          host.innerHTML = `<div class="callout"><strong>demo error:</strong> ${name} — ${e.message}</div>`;
          console.error(e);
        }
      }
    });
  });
})();
