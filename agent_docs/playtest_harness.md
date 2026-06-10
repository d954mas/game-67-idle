# Agent Playtest Harness

Project-specific runtime check for fast native gameplay/UI iterations.

This is the Game 67 entry point for the generic `game-runtime-automation` skill.
Use it before inventing ad hoc runtime checks.

## Default Command

```powershell
py -3.12 tools\devapi\agent_playtest.py 9123 --full-loop
```

The harness launches the native desktop build through `tools/devapi/devapi_client.py::running_game()`.
That means stdout/stderr are captured under `build/logs/` and printed as a launch log path.

## What It Proves

- Native build starts with DevAPI.
- Required gameplay endpoints exist.
- Required gameplay UI ids exist.
- Fresh state is valid.
- First action changes state.
- First upgrade changes progression.
- Screenshot capture works.
- Pixel-health check passes for captured screenshots.
- With `--full-loop`, first timed job can start, become ready, and be claimed.

## Evidence Outputs

Default output folder:

```text
build/captures/agent_playtest/
```

Important files:

- `agent_playtest_report.json`
- `screenshots/agent_initial_<timestamp>.png`
- `screenshots/agent_after_upgrade_<timestamp>.png`
- `screenshots/agent_after_claim_<timestamp>.png` when `--full-loop` is used
- launch log under `build/logs/native_devapi_<port>_*.log`

## When To Run

Run this after gameplay, UI, visual, balance, DevAPI, or state changes that affect the first playable loop.

For deeper checks, run targeted scripts after this harness:

```powershell
py -3.12 tools\devapi\smoke_test.py 9123
py -3.12 tools\devapi\full_probe.py 9123
py -3.12 tools\devapi\capture_demo.py 9123 build\captures\manual_check.png --full-loop
```

## Failure Protocol

1. Read the console failure.
2. Open `agent_playtest_report.json` if it was written.
3. Inspect the launch log path printed by the harness.
4. Inspect the latest screenshots.
5. Fix the smallest issue that blocks the first loop.
6. Re-run the same command.

Do not diagnose from screenshots alone when DevAPI or launch logs show errors.

