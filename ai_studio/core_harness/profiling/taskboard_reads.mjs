import { relative } from "node:path";
import { performance } from "node:perf_hooks";

import { agentEpicRow, agentProjectRow, agentTaskRow } from "../../taskboard/store.mjs";
import {
  agentContextPayloadForStores,
  findTaskboardDoc,
  listTaskboardStores,
  taskboardStoreSummary,
} from "../../taskboard/stores.mjs";

const DEFAULT_CONTEXT_LIMIT = 5;

function slash(value) {
  return String(value || "").replace(/\\/g, "/");
}

function byteLength(value) {
  return Buffer.byteLength(`${JSON.stringify(value, null, 2)}\n`, "utf8");
}

function median(values) {
  const sorted = [...values].sort((a, b) => a - b);
  const middle = Math.floor(sorted.length / 2);
  if (sorted.length % 2) return sorted[middle];
  return (sorted[middle - 1] + sorted[middle]) / 2;
}

function detailedRow(root, doc, store) {
  const options = { store, includeBody: true };
  const row = doc.kind === "task"
    ? agentTaskRow(root, doc, options)
    : (doc.kind === "epic" ? agentEpicRow(root, doc, options) : agentProjectRow(root, doc, options));
  return { schema: "ai_studio.taskboard.doc.v1", doc: row };
}

function measure(runs, read) {
  const durations = [];
  let value;
  for (let index = 0; index < runs; index++) {
    const startedAt = performance.now();
    value = read();
    durations.push(performance.now() - startedAt);
  }
  return {
    value,
    durationMs: Number(median(durations).toFixed(3)),
  };
}

function contextRecord(root, store, operation, runs) {
  const measured = measure(runs, () => agentContextPayloadForStores(root, [store], { limit: DEFAULT_CONTEXT_LIMIT }));
  const payload = measured.value;
  return {
    storeId: store.storeId,
    visibility: store.visibility,
    operation,
    path: slash(relative(root, store.itemsRoot)),
    query: `${operation} --store ${store.storeId} --tasks-limit ${DEFAULT_CONTEXT_LIMIT}`,
    bytes: byteLength(payload),
    durationMs: measured.durationMs,
    truncated: payload.counts.currentWork > payload.currentWork.length,
    resultCount: payload.currentWork.length,
  };
}

function showRecord(root, store, runs) {
  const context = agentContextPayloadForStores(root, [store], { limit: 1 });
  const first = context.currentWork[0] || null;
  const qualifiedId = first ? first.qualifiedId : "";
  const measured = measure(runs, () => {
    if (!qualifiedId) return null;
    const resolved = findTaskboardDoc(root, qualifiedId, { activeStoreId: store.storeId });
    return resolved ? detailedRow(root, resolved.doc, store) : null;
  });
  return {
    storeId: store.storeId,
    visibility: store.visibility,
    operation: "show",
    path: slash(relative(root, store.itemsRoot)),
    query: qualifiedId ? `show ${qualifiedId}` : `show --store ${store.storeId} <no-current-task>`,
    bytes: measured.value ? byteLength(measured.value) : 0,
    durationMs: measured.durationMs,
    truncated: false,
    resultCount: measured.value ? 1 : 0,
  };
}

export function profileTaskboardReads(root, options = {}) {
  const parsedRuns = Number(options.runs);
  const runs = Number.isFinite(parsedRuns) && parsedRuns > 0 ? Math.min(100, Math.floor(parsedRuns)) : 5;
  const stores = listTaskboardStores(root, { includePrivate: true });
  const records = [];
  for (const store of stores) {
    records.push(contextRecord(root, store, "summary", runs));
    records.push(contextRecord(root, store, "context", runs));
    records.push(showRecord(root, store, runs));
  }
  return {
    schema: "ai_studio.taskboard.read_profile.v1",
    runs,
    stores: stores.map(taskboardStoreSummary),
    records,
    privacy: "Metrics contain store metadata and aggregate sizes/timings only; task titles and bodies are never emitted.",
  };
}
