// The overview answers "is everything fine" on one screen: the fleet against
// the norms, today's online against yesterday's, and the rooms worth a look.
// Shares app.js and capacity.js globals.

const overview = { chart: null, seq: 0, history: null, historyAt: 0 };
// A room ticks 30 times a second; a pass longer than the tick is a late tick.
const TICK_BUDGET_US = 33_333;

// Online by UTC hour: the last 24 hours and the 24 before them, from the
// hourly capacity history, so both lines share one hour axis.
function onlineByHour(points) {
  const hours = points.slice(-48);
  const today = hours.slice(-24), yesterday = hours.slice(0, Math.max(0, hours.length - 24));
  const value = (p) => p && p.status !== "missing" ? p.seated_max : null;
  const shift = today.length - yesterday.length;
  return [today.map((p) => Date.parse(p.t) / 1000), today.map(value), today.map((p, i) => value(yesterday[i - shift]))];
}

function renderOnlineChart(data) {
  const root = $("overview-online");
  const series = onlineByHour(data.points);
  if (!overview.chart) {
    root.replaceChildren();
    root.setAttribute("role", "img");
    root.setAttribute("aria-label", "Онлайн по часам за сутки против предыдущих суток");
    const accent = css("--accent"), muted = css("--muted");
    overview.chart = new uPlot(chartOptions(chartWidth(root), {
      series: [line("Сегодня, пик за час", accent, "", { fill: "rgba(233,173,92,.12)" }),
        line("Сутки назад", muted, "", { dash: [4, 4], width: 1.5 })],
      axes: [axis("игроки", (u, v) => v.map((x) => number(x)))],
      scales: { y: { range: upTo(10, 1.15) } },
    }), [[], [], []], root);
  } else if (Math.abs(overview.chart.width - chartWidth(root)) > 1) {
    overview.chart.setSize({ width: chartWidth(root), height: 210 });
  }
  // Shared with the capacity charts: the axis starts where collection began.
  capacity.xMin = data.collecting_since ? Date.parse(data.collecting_since) / 1000 : null;
  overview.chart.setData(series);
}

// Rooms without a status first, then the longest passes against the tick budget.
function renderAttentionRooms(snapshot) {
  const body = $("overview-rooms");
  body.replaceChildren();
  const rooms = snapshot.rooms || [];
  const dead = rooms.filter((room) => !room.alive).length;
  const slow = rooms.filter((room) => room.alive && room.pass_max_us > TICK_BUDGET_US).length;
  $("overview-rooms-note").textContent = snapshot.rooms == null ? "Реестр недоступен" :
    !rooms.length ? "Комнат нет" : dead || slow ?
      "Без связи " + number(dead) + " · с опозданием тика " + number(slow) + " из " + number(rooms.length) :
      "Все " + number(rooms.length) + " комнат живы и укладываются в тик";
  if (snapshot.rooms_retired) $("overview-rooms-note").textContent += " · выведено из работы " + number(snapshot.rooms_retired);
  const ordered = [...rooms].sort((a, b) => Number(a.alive) - Number(b.alive) || (b.pass_max_us ?? 0) - (a.pass_max_us ?? 0)).slice(0, 6);
  if (!ordered.length) {
    emptyRow(body, 5, snapshot.rooms == null ? "Реестр комнат недоступен." : "Комнат пока нет.");
    return;
  }
  for (const room of ordered) {
    const row = document.createElement("tr");
    const name = document.createElement("td");
    name.append(text("strong", "#" + room.id), text("span", room.host || "Хост не указан", "sub"));
    row.append(name);
    const [label, tone] = roomStatus(room);
    const status = document.createElement("td");
    status.append(text("span", label, "status " + tone));
    row.append(status);
    addCell(row, room.alive ? number(room.seated ?? 0) + " / " + number(room.capacity ?? 0) : "—", "num");
    const share = room.pass_max_us == null ? null : 100 * room.pass_max_us / TICK_BUDGET_US;
    addCell(row, share == null ? "—" : fixed(room.pass_max_us / 1000, 1) + " мс · " + fixed(share, 0) + "% тика",
      share > 100 ? "num bad-text" : share > 80 ? "num warn-text" : "num");
    addCell(row, room.never_reported || room.age_s == null ? "—" : Math.round(room.age_s) + " с назад", "num");
    body.append(row);
  }
}

async function loadOverview(quiet = false) {
  const seq = ++overview.seq;
  if (!quiet) setStatus("Загружаю состояние сервера…");
  const hourly = Date.now() - overview.historyAt > 300_000 ?
    api("/admin-api/v1/capacity?range=7d&step=3600").catch(() => null) : Promise.resolve(overview.history);
  const [snapshot, day, week] = await Promise.all([api("/admin-api/v1/overview"),
    api("/admin-api/v1/capacity?range=24h&step=3600").catch(() => null), hourly]);
  // Cached even when a newer load has started: it is the same history.
  if (week && week !== overview.history) { overview.history = week; overview.historyAt = Date.now(); }
  // A newer load, or another page, owns the status line now.
  if (seq !== overview.seq || state.page !== "overview") return;
  const history = day;
  setStatus(history ? "" : "История мощности недоступна; показан только живой снимок.", history ? "" : "warning");
  updateFreshness(snapshot);
  if (history) {
    renderState(history, "overview-state");
    if (overview.history) renderOnlineChart(overview.history);
    else $("overview-online").replaceChildren(text("div", "История по часам недоступна.", "empty"));
  } else {
    $("overview-state").replaceChildren();
  }
  renderAttentionRooms(snapshot);
  facts("overview-sources", [["Метасервер", utc(snapshot.now)], ["Запущен", utc(snapshot.since)],
    ["Реестр комнат", snapshot.rooms == null ? "Недоступен" : "Доступен"],
    ["Аккаунты игроков", snapshot.players == null ? "База недоступна" : number(snapshot.players.accounts)],
    ["История мощности", history?.collecting_since ? "с " + utc(history.collecting_since) : "Нет"]]);
}

document.addEventListener("DOMContentLoaded", () => {
  addEventListener("resize", () => {
    if (state.page === "overview" && overview.chart) {
      overview.chart.setSize({ width: chartWidth($("overview-online")), height: 210 });
    }
  });
  // Live state changes by the second; a hidden tab or another page asks nothing.
  setInterval(() => {
    if (state.page !== "overview" || document.hidden) return;
    const seq = overview.seq + 1;
    // A failed refresh must not leave old numbers marked fresh.
    loadOverview(true).catch(() => {
      if (state.page === "overview" && seq === overview.seq) setSource("Источник недоступен", "unavailable");
    });
  }, 15_000);
});
