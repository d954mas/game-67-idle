import test from "node:test";
import assert from "node:assert/strict";
import { LICENSE_URLS } from "./licenses.mjs";

test("LICENSE_URLS maps known SPDX ids to https license URLs", () => {
  for (const id of ["CC0-1.0", "OFL-1.1", "CC-BY-4.0", "CC-BY-SA-4.0"]) {
    assert.match(LICENSE_URLS[id], /^https:\/\/\S+$/, `${id} needs an https url`);
  }
});
