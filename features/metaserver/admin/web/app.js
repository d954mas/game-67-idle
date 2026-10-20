const pages = {
  overview: ["ОБЗОР", "Состояние сервера и игроков по подтвержденным данным."],
  players: ["ИГРОКИ", "Поиск игрока и последнего сохраненного состояния."],
  rooms: ["КОМНАТЫ", "Тик флота, машины и каждая комната против бюджета тика 33 мс."],
  capacity: ["МОЩНОСТЬ", "Онлайн, нагрузка машин, трафик и ошибки по минутной истории сервера."],
  incidents: ["СБОИ", "Алерты, ошибки, события, API и сертификаты метасервера за 24 часа."],
  system: ["СИСТЕМА", "Версии, источники состояния и резервные копии."],
};

const pageTitles = { overview: "Обзор", players: "Игроки", rooms: "Комнаты", capacity: "Мощность", incidents: "Сбои", system: "Система" };
const state = { page: "overview", csrf: "", playerQuery: "", playerCursor: 0 };
const $ = (id) => document.getElementById(id);
const number = (value) => new Intl.NumberFormat("ru-RU").format(value);

function utc(value) {
  if (!value) return "—";
  const date = new Date(value);
  if (Number.isNaN(date.valueOf())) return "—";
  return new Intl.DateTimeFormat("ru-RU", { timeZone: "UTC", day: "2-digit", month: "short",
    hour: "2-digit", minute: "2-digit" }).format(date) + " UTC";
}

function text(tag, value, className) {
  const node = document.createElement(tag);
  node.textContent = value == null ? "—" : String(value);
  if (className) node.className = className;
  return node;
}

function setStatus(message, tone = "") {
  $("page-status").textContent = message;
  $("page-status").dataset.tone = tone;
}

function setSource(message, stateName) {
  $("source-state").textContent = message;
  $("source-state").dataset.state = stateName;
}

function emptyRow(body, columns, message) {
  body.replaceChildren();
  const row = document.createElement("tr");
  const cell = text("td", message, "muted");
  cell.colSpan = columns;
  row.append(cell);
  body.append(row);
}

function addCell(row, value, className) {
  row.append(text("td", value, className));
}

function facts(id, entries) {
  const root = $(id);
  root.replaceChildren();
  for (const [label, value] of entries) {
    root.append(text("dt", label), text("dd", value));
  }
}

async function api(path, options = {}) {
  const response = await fetch(path, { credentials: "same-origin", cache: "no-store", ...options });
  if (response.status === 401 && path !== "/admin-api/v1/login") showLogin();
  if (!response.ok) {
    const error = new Error("Запрос завершился с кодом " + response.status);
    error.status = response.status;
    throw error;
  }
  return response.status === 204 ? null : response.json();
}

function showLogin(message = "") {
  $("app").hidden = true;
  $("login-screen").hidden = false;
  $("login-error").textContent = message;
  $("login-error").hidden = !message;
  $("login-password").value = "";
}

function showApp(user, csrf) {
  state.csrf = csrf;
  $("operator-name").textContent = user.username;
  $("login-screen").hidden = true;
  $("app").hidden = false;
  navigate(new URLSearchParams(location.search).get("page") || "overview", false);
}

async function login(event) {
  event.preventDefault();
  const form = event.currentTarget;
  const button = form.querySelector("button");
  button.disabled = true;
  $("login-error").hidden = true;
  try {
    const credentials = { username: $("login-user").value.trim(), password: $("login-password").value };
    const data = await api("/admin-api/v1/login", { method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(credentials) });
    showApp(data.user, data.csrf);
  } catch (error) {
    showLogin(error.status === 401 ? "Неверные данные входа." :
      "Вход сейчас недоступен. Проверьте соединение и повторите попытку.");
  } finally {
    button.disabled = false;
  }
}

async function logout() {
  try {
    await api("/admin-api/v1/logout", { method: "POST", headers: { "X-CSRF-Token": state.csrf } });
    showLogin();
  } catch {
    setStatus("Выход не подтвержден сервером. Повторите попытку.", "error");
  }
}

function navigate(page, push = true) {
  if (!Object.hasOwn(pages, page)) page = "overview";
  state.page = page;
  if (push) history.pushState(null, "", "/?page=" + encodeURIComponent(page));
  $("page-title").textContent = pageTitles[page];
  $("page-eyebrow").textContent = "ОПЕРАЦИИ / " + pages[page][0];
  $("page-description").textContent = pages[page][1];
  for (const section of document.querySelectorAll(".page")) section.hidden = section.dataset.page !== page;
  for (const link of document.querySelectorAll("[data-page-link]")) {
    if (link.dataset.pageLink === page) link.setAttribute("aria-current", "page");
    else link.removeAttribute("aria-current");
  }
  setStatus("");
  loadPage();
  $("main").focus({ preventScroll: true });
}

async function loadPage() {
  try {
    if (state.page === "overview") await loadOverview();
    else if (state.page === "players") await loadPlayers(false);
    else if (state.page === "rooms") await loadRoomsPage();
    else if (state.page === "capacity") await loadCapacity();
    else if (state.page === "incidents") await loadIncidents();
    else await loadSystem();
  } catch (error) {
    setStatus("Данные недоступны: " + error.message + ". Прошлые значения не считаются текущими.", "error");
    setSource("Источник недоступен", "unavailable");
  }
}

// The admin service fetched the snapshot for this request, so its time is
// current; a browser clock set wrong must not call it stale.
function updateFreshness(snapshot) {
  if (snapshot.error) {
    setSource("Данные неполны", "unavailable");
    setStatus("Метасервер сообщил о недоступной части данных.", "warning");
  } else {
    setSource("Обновлено " + utc(snapshot.now), "fresh");
  }
}

function roomStatus(room) {
  if (room.never_reported) return ["Не запускалась", "bad"];
  return room.alive ? ["Жива", "good"] : ["Нет связи", "bad"];
}

async function loadPlayers(append) {
  if (!append) state.playerCursor = 0;
  setStatus("Ищу игроков…");
  const query = new URLSearchParams({ q: state.playerQuery, after: String(state.playerCursor) });
  const data = await api("/admin-api/v1/players?" + query);
  const body = $("players-list");
  if (!append) body.replaceChildren();
  if (!data.items.length && !append) emptyRow(body, 4, "Игроки не найдены.");
  for (const player of data.items) {
    const row = document.createElement("tr");
    const id = document.createElement("td");
    const button = text("button", "#" + player.id, "player-link");
    button.type = "button";
    button.addEventListener("click", () => showPlayer(player.id));
    id.append(button);
    row.append(id);
    addCell(row, player.login || "Гость");
    addCell(row, utc(player.created_at));
    addCell(row, player.profile_rev == null ? "Нет" : "rev " + player.profile_rev, "num");
    body.append(row);
  }
  state.playerCursor = data.next_cursor || 0;
  $("players-more").hidden = !state.playerCursor;
  $("players-count").textContent = data.items.length ? "Показано " + number(body.rows.length) : "";
  setStatus("");
}

// The saved profile, every system as saved: the console knows no game's fields.
function playerProfile(player) {
  const box = document.createElement("div");
  box.className = "profile";
  box.append(text("h3", "Профиль игры"));
  if (!player.profile) {
    box.append(text("p", player.profile_error === "profile unreadable" ? "Профиль сохранен в неизвестном формате."
      : player.profile_error ? "Профиль недоступен." : "Игрок еще ничего не сохранил.", "empty"));
    return box;
  }
  box.append(text("p", player.profile.format, "empty"));
  for (const system of player.profile.systems) {
    box.append(text("h4", system.name + " " + system.version));
    const list = document.createElement("dl");
    list.className = "fact-list";
    for (const field of system.fields) list.append(text("dt", field.key), text("dd", field.value));
    box.append(list);
  }
  return box;
}

async function showPlayer(id) {
  const root = $("player-detail");
  root.textContent = "Загружаю игрока…";
  try {
    const player = await api("/admin-api/v1/players/" + encodeURIComponent(id));
    root.replaceChildren();
    const list = document.createElement("dl");
    list.className = "fact-list";
    root.append(list);
    for (const [label, value] of [["ID", player.id], ["Логин", player.login || "Гость"],
      ["Создан", utc(player.created_at)], ["Сохранение", player.profile_rev == null ? "Нет" : "rev " + player.profile_rev],
      ["Сохранено", utc(player.profile_saved_at)]]) {
      list.append(text("dt", label), text("dd", value));
    }
    root.append(playerProfile(player));
  } catch {
    root.textContent = "Карточка недоступна.";
  }
}

async function loadSystem() {
  setStatus("Загружаю состояние метасервера…");
  const snapshot = await api("/admin-api/v1/overview");
  setStatus("");
  updateFreshness(snapshot);
  facts("system-meta", [["Последний снимок", utc(snapshot.now)], ["Запущен", utc(snapshot.since)],
    ["Комнаты", snapshot.rooms == null ? "Недоступны" : number(snapshot.rooms.length)],
    ["Игроки", snapshot.players == null ? "Недоступны" : "Доступны"],
    ["Память Go", snapshot.process ? number(Math.round(snapshot.process.heap_bytes / 1_048_576)) + " МиБ" : "—"],
    ["Горутины", snapshot.process ? number(snapshot.process.goroutines) : "—"],
    ["Соединения БД", snapshot.process ? number(snapshot.process.db_acquired) + " / " + number(snapshot.process.db_max) : "—"],
    ["Ошибки БД, 24 ч", snapshot.db ? number(snapshot.db.errors) : "—"],
    ["Средний запрос БД", snapshot.db ? snapshot.db.avg_ms.toFixed(1) + " мс" : "—"],
    ["Ошибки загрузки", snapshot.rooms_api ? number(snapshot.rooms_api.loads_failed) : "—"],
    ["Ошибки сохранения", snapshot.rooms_api ? number(snapshot.rooms_api.saves_failed) : "—"]]);
}

function updateClock() {
  const now = new Date();
  $("utc-clock").dateTime = now.toISOString();
  $("utc-clock").textContent = new Intl.DateTimeFormat("ru-RU", { timeZone: "UTC", hour: "2-digit",
    minute: "2-digit", second: "2-digit" }).format(now) + " UTC";
}

document.addEventListener("DOMContentLoaded", async () => {
  updateClock();
  setInterval(updateClock, 1000);
  $("login-form").addEventListener("submit", login);
  $("logout").addEventListener("click", logout);
  $("mobile-logout").addEventListener("click", logout);
  $("refresh").addEventListener("click", loadPage);
  $("player-search").addEventListener("submit", (event) => {
    event.preventDefault();
    state.playerQuery = $("player-query").value.trim();
    loadPlayers(false).catch(() => setStatus("Поиск недоступен.", "error"));
  });
  $("players-more").addEventListener("click", () => loadPlayers(true).catch(() =>
    setStatus("Следующая страница недоступна.", "error")));
  document.querySelectorAll("[data-page-link], .panel-head a").forEach((link) => {
    link.addEventListener("click", (event) => {
      event.preventDefault();
      navigate(new URL(link.href).searchParams.get("page") || "overview");
    });
  });
  addEventListener("popstate", () => navigate(new URLSearchParams(location.search).get("page") || "overview", false));
  try {
    const session = await api("/admin-api/v1/session");
    showApp(session.user, session.csrf);
  } catch {
    showLogin();
  }
});
