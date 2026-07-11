# Historical Skinned Mesh Renderer Proposal

Status: superseded by quarantine. This document preserves the intended split;
it is not an implemented renderer contract or a current production plan.

The proof separates Ozz clip sampling (`nt_skeletal_anim_*`) from CPU mesh
skinning (`nt_skeletal_mesh_*`). A future reviewed implementation may retain
that separation, but must first resolve every item in
[known-defects.md](known-defects.md), define skeleton-to-mesh joint mapping and
ownership, implement an actual renderer/GPU-buffer lifecycle, and establish a
measured budget.

Current facts:

- compatible Ozz archives can be loaded and sampled;
- model matrices, CPU-skinned positions, and socket matrices can be copied;
- source GLB import, GPU buffers, draw calls, materials, and production asset
  registration do not exist here;
- the proof has no universal asset assumptions or game-ready lifecycle;
- `external/neotolis-engine` remains read-only and outside this extension.

The earlier proposed `nt_skeletal_mesh_instance_draw` API was removed because
its implementation was a silent no-op. Renderer work, GPU skinning, equipment,
and final performance budgets require a future task rather than additions to
this quarantined proof.
