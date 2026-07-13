import test from "node:test";
import assert from "node:assert/strict";
import { createPermissionBroker } from "../permission_broker.mjs";

const binding = { storeId: "studio", projectId: "proj-1", turnId: "turn-1" };

test("permission broker preserves opaque requests and allows exactly once", async () => {
  const broker = createPermissionBroker({ ttlMs: 1000 });
  const exactRequest = { method: "tools/call", params: { name: "shell", arguments: { command: "echo sentinel" } } };
  const pending = broker.request({ ...binding, exactRequest });
  assert.notEqual(pending.permission.exactRequest, exactRequest);
  assert.deepEqual(pending.permission.exactRequest, exactRequest);
  exactRequest.params.arguments.command = "mutated after request";
  assert.equal(pending.permission.exactRequest.params.arguments.command, "echo sentinel");
  assert.equal(pending.permission.state, "pending");
  broker.decide({ ...binding, permissionId: pending.permission.id, decision: "allow" });
  const settled = await pending.settled;
  assert.equal(settled.state, "allowed");
  assert.equal(settled.exactRequest.params.arguments.command, "echo sentinel");
  assert.equal(broker.pendingCount(), 0);
  assert.throws(
    () => broker.decide({ ...binding, permissionId: pending.permission.id, decision: "deny" }),
    /unknown permission/,
  );
});

test("permission broker denies wrong bindings and settles deny and cancel", async () => {
  const broker = createPermissionBroker({ ttlMs: 1000 });
  const denied = broker.request({ ...binding, exactRequest: { capability: "write" } });
  assert.throws(
    () => broker.decide({ ...binding, projectId: "other", permissionId: denied.permission.id, decision: "allow" }),
    /binding mismatch/,
  );
  broker.decide({ ...binding, permissionId: denied.permission.id, decision: "deny" });
  assert.equal((await denied.settled).state, "denied");
  assert.equal(broker.pendingCount(), 0);

  const cancelled = broker.request({ ...binding, exactRequest: { capability: "spawn" } });
  assert.equal(broker.cancelTurn(binding), 1);
  assert.equal((await cancelled.settled).state, "cancelled");
  assert.equal(broker.pendingCount(), 0);
});

test("permission broker expires pending requests and refuses stale decisions", async () => {
  let expire;
  const broker = createPermissionBroker({ ttlMs: 5, schedule: (fn) => { expire = fn; return 1; }, cancelSchedule() {} });
  const pending = broker.request({ ...binding, exactRequest: { arbitrary: [1, 2, 3] } });
  expire();
  assert.equal((await pending.settled).state, "expired");
  assert.equal(broker.pendingCount(), 0);
  assert.throws(
    () => broker.decide({ ...binding, permissionId: pending.permission.id, decision: "allow" }),
    /unknown permission/,
  );
});
