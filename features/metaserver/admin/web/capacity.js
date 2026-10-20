// The capacity page: fleet state against the norms, synced uPlot charts over
// the per-minute history, and the machine, traffic and API tables under them.
// Shares app.js globals ($, api, text, number, utc, setStatus, setSource, state).

const capacity = { range: "24h", host: "", step: 300, charts: [], chartsHost: "", xMin: null, data: null, seq: 0 };
const CPU_NORM = 80;
const CPU_ALARM = 90;
const routeNames = {
  enter: "Вход в игру", saves: "Сохранения", save: "Сохранение", load: "Загрузка профиля", guest: "Создание гостя",
  status: "Статусы комнат", probe: "Проверка доступности", ticket: "Билеты комнат",
};
const stateWords = { ok: ["Норма", "good"], warn: ["Внимание", "warn"], alarm: ["Перегрузка", "bad"], unknown: ["Нет данных", ""] };
const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
const fixed = (value, digits = 1) => value == null ? "—" :
  new Intl.NumberFormat("ru-RU", { maximumFractionDigits: digits, minimumFractionDigits: 0 }).format(value);

function cpuWord(cpu) {
  if (cpu == null) return stateWords.unknown;
  if (cpu > CPU_ALARM) return stateWords.alarm;
  if (cpu > CPU_NORM) return stateWords.warn;
  return stateWords.ok;
}

function stateCard(root, title, word, value, norm, note) {
  const card = document.createElement("div");
  card.className = "state-card";
  if (word[1]) card.dataset.tone = word[1];
  const head = document.createElement("div");
  head.className = "state-head";
  head.append(text("span", title, "metric-label"), text("span", word[0], "status " + word[1]));
  card.append(head, text("strong", value, "metric-value"), text("span", norm, "state-norm"), text("span", note, "metric-note"));
  root.append(card);
}

function sum(items, pick) {
  let total = 0, seen = false;
  for (const item of items) {
    const value = pick(item);
    if (value != null) { total += value; seen = true; }
  }
  return seen ? total : null;
}

function renderState(data, rootId = "capacity-state") {
  const root = $(rootId);
  root.replaceChildren();
  const machines = (data.machines || []).filter((m) => m.state !== "unknown");
  const points = data.points.filter((p) => p.status !== "missing");
  const seated = sum(machines, (m) => m.seated);
  const seats = sum(machines, (m) => m.seats);
  const peak = points.reduce((best, p) => p.seated_max != null && (!best || p.seated_max > best.seated_max) ? p : best, null);
  stateCard(root, "Онлайн сейчас", ["", ""], seated == null ? "—" : number(seated),
    seats ? "из " + number(seats) + " мест (" + fixed(100 * seated / seats, 0) + "%)" : "мест нет",
    peak ? "Пик за период " + number(peak.seated_max) + " · " + utc(peak.t) : "Нет данных за период");

  const busiest = machines.reduce((best, m) => m.cpu_pct != null && (!best || m.cpu_pct > best.cpu_pct) ? m : best, null);
  const meanCPU = machines.length ? sum(machines, (m) => m.cpu_pct) / machines.filter((m) => m.cpu_pct != null).length : null;
  stateCard(root, "Нагрузка CPU", cpuWord(busiest?.cpu_pct), busiest ? fixed(busiest.cpu_pct, 0) + "%" : "—",
    "Норма до " + CPU_NORM + "%, перегрузка выше " + CPU_ALARM + "%",
    busiest ? "Самая загруженная: " + busiest.host + " · среднее " + fixed(meanCPU, 0) + "%" : "Машины не прислали нагрузку");

  const rx = sum(machines, (m) => m.rx_mbit_s), tx = sum(machines, (m) => m.tx_mbit_s);
  const traffic = data.traffic;
  const monthGB = traffic ? sum(traffic.hosts || [], (h) => h.month_rx_gb + h.month_tx_gb) : null;
  const plan = traffic?.month_allowance_gb || 0;
  const hosts = traffic?.hosts?.length || 0;
  let trafficWord = ["", ""], trafficNorm = "Тариф не задан";
  if (plan > 0 && monthGB != null) {
    const share = monthGB / (plan * Math.max(hosts, 1));
    trafficWord = share > 1 ? stateWords.alarm : share > 0.8 ? stateWords.warn : stateWords.ok;
    trafficNorm = "Месяц: " + fixed(monthGB, 1) + " ГБ из " + fixed(plan * Math.max(hosts, 1), 0) + " ГБ (" + fixed(100 * share, 0) + "%)";
  } else if (monthGB != null) {
    trafficNorm = "Месяц: " + fixed(monthGB, 1) + " ГБ, тариф не задан";
  }
  stateCard(root, "Трафик сейчас", trafficWord, tx == null ? "—" : fixed(tx + (rx || 0), 1) + " Мбит/с",
    trafficNorm, tx == null ? (data.traffic_error ? "Учет трафика недоступен" : "Нет данных") :
      "Исходящий " + fixed(tx, 1) + " · входящий " + fixed(rx, 1) + " Мбит/с");

  const endpoints = data.endpoints || [];
  const server = sum(endpoints, (e) => e.server_errors) || 0;
  const requests = sum(endpoints, (e) => e.requests) || 0;
  const client = sum(endpoints, (e) => e.client_errors) || 0;
  stateCard(root, "Ошибки сервера", server ? stateWords.warn : stateWords.ok, number(server),
    "Норма: 0 ошибок 5xx за период", number(requests) + " запросов · отказов 4xx " + number(client));
}

function axis(label, values) {
  return { label, stroke: css("--muted"), labelFont: "11px " + css("--font"), font: "11px " + css("--mono"),
    grid: { stroke: "#2b323d", width: 1 }, ticks: { stroke: "#2b323d", width: 1 }, values, size: 56, labelSize: 18 };
}

// UTC, 24-hour; a tick on midnight, or the first tick, carries the date.
function timeAxis() {
  const two = (n) => String(n).padStart(2, "0");
  return { stroke: css("--muted"), font: "11px " + css("--mono"), grid: { stroke: "#2b323d", width: 1 },
    ticks: { stroke: "#2b323d", width: 1 }, size: 44,
    values: (u, splits) => splits.map((ts, i) => {
      const d = new Date(ts * 1000);
      const time = two(d.getUTCHours()) + ":" + two(d.getUTCMinutes());
      const date = two(d.getUTCDate()) + "." + two(d.getUTCMonth() + 1);
      return i === 0 || (d.getUTCHours() === 0 && d.getUTCMinutes() === 0) ? time + "\n" + date : time;
    }) };
}

// Horizontal norm lines drawn under the series.
function normLines(lines) {
  return { hooks: { drawClear: [(u) => {
    const ctx = u.ctx;
    ctx.save();
    ctx.setLineDash([6, 5]);
    for (const [value, color] of lines) {
      const y = Math.round(u.valToPos(value, "y", true));
      ctx.strokeStyle = color;
      ctx.lineWidth = 1.5 * devicePixelRatio;
      ctx.beginPath();
      ctx.moveTo(u.bbox.left, y);
      ctx.lineTo(u.bbox.left + u.bbox.width, y);
      ctx.stroke();
    }
    ctx.restore();
  }] } };
}

function chartOptions(width, spec) {
  return {
    width, height: 210, series: [{ label: "Время", value: (u, ts) => ts == null ? "—" : utc(ts * 1000) }, ...spec.series],
    axes: [timeAxis(), ...spec.axes], bands: spec.bands || [], plugins: spec.plugins || [],
    // While the history is shorter than the period, the axis starts where collection began.
    scales: { x: { time: true, range: (u, min, max) => [capacity.xMin != null ? Math.min(Math.max(min, capacity.xMin), max) : min, max] }, ...spec.scales },
    tzDate: (ts) => uPlot.tzDate(new Date(ts * 1000), "Etc/UTC"),
    cursor: { sync: { key: "capacity" }, points: { size: 6 } }, legend: { show: true, live: true },
  };
}

function line(label, color, unit, extra = {}) {
  return { label, stroke: color, width: 2, spanGaps: false, points: { show: false },
    value: (u, v) => v == null ? "—" : fixed(v, unit === "%" ? 0 : 1) + (unit ? " " + unit : ""), ...extra };
}

const upTo = (floor, headroom) => (u, min, max) => [0, Math.max(floor, (max || 0) * headroom)];

// Each chart: the mean as the main line and the bucket's peak above it, the
// band between them showing how far minutes swung. Never smoothed: a capacity
// call rests on the peak, which a moving average would cut.
function chartSpecs() {
  const accent = css("--accent"), good = css("--good"), warn = css("--warn"), bad = css("--bad"), muted = css("--muted");
  const bars = uPlot.paths.bars({ size: [0.7, 24] });
  const count = () => (u, v) => v == null ? "—" : number(v) + " за " + stepLabel();
  return [
    { id: "chart-online", label: "График онлайна", scales: { y: { range: upTo(10, 1.1) } },
      series: [line("Пик", accent, "", { width: 1 }), line("Среднее", accent, ""), line("Места в живых комнатах", muted, "", { dash: [4, 4], width: 1.5 })],
      bands: [{ series: [1, 2], fill: "rgba(233,173,92,.18)" }],
      axes: [axis("игроки", (u, v) => v.map((x) => number(x)))],
      values: (p) => [p.seated_max, p.seated_avg, p.seats] },
    { id: "chart-cpu", label: "График нагрузки CPU", scales: { y: { range: [0, 100] } },
      series: [line(capacity.host ? "Пик машины" : "Самая загруженная машина", bad, "%", { width: 1 }),
        line(capacity.host ? "Среднее машины" : "Среднее по машинам", warn, "%")],
      bands: [{ series: [1, 2], fill: "rgba(239,147,142,.16)" }],
      axes: [axis("% CPU", (u, v) => v.map((x) => x + "%"))],
      plugins: [normLines([[CPU_NORM, "rgba(242,199,120,.55)"], [CPU_ALARM, "rgba(239,147,142,.55)"]])],
      values: (p) => [p.cpu_max, p.cpu_avg] },
    { id: "chart-traffic", label: "График трафика", scales: { y: { range: upTo(1, 1.1) } },
      series: [line("Исходящий, пик", accent, "Мбит/с", { width: 1 }), line("Исходящий", accent, "Мбит/с"), line("Входящий", good, "Мбит/с")],
      bands: [{ series: [1, 2], fill: "rgba(233,173,92,.18)" }],
      axes: [axis("Мбит/с", (u, v) => v.map((x) => fixed(x, 1)))],
      values: (p) => [p.tx_mbit_s_peak, p.tx_mbit_s, p.rx_mbit_s] },
    { id: "chart-errors", label: "График запросов и ошибок API", fleet: true, scales: { y: { range: upTo(5, 1.2) }, req: { range: upTo(1, 1.1) } },
      series: [line("Запросы", muted, "в с", { scale: "req" }),
        { ...line("Отказы 4xx", warn, ""), paths: bars, fill: "rgba(242,199,120,.6)", width: 0, value: count() },
        { ...line("Ошибки 5xx", bad, ""), paths: bars, fill: bad, width: 0, value: count() }],
      axes: [axis("ошибок за точку", (u, v) => v.map((x) => number(x))),
        { ...axis("запросов в секунду", (u, v) => v.map((x) => fixed(x, 1))), side: 1, scale: "req", grid: { show: false } }],
      // Requests as a rate stay comparable when the bucket changes with the period.
      values: (p) => [p.requests == null || !p.minutes ? null : p.requests / (p.minutes * 60), p.client_errors, p.server_errors] },
  ];
}

function stepLabel() {
  const minutes = capacity.step / 60;
  return minutes < 60 ? minutes + " мин" : minutes / 60 + " ч";
}

// The bucket keeps about one point per two pixels of the chart: denser points
// draw minute noise as a solid sawtooth. Steps divide a UTC day (see the API).
const STEPS = [60, 120, 300, 600, 900, 1800, 3600, 7200, 10800, 21600];
const RANGE_SECONDS = { "24h": 86400, "7d": 604800, "30d": 2592000 };
// The chart box has padding; uPlot takes the width it may draw in.
function chartWidth(root) {
  const style = getComputedStyle(root);
  return Math.max(200, root.clientWidth - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight) || 600);
}

function sizeCharts() {
  for (const chart of capacity.charts) {
    const width = chartWidth(chart.root.parentElement);
    if (Math.abs(chart.width - width) > 1) chart.setSize({ width, height: 210 });
  }
}

function stepFor(range) {
  const width = chartWidth($("chart-online"));
  const wanted = RANGE_SECONDS[range] / Math.max(60, width / 2);
  return STEPS.find((step) => step >= wanted && RANGE_SECONDS[range] / step <= 1500) || STEPS.at(-1);
}

// Charts are built once and fed with setData; rebuilding them on every
// refresh is what costs time in uPlot, not the points.
function renderCharts(data, fleet) {
  const specs = chartSpecs();
  const hostLabels = capacity.chartsHost !== capacity.host;
  if (!capacity.charts.length || hostLabels) {
    for (const chart of capacity.charts) chart.destroy();
    capacity.charts = specs.map((spec) => {
      const root = $(spec.id);
      root.replaceChildren();
      root.setAttribute("role", "img");
      root.setAttribute("aria-label", spec.label + "; значения есть в таблицах ниже и в подсказке при наведении");
      const empty = [[], ...spec.series.map(() => [])];
      return new uPlot(chartOptions(chartWidth(root), spec), empty, root);
    });
    capacity.chartsHost = capacity.host;
  }
  sizeCharts();
  capacity.xMin = data.collecting_since ? Date.parse(data.collecting_since) / 1000 : null;
  specs.forEach((spec, index) => {
    const source = spec.fleet ? fleet : data;
    const xs = source.points.map((p) => Date.parse(p.t) / 1000);
    const columns = spec.series.map(() => []);
    for (const point of source.points) {
      const values = point.status === "missing" ? spec.series.map(() => null) : spec.values(point);
      values.forEach((value, i) => columns[i].push(value ?? null));
    }
    capacity.charts[index].setData([xs, ...columns]);
  });
  $("errors-unit").textContent = "Запросы в секунду; ошибки — штук за " + stepLabel();
}

function renderMachines(data) {
  const body = $("capacity-machines");
  body.replaceChildren();
  const machines = data.machines || [];
  if (!machines.length) {
    const row = document.createElement("tr");
    const cell = text("td", "За период машины не прислали ни одной минуты.", "muted");
    cell.colSpan = 7;
    row.append(cell);
    body.append(row);
    return;
  }
  for (const m of machines) {
    const row = document.createElement("tr");
    row.dataset.host = m.host;
    const [word, tone] = stateWords[m.state] || stateWords.unknown;
    const stateCell = document.createElement("td");
    stateCell.append(text("span", word, "status " + tone));
    row.append(stateCell);
    const name = document.createElement("td");
    const pick = text("button", m.host, "host-button");
    pick.type = "button";
    pick.setAttribute("aria-pressed", String(m.host === capacity.host));
    pick.setAttribute("aria-label", "Показать графики машины " + m.host);
    if (m.host === capacity.host) row.classList.add("selected");
    name.append(pick, text("span", number(m.live_rooms) + " из " + number(m.rooms) + " комнат живы", "sub"));
    row.append(name);
    addCell(row, m.state === "unknown" ? "—" : number(m.seated) + " / " + number(m.seats), "num");
    const cpu = document.createElement("td");
    cpu.className = "num";
    cpu.append(text("span", m.cpu_pct == null ? "—" : fixed(m.cpu_pct, 0) + "%"));
    if (m.cpu_pct != null) {
      const bar = document.createElement("span");
      bar.className = "cpu-bar";
      bar.dataset.tone = cpuWord(m.cpu_pct)[1];
      bar.style.setProperty("--value", Math.min(100, m.cpu_pct) + "%");
      cpu.append(bar);
    }
    row.append(cpu);
    // A machine calm now may still have run past the norm within the period.
    const peakTone = cpuWord(m.cpu_p95)[1];
    addCell(row, (m.cpu_p95 == null ? "—" : fixed(m.cpu_p95, 0) + "%") + " / " + (m.cpu_max == null ? "—" : fixed(m.cpu_max, 0) + "%"),
      peakTone === "bad" ? "num bad-text" : peakTone === "warn" ? "num warn-text" : "num");
    addCell(row, m.tx_mbit_s == null ? "—" : "↑ " + fixed(m.tx_mbit_s, 1) + "  ↓ " + fixed(m.rx_mbit_s, 1), "num");
    addCell(row, utc(m.last_at), "num");
    const choose = () => selectHost(capacity.host === m.host ? "" : m.host);
    row.addEventListener("click", choose);
    body.append(row);
  }
}

function renderTraffic(data) {
  const body = $("capacity-traffic");
  body.replaceChildren();
  const traffic = data.traffic;
  const plan = traffic?.month_allowance_gb || 0;
  $("traffic-plan").textContent = !traffic ? "Источник недоступен" : plan > 0 ? "Тариф " + fixed(plan, 0) + " ГБ на машину в месяц" : "Тариф не задан";
  const hosts = traffic?.hosts || [];
  if (!hosts.length) {
    const row = document.createElement("tr");
    const cell = text("td", traffic ? "Машины еще не прислали счетчики трафика." : "Учет трафика метасервера недоступен.", "muted");
    cell.colSpan = 4;
    row.append(cell);
    body.append(row);
    return;
  }
  for (const h of hosts) {
    const row = document.createElement("tr");
    const name = document.createElement("td");
    name.append(text("strong", h.host), text("span", "учет с " + fixed(h.counted_hours, 0) + " ч этого месяца", "sub"));
    row.append(name);
    addCell(row, "↑ " + fixed(h.today_tx_gb, 2) + "  ↓ " + fixed(h.today_rx_gb, 2), "num");
    addCell(row, "↑ " + fixed(h.month_tx_gb, 2) + "  ↓ " + fixed(h.month_rx_gb, 2), "num");
    const used = h.month_tx_gb + h.month_rx_gb;
    addCell(row, plan > 0 ? fixed(100 * used / plan, 0) + "%" : "—", "num");
    body.append(row);
  }
}

function renderEndpoints(data) {
  const body = $("capacity-endpoints");
  body.replaceChildren();
  const endpoints = data.endpoints || [];
  if (!endpoints.length) {
    const row = document.createElement("tr");
    const cell = text("td", "За период запросов не было или история еще не собрана.", "muted");
    cell.colSpan = 6;
    row.append(cell);
    body.append(row);
    return;
  }
  for (const e of endpoints) {
    const row = document.createElement("tr");
    const name = document.createElement("td");
    name.append(text("strong", routeNames[e.endpoint] || e.endpoint), text("span", e.endpoint, "sub"));
    row.append(name);
    addCell(row, number(e.requests), "num");
    addCell(row, number(e.server_errors), e.server_errors ? "num bad-text" : "num");
    addCell(row, number(e.client_errors), "num");
    addCell(row, number(e.rate_limited), "num");
    addCell(row, fixed(e.avg_ms, 1) + " / " + fixed(e.max_ms, 1), "num");
    body.append(row);
  }
}

// The period and the machine live in the URL, so a view can be opened again from a link.
function rememberView() {
  const query = new URLSearchParams({ page: "capacity", range: capacity.range });
  if (capacity.host) query.set("host", capacity.host);
  history.replaceState(null, "", "/?" + query);
}

function showRange(range) {
  capacity.range = range;
  for (const button of document.querySelectorAll("[data-range]")) button.setAttribute("aria-pressed", String(button.dataset.range === range));
}

function showHost(host) {
  capacity.host = host;
  const chip = $("capacity-host");
  chip.hidden = !host;
  chip.replaceChildren();
  if (host) {
    chip.append(text("span", "Графики машины " + host));
    const clear = text("button", "Все машины", "text-button");
    clear.type = "button";
    clear.addEventListener("click", () => selectHost(""));
    chip.append(clear);
  }
}

function selectHost(host) {
  showHost(host);
  rememberView();
  reloadCapacity();
}

// Every load reads the view it started with; only the newest load renders, so
// a click during a background refresh is never answered with stale data.
async function loadCapacity(quiet = false) {
  const seq = ++capacity.seq;
  const view = { range: capacity.range, host: capacity.host, step: stepFor(capacity.range) };
  if (!quiet) setStatus("Загружаю историю мощности…");
  const query = new URLSearchParams({ range: view.range, step: String(view.step) });
  const fleet = await api("/admin-api/v1/capacity?" + query);
  let data = fleet;
  if (view.host) {
    query.set("host", view.host);
    const one = await api("/admin-api/v1/capacity?" + query);
    data = { ...fleet, points: one.points, host: view.host };
  }
  if (seq !== capacity.seq || state.page !== "capacity") return;
  capacity.step = view.step;
  capacity.data = data;
  setStatus(data.traffic_error ? "Учет трафика метасервера недоступен; остальное показано." : "",
    data.traffic_error ? "warning" : "");
  $("capacity-collecting").textContent = data.collecting_since ?
    "История с " + utc(data.collecting_since) + "; раньше данных нет" : "История еще не собрана";
  renderState(fleet);
  renderCharts(data, fleet);
  renderMachines(fleet);
  renderTraffic(fleet);
  renderEndpoints(fleet);
  const fresh = fleet.machines?.some((m) => m.state !== "unknown");
  setSource(fresh ? "Последняя минута " + utc(fleet.last_minute) : "Свежих минут нет", fresh ? "fresh" : "unavailable");
}

function reloadCapacity() {
  loadCapacity().catch((error) => setStatus("История мощности недоступна: " + error.message + ".", "error"));
}

document.addEventListener("DOMContentLoaded", () => {
  for (const button of document.querySelectorAll("[data-range]")) {
    button.addEventListener("click", () => {
      showRange(button.dataset.range);
      rememberView();
      reloadCapacity();
    });
  }
  const params = new URLSearchParams(location.search);
  if (["24h", "7d", "30d"].includes(params.get("range"))) showRange(params.get("range"));
  if (params.get("host")) showHost(params.get("host").slice(0, 253));
  let resizeTimer = 0;
  addEventListener("resize", () => {
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => {
      // A hidden page measures 0 wide; its charts are sized when it renders again.
      if (state.page !== "capacity") return;
      sizeCharts();
      // A new width may want another bucket.
      if (stepFor(capacity.range) !== capacity.step) loadCapacity(true).catch(() => {});
    }, 120);
  });
  // The history gains a point a minute; a hidden tab or another page asks nothing.
  setInterval(() => {
    if (state.page !== "capacity" || document.hidden) return;
    const seq = capacity.seq + 1;
    loadCapacity(true).catch(() => {
      if (state.page === "capacity" && seq === capacity.seq) setSource("Источник недоступен", "unavailable");
    });
  }, 30_000);
});
