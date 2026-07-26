# WP1 OBS Spike Cleanup Incident

Status: **closed / verified 2026-07-26**

Recorded: 2026-07-26.

## Scope

The WP1 OBS spike created firewall state only for its disposable portable OBS
executable. No private-consumer identity is part of the rule.

Exact local identity:

- display name created by the spike:
  `NT Capture OBS Spike Block 20260726`;
- application:
  `C:\tmp\nt-capture-obs-spike-20260726\bin\64bit\obs64.exe`;
- intended rule: inbound, block, all profiles;
- cleanup scope: every firewall rule whose application filter resolves to that
  exact executable, including any Windows-generated allow rule;
- owner: Runtime Automation WP1 spike; local lead/admin closes the incident.

The OBS process was stopped. The temporary WebSocket server was disabled and
its ephemeral password was replaced before the cleanup verification.

## Initial failed cleanup evidence

- non-admin `Remove-NetFirewallRule`: access denied;
- elevated `Start-Process -Verb RunAs`: process creation failed with
  `0xc0000142` in the automation context;
- final rule enumeration also requires administrator access, so zero remaining
  rules is unverified.

## Closure evidence

The lead ran the administrator cleanup helper. Its dated result recorded:

- exact application:
  `C:\tmp\nt-capture-obs-spike-20260726\bin\64bit\obs64.exe`;
- rules removed: `1`;
- rules remaining: `0`;
- cleanup evidence SHA-256:
  `d85f5e59b520a4eef10dc0997d80f2b45c38b340c3a8a78400e2cfde59e05d12`.

After the final recorder comparison, process enumeration found no owned
`obs64` process. The portable tree resolved to the exact intended `C:\tmp`
target and was removed recursively. Both temporary firewall helper scripts were
removed. The result JSON and recorder benchmark artifacts remain local as
evidence; they create no firewall or running-process state.
