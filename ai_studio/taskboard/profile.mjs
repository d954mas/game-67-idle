import { agentContextPayloadForStores, listTaskboardStores } from "./stores.mjs";

const DEFAULT_CONTEXT_LIMIT = 5;

function byteLength(value) {
  return Buffer.byteLength(`${JSON.stringify(value, null, 2)}\n`, "utf8");
}

export function profileTaskboardReads(root) {
  const records = listTaskboardStores(root, { includePrivate: true }).map((store) => {
    const payload = agentContextPayloadForStores(root, [store], { limit: DEFAULT_CONTEXT_LIMIT });
    const returnedCount = payload.currentWork.length;
    const totalCount = payload.counts.currentWork;
    return {
      storeId: store.storeId,
      contextBytes: byteLength(payload),
      returnedCount,
      totalCount,
      truncated: returnedCount < totalCount,
    };
  });
  return {
    schema: "ai_studio.taskboard.context_profile.v1",
    records,
  };
}
