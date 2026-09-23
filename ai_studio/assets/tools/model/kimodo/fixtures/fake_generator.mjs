// Stand-in for `kmd-generate --server`: prints a stray stdout line, answers the first
// request with OK and the second with ERR, then dies on the third.
import { createInterface } from "node:readline";

let count = 0;
process.stdout.write("loading models\n");
for await (const line of createInterface({ input: process.stdin })) {
  count += 1;
  const frames = line.split("\t")[4];
  if (count === 1) process.stdout.write(`OK\t${frames}\t30\n`);
  else if (count === 2) process.stdout.write("ERR\tbad prompt\n");
  else process.exit(3);
}
