// Sidebar TOC + subnav highlighting.
// All paths relative to docs/HTML/. The sidebar HTML is identical across pages,
// and a `data-page` attribute on <body> selects the active entry.

(function () {
  const pages = [
    { num: "00",  href: "index.html",                   title: "Contents" },
    { num: "01",  href: "001-architecture.html",        title: "Architecture" },
    { num: "02",  href: "002-roadmap.html",             title: "Roadmap" },
    { num: "03",  href: "003-driver-system.html",       title: "Driver System" },
    { num: "04",  href: "004-ipc-and-threading.html",   title: "IPC & Threading" },
    { num: "05",  href: "005-language-specs.html",      title: "Language Specs" },
  ];

  function build() {
    const aside = document.querySelector(".sidebar");
    if (!aside) return;

    const current = document.body.dataset.page || "";

    const brand = `
      <a href="index.html" class="brand">
        <span class="mark"><em class="accent">Sora</em>Mech</span>
        <span class="sub">Notebook · vol. one</span>
      </a>`;

    const tocItems = pages.map(p => {
      const isCurrent = (p.href === current) ? " current" : "";
      return `
        <li>
          <a class="page-link${isCurrent}" href="${p.href}">
            <span class="num">§${p.num}</span>
            <span class="title">${p.title}</span>
          </a>
          ${isCurrent ? `<ul class="toc-subnav" data-subnav></ul>` : ``}
        </li>`;
    }).join("");

    const foot = `
      <div class="sidebar-foot">
        <div>Source: <a href="../">../docs/</a></div>
        <div>Last assembled by lamplight.</div>
        <div style="margin-top:0.6rem;">
          <span class="scribble" style="display:block; font-size:0.95rem; transform:rotate(-2deg)">strings flow like ink ↗</span>
        </div>
      </div>`;

    aside.innerHTML = `
      ${brand}
      <span class="toc-label">Documents</span>
      <ul class="toc-list">${tocItems}</ul>
      ${foot}
    `;

    // Sub-nav from page <h2> elements
    const subnav = aside.querySelector("[data-subnav]");
    if (subnav) {
      const heads = Array.from(document.querySelectorAll("main h2[id]"));
      subnav.innerHTML = heads.map(h =>
        `<li><a href="#${h.id}" data-sub-target="${h.id}">${h.textContent.trim()}</a></li>`
      ).join("");

      // Scroll spy
      const linkFor = id => subnav.querySelector(`[data-sub-target="${CSS.escape(id)}"]`);
      const obs = new IntersectionObserver((entries) => {
        entries.forEach(e => {
          const a = linkFor(e.target.id);
          if (!a) return;
          if (e.isIntersecting) {
            subnav.querySelectorAll("a").forEach(x => x.classList.remove("current"));
            a.classList.add("current");
          }
        });
      }, { rootMargin: "-20% 0px -70% 0px" });
      heads.forEach(h => obs.observe(h));
    }

    // Number every page-level h2 in the document using its data-num if present,
    // otherwise inject section numbers like "§01.A" sequentially.
    const h2s = document.querySelectorAll("main h2");
    let n = 0;
    h2s.forEach(h => {
      if (h.hasAttribute("data-num")) return;
      n++;
      const letter = String.fromCharCode(64 + n); // A, B, C
      const ts = (document.body.dataset.page || "").match(/(\d{3})/);
      const docNum = ts ? ts[1].slice(1) : "00";
      h.setAttribute("data-num", `§${docNum}.${letter}`);
    });
  }

  document.addEventListener("DOMContentLoaded", build);
})();
