# Generation Paths

Read when selecting a tool or recovering from a failed image-generation route.
Use current tool capabilities and documentation rather than a fixed model name,
price assumption or machine-specific recipe.

## Default Route

Use the host's official native image tool when available. Supply the requested
subject, composition, output constraints and references; preserve the returned
image and tool provenance. Use its supported transparent-background option for
cutouts and inspect the result. Follow the host's image-editing requirements.

If the native tool is unavailable, choose an official CLI or API only when its
current capabilities, credentials and execution permissions cover the task.
Use normal sandbox/approval settings and explicit output paths. Existing task
authorization carries forward, but does not grant access to a different account,
paid service or execution boundary.

For API work, use the provider's documented endpoint and configured API-key
route. Do not infer API billing permission from a chat subscription, read browser
or CLI credential stores, or use an undocumented backend as an automatic fallback.
If no supported authorized path exists, report the specific missing capability.

## Existing Adapter Limitations

The bundled scripts have legacy defaults; their presence is not a safe fallback:

- `scripts/codex_imagegen.sh` unconditionally disables sandbox and approvals.
- `scripts/gen_both.sh` invokes that wrapper and bypasses agy permissions.
- `scripts/generate_image.py` can fall back to CLI OAuth/raw backend access,
  pins provider details and changes TLS revocation behavior.
- `scripts/gen_batch.py` uses `generate_image.py` unless an adapter is supplied.
- Canvas recipe/pack and AI dual-plate defaults also delegate to legacy
  generators; a Canvas command does not remove the adapter restriction.

Do not run these default generation routes under this skill. A maintained
adapter must preserve the current host permissions and use a supported official
transport before it can be selected. Deterministic preparation tools remain
available independently of generation. Until a permitted Canvas generation
adapter is available, generate with the native tool and import the source
through Canvas's supported operations.

## Comparison And Recovery

Compare providers or candidates only when the requested style decision needs
it and the generation scope covers the extra calls. Keep composition comparable.
Inspect failed output and provider errors before retrying; change the prompt,
references or supported route based on the failure. Preserve certificate checks
and do not weaken execution permissions to make a route work.
