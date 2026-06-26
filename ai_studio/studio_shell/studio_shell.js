(() => {
  const storageKey = "ai_studio.sidebar.collapsed.v1";

  function initStudioShell() {
    const shell = document.querySelector(".studio-shell");
    const toggle = document.querySelector("[data-studio-sidebar-toggle]");
    if (!shell || !toggle) return;

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
