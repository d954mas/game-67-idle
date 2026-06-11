async function readJson(path) {
  const response = await fetch(path);
  if (!response.ok) {
    throw new Error(`Failed to load ${path}: ${response.status}`);
  }
  return response.json();
}

function el(tag, className, text) {
  const node = document.createElement(tag);
  if (className) node.className = className;
  if (text) node.textContent = text;
  return node;
}

function addMeta(parent, values) {
  const meta = el("div", "meta");
  values.filter(Boolean).forEach((value) => meta.appendChild(el("span", "", value)));
  parent.appendChild(meta);
}

function addCards(containerId, items, render) {
  const container = document.querySelector(containerId);
  if (!container) return;
  items.forEach((item) => container.appendChild(render(item)));
}

async function init() {
  const [balance, uiFlow, combat, progression, contentModel, roadmap, tasks] = await Promise.all([
    readJson("data/balance.json"),
    readJson("data/ui_flow.json"),
    readJson("data/combat.json"),
    readJson("data/progression.json"),
    readJson("data/content_model.json"),
    readJson("data/roadmap.json"),
    readJson("data/implementation_tasks.json"),
  ]);

  addCards("#progressionGrid", progression.layers, (layer) => {
    const card = el("article", "data-card");
    card.appendChild(el("b", "", layer.label));
    card.appendChild(el("p", "", layer.purpose));
    addMeta(card, [`first: ${layer.first_slice.join(", ")}`, `future: ${layer.future.slice(0, 3).join(", ")}`]);
    return card;
  });

  addCards("#contentGrid", Object.entries(contentModel.templates), ([name, fields]) => {
    const card = el("article", "data-card");
    card.appendChild(el("b", "", name));
    card.appendChild(el("p", "", `Fields: ${fields.slice(0, 6).join(", ")}.`));
    addMeta(card, fields.slice(6, 10));
    return card;
  });

  addCards("#economyGrid", [...balance.currencies, ...balance.stats], (item) => {
    const card = el("article", "data-card");
    card.appendChild(el("b", "", item.label));
    card.appendChild(el("p", "", item.first_slice_role || item.role || ""));
    addMeta(card, [
      item.type ? `type: ${item.type}` : "",
      Number.isFinite(item.starts_at) ? `start: ${item.starts_at}` : "",
      Number.isFinite(item.max) ? `max: ${item.max}` : "",
    ]);
    return card;
  });

  addCards("#screenMap", uiFlow.screens, (screen) => {
    const card = el("article", "screen-card");
    card.appendChild(el("b", "", screen.label));
    card.appendChild(el("p", "", screen.purpose));
    addMeta(card, screen.primary_actions.slice(0, 4));
    return card;
  });

  const combatCards = [
    ...combat.enemies.map((enemy) => ({ kind: "enemy", item: enemy })),
    ...combat.player_actions.map((action) => ({ kind: "action", item: action })),
  ];
  addCards("#combatGrid", combatCards, ({ kind, item }) => {
    const card = el("article", "data-card");
    card.appendChild(el("b", "", item.label));
    if (kind === "enemy") {
      card.appendChild(el("p", "", `HP ${item.health}; ${item.actions.map((action) => `${action.label} ${action.damage}`).join(", ")} damage.`));
      addMeta(card, item.reward.map((reward) => `${reward.id}: ${reward.value}`));
    } else {
      const effect = item.effect
        ? Object.entries(item.effect).map(([key, value]) => `${key}: ${value}`).join("; ")
        : item.success_outcome || "";
      card.appendChild(el("p", "", effect));
      addMeta(card, [
        item.cost ? `cost: ${item.cost.map((cost) => `${cost.id} ${cost.value}`).join(", ")}` : "",
        item.success_chance ? `success: ${Math.round(item.success_chance * 100)}%` : "",
      ]);
    }
    return card;
  });

  addCards("#roadmapGrid", roadmap.iterations, (iteration) => {
    const card = el("article", "task-card");
    card.appendChild(el("b", "", iteration.label));
    card.appendChild(el("p", "", iteration.goal));
    addMeta(card, iteration.done_when.slice(0, 3));
    return card;
  });

  addCards("#taskGrid", tasks.phases, (phase) => {
    const card = el("article", "task-card");
    card.appendChild(el("b", "", phase.label));
    card.appendChild(el("p", "", phase.must_prove));
    addMeta(card, phase.done_when.slice(0, 2));
    return card;
  });
}

init().catch((error) => {
  document.body.appendChild(el("pre", "", error.stack || String(error)));
});
