// Pack inspector client: /game/<id>/pack?path=<rel> renders one parsed ntpack.
(function () {
  const match = /^\/game\/([^/]+)\/pack\/?$/.exec(decodeURIComponent(location.pathname));
  const gameId = match ? match[1] : "";
  const packPath = new URLSearchParams(location.search).get("path") || "";

  const byId = (id) => document.getElementById(id);

  function formatBytes(bytes) {
    if (bytes == null) return "";
    if (bytes >= 1024 * 1024) return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
    if (bytes >= 1024) return `${(bytes / 1024).toFixed(1)} KB`;
    return `${bytes} B`;
  }

  function fail(message) {
    byId("packTitle").textContent = "Pack unavailable";
    byId("packSubtitle").textContent = message;
  }

  function cell(text) {
    const td = document.createElement("td");
    td.textContent = text;
    return td;
  }

  function render(dump) {
    byId("packTitle").textContent = dump.pack.rel;
    const gzTotal = (dump.entries || []).reduce((sum, entry) => sum + (entry.gzBytes || 0), 0);
    byId("packSubtitle").textContent =
      `${dump.game.id} · ${formatBytes(dump.pack.bytes)} (gz ~${formatBytes(gzTotal)}) · ` +
      `${dump.header ? dump.header.assetCount : 0} assets · pack v${dump.header ? dump.header.version : "?"}`;
    if (dump.error) return fail(dump.error);
    byId("packSummaryNote").textContent = dump.pack.namesFrom
      ? `Names resolved from ${dump.pack.namesFrom}.`
      : "No generated name header found; names shown as hashes.";

    const summaryBox = byId("packSummary");
    summaryBox.textContent = "";
    for (const [type, row] of Object.entries(dump.summary || {})) {
      const chip = document.createElement("span");
      chip.className = "pack-chip";
      const dup = row.dupCount ? ` +${row.dupCount} dup` : "";
      chip.textContent = `${type}: ${row.count} (${formatBytes(row.bytes)}${dup})`;
      summaryBox.append(chip);
    }

    const tbody = byId("packTable").querySelector("tbody");
    tbody.textContent = "";
    for (const entry of dump.entries || []) {
      const tr = document.createElement("tr");
      const notes = [];
      if (entry.dupOfIndex != null) notes.push(`dup of #${entry.dupOfIndex}`);
      if (!entry.inBounds) notes.push("OUT OF BOUNDS");
      tr.append(
        cell(String(entry.index)),
        cell(entry.name),
        cell(entry.typeTag),
        cell(formatBytes(entry.size)),
        cell(entry.gzBytes != null ? formatBytes(entry.gzBytes) : ""),
        cell(notes.join(", ")),
      );
      if (entry.dupOfIndex != null) tr.className = "pack-row-dup";
      tbody.append(tr);
    }
  }

  async function load() {
    if (!gameId || !packPath) return fail("Missing game id or pack path in the URL.");
    const query = `game=${encodeURIComponent(gameId)}&path=${encodeURIComponent(packPath)}`;
    const response = await fetch(`/api/game-page/pack?${query}`, { cache: "no-store" });
    if (!response.ok) return fail(`Pack request failed: ${response.status}`);
    render(await response.json());
  }

  load().catch((error) => fail(error?.message || String(error)));
})();
