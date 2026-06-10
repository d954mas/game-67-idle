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

async function init() {
  const [balance, uiFlow, tasks] = await Promise.all([
    readJson("data/balance.json"),
    readJson("data/ui_flow.json"),
    readJson("data/implementation_tasks.json"),
  ]);

  const economyGrid = document.querySelector("#economyGrid");
  [...balance.currencies, ...balance.stats].forEach((item) => {
    const card = el("article", "data-card");
    card.appendChild(el("b", "", item.label));
    card.appendChild(el("p", "", item.first_slice_role || item.role || ""));
    addMeta(card, [
      item.type ? `type: ${item.type}` : "",
      Number.isFinite(item.starts_at) ? `start: ${item.starts_at}` : "",
      Number.isFinite(item.max) ? `max: ${item.max}` : "",
    ]);
    economyGrid.appendChild(card);
  });

  const screenMap = document.querySelector("#screenMap");
  uiFlow.screens.forEach((screen) => {
    const card = el("article", "screen-card");
    card.appendChild(el("b", "", screen.label));
    card.appendChild(el("p", "", screen.purpose));
    addMeta(card, screen.primary_actions.slice(0, 4));
    screenMap.appendChild(card);
  });

  const taskGrid = document.querySelector("#taskGrid");
  tasks.phases.forEach((phase) => {
    const card = el("article", "task-card");
    card.appendChild(el("b", "", phase.label));
    card.appendChild(el("p", "", phase.must_prove));
    addMeta(card, phase.done_when.slice(0, 2));
    taskGrid.appendChild(card);
  });
}

init().catch((error) => {
  document.body.appendChild(el("pre", "", error.stack || String(error)));
});
