// The rooms page: the fleet's ticks over the last minute and the day, every
// machine, every room against the tick budget, and one room in full.
// Shares app.js and capacity.js globals.

const roomsView = { seq: 0, roomSeq: 0, rooms: [], plans: [], retired: 0, hosts: "", perf: null, sort: "problem", desc: true, room: null, chart: null, chartAt: 0 };
const TICK_US = 33_333;

const usToMs = (us) => us == null ? "—" : fixed(us / 1000, 1) + " мс";
const tickTone = (us) => us == null ? "" : us > TICK_US ? "bad" : us > 0.8 * TICK_US ? "warn" : "good";

// A room is worth a look when it is silent, ticks late or skips, nears the
// tick budget, or drops players for reasons other than leaving.
// Against its machine's plan: a planned room that is silent, or never came
// up, is an alarm; a room answering outside the plan is worth a look.
function roomProblem(room) {
  if (!room.alive) return 4;
  const p = room.perf;
  if (!p) return 3;
  if (p.dropped_ticks > 0 || p.step_p99_us > TICK_US) return 2;
  if (p.late_ticks > 0 || p.step_p99_us > 0.8 * TICK_US || p.abnormal_closes > 0 || (room.has_plan && !room.planned)) return 1;
  return 0;
}

function roomWord(room) {
  if (room.never_reported) return ["Не запускалась", "bad"];
  if (!room.alive) return ["Нет связи", "bad"];
  if (room.has_plan && !room.planned && roomProblem(room) < 2) return ["Вне плана", "warn"];
  return [["Норма", "good"], ["Внимание", "warn"], ["Не успевает", "bad"], ["Нет замеров", "warn"]][roomProblem(room)];
}

function renderFleetTick(perf) {
  const root = $("rooms-fleet");
  root.replaceChildren();
  const f = perf?.fleet;
  if (!f) {
    root.append(text("div", "За последнюю минуту комнаты не прислали замеров.", "empty"));
    return;
  }
  const step = f.stages?.step?.p99_us;
  const late = f.stages?.late?.p99_us;
  stateCard(root, "CPU на игрока", ["", ""], f.cpu_ms_per_player_s == null ? "—" : fixed(f.cpu_ms_per_player_s, 1) + " мс",
    "Миллисекунды CPU комнат на секунду игры одного игрока",
    "В среднем " + fixed(f.avg_seated, 1) + " игроков; при малом онлайне цифру раздувают пустые комнаты");
  stateCard(root, "Шаг тика, p99", step == null ? stateWords.unknown : step > TICK_US ? ["Не успевает", "bad"] :
    step > 0.8 * TICK_US ? stateWords.warn : stateWords.ok, usToMs(step),
    "Бюджет тика 33 мс (30 тиков в секунду)", "Репликация p99 " + usToMs(f.stages?.replicate?.p99_us));
  stateCard(root, "Опоздавшие тики", f.dropped_ticks ? ["Пропуски", "bad"] : f.late_ticks ? stateWords.warn : stateWords.ok,
    number(f.late_ticks), "Норма: 0 опоздавших и 0 пропущенных",
    "Пропущено " + number(f.dropped_ticks) + " · опоздание p99 " + usToMs(late));
  stateCard(root, "Исходящий трафик на игрока", ["", ""], fixed(f.tx_kbit_s_per_player, 1) + " кбит/с",
    "Сколько комната отправляет одному игроку", "Входящих сообщений " + fixed(f.rx_msg_per_player_s, 1) + " в секунду на игрока");
}

// The day's ticks from the per-minute history: stage p99 against the budget,
// and CPU per player-second on its own axis.
async function renderTickChart() {
  const root = $("rooms-tick-chart");
  if (roomsView.chart) sizeOne(roomsView.chart, root);
  if (roomsView.chart && Date.now() - roomsView.chartAt < 60_000) return;
  roomsView.chartAt = Date.now();
  const step = stepFor("24h");
  let data;
  try {
    data = await api("/admin-api/v1/capacity?" + new URLSearchParams({ range: "24h", step: String(step) }));
  } catch (error) {
    roomsView.chartAt = 0;
    throw error;
  }
  if (state.page !== "rooms") { roomsView.chartAt = 0; return; }
  const xs = data.points.map((p) => Date.parse(p.t) / 1000);
  const pick = (name, scale = 1) => data.points.map((p) => p.status === "missing" || p[name] == null ? null : p[name] / scale);
  if (!roomsView.chart) {
    root.replaceChildren();
    root.setAttribute("role", "img");
    root.setAttribute("aria-label", "График тика флота за сутки: p99 стадий против бюджета 33 мс и CPU на игрока");
    const accent = css("--accent"), warn = css("--warn"), bad = css("--bad"), muted = css("--muted");
    roomsView.chart = new uPlot(chartOptions(chartWidth(root), {
      series: [line("Шаг p99", accent, "мс"), line("Репликация p99", warn, "мс"), line("Опоздание старта p99", bad, "мс"),
        line("CPU на игрока", muted, "мс", { scale: "cpu", dash: [4, 4], width: 1.5 })],
      axes: [axis("мс, p99 за точку", (u, v) => v.map((x) => fixed(x, 0))),
        { ...axis("мс CPU на игрока·с", (u, v) => v.map((x) => fixed(x, 1))), side: 1, scale: "cpu", grid: { show: false } }],
      scales: { y: { range: upTo(40, 1.15) }, cpu: { range: upTo(1, 1.15) } },
      plugins: [normLines([[TICK_US / 1000, "rgba(239,147,142,.55)"]])],
    }), [[], [], [], [], []], root);
  } else {
    sizeOne(roomsView.chart, root);
  }
  capacity.xMin = data.collecting_since ? Date.parse(data.collecting_since) / 1000 : null;
  roomsView.chart.setData([xs, pick("step_p99_us", 1000), pick("replicate_p99_us", 1000), pick("late_p99_us", 1000),
    pick("cpu_ms_per_player_s")]);
}

function sizeOne(chart, root) {
  const width = chartWidth(root);
  if (Math.abs(chart.width - width) > 1) chart.setSize({ width, height: 210 });
}

// Every machine that measured itself or has a plan: a planned machine without
// a window in the last minute is down, with all its rooms.
function renderMachinesLive(perf) {
  const body = $("rooms-machines");
  body.replaceChildren();
  const measured = new Map((perf?.machines || []).map((m) => [m.host, m]));
  const plans = new Map(roomsView.plans.map((p) => [p.host, p]));
  const hosts = [...new Set([...measured.keys(), ...plans.keys()])].sort();
  if (!hosts.length) {
    emptyRow(body, 8, "Машины не прислали замеров за последнюю минуту.");
    return;
  }
  for (const host of hosts) {
    const m = measured.get(host), plan = plans.get(host);
    const row = document.createElement("tr");
    const name = document.createElement("td");
    name.append(text("strong", host), text("span", m ? number(m.rooms) + " комнат · " + number(m.seated) + " игроков" :
      "Замеров нет", "sub"));
    row.append(name);
    const planned = document.createElement("td");
    planned.className = "num";
    if (plan) {
      planned.append(text("span", number(plan.live) + " из " + number(plan.rooms) + " "),
        text("span", plan.missing ? "Не хватает " + number(plan.missing) : "Все живы", "status " + (plan.missing ? "bad" : "good")));
    } else {
      planned.append(text("span", "План не сообщен", "muted"));
    }
    row.append(planned);
    if (!m) {
      const down = text("td", "Машина не прислала замеров за последнюю минуту", "bad-text");
      down.colSpan = 6;
      row.append(down);
      body.append(row);
      continue;
    }
    const [word, tone] = cpuWord(m.cpu_pct);
    const cpu = document.createElement("td");
    cpu.className = "num";
    cpu.append(text("span", fixed(m.cpu_pct, 0) + "% "), text("span", word, "status " + tone));
    row.append(cpu);
    addCell(row, fixed(m.user_pct, 0) + " / " + fixed(m.system_pct, 0) + " / " + fixed(m.softirq_pct, 0) + "%", "num");
    addCell(row, fixed(m.steal_pct, 1) + "%", m.steal_pct > 5 ? "num warn-text" : "num");
    addCell(row, "↑ " + fixed(m.tx_mbit_s, 1) + "  ↓ " + fixed(m.rx_mbit_s, 1), "num");
    addCell(row, "↑ " + number(Math.round(m.tx_pps)) + "  ↓ " + number(Math.round(m.rx_pps)), "num");
    addCell(row, Math.round(m.age_s) + " с назад", "num");
    body.append(row);
  }
}

const roomColumns = [
  { key: "problem", label: "Состояние", value: (r) => roomProblem(r) },
  { key: "id", label: "Комната", value: (r) => r.id },
  { key: "seated", label: "Онлайн / места", value: (r) => r.alive ? r.seated : -1 },
  { key: "cpu", label: "CPU ядра", value: (r) => r.perf?.cpu_core_pct ?? -1 },
  { key: "cpu_player", label: "CPU на игрока", value: (r) => r.perf?.cpu_ms_per_player_s ?? -1 },
  { key: "step", label: "Шаг p99", value: (r) => r.perf?.step_p99_us ?? -1 },
  { key: "late", label: "Опоздавшие / пропущенные", value: (r) => r.perf ? [r.perf.dropped_ticks, r.perf.late_ticks] : [-1, -1] },
  { key: "tx", label: "Исх. на игрока", value: (r) => r.perf?.tx_kbit_s_per_player ?? -1 },
  { key: "rtt", label: "RTT игроков p99", value: (r) => r.perf?.rtt_p99_ms ?? -1 },
  { key: "closes", label: "Обрывы", value: (r) => r.perf?.abnormal_closes ?? -1 },
  { key: "age", label: "Статус получен", value: (r) => r.never_reported ? 1e9 : r.age_s ?? 1e9 },
];

// Numbers compare as numbers; [major, minor] tuples compare in order.
function compareValues(a, b) {
  if (Array.isArray(a)) return a[0] - b[0] || a[1] - b[1];
  return a - b;
}

function renderRoomHeader() {
  const head = $("rooms-head");
  head.replaceChildren();
  for (const column of roomColumns) {
    const th = document.createElement("th");
    th.scope = "col";
    const button = text("button", column.label + (roomsView.sort === column.key ? (roomsView.desc ? " ↓" : " ↑") : ""), "sort-button");
    button.type = "button";
    th.setAttribute("aria-sort", roomsView.sort === column.key ? (roomsView.desc ? "descending" : "ascending") : "none");
    button.addEventListener("click", () => {
      roomsView.desc = roomsView.sort === column.key ? !roomsView.desc : column.key !== "id";
      roomsView.sort = column.key;
      renderRoomTable();
    });
    th.append(button);
    head.append(th);
  }
}

function renderRoomTable() {
  renderRoomHeader();
  const body = $("rooms-list");
  body.replaceChildren();
  const host = $("rooms-host").value, onlyProblems = $("rooms-problems").checked;
  const query = $("rooms-query").value.trim().replace(/^#/, "");
  const column = roomColumns.find((c) => c.key === roomsView.sort) || roomColumns[0];
  const rows = roomsView.rooms
    .filter((r) => (!host || r.host === host) && (!onlyProblems || roomProblem(r) > 0) && (!query || String(r.id).startsWith(query)))
    .sort((a, b) => compareValues(column.value(a), column.value(b)) * (roomsView.desc ? -1 : 1) || a.id - b.id);
  const problems = roomsView.rooms.filter((r) => roomProblem(r) > 0).length;
  $("rooms-count").textContent = "Показано " + number(rows.length) + " из " + number(roomsView.rooms.length) +
    " · требуют внимания " + number(problems) + (roomsView.retired ? " · выведено из работы " + number(roomsView.retired) : "");
  if (!rows.length) {
    emptyRow(body, roomColumns.length, roomsView.rooms.length ? "Под фильтр не попала ни одна комната." : "Комнат нет или их реестр недоступен.");
    return;
  }
  for (const room of rows) {
    const p = room.perf;
    const row = document.createElement("tr");
    if (room.id === roomsView.room) row.classList.add("selected");
    const [word, tone] = roomWord(room);
    const state = document.createElement("td");
    state.append(text("span", word, "status " + tone));
    row.append(state);
    const name = document.createElement("td");
    const open = text("button", "#" + room.id, "host-button");
    open.type = "button";
    open.setAttribute("aria-pressed", String(room.id === roomsView.room));
    open.setAttribute("aria-label", "Открыть комнату " + room.id);
    name.append(open, text("span", room.host || "Хост не указан", "sub"));
    row.append(name);
    addCell(row, room.alive ? number(room.seated) + " / " + number(room.capacity) : "—", "num");
    addCell(row, p?.cpu_core_pct == null ? "—" : fixed(p.cpu_core_pct, 0) + "%", "num");
    addCell(row, p?.cpu_ms_per_player_s == null ? "—" : fixed(p.cpu_ms_per_player_s, 1) + " мс", "num");
    addCell(row, p ? usToMs(p.step_p99_us) : "—", "num " + ({ bad: "bad-text", warn: "warn-text" }[tickTone(p?.step_p99_us)] || ""));
    addCell(row, p ? number(p.late_ticks) + " / " + number(p.dropped_ticks) : "—",
      p?.dropped_ticks ? "num bad-text" : p?.late_ticks ? "num warn-text" : "num");
    addCell(row, p ? fixed(p.tx_kbit_s_per_player, 1) + " кбит/с" : "—", "num");
    addCell(row, p?.rtt_p99_ms == null ? "—" : fixed(p.rtt_p99_ms, 0) + " мс", "num");
    addCell(row, p ? number(p.abnormal_closes) : "—", p?.abnormal_closes ? "num warn-text" : "num");
    addCell(row, room.never_reported || room.age_s == null ? "—" : Math.round(room.age_s) + " с назад", "num");
    row.addEventListener("click", () => openRoom(room.id));
    body.append(row);
  }
}

function factGroup(title, entries) {
  const section = document.createElement("section");
  section.className = "fact-group";
  section.append(text("h3", title));
  const list = document.createElement("dl");
  list.className = "fact-list";
  for (const [label, value, hint] of entries) {
    const dt = text("dt", label);
    if (hint) dt.title = hint;
    list.append(dt, text("dd", value));
  }
  section.append(list);
  return section;
}

function stageTable(stages) {
  const names = { step: "Шаг симуляции", replicate: "Репликация", late: "Опоздание старта тика",
    input_wait: "Ожидание ввода", meta: "Обмен с метасервером" };
  const order = Object.keys(names);
  const rank = (name) => order.includes(name) ? order.indexOf(name) : order.length;
  const table = document.createElement("table");
  const head = document.createElement("tr");
  for (const h of ["Стадия", "p50", "p95", "p99", "Макс."]) head.append(text("th", h));
  const thead = document.createElement("thead");
  thead.append(head);
  const body = document.createElement("tbody");
  for (const [name, s] of Object.entries(stages || {}).sort(([a], [b]) => rank(a) - rank(b) || a.localeCompare(b))) {
    const row = document.createElement("tr");
    row.append(text("td", names[name] || name));
    for (const v of [s.p50_us, s.p95_us, s.p99_us, s.max_us]) addCell(row, usToMs(v), "num");
    body.append(row);
  }
  table.append(thead, body);
  return table;
}

// A refresh (quiet) keeps the card in place and keeps it on a failed read;
// only the newest request for the open room may draw.
async function openRoom(id, quiet = false) {
  const seq = ++roomsView.roomSeq;
  roomsView.room = id;
  const root = $("room-detail");
  if (!quiet) {
    renderRoomTable();
    $("room-detail-panel").hidden = false;
    $("room-detail-title").textContent = "Комната #" + id;
    root.replaceChildren(text("div", "Загружаю замеры комнаты…", "empty"));
  }
  let data;
  try {
    data = await api("/admin-api/v1/rooms/" + encodeURIComponent(id));
  } catch (error) {
    if (seq !== roomsView.roomSeq || quiet) return;
    root.replaceChildren(text("div", error.status === 404 ? "Комната не присылала замеров последнюю минуту." :
      "Замеры комнаты недоступны.", "empty"));
    return;
  }
  if (seq !== roomsView.roomSeq || roomsView.room !== id || !data?.room) return;
  const r = data.room, c = r.clients, i = r.input || {};
  const pct = (v, d = 0) => v == null ? "—" : fixed(v, d) + "%";
  root.replaceChildren();
  const tick = document.createElement("section");
  tick.className = "fact-group wide";
  tick.append(text("h3", "Тик за последнюю минуту (" + number(r.windows) + " окон, " + fixed(r.window_s, 0) + " с)"), stageTable(r.stages));
  root.append(tick, factGroup("Нагрузка", [
    ["Игроков в среднем", fixed(r.avg_seated, 1)],
    ["Тиков в секунду", fixed(r.ticks_per_s, 1), "Норма 30"],
    ["Опоздавшие / пропущенные тики", number(r.late_ticks) + " / " + number(r.dropped_ticks)],
    ["CPU процесса, % ядра", pct(r.cpu_core_pct), "user + system"],
    ["Из них system", pct(r.system_core_pct)],
    ["CPU на игрока·с", r.cpu_ms_per_player_s == null ? "—" : fixed(r.cpu_ms_per_player_s, 2) + " мс"],
    ["Работа тика, % ядра", pct(r.tick_work_core_pct, 1), "Шаг и репликация по настенному времени"],
    ["Память процесса", r.rss_mb == null ? "—" : fixed(r.rss_mb, 0) + " МБ"],
    ["Пробуждений цикла в секунду", fixed(r.pumps_per_s, 0)],
  ]), factGroup("Сеть", [
    ["Входящие сообщения в секунду", fixed(r.rx_msg_per_s, 0) + " · на игрока " + fixed(r.rx_msg_per_player_s, 1)],
    ["Исходящие сообщения в секунду", fixed(r.tx_msg_per_s, 0)],
    ["Записей в сокет на сообщение", fixed(r.writes_per_msg, 2)],
    ["Трафик вх. / исх.", fixed(r.rx_kbit_s, 0) + " / " + fixed(r.tx_kbit_s, 0) + " кбит/с"],
    ["Исходящий на игрока", fixed(r.tx_kbit_s_per_player, 1) + " кбит/с"],
    ["Пакетов в секунду · байт в пакете", fixed(r.packets_per_s, 0) + " · " + fixed(r.bytes_per_packet, 0)],
    ["Доля строк состояния в байтах", pct(r.row_bytes_pct, 1)],
    ["Строк отложено", pct(r.rows_deferred_pct, 1), "Строки, не уместившиеся в пакет, ждут следующего"],
    ["Пропущено пакетов (сокет не готов)", number(r.skipped)],
    ["Частичных / отклоненных записей", number(r.tx_partial) + " / " + number(r.tx_refused)],
    ["Пик очереди отправки", fixed(r.queue_peak_bytes / 1024, 0) + " КБ"],
  ]), factGroup("Ввод игроков", [
    ["Недоборы / переборы", number(i.underruns) + " / " + number(i.overruns), "Тик без ввода игрока / лишний ввод"],
    ["Обрезано", number(i.trimmed)],
    ["Битых сообщений", number(i.malformed)],
    ["Потерянных нажатий", number(i.lost_edges)],
    ["Пик очереди ввода", number(i.queue_peak)],
  ]), factGroup("Как играется у игроков", c ? [
    ["Отчетов клиентов", number(c.reports)],
    ["RTT p50 / p95 / p99", fixed(c.rtt_p50_ms, 0) + " / " + fixed(c.rtt_p95_ms, 0) + " / " + fixed(c.rtt_p99_ms, 0) + " мс"],
    ["RTT макс.", number(c.rtt_max_ms) + " мс"],
    ["Коррекций в минуту", fixed(c.corrections_per_min, 1), "Сервер поправил предсказание клиента"],
    ["Рывков / зависаний в минуту", fixed(c.snaps_per_min, 1) + " / " + fixed(c.stalls_per_min, 1)],
    ["FPS сред. · ниже 30", fixed(c.fps_avg, 0) + " · " + pct(c.low_fps_pct)],
    ["Переподключений", number(c.reconnects)],
    ["Браузер / тач / бенч", pct(c.web_pct) + " / " + pct(c.touch_pct) + " / " + pct(c.bench_pct)],
  ] : [["Отчетов клиентов", "Нет за последнюю минуту"]]), factGroup("Закрытия подключений", Object.entries(r.closes || {}).map(([reason, count]) =>
    [{ peer: "Игрок ушел", protocol: "Ошибка протокола", rate: "Превышение частоты", slow: "Медленный клиент", app: "Решение игры", other: "Другое" }[reason] || reason,
      number(count)])));
}

async function loadRoomsPage(quiet = false) {
  const seq = ++roomsView.seq;
  if (!quiet) setStatus("Загружаю комнаты…");
  const [registry, perf] = await Promise.all([api("/admin-api/v1/rooms"), api("/admin-api/v1/performance").catch(() => null)]);
  if (seq !== roomsView.seq || state.page !== "rooms") return;
  const byId = new Map((perf?.rooms || []).map((p) => [p.id, p]));
  roomsView.rooms = (registry.rooms || []).map((room) => ({ ...room, perf: byId.get(room.id) || null }));
  roomsView.plans = registry.plans || [];
  roomsView.retired = registry.retired || 0;
  roomsView.perf = perf;
  const select = $("rooms-host"), chosen = select.value;
  const hosts = [...new Set(roomsView.rooms.map((r) => r.host).filter(Boolean))].sort();
  // Rebuilding closes an open list; only a changed fleet rebuilds it.
  if (hosts.join(" ") !== roomsView.hosts) {
    roomsView.hosts = hosts.join(" ");
    select.replaceChildren(text("option", "Все машины"));
    select.options[0].value = "";
    for (const host of hosts) {
      const option = text("option", host);
      option.value = host;
      select.append(option);
    }
    select.value = hosts.includes(chosen) ? chosen : "";
  }
  setStatus(registry.source_error ? "Реестр комнат частично недоступен." : perf ? "" : "Замеры комнат недоступны; показан только реестр.",
    registry.source_error || !perf ? "warning" : "");
  setSource("Обновлено " + utc(registry.updated_at), registry.source_error ? "unavailable" : "fresh");
  renderFleetTick(perf);
  renderMachinesLive(perf);
  renderRoomTable();
  renderTickChart().catch(() => {});
}

document.addEventListener("DOMContentLoaded", () => {
  for (const id of ["rooms-host", "rooms-problems"]) $(id).addEventListener("change", renderRoomTable);
  $("rooms-query").addEventListener("input", renderRoomTable);
  $("room-detail-close").addEventListener("click", () => {
    roomsView.room = null;
    $("room-detail-panel").hidden = true;
    renderRoomTable();
  });
  addEventListener("resize", () => {
    if (state.page === "rooms" && roomsView.chart) sizeOne(roomsView.chart, $("rooms-tick-chart"));
  });
  setInterval(() => {
    if (state.page !== "rooms" || document.hidden) return;
    const seq = roomsView.seq + 1;
    loadRoomsPage(true).catch(() => {
      if (state.page === "rooms" && seq === roomsView.seq) setSource("Источник недоступен", "unavailable");
    });
    if (roomsView.room != null) openRoom(roomsView.room, true).catch(() => {});
  }, 15_000);
});
