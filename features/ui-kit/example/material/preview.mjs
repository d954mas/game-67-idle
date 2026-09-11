import { themes, fonts } from './themes.mjs';

const kit = document.querySelector('#kit');
const dialog = document.querySelector('#live-dialog');
const params = new URLSearchParams(location.search);
const initialTheme = Object.hasOwn(themes, params.get('theme')) ? params.get('theme') : 'studio';
const state = { theme: initialTheme, mode: params.get('mode') === 'dark' ? 'dark' : 'light', accent: null, ...themeOptions(initialTheme) };
const settings = { music: true, sound: true, volume: '70', language: 'ru' };
let dialogTrigger;
let noticeTimer;
let level = 12;

function themeOptions(id) {
  const { shape, font, density, depth } = themes[id];
  return { shape, font, density, depth };
}

function luminance(hex) {
  const linear = hex.match(/[\da-f]{2}/gi).map(value => {
    const channel = parseInt(value, 16) / 255;
    return channel <= .04045 ? channel / 12.92 : ((channel + .055) / 1.055) ** 2.4;
  });
  return linear[0] * .2126 + linear[1] * .7152 + linear[2] * .0722;
}

function contrast(a, b) {
  const first = luminance(a), second = luminance(b);
  return (Math.max(first, second) + .05) / (Math.min(first, second) + .05);
}

function onAccent(hex) {
  return contrast(hex, '#ffffff') >= contrast(hex, '#000000') ? '#ffffff' : '#000000';
}

function readableIndicator(color, surfaces) {
  const leastContrast = candidate => Math.min(...surfaces.map(surface => contrast(candidate, surface)));
  if (leastContrast(color) >= 3.1) return color;
  const target = leastContrast('#000000') > leastContrast('#ffffff') ? 0 : 255;
  const channels = color.match(/[\da-f]{2}/gi).map(channel => parseInt(channel, 16));
  for (let step = 1; step <= 20; step++) {
    const mixed = '#' + channels.map(channel => Math.round(channel + (target - channel) * step / 20).toString(16).padStart(2, '0')).join('');
    if (leastContrast(mixed) >= 3.1) return mixed;
  }
  return target === 0 ? '#000000' : '#ffffff';
}

function themeSnapshot() {
  const colors = { ...themes[state.theme][state.mode] };
  if (state.accent) Object.assign(colors, { primary: state.accent, onPrimary: onAccent(state.accent) });
  colors.indicator = readableIndicator(colors.primary, [colors.surface, colors.surfaceLow, colors.surfaceHigh]);
  colors.focus = colors.indicator;
  colors.onIndicator = onAccent(colors.indicator);
  colors.primaryBorder = colors.indicator;
  return {
    schema: 'neotolis.ui-preview-theme.v1',
    base: state.theme, mode: state.mode, colors,
    typography: { preset: state.font, family: fonts[state.font] },
    shape: state.shape, density: state.density, elevation: state.depth,
    motion: { durationMs: 150, respectsReducedMotion: true }
  };
}

function applyTheme() {
  const theme = themeSnapshot();
  for (const [name, value] of Object.entries(theme.colors)) {
    kit.style.setProperty(`--nt-${name.replace(/[A-Z]/g, char => `-${char.toLowerCase()}`)}`, value);
  }
  kit.style.setProperty('--nt-font', fonts[state.font]);
  for (const name of ['mode', 'shape', 'density', 'depth']) kit.dataset[name] = state[name];
  for (const button of document.querySelectorAll('[data-preset]')) button.setAttribute('aria-pressed', String(button.dataset.preset === state.theme));
  for (const button of document.querySelectorAll('[data-mode]')) button.setAttribute('aria-pressed', String(button.dataset.mode === state.mode));
  for (const name of ['shape', 'font', 'density', 'depth']) document.getElementById(name).value = state[name];
  document.querySelector('#accent').value = theme.colors.primary;
  document.querySelector('#accent-value').textContent = theme.colors.primary;
  document.querySelector('#theme-label').textContent = `${themes[state.theme].label} · ${state.mode === 'light' ? 'Светлая' : 'Темная'}${state.accent ? ' · Свой акцент' : ''}`;
  document.querySelector('#theme-json').textContent = JSON.stringify(theme, null, 2);
}

function populate(mount, template) {
  mount.replaceChildren(document.getElementById(template).content.cloneNode(true));
  syncSettings();
}

function syncSettings() {
  for (const input of kit.querySelectorAll('[data-setting]')) {
    const value = settings[input.dataset.setting];
    if (input.type === 'checkbox') input.checked = value;
    else input.value = value;
  }
  for (const output of kit.querySelectorAll('[data-volume-output]')) output.textContent = `${settings.volume}%`;
}

function notify(message) {
  const notice = document.querySelector('#notice');
  clearTimeout(noticeTimer);
  notice.querySelector('span').textContent = message;
  notice.hidden = false;
  noticeTimer = setTimeout(() => { notice.hidden = true; }, 3500);
}

function openDialog(kind, trigger) {
  dialogTrigger = trigger;
  populate(document.querySelector('#dialog-body'), `${kind}-template`);
  const heading = dialog.querySelector('h3, .nt-badge');
  heading.id = 'dialog-title';
  dialog.showModal();
}

function closePanel(button) {
  if (dialog.contains(button)) {
    dialog.close();
    return;
  }
  const mount = button.closest('.example-panel');
  if (!mount) return;
  const reopen = document.createElement('button');
  reopen.className = 'nt-button nt-button--tonal';
  reopen.textContent = 'Открыть настройки';
  reopen.dataset.action = 'reopen-settings';
  mount.replaceChildren(reopen);
  reopen.focus();
}

for (const kind of ['settings', 'result']) populate(document.querySelector(`.${kind}-mount`), `${kind}-template`);
applyTheme();
const narrowViewport = matchMedia('(max-width: 700px)');
const customizer = document.querySelector('.customizer');
customizer.open = !narrowViewport.matches;
narrowViewport.addEventListener('change', event => { customizer.open = !event.matches; });

document.querySelector('.preset-list').addEventListener('click', event => {
  const button = event.target.closest('[data-preset]');
  if (!button) return;
  Object.assign(state, { theme: button.dataset.preset, accent: null }, themeOptions(button.dataset.preset));
  applyTheme();
});
document.querySelector('.mode-control').addEventListener('click', event => {
  const button = event.target.closest('[data-mode]');
  if (!button) return;
  state.mode = button.dataset.mode;
  applyTheme();
});
for (const name of ['shape', 'font', 'density', 'depth']) document.getElementById(name).addEventListener('change', event => {
  state[name] = event.target.value;
  applyTheme();
});
document.querySelector('#accent').addEventListener('input', event => {
  state.accent = event.target.value;
  applyTheme();
});
document.querySelector('#reset-theme').addEventListener('click', () => {
  Object.assign(state, { theme: 'studio', mode: 'light', accent: null }, themeOptions('studio'));
  applyTheme();
});
document.querySelector('#export-theme').addEventListener('click', () => {
  const url = URL.createObjectURL(new Blob([JSON.stringify(themeSnapshot(), null, 2) + '\n'], { type: 'application/json' }));
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = `neotolis-${state.theme}-${state.mode}-preview.json`;
  anchor.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
  notify('Тема прототипа сохранена в JSON');
});

kit.addEventListener('input', event => {
  const input = event.target.closest('[data-setting]');
  if (!input) return;
  settings[input.dataset.setting] = input.type === 'checkbox' ? input.checked : input.value;
  syncSettings();
});
kit.addEventListener('click', event => {
  const button = event.target.closest('button');
  if (!button || button.disabled) return;
  if (button.closest('#notice')) {
    clearTimeout(noticeTimer);
    document.querySelector('#notice').hidden = true;
    return;
  }
  if (button.matches('.nt-chip, .nt-segment')) {
    for (const sibling of button.parentElement.querySelectorAll('button')) sibling.setAttribute('aria-pressed', String(sibling === button));
  }
  if (button.dataset.notice) notify(button.dataset.notice);
  switch (button.dataset.action) {
    case 'settings': openDialog('settings', button); break;
    case 'result': openDialog('result', button); break;
    case 'close': closePanel(button); break;
    case 'reopen-settings':
      populate(button.parentElement, 'settings-template');
      document.querySelector('.settings-mount button').focus();
      break;
    case 'next':
      level++;
      document.querySelector('.game-title > span').textContent = `Уровень ${level}`;
      if (dialog.open) dialog.close();
      notify(`Открыт уровень ${level}`);
      break;
    case 'replay':
      if (dialog.open) dialog.close();
      notify(`Повтор уровня ${level}`);
      break;
  }
});
dialog.addEventListener('keydown', event => {
  if (event.key !== 'Tab') return;
  const controls = [...dialog.querySelectorAll('button:not([disabled]), input:not([disabled]), select:not([disabled])')].filter(element => element.getClientRects().length);
  const first = controls[0], last = controls.at(-1);
  if ((!event.shiftKey && document.activeElement === last) || (event.shiftKey && document.activeElement === first)) {
    event.preventDefault();
    (event.shiftKey ? last : first)?.focus();
  }
});
dialog.addEventListener('close', () => {
  if (dialogTrigger?.isConnected) dialogTrigger.focus();
});
dialog.addEventListener('click', event => {
  if (event.target !== dialog) return;
  const bounds = dialog.getBoundingClientRect();
  if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom) dialog.close();
});
document.querySelector('.section-nav').addEventListener('click', event => {
  const anchor = event.target.closest('a');
  if (!anchor) return;
  for (const link of document.querySelectorAll('.section-nav a')) link.classList.toggle('active', link === anchor);
});
