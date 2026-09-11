import { themes, fonts } from './themes.mjs';

const kit = document.querySelector('#kit');
const stage = document.querySelector('#game-stage');
const dialog = document.querySelector('#game-dialog');
const dialogBody = document.querySelector('#dialog-body');
const themeState = { name:'studio', shape:'standard', font:'compact' };
const demo = { wallet:1240, power:20, powerPrice:120, radius:2, level:7, ability:'hit', settings:{music:true, sound:true, volume:70} };
const number = new Intl.NumberFormat('ru-RU');
let returnFocus = null;
let noticeTimer;

function applyTheme() {
  for (const [name, value] of Object.entries(themes[themeState.name].colors)) {
    kit.style.setProperty('--' + name.replace(/[A-Z]/g, value => '-' + value.toLowerCase()), value);
  }
  kit.style.setProperty('--radius', {standard:'12px', sharp:'5px', round:'20px'}[themeState.shape]);
  kit.style.setProperty('--font', fonts[themeState.font]);
  document.querySelectorAll('[data-theme]').forEach(button => button.setAttribute('aria-pressed', button.dataset.theme === themeState.name));
}

function textAll(selector, text) {
  document.querySelectorAll(selector).forEach(element => { element.textContent = text; });
}

function renderDemo() {
  textAll('[data-wallet]', number.format(demo.wallet));
  textAll('[data-level]', demo.level);
  textAll('[data-power-before]', demo.power);
  textAll('[data-power-after]', demo.power + 5);
  textAll('[data-price="power"]', number.format(demo.powerPrice));
  textAll('[data-radius-before]', demo.radius.toFixed(1).replace('.', ','));
  textAll('[data-radius-after]', (demo.radius + .5).toFixed(1).replace('.', ','));
  const affordable = Number(demo.wallet >= demo.powerPrice) + Number(demo.wallet >= 1600);
  const badge = document.querySelector('[data-affordable]');
  badge.textContent = affordable;
  badge.hidden = affordable === 0;
  for (const button of dialog.querySelectorAll('[data-buy]')) {
    const isPower = button.dataset.buy === 'power';
    const price = isPower ? demo.powerPrice : 1600;
    button.disabled = demo.wallet < price;
    button.setAttribute('aria-label', `Улучшить ${isPower ? 'силу' : 'радиус'} за ${number.format(price)} монет`);
  }
  for (const input of dialog.querySelectorAll('[data-setting]')) {
    if (input.type === 'checkbox') input.checked = demo.settings[input.dataset.setting];
    else input.value = demo.settings[input.dataset.setting];
  }
  textAll('#volume-value', `${demo.settings.volume}%`);
}

function openDialog(kind, opener) {
  const template = document.querySelector(`#${kind}-template`);
  if (!template) return;
  clearTimeout(noticeTimer);
  document.querySelector('.global-notice').hidden = true;
  if (!dialog.open) returnFocus = opener;
  dialogBody.replaceChildren(template.content.cloneNode(true));
  renderDemo();
  if (!dialog.open) dialog.showModal();
  dialog.querySelector('button:not(:disabled)')?.focus();
}

function notify(message) {
  const notice = document.querySelector('.global-notice');
  clearTimeout(noticeTimer);
  notice.textContent = message;
  notice.hidden = false;
  noticeTimer = setTimeout(() => { notice.hidden = true; }, 2600);
}

function buy(kind) {
  const price = kind === 'power' ? demo.powerPrice : 1600;
  if (demo.wallet < price) return;
  const purchaseButton = dialog.querySelector(`[data-buy="${kind}"]`);
  const hadFocus = document.activeElement === purchaseButton;
  demo.wallet -= price;
  if (kind === 'power') { demo.power += 5; demo.powerPrice += 60; }
  else demo.radius += .5;
  renderDemo();
  const feedback = dialog.querySelector('.purchase-feedback');
  feedback.textContent = kind === 'power' ? `Сила удара увеличена до ${demo.power}` : `Радиус увеличен до ${demo.radius.toFixed(1).replace('.', ',')}`;
  if (hadFocus && purchaseButton.disabled) dialog.querySelector('[data-close]')?.focus();
}

function exportTheme() {
  const data = { schema:'neotolis.ui-preview-theme.v2', direction:'B', name:themes[themeState.name].name, colors:themes[themeState.name].colors, shape:themeState.shape, typography:{preset:themeState.font, stack:fonts[themeState.font]}, contourPx:3, buttonLiftPx:4 };
  const blob = new Blob([JSON.stringify(data, null, 2) + '\n'], {type:'application/json'});
  const url = URL.createObjectURL(blob);
  const anchor = document.createElement('a');
  anchor.href = url;
  anchor.download = `neotolis-ui-b-${themeState.name}.json`;
  anchor.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
}

document.addEventListener('click', event => {
  const button = event.target.closest('button');
  if (!button || button.disabled) return;
  if (button.dataset.open) openDialog(button.dataset.open, button);
  if (button.hasAttribute('data-close')) dialog.close();
  if (button.dataset.screen === 'game') { if (dialog.open) dialog.close(); stage.scrollIntoView({block:'center', behavior:'instant'}); }
  if (button.dataset.theme) { themeState.name = button.dataset.theme; applyTheme(); }
  if (button.dataset.viewport) {
    stage.dataset.viewport = button.dataset.viewport;
    document.querySelectorAll('[data-viewport]').forEach(element => {
      if (element.tagName === 'BUTTON') element.setAttribute('aria-pressed', element === button);
    });
  }
  if (button.dataset.ability) {
    demo.ability = button.dataset.ability;
    const mark = stage.querySelector('.socket-mark');
    for (const ability of stage.querySelectorAll('[data-ability]')) {
      const selected = ability === button;
      ability.setAttribute('aria-pressed', selected);
      ability.classList.toggle('secondary', !selected);
    }
    button.append(mark);
  }
  if (button.dataset.buy) buy(button.dataset.buy);
  if (button.hasAttribute('data-next')) {
    demo.wallet += 240;
    demo.level += 1;
    renderDemo();
    dialog.close();
    notify('Следующий уровень · награда +240 монет, +15 деталей');
  }
  if (button.dataset.toast) notify(button.dataset.toast);
  if (button.id === 'export-theme') exportTheme();
});

document.addEventListener('change', event => {
  if (event.target.id === 'shape') { themeState.shape = event.target.value; applyTheme(); }
  if (event.target.id === 'font') { themeState.font = event.target.value; applyTheme(); }
  if (event.target.dataset.setting && event.target.type === 'checkbox') demo.settings[event.target.dataset.setting] = event.target.checked;
});

dialog.addEventListener('input', event => {
  if (event.target.dataset.setting === 'volume') {
    demo.settings.volume = Number(event.target.value);
    textAll('#volume-value', `${demo.settings.volume}%`);
  }
});

dialog.addEventListener('keydown', event => {
  if (event.key !== 'Tab') return;
  const controls = [...dialog.querySelectorAll('button:not(:disabled), input:not(:disabled), select:not(:disabled), a[href]')].filter(element => element.getClientRects().length);
  const first = controls[0], last = controls.at(-1);
  if (!first) return;
  if ((event.shiftKey && document.activeElement === first) || (!event.shiftKey && document.activeElement === last)) {
    event.preventDefault();
    (event.shiftKey ? last : first).focus();
  }
});

dialog.addEventListener('click', event => {
  if (event.target !== dialog) return;
  const bounds = dialog.getBoundingClientRect();
  if (event.clientX < bounds.left || event.clientX > bounds.right || event.clientY < bounds.top || event.clientY > bounds.bottom) dialog.close();
});

dialog.addEventListener('close', () => {
  if (returnFocus?.isConnected) returnFocus.focus({preventScroll:true});
  returnFocus = null;
});

applyTheme();
renderDemo();
