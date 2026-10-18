import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const studio = fileURLToPath(new URL("../studio.mjs", import.meta.url));
const result = spawnSync(process.execPath, [studio, "verify", "--domain", "features"], {
  stdio: "inherit",
  shell: false,
});
if (result.error) console.error(result.error.message);
process.exitCode = result.status ?? 2;
