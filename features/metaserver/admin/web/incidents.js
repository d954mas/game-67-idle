// The incidents page: what went wrong in the metaserver's 24-hour window.
// Shares app.js and capacity.js globals.

const incidents = { seq: 0 };
const levelWords = { ERROR: ["Ошибка", "bad"], WARN: ["Предупреждение", "warn"], INFO: ["Сведение", ""] };
// The heartbeat fails under 14 days (certs.go); a week earlier is worth a look.
const CERT_WARN_DAYS = 21;
const CERT_ALARM_DAYS = 14;

// An unreachable host leaves the expiry unset (year 1): no days to count.
function daysLeft(cert, now) {
  const expires = Date.parse(cert.expires);
  return cert.unreachable || !(expires > 0) ? null : (expires - Date.parse(now)) / 86_400_000;
}

function certState(cert, now) {
  if (cert.invalid) return ["Недействителен", "bad"];
  if (cert.unreachable) return ["Не проверен", "warn"];
  const days = daysLeft(cert, now);
  if (days < CERT_ALARM_DAYS) return ["Истекает", "bad"];
  if (days < CERT_WARN_DAYS) return ["Скоро продлить", "warn"];
  return ["Действует", "good"];
}

function renderIncidentState(data) {
  const root = $("incidents-state");
  root.replaceChildren();
  const firing = data.alerts.filter((a) => a.firing).length;
  const holding = data.alerts.length - firing;
  stateCard(root, "Алерты", firing ? ["Сработал", "bad"] : holding ? stateWords.warn : stateWords.ok, number(firing),
    "Срабатывает, если условие держится 3 минуты", holding ? "Еще " + number(holding) + " ждут подтверждения" : "Ожидающих нет");
  const endpoints = Object.values(data.observability?.endpoints || {});
  const server = endpoints.reduce((sum, e) => sum + e.server_errors, 0);
  const requests = endpoints.reduce((sum, e) => sum + e.requests, 0);
  stateCard(root, "Ошибки сервера за 24 ч", server ? stateWords.warn : stateWords.ok, number(server),
    "Норма: 0 ответов 5xx", number(requests) + " запросов к метасерверу");
  const db = data.db;
  stateCard(root, "Ошибки базы за 24 ч", !db ? stateWords.unknown : db.errors ? stateWords.warn : stateWords.ok,
    db ? number(db.errors) : "—", "Норма: 0 ошибок запросов",
    db ? number(db.queries) + " запросов · сред. " + fixed(db.avg_ms, 1) + " мс" : "Нет данных");
  const certs = data.certificates;
  const dated = certs.filter((c) => daysLeft(c, data.as_of) != null);
  const soonest = dated.reduce((best, c) => !best || Date.parse(c.expires) < Date.parse(best.expires) ? c : best, null);
  const worst = certs.map((c) => certState(c, data.as_of)[1]);
  const certWord = !certs.length ? stateWords.unknown : worst.includes("bad") ? ["Продлить", "bad"] : worst.includes("warn") ? stateWords.warn : stateWords.ok;
  stateCard(root, "Сертификаты комнат", certWord, soonest ? fixed(daysLeft(soonest, data.as_of), 0) + " дн." : "—",
    "Продление за " + CERT_ALARM_DAYS + " дней до конца", soonest ? "Ближайший: " + soonest.host : "Комнаты не проверялись");
}

function renderAlerts(data) {
  const body = $("incidents-alerts");
  body.replaceChildren();
  if (!data.alerts.length) {
    emptyRow(body, 3, "Активных алертов нет.");
    return;
  }
  for (const alert of [...data.alerts].sort((a, b) => Number(b.firing) - Number(a.firing))) {
    const row = document.createElement("tr");
    const status = document.createElement("td");
    status.append(text("span", alert.firing ? "Сработал" : "Ждет 3 минуты", "status " + (alert.firing ? "bad" : "warn")));
    row.append(status);
    const what = document.createElement("td");
    what.className = "wrap";
    what.append(text("span", alert.text), text("span", alert.kind, "sub"));
    row.append(what);
    addCell(row, utc(alert.since), "num");
    body.append(row);
  }
}

function levelCell(level) {
  const [word, tone] = levelWords[level] || [level, ""];
  const cell = document.createElement("td");
  cell.append(text("span", word, "status " + tone));
  return cell;
}

// The detail of a group can be long; it opens on demand and keeps its state across refreshes.
function renderGroups(data) {
  const body = $("incidents-groups");
  const open = new Set([...body.querySelectorAll("details[open]")].map((d) => d.dataset.key));
  body.replaceChildren();
  const groups = data.observability?.groups || [];
  if (!groups.length) {
    emptyRow(body, 5, "За 24 часа ошибок и предупреждений не было.");
    return;
  }
  for (const group of groups) {
    const row = document.createElement("tr");
    const what = document.createElement("td");
    what.className = "wrap";
    const details = document.createElement("details");
    // The group is its level and message; its detail is the latest occurrence's.
    details.dataset.key = group.level + "|" + group.message;
    details.open = open.has(details.dataset.key);
    details.append(text("summary", group.message), text("pre", group.detail || "Подробностей нет"));
    what.append(details);
    row.append(what, levelCell(group.level));
    addCell(row, number(group.count), "num");
    addCell(row, utc(group.first_at), "num");
    addCell(row, utc(group.last_at), "num");
    body.append(row);
  }
}

function renderEvents(data) {
  const body = $("incidents-events");
  body.replaceChildren();
  const events = data.observability?.events || [];
  if (!events.length) {
    emptyRow(body, 3, "Событий за 24 часа нет.");
    return;
  }
  for (const event of events.slice(0, 50)) {
    const row = document.createElement("tr");
    addCell(row, utc(event.at), "num");
    row.append(levelCell(event.level));
    const what = document.createElement("td");
    what.className = "wrap";
    what.append(text("span", event.message));
    if (event.detail) what.append(text("span", event.detail, "sub mono"));
    row.append(what);
    body.append(row);
  }
}

function renderRoutes(data) {
  const body = $("incidents-routes");
  body.replaceChildren();
  const routes = Object.entries(data.observability?.endpoints || {})
    .sort(([, a], [, b]) => b.server_errors - a.server_errors || b.requests - a.requests);
  if (!routes.length) {
    emptyRow(body, 6, "Запросов за 24 часа не было.");
    return;
  }
  for (const [name, e] of routes) {
    const row = document.createElement("tr");
    const cell = document.createElement("td");
    cell.append(text("strong", routeNames[name] || name), text("span", name, "sub"));
    row.append(cell);
    addCell(row, number(e.requests), "num");
    addCell(row, number(e.server_errors), e.server_errors ? "num bad-text" : "num");
    addCell(row, number(e.client_errors), "num");
    addCell(row, number(e.rate_limited), e.rate_limited ? "num warn-text" : "num");
    addCell(row, fixed(e.avg_ms, 1) + " / " + fixed(e.max_ms, 1), "num");
    body.append(row);
  }
}

function renderBackends(data) {
  const db = data.db, rooms = data.rooms_api;
  facts("incidents-backends", [
    ["Запросы к базе за 24 ч", db ? number(db.queries) : "—"],
    ["Ошибки базы", db ? number(db.errors) : "—"],
    ["Время запроса, сред. / макс.", db ? fixed(db.avg_ms, 1) + " / " + fixed(db.max_ms, 1) + " мс" : "—"],
    ["Последняя ошибка базы", db?.last_error ? utc(db.last_error.at) + " · " + db.last_error.text : "Нет"],
    ["Статусы комнат отклонены", rooms ? number(rooms.statuses_refused) + " из " + number(rooms.statuses) : "—"],
    ["Билеты в комнаты отклонены", rooms ? number(rooms.tickets_refused) : "—"],
    ["Загрузки профилей: отказ / сбой", rooms ? number(rooms.loads_refused) + " / " + number(rooms.loads_failed) + " из " + number(rooms.loads) : "—"],
    ["Сохранения: отказ / сбой", rooms ? number(rooms.saves_refused) + " / " + number(rooms.saves_failed) + " из " + number(rooms.saves) : "—"],
  ]);
  $("incidents-backends-note").textContent = "Счетчики комнат — с запуска " + utc(data.since);
}

function renderCertificates(data) {
  const body = $("incidents-certs");
  body.replaceChildren();
  if (!data.certificates.length) {
    emptyRow(body, 4, "Сертификаты комнат еще не проверялись.");
    return;
  }
  for (const cert of data.certificates) {
    const row = document.createElement("tr");
    const [word, tone] = certState(cert, data.as_of);
    const status = document.createElement("td");
    status.append(text("span", word, "status " + tone));
    row.append(status);
    const host = document.createElement("td");
    host.append(text("strong", cert.host));
    if (cert.invalid || cert.unreachable) host.append(text("span", cert.invalid || cert.unreachable, "sub"));
    row.append(host);
    const days = daysLeft(cert, data.as_of);
    addCell(row, days == null ? "—" : utc(cert.expires) + " · " + fixed(days, 0) + " дн.", "num");
    addCell(row, utc(cert.checked), "num");
    body.append(row);
  }
}

async function loadIncidents(quiet = false) {
  const seq = ++incidents.seq;
  if (!quiet) setStatus("Загружаю сбои…");
  const data = await api("/admin-api/v1/incidents");
  if (seq !== incidents.seq || state.page !== "incidents") return;
  const history = data.observability?.history;
  setStatus(history?.error ? "История наблюдений не сохраняется: " + history.error : "", history?.error ? "warning" : "");
  setSource("Обновлено " + utc(data.as_of), "fresh");
  renderIncidentState(data);
  renderAlerts(data);
  renderGroups(data);
  renderEvents(data);
  renderRoutes(data);
  renderBackends(data);
  renderCertificates(data);
}

document.addEventListener("DOMContentLoaded", () => {
  setInterval(() => {
    if (state.page !== "incidents" || document.hidden) return;
    const seq = incidents.seq + 1;
    loadIncidents(true).catch(() => {
      if (state.page === "incidents" && seq === incidents.seq) setSource("Источник недоступен", "unavailable");
    });
  }, 30_000);
});
