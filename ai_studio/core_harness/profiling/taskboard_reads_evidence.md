# Taskboard Read Evidence (T0374)

Captured on 2026-07-12 on Windows from the repository root. The before/after
CLI sample used one warm-up followed by six fresh-process runs per operation;
latency is the median wall time and bytes are UTF-8 stdout bytes.

The baseline is clean commit `6042609f0`; the after state is the T0374 diff in
this commit on top of that baseline. Both snapshots used this exact capture
command from their repository root:

```powershell
node --input-type=module --eval "import{spawnSync}from'node:child_process';import{performance}from'node:perf_hooks';const cases=[['summary','--json'],['context','--json'],['show','T0374','--json']];for(const args of cases){const samples=[];for(let i=0;i<7;i++){const start=performance.now();const result=spawnSync(process.execPath,['ai_studio/taskboard/cli.mjs',...args],{encoding:'utf8'});const ms=performance.now()-start;if(result.status!==0)throw new Error(result.stderr);if(i>0)samples.push({ms,bytes:Buffer.byteLength(result.stdout,'utf8'),rows:JSON.parse(result.stdout).currentWork?.length??1});}console.log(JSON.stringify({command:args.join(' '),samples}));}"
```

| Agent routing operation | Before rows / bytes / median ms | After rows / bytes / median ms |
| --- | ---: | ---: |
| `summary --json` | 5 / 3,184 / 64.859 | 5 / 3,184 / 84.531 |
| `context --json` | 25 / 13,849 / 65.674 | 5 / 3,184 / 82.463 |
| `show T0374 --json` | 1 / 1,767 / 46.727 | 1 / 1,767 / 57.667 |

The default context payload dropped by 10,665 bytes (77.0%) because it now
returns five summaries instead of 25. Fresh-process timings varied upward for
all three unchanged/startup-dominated routes in the after sample, so this
evidence makes no latency-improvement claim.

The repeatable in-process profiler excludes Node startup and measures the
actual read/payload boundary with monotonic timing. Run:

```powershell
node ai_studio/taskboard/cli.mjs profile --json --runs 7
```

The after sample reported Studio summary `3,184 bytes / 11.228 ms`, context
`3,184 bytes / 10.465 ms`, and explicit show `1,490 bytes / 0.271 ms`.
Fixture regressions additionally prove that the same command includes public
and private registered Taskboard mounts, excludes mounts without Taskboard
enabled, and never serializes task titles or bodies.
