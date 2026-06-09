import http from 'node:http';
import path from 'node:path';
import { promises as fs } from 'node:fs';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const host = process.env.HOST || '127.0.0.1';
const port = Number(process.env.PORT || 8067);

const editableFiles = new Map([
  ['common/index.md', 'Common index'],
  ['common/my_design_rules.md', 'Common: мои правила'],
  ['common/what_worked.md', 'Common: что сработало'],
  ['common/design_decisions.md', 'Common: решения'],
  ['common/concept_map.md', 'Common: карта связей'],
  ['common/core_loop.md', 'Common: core loop'],
  ['common/child_safe_copy.md', 'Common: безопасный текст'],
  ['common/playtest_gates.md', 'Common: playtest gates'],
  ['common/store_readiness.md', 'Common: store readiness'],
  ['common/glossary.md', 'Common: словарь'],
  ['common/game_design_bible.md', 'Common: bible'],
  ['common/templates/note.md', 'Common: шаблон заметки'],
  ['art_bible.html', 'Runtime Art Bible HTML'],
  ['gdd.md', 'GDD'],
  ['concept.md', 'Концепт'],
  ['market_research.md', 'Исследование рынка'],
  ['playtest_review_round_01.md', 'Разбор критика и игрока'],
  ['prototype_mvp_spec.md', 'Source of truth MVP'],
  ['dev_design_handoff_plan.md', 'Dev + design handoff plan'],
  ['prototype_build_handoff.md', 'Build handoff'],
  ['prototype_technical_blueprint.md', 'Technical blueprint'],
  ['game_implementation_plan.md', 'Game implementation plan'],
  ['ui_bible.md', 'UI bible'],
  ['screen_mockups_spec.md', 'Screen mockups spec'],
  ['asset_generation_brief.md', 'Asset generation brief'],
  ['handoff_status.md', 'Handoff status'],
  ['implementation_backlog.md', 'Implementation backlog'],
  ['runtime_test_plan.md', 'Runtime test plan'],
  ['playtest_acceptance_gates.md', 'Playtest gates'],
  ['playtest_script.md', 'Playtest script'],
  ['gameplay_spec.md', 'Геймплей'],
  ['prototype_30min_plan.md', 'План прототипа 30 минут'],
  ['mobile_web_ux_spec.md', 'Mobile/Web UX'],
  ['analytics_spec.md', 'Аналитика playtest'],
  ['parent_playtest_note.md', 'Parent playtest note'],
  ['content_matrix.md', 'Матрица контента'],
  ['p0_ui_copy.md', 'P0 UI copy'],
  ['asset_manifest.md', 'Asset manifest'],
  ['visual_contract.md', 'Visual contract'],
  ['compliance_checklist.md', 'Детская безопасность'],
  ['lore.md', 'Лор'],
  ['references.md', 'Референсы'],
  ['open_questions.md', 'Вопросы'],
  ['data/balance.json', 'Баланс MVP'],
  ['data/reducer_test_vectors.json', 'Reducer test vectors'],
  ['data/asset_manifest.json', 'Asset manifest JSON'],
  ['assets/asset_pack_manifest.json', 'Ready asset pack manifest JSON'],
  ['assets/generated/runtime_asset_manifest.json', 'Generated runtime asset manifest JSON'],
  ['assets/ui/slice9.json', 'Ready UI slice9 JSON'],
  ['data/analytics_events.json', 'Analytics events JSON'],
  ['data/ui_flow.json', 'UI flow JSON'],
  ['data/ui_components.json', 'UI components JSON'],
  ['data/asset_generation_queue.json', 'Asset generation queue JSON'],
  ['data/release_readiness.json', 'Release readiness JSON'],
  ['data/implementation_tasks.json', 'Implementation tasks JSON'],
  ['data/risk_register.json', 'Risk register JSON'],
  ['data/playtest_observation_schema.json', 'Playtest observation schema JSON'],
  ['data/runtime_evidence_manifest.json', 'Runtime evidence manifest JSON'],
  ['tools/validate_site.mjs', 'Site validator'],
  ['tools/validate_build_readiness.mjs', 'Build readiness validator'],
  ['tools/validate_copy.mjs', 'Copy validator'],
  ['tools/validate_reducer_vectors.mjs', 'Reducer vectors validator'],
  ['tools/validate_assets.mjs', 'Asset validator'],
  ['tools/validate_analytics.mjs', 'Analytics validator'],
  ['tools/validate_ui_flow.mjs', 'UI flow validator'],
  ['tools/validate_ui_components.mjs', 'UI components validator'],
  ['tools/validate_asset_generation_queue.mjs', 'Asset generation queue validator'],
  ['tools/validate_ready_assets.mjs', 'Ready asset pack validator'],
  ['tools/validate_technical_blueprint.mjs', 'Technical blueprint validator'],
  ['tools/validate_release_readiness.mjs', 'Release readiness validator'],
  ['tools/validate_implementation_tasks.mjs', 'Implementation tasks validator'],
  ['tools/validate_risk_register.mjs', 'Risk register validator'],
  ['tools/validate_playtest_observation_schema.mjs', 'Playtest observation schema validator'],
  ['tools/validate_handoff_status.mjs', 'Handoff status validator'],
  ['tools/validate_runtime_evidence_manifest.mjs', 'Runtime evidence manifest validator'],
  ['tools/validate_all.mjs', 'All validators runner'],
]);

const mimeTypes = new Map([
  ['.html', 'text/html; charset=utf-8'],
  ['.css', 'text/css; charset=utf-8'],
  ['.js', 'text/javascript; charset=utf-8'],
  ['.mjs', 'text/javascript; charset=utf-8'],
  ['.json', 'application/json; charset=utf-8'],
  ['.md', 'text/markdown; charset=utf-8'],
  ['.png', 'image/png'],
  ['.jpg', 'image/jpeg'],
  ['.jpeg', 'image/jpeg'],
  ['.webp', 'image/webp'],
  ['.svg', 'image/svg+xml; charset=utf-8'],
]);

function send(res, status, body, contentType = 'text/plain; charset=utf-8') {
  res.writeHead(status, { 'Content-Type': contentType });
  res.end(body);
}

function sendJson(res, status, body) {
  send(res, status, JSON.stringify(body), 'application/json; charset=utf-8');
}

async function readBody(req) {
  const chunks = [];
  for await (const chunk of req) {
    chunks.push(chunk);
    if (Buffer.concat(chunks).length > 1024 * 1024) {
      throw new Error('Request body is too large');
    }
  }
  return Buffer.concat(chunks).toString('utf8');
}

function editablePath(file) {
  if (!editableFiles.has(file)) {
    return null;
  }

  const absolutePath = path.resolve(__dirname, file);
  if (!absolutePath.startsWith(__dirname + path.sep)) {
    return null;
  }

  return absolutePath;
}

async function handleApi(req, res, url) {
  if (url.pathname === '/api/files' && req.method === 'GET') {
    sendJson(res, 200, {
      files: Array.from(editableFiles, ([pathName, label]) => ({ path: pathName, label })),
    });
    return;
  }

  if (url.pathname === '/api/file' && req.method === 'GET') {
    const file = url.searchParams.get('path') || '';
    const absolutePath = editablePath(file);
    if (!absolutePath) {
      sendJson(res, 403, { error: 'File is not editable' });
      return;
    }

    const content = await fs.readFile(absolutePath, 'utf8');
    sendJson(res, 200, { path: file, label: editableFiles.get(file), content });
    return;
  }

  if (url.pathname === '/api/file' && req.method === 'POST') {
    const payload = JSON.parse(await readBody(req));
    const file = String(payload.path || '');
    const content = String(payload.content ?? '');
    const absolutePath = editablePath(file);
    if (!absolutePath) {
      sendJson(res, 403, { error: 'File is not editable' });
      return;
    }

    if (file.endsWith('.json')) {
      JSON.parse(content);
    }

    await fs.writeFile(absolutePath, content, 'utf8');
    sendJson(res, 200, { ok: true, path: file, savedAt: new Date().toISOString() });
    return;
  }

  sendJson(res, 404, { error: 'Unknown API route' });
}

async function serveStatic(res, url) {
  let requestedPath = decodeURIComponent(url.pathname);
  if (requestedPath === '/') {
    requestedPath = '/index.html';
  }

  const absolutePath = path.resolve(__dirname, '.' + requestedPath);
  if (!absolutePath.startsWith(__dirname + path.sep)) {
    send(res, 403, 'Forbidden');
    return;
  }

  const stat = await fs.stat(absolutePath).catch(() => null);
  if (!stat || !stat.isFile()) {
    send(res, 404, 'Not found');
    return;
  }

  const ext = path.extname(absolutePath).toLowerCase();
  const contentType = mimeTypes.get(ext) || 'application/octet-stream';
  const content = await fs.readFile(absolutePath);
  res.writeHead(200, { 'Content-Type': contentType });
  res.end(content);
}

const server = http.createServer(async (req, res) => {
  try {
    const url = new URL(req.url || '/', `http://${host}:${port}`);
    if (url.pathname.startsWith('/api/')) {
      await handleApi(req, res, url);
      return;
    }

    await serveStatic(res, url);
  } catch (error) {
    sendJson(res, 500, { error: error.message || 'Server error' });
  }
});

server.listen(port, host, () => {
  console.log(`Game 67 GDD server: http://${host}:${port}/`);
  console.log(`Editor: http://${host}:${port}/editor.html`);
});
