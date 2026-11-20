# Install net bits

## Link

Add the in-place module after defining the consuming target. It needs only
libc and libm.

```cmake
set(NET_BITS_DIR "${GAME_REPO_ROOT}/features/net-bits")
add_subdirectory("${NET_BITS_DIR}" "${CMAKE_BINARY_DIR}/net-bits")
target_link_libraries(game PRIVATE net_bits)
```

Link the `net_bits` target; do not compile its sources into another target.
The target carries `-ffp-contract=off -fno-fast-math`, which both ends need to
reconstruct the same numbers, and the sources refuse to compile under
fast-math or excess precision.

Record the feature source and version from `feature.json` in the consuming
game's `dependencies.json`.

## Use

1. Describe each record as a `static const net_bits_field_t` table built from
   the `NET_BITS_*` macros, and call `net_bits_layout_init` once at start-up
   with `sizeof` the record struct. A table that fails is a bug in the table:
   stop there. Pick ranges by README.md, "Range pitfalls".
2. Mix every layout's `hash` and the protocol version into one value with
   `net_bits_hash_mix`, in a fixed order, and compare it in the handshake.
3. To send: `net_bits_writer_init` over the packet buffer, ask
   `net_bits_writer_room` before each record, write, then
   `net_bits_writer_finish` for the size. Zero means a value or the room was
   wrong.
4. To receive: `net_bits_reader_init`, read in the same order while `ok` and
   `net_bits_reader_left` allow, then drop the packet unless `reader.ok` and
   `net_bits_reader_done`. Members of a record that failed are not to be used.
5. A relay forwards the values it decoded once; it never re-encodes a value it
   decoded from its own re-encoding.

## Verify

```powershell
cmake --build ai_studio/feature_testing/consumer/build/feature-verify --target test_net_bits test_net_bits_record
ctest --test-dir ai_studio/feature_testing/consumer/build/feature-verify -R "^test_net_bits" --output-on-failure
node features/validate_contracts.mjs
```
