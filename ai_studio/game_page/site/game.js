// Game page client: reads the game id from /game/<id> and renders overview data.
(function () {
  const match = /^\/game\/([^/]+)\/?$/.exec(decodeURIComponent(location.pathname));
  const gameId = match ? match[1] : "";

  const byId = (id) => document.getElementById(id);
  const setText = (id, value) => {
    const node = byId(id);
    if (node) node.textContent = value;
  };

  function fail(message) {
    setText("gameTitle", "Game not found");
    const links = byId("overviewLinks");
    if (links) links.innerHTML = `<p class="game-placeholder game-error"></p>`;
    if (links) links.firstChild.textContent = message;
  }

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

  async function load() {
    if (!gameId) return fail("No game id in the URL.");
    const response = await fetch(`/api/game-page/overview?game=${encodeURIComponent(gameId)}`, { cache: "no-store" });
    if (!response.ok) return fail(`Overview request failed: ${response.status}`);
    const overview = await response.json();
    const game = overview.game || {};
    document.title = `${game.title || game.id} — AI Studio`;
    setText("gameTitle", game.title || game.id);
    setText("gameId", game.id || "");
    setText("gameVisibility", game.visibility || "");
    setText("gameVersion", game.version ? `v${game.version}` : "");
    setText("gameRoot", game.root || "");

    const links = byId("overviewLinks");
    links.textContent = "";
    for (const doc of overview.designDocs || []) {
      links.append(linkRow("Docs", doc.label, doc.rel, `/game-file/${encodeURIComponent(game.id)}/${doc.rel}`, "View"));
    }
    for (const project of overview.taskboardProjects || []) {
      links.append(linkRow("Tasks", `${project.id} ${project.title}`, `Taskboard project (${project.status}, ${project.store})`, overview.links?.taskboard, "Open"));
    }
    links.append(linkRow("Assets", "Asset Viewer", "Browse this game's asset folders.", overview.links?.assetViewer, "Open"));
    if (!links.childElementCount) {
      links.innerHTML = `<p class="game-placeholder">Nothing to show yet.</p>`;
    }
  }

  load().catch((error) => fail(error?.message || String(error)));
})();
