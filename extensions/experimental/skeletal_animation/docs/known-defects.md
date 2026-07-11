# Known Defects And Reproduction Evidence

These are accepted quarantine defects, not production guarantees. Run the
`experimental_skeletal_quarantine_contract` CTest after explicit opt-in to
verify that this boundary and its evidence remain visible.

1. `wrap_time` uses duration-sized loops. Positive/negative infinity never
   terminates, NaN reaches Ozz, and a huge finite time takes unbounded work.
   Inspect `src/nt_skeletal_animation_ozz.cpp`: there is no finite/range guard.
2. `nt_skeletal_anim_copy_model_matrices` has no sampled-pose state. Calling it
   after load but before sample copies matrices that have not been produced by
   `LocalToModelJob`.
3. Mesh joint indices are consumed as animation matrix indices. No API maps the
   mesh `joint_names` order to the Ozz skeleton order, so equal counts with a
   different order silently skin against the wrong joints.
4. C-linkage entry points call Ozz, `std::vector`, and `std::string` operations
   without a catch boundary. Allocation/archive exceptions can cross the C ABI.
5. An instance stores a raw mesh pointer. Destroying the mesh before its
   instances leaves later count/update/socket operations with a possible
   use-after-free. Instance destruction itself does not dereference the mesh,
   but the required ownership order is still undocumented and unenforced.
6. Descriptor weights and inverse-bind/model matrices are not checked for
   finiteness. NaN weights pass the current comparisons; non-finite matrices
   propagate into positions and socket transforms.
7. The exported draw path was a silent no-op. Quarantine removes it from the
   header and source instead of claiming renderer success.

The mesh probe covers valid CPU positions/socket transforms and explicit error
paths already implemented. It does not make the unresolved defects above safe.

## Bounded Reproduction

- Normal `experimental_skeletal_mesh_contract` executes safe reproductions for
  NaN weights, infinite inverse-bind data, reversed mesh joint names, and a NaN
  pose matrix. It prints `KNOWN_DEFECT reproduced` for each accepted defect.
- With explicit Ozz fixtures, `experimental_skeletal_ozz_sampling` calls
  `nt_skeletal_anim_copy_model_matrices` before the first sample and requires
  the current erroneous success result before continuing with normal sampling.
- To reproduce the wrap hang, run the Ozz probe in a child process with
  `--known-wrap-time inf` (or `3.4e38`) and an external 2-second timeout, then
  terminate the child. This mode is intentionally never registered in CTest.
- To reproduce mesh-before-instance destruction, build with AddressSanitizer
  and run `experimental_skeletal_mesh_contract_probe --known-uaf` as a child
  process. Require the sanitizer report/non-zero exit; never run it in-process
  or as a normal success test.
- To reproduce the C-ABI exception risk, use allocator fault injection on the
  first `std::vector::assign` in `nt_skeletal_mesh_create` from a C caller. The
  absence of any `catch` boundary is also guarded by the quarantine check.

All dangerous modes require a disposable subprocess. They are evidence only,
not supported operations or acceptance tests.
