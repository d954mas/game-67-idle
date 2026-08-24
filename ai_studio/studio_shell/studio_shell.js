(() => {
  const storageKey = "ai_studio.sidebar.collapsed.v1";

  // Single source of the sidebar: pages ship an empty <nav class="studio-side-nav">
  // and this script fills it, so navigation cannot drift between surfaces.
  const NAV_ITEMS = [
    { href: "/", icon: "S", iconClass: "", label: "Studio Home" },
    { href: "/games/", icon: "G", iconClass: "games", label: "Games" },
    { href: "/architecture_map/", icon: "M", iconClass: "map", label: "Architecture Map" },
    { href: "/taskboard/", icon: "T", iconClass: "tasks", label: "Taskboard" },
    { href: "/asset_viewer/", icon: "A", iconClass: "assets", label: "Asset Viewer" },
    { href: "/canvas", icon: "C", iconClass: "prep", label: "Canvas" },
    { href: "/quality/", icon: "Q", iconClass: "quality", label: "Quality Checks" },
  ];

  function renderNav(nav) {
    nav.textContent = "";
    const section = document.createElement("p");
    section.className = "studio-nav-section";
    section.textContent = "Workspace";
    nav.append(section);
    for (const item of NAV_ITEMS) {
      const link = document.createElement("a");
      link.className = "studio-nav-item";
      const active = item.href === "/"
        ? location.pathname === "/"
        : item.href === "/games/"
          ? location.pathname.startsWith("/games") || location.pathname.startsWith("/game/")
          : location.pathname.startsWith(item.href);
      if (active) link.classList.add("is-active");
      link.href = item.href;
      link.setAttribute("aria-label", item.label);
      const icon = document.createElement("span");
      icon.className = `studio-nav-icon ${item.iconClass}`.trim();
      icon.textContent = item.icon;
      const label = document.createElement("span");
      label.className = "studio-nav-label";
      label.textContent = item.label;
      link.append(icon, label);
      nav.append(link);
    }
  }

  function formatBytes(bytes) {
    if (bytes == null) return "";
    if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
    if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${bytes} B`;
  }

  function formatDate(ms) {
    return ms ? new Date(ms).toISOString().slice(0, 16).replace("T", " ") : "";
  }

  // Standard surface-list row used by home and the game pages.
  function linkRow(pill, title, description, href, linkText) {
    const row = document.createElement("article");
    row.className = "surface-row";
    const pillSpan = document.createElement("span");
    pillSpan.className = "type-pill state";
    pillSpan.textContent = pill;
    const main = document.createElement("div");
    main.className = "surface-main";
    const h4 = document.createElement("h4");
    h4.textContent = title;
    const p = document.createElement("p");
    p.textContent = description;
    main.append(h4, p);
    row.append(pillSpan, main);
    if (href) {
      const a = document.createElement("a");
      a.className = "open-link";
      a.href = href;
      a.textContent = linkText || "Open";
      row.append(a);
    }
    return row;
  }

  window.studioShell = { formatBytes, formatDate, linkRow };

  function initStudioShell() {
    const shell = document.querySelector(".studio-shell");
    const toggle = document.querySelector("[data-studio-sidebar-toggle]");
    if (!shell || !toggle) return;

    const nav = shell.querySelector(".studio-side-nav");
    if (nav && !nav.childElementCount) renderNav(nav);

    // ?embed=1: the page is hosted inside another studio surface (an iframe),
    // which already owns navigation — hide this page's sidebar chrome.
    if (new URLSearchParams(location.search).has("embed")) {
      shell.classList.add("is-embedded");
    }

    const apply = (collapsed) => {
      shell.classList.toggle("is-collapsed", collapsed);
      toggle.setAttribute("aria-expanded", String(!collapsed));
      toggle.textContent = collapsed ? "›" : "‹";
      toggle.title = collapsed ? "Expand sidebar" : "Collapse sidebar";
    };

    apply(localStorage.getItem(storageKey) === "1");

    toggle.addEventListener("click", () => {
      const collapsed = !shell.classList.contains("is-collapsed");
      localStorage.setItem(storageKey, collapsed ? "1" : "0");
      apply(collapsed);
    });
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initStudioShell);
  } else {
    initStudioShell();
  }
})();
