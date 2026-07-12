import { randomUUID } from "node:crypto";

const TERMINAL_STATES = new Set(["allowed", "denied", "cancelled", "expired"]);

function sameBinding(permission, binding) {
  return permission.storeId === binding.storeId
    && permission.projectId === binding.projectId
    && (binding.turnId === undefined || permission.turnId === binding.turnId);
}

export function createPermissionBroker({
  ttlMs = 5 * 60 * 1000,
  schedule = setTimeout,
  cancelSchedule = clearTimeout,
} = {}) {
  const permissions = new Map();

  function publicPermission(permission) {
    return {
      id: permission.id,
      storeId: permission.storeId,
      projectId: permission.projectId,
      turnId: permission.turnId,
      exactRequest: structuredClone(permission.exactRequest),
      state: permission.state,
    };
  }

  function settle(permission, state) {
    if (permission.state !== "pending") {
      throw new Error(`permission ${permission.id} is not pending (state: ${permission.state})`);
    }
    if (!TERMINAL_STATES.has(state)) throw new Error(`invalid permission state ${state}`);
    permission.state = state;
    cancelSchedule(permission.timer);
    const result = publicPermission(permission);
    permissions.delete(permission.id);
    permission.resolve(result);
    return result;
  }

  function request({ storeId, projectId, turnId, exactRequest }) {
    if (!storeId || !projectId || !turnId || exactRequest === undefined) {
      throw new Error("permission request requires storeId, projectId, turnId, and exactRequest");
    }
    let resolve;
    const settled = new Promise((done) => { resolve = done; });
    const permission = {
      id: randomUUID(), storeId, projectId, turnId,
      exactRequest: structuredClone(exactRequest), state: "pending", resolve, timer: null,
    };
    permission.timer = schedule(() => {
      if (permission.state === "pending") settle(permission, "expired");
    }, ttlMs);
    permissions.set(permission.id, permission);
    return { permission: publicPermission(permission), settled };
  }

  function decide({ storeId, projectId, turnId, permissionId, decision }) {
    const permission = permissions.get(permissionId);
    if (!permission) throw new Error(`unknown permission ${permissionId}`);
    if (!sameBinding(permission, { storeId, projectId, turnId })) throw new Error("permission binding mismatch");
    if (decision !== "allow" && decision !== "deny") throw new Error("decision must be allow or deny");
    return settle(permission, decision === "allow" ? "allowed" : "denied");
  }

  function cancelTurn(binding) {
    let count = 0;
    for (const permission of permissions.values()) {
      if (permission.state === "pending" && sameBinding(permission, binding)) {
        settle(permission, "cancelled");
        count += 1;
      }
    }
    return count;
  }

  return { request, decide, cancelTurn, pendingCount: () => permissions.size };
}
