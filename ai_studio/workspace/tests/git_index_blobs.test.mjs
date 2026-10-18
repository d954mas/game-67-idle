import assert from "node:assert/strict";
import test from "node:test";

import {
  batchInput,
  parseBatchBodies,
  parseBatchCheck,
  parseIndexEntries,
  parseUntrackedGitPath,
  splitBatchOids,
} from "../git_index_blobs.mjs";

const OID_A = "a".repeat(40);
const OID_B = "b".repeat(40);

test("index blob parser preserves spaces and Unicode paths while excluding gitlinks", () => {
  const entries = parseIndexEntries(Buffer.from(
    `100644 ${OID_A} 0\tnotes/space name-кириллица.txt\0`
      + `120000 ${OID_B} 0\tlinks/target\0`
      + `160000 ${"c".repeat(40)} 0\texternal/engine\0`,
    "utf8",
  ));

  assert.deepEqual(entries, [
    { mode: "100644", oid: OID_A, path: "notes/space name-кириллица.txt" },
    { mode: "120000", oid: OID_B, path: "links/target" },
  ]);
  assert.deepEqual(batchInput([OID_A, OID_B]), Buffer.from(`${OID_A}\n${OID_B}\n`, "ascii"));
  assert.deepEqual(parseIndexEntries(Buffer.from(`160000 ${OID_A} 0\texternal/engine\0`), { includeGitlinks: true }), [
    { mode: "160000", oid: OID_A, path: "external/engine" },
  ]);
});

test("index blob parser fails closed for unmerged entries and malformed stage records", () => {
  assert.throws(
    () => parseIndexEntries(Buffer.from(`100644 ${OID_A} 1\tconflict.txt\0`, "utf8")),
    /unmerged index entry/i,
  );
  assert.throws(
    () => parseIndexEntries(Buffer.from(`100600 ${OID_A} 0\tbad-mode.txt\0`, "utf8")),
    /invalid index entry/i,
  );
  assert.throws(
    () => parseIndexEntries(Buffer.from(`100644 ${OID_A} 0\tpartial.txt`, "utf8")),
    /truncated index entry list/i,
  );
  for (const path of ["../outside.bin", "/outside.bin", "C:\\outside.bin"]) {
    assert.throws(
      () => parseIndexEntries(Buffer.from(`100644 ${OID_A} 0\t${path}\0`, "utf8")),
      /unsafe Git-relative path/i,
    );
  }
});

test("untracked directory paths allow exactly one terminal slash", () => {
  assert.equal(parseUntrackedGitPath("tmp/nested/"), "tmp/nested");
  for (const path of ["tmp/nested//", "../outside/", "/outside/", "C:\\outside/"]) {
    assert.throws(() => parseUntrackedGitPath(path), /unsafe Git-relative path/i);
  }
});

test("batch parser validates exact headers, sizes, and content framing", () => {
  const metadata = parseBatchCheck(
    Buffer.from(`${OID_A} blob 3\n${OID_B} blob 4\n`, "ascii"),
    [OID_A, OID_B],
  );
  assert.deepEqual([...metadata], [[OID_A, 3], [OID_B, 4]]);
  assert.deepEqual(splitBatchOids([OID_A, OID_B], metadata, 256), [[OID_A, OID_B]]);

  const blobs = parseBatchBodies(
    Buffer.concat([
      Buffer.from(`${OID_A} blob 3\nabc\n${OID_B} blob 4\n`, "ascii"),
      Buffer.from([0x00, 0x62, 0x69, 0x6e, 0x0a]),
    ]),
    [OID_A, OID_B],
    metadata,
  );
  assert.deepEqual(blobs.get(OID_A), Buffer.from("abc"));
  assert.deepEqual(blobs.get(OID_B), Buffer.from([0x00, 0x62, 0x69, 0x6e]));
});

test("batch parser fails closed for malformed, truncated, or oversized blob output", () => {
  const metadata = new Map([[OID_A, 4]]);
  assert.throws(
    () => parseBatchBodies(Buffer.from(`${OID_A} blob 4\nab`, "ascii"), [OID_A], metadata),
    /truncated batch blob/i,
  );
  assert.throws(
    () => parseBatchBodies(Buffer.from(`${OID_A} blob 3\nabc\n`, "ascii"), [OID_A], metadata),
    /does not match checked size/i,
  );
  assert.throws(
    () => parseBatchCheck(Buffer.from(`${OID_A} blob 99\n`, "ascii"), [OID_A], { maxBlobBytes: 4 }),
    /exceeds limit/i,
  );
});
