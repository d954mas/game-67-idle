const fileSelect = document.querySelector('#fileSelect');
const fileContent = document.querySelector('#fileContent');
const saveButton = document.querySelector('#saveButton');
const saveStatus = document.querySelector('#saveStatus');

let currentPath = '';

function setStatus(text, mode = '') {
  saveStatus.textContent = text;
  saveStatus.dataset.mode = mode;
}

async function loadFiles() {
  const response = await fetch('/api/files');
  const payload = await response.json();

  fileSelect.innerHTML = '';
  for (const file of payload.files) {
    const option = document.createElement('option');
    option.value = file.path;
    option.textContent = `${file.label} - ${file.path}`;
    fileSelect.append(option);
  }

  await loadFile(fileSelect.value);
}

async function loadFile(path) {
  currentPath = path;
  setStatus('Загрузка...');
  const response = await fetch(`/api/file?path=${encodeURIComponent(path)}`);
  const payload = await response.json();

  if (!response.ok) {
    setStatus(payload.error || 'Ошибка загрузки', 'error');
    return;
  }

  fileContent.value = payload.content;
  setStatus('Готово');
}

async function saveFile() {
  saveButton.disabled = true;
  setStatus('Сохранение...');

  try {
    const response = await fetch('/api/file', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ path: currentPath, content: fileContent.value }),
    });
    const payload = await response.json();

    if (!response.ok) {
      setStatus(payload.error || 'Ошибка сохранения', 'error');
      return;
    }

    setStatus(`Сохранено ${new Date(payload.savedAt).toLocaleTimeString()}`, 'ok');
  } catch (error) {
    setStatus(error.message || 'Ошибка сохранения', 'error');
  } finally {
    saveButton.disabled = false;
  }
}

fileSelect.addEventListener('change', () => loadFile(fileSelect.value));
saveButton.addEventListener('click', saveFile);

loadFiles().catch((error) => {
  setStatus(error.message || 'Ошибка запуска редактора', 'error');
});

