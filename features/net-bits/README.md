# Net bits

## Purpose

A bit-level codec for game state that crosses a network. Each value takes the
fewest bits its stated range and precision need, and a record (a C struct) is
described once by a table that says how each member travels. The writer and
the reader both walk that table, so they cannot disagree, and the table's hash
names the wire format: a session (`coop-net`) compares hashes in its handshake
and refuses a peer whose build would read its packets as noise. `body-sync`
defines its body records with these tables.

The read side takes bytes from another player, which nobody vouches for. A
truncated, oversized or garbage buffer gives a refusal, never an
out-of-bounds read or write, an assert, a NaN or a value outside the table.
No allocation, no global state, no dependency beyond libc and libm.

## Public surface

| Header | What it gives |
|---|---|
| `features/net_bits/net_bits.h` | Writer and reader over a caller's buffer (`net_bits_writer_*`, `net_bits_reader_*`), raw `net_bits_write` / `net_bits_read` of 1..32 bits, `net_bits_width`, whole numbers in a range (`net_bits_write_int` / `read_int`), reals on a grid (`net_bits_range_t`, `net_bits_range_width`, `net_bits_quantize`, `write_ranged` / `read_ranged`), rotations as "smallest three" (`write_quat` / `read_quat`, `net_bits_quat_error`). |
| `features/net_bits/net_bits_record.h` | Field kinds, table lines (`NET_BITS_BOOL`, `UINT`, `INT`, `RANGED`, `QUAT`), `net_bits_layout_init` (validates, computes `bits` and `hash`), `net_bits_write_record` / `read_record`, `net_bits_hash_mix`. |

The writer and reader are plain structs on the stack; copying one remembers a
position, and a writer restored from a copy writes over what followed.

How to use the result:

- A writer or reader that refused keeps `ok == false` and ignores every later
  call. Check `ok` once at the end; `net_bits_writer_finish` returns zero for a
  refused writer, and `net_bits_reader_done` is the last check of a packet.
- A refused read returns a harmless value: `min` for ints and reals, no
  rotation for a quaternion, zero for a raw read.

Member types per kind, checked at compile time by the table macros:

| Kind | Member | Bits on the wire |
|---|---|---|
| `BOOL` | `bool` | 1 |
| `UINT` | `uint32_t` | `bits`, 1..32 |
| `INT` | `int32_t` | `net_bits_width(int_max - int_min + 1)`, 0..32 |
| `RANGED` | `float` | `net_bits_range_width(range)` |
| `QUAT` | `float[4]` x, y, z, w | `2 + 3 * bits`, `bits` 4..20 |

## Wire rules

Everything a second implementation needs to read and write the same bytes.

**Arithmetic.** Both ends evaluate in IEEE-754 single and double precision
(`FLT_EVAL_METHOD` 0), round to nearest, with no contraction into fused
multiply-adds and no fast-math. Fast-math, finite-math-only and excess
precision are refused at compile time. Contraction is turned off by the
target's build flag (`-ffp-contract=off -fno-fast-math`) or, under Clang, by
`#pragma STDC FP_CONTRACT OFF`. The known answers below catch a build that
still evaluates differently. Between such builds everything on the wire and
every value read is bit-identical.

**Bits.** The first bit written is the least significant bit of byte 0; a
value of several bits goes low bits first, so a value that crosses a byte
boundary keeps its low part in the earlier byte. The last byte is filled with
zero bits. A packet is well formed only when, after its last value, nothing is
left but those zero bits: spare bytes or a set padding bit are a malformed
packet (`net_bits_reader_done`). There is no header, length or alignment.

**Whole numbers** in `[min, max]`: the code is `value - min` in
`width = net_bits_width(max - min + 1)` bits (none when `min == max`). Writing
a value outside the range is a caller bug and clears `ok`; reading a code past
`max - min` clears `ok`.

**Reals** on a range `{min, max, step}`, all arithmetic in IEEE-754 double from
the float inputs, round to nearest, no fused multiply-add:

- `last = ceil((max - min) / step)`; the grid is `min + i * step` for
  `i = 0 .. last`, so its top point is the first one at or past `max`, within
  the rounding of that division;
  `width = bits of last` (the smallest `w` with `last < 2^w`).
- The range is malformed (width 0, every write and read refused) when any of
  min, max, step is not finite, `max <= min`, `step <= 0`,
  `max(|min|, |min + last * step|)` exceeds `FLT_MAX`, or the step is below two
  float spacings at that magnitude. The spacing of `x` is `2^(e - 24)` with
  `x = f * 2^e`, `0.5 <= f < 1` (`2^-149` below `FLT_MIN`). This keeps each
  decoded point within a quarter step of its exact value, so every grid point
  re-encodes to its own code. It also caps `last` near `2^24`, since the span
  is at most twice that magnitude: a ranged field holds at most 25 bits.
- Encode: `t = (value - min) / step`. Not `t > 0` (below min, NaN, -inf): code
  0. `t >= last` (past the top point, +inf): code `last`. Otherwise
  `floor(t + 0.5)`, ties toward max.
- Decode: a code past `last` is refused; else `(float)(min + code * step)`.
- Result: inside `[min, max]` the error is at most half a step plus the float
  rounding of the value read: half a float spacing at that value, and nothing
  when grid points are exact floats (a power-of-two step with `min` on it).
  At the two-spacing limit that is up to three quarters of a step. A grid
  point reads back bit for bit and re-encodes to the same code, so a relayed
  value never drifts.

**Rotations**, quaternion order x, y, z, w, `A = 0.70710678118654752440`
(the double nearest `1/sqrt(2)`), `mid = 2^(bits - 1) - 1`, `unit = A / mid`:

- Write: a quaternion with a non-finite component or all zero becomes
  (0, 0, 0, 1). Otherwise divide by the largest absolute component, then by
  the length. The dropped component is the largest in absolute value, the
  lowest index on a tie; flip all four if it is negative.
- Wire: the dropped index in 2 bits, then the other three in index order,
  each `code = mid + floor(c / unit + 0.5)`, clamped to `0 .. 2 * mid`. Code
  `mid` is exactly zero, so an axis-aligned rotation reads back exactly. The
  one spare code `2^bits - 1` is refused.
- Read: `c = (code - mid) * unit`; `rest = 1 - (c0^2 + c1^2 + c2^2)` summed in
  index order; the dropped component is `sqrt(rest)`, or 0 when `rest <= 0`;
  divide all four by `sqrt(sum + dropped^2)` and convert to float. Any bit
  pattern that is not refused gives a finite unit quaternion.
- Relays: a rotation read is not guaranteed to re-encode to the same codes.
  Near a tie of the largest component the dropped index can flip, and the
  error then adds up per hop. A relay forwards the value it decoded once and
  never re-encodes a re-decoded one.

**Refusal** means `ok` cleared: a read past the end, a width out of range, a
code outside its field's table (int past max, real past the grid, the
quaternion's spare code), a malformed range or table line, a layout with no
bits, or a writer or reader whose fields were set past its buffer or to no
data with a non-zero size. A refused writer wrote nothing for the call that
failed and nothing outside its buffer. A refused reader, and every read after
it, returns 0 from `net_bits_read`, `min` from `read_int` and `read_ranged`,
and no rotation from `read_quat`. A record whose read failed must not be used.
A parser loops on `ok` and `net_bits_reader_left`, and checks
`net_bits_reader_done` once at the end.

## Rotation error bound

`net_bits_quat_error(bits)` is a true upper bound on the angle between the
rotation written and the one read, with q and -q the same rotation.

Let the written unit quaternion be `q = (s, a)`: `s` the three kept
components, `a` the dropped one. Because `a` is the largest, `a >= 1/2`, so
`|s| <= r = sqrt(3)/2`, and each kept component is within `[-A, A]`. Rounding
moves each kept component by at most `h = unit / 2`, so the kept part moves by
`|e| <= d = sqrt(3) * h`. Without clamping (`r + d < 1`, which holds at every
width from 4) the read quaternion is `p = (c, b)` with `b = sqrt(1 - |c|^2)`:

- `|b - a| = ||s|^2 - |c|^2| / (a + b) <= d (2r + d) / (1/2 + sqrt(1 - (r + d)^2))`,
  since the right side grows with `|s|` and `|s| <= r`;
- the chord `|p - q| <= sqrt(d^2 + |b - a|^2)`, plus `2^-22` for the float
  rounding of the result and of the check's renormalisation;
- the angle between unit 4-vectors is `2 asin(chord / 2)` and the rotation
  angle is at most twice that: `bound = 4 asin(chord / 2)`, rounded up to float.

The worst case is the corner (1/2, 1/2, 1/2, 1/2) with every rounding error
outward, so the bound is tight to first order; the tests reach 0.7 to 0.98 of
it. Some values: 4 bits 24.9°, 8 bits 1.1°, 9 bits about 0.55°, 12 bits
0.069°, 16 bits 0.0043°. A width outside 4..20 returns pi.

## Table hash

`layout.hash` is FNV-1a 64 (basis `0xcbf29ce484222325`, prime
`0x100000001b3`) over one byte naming the wire rules of this document
(currently `1`), then 22 bytes per line, in table order:

| Bytes | Content |
|---|---|
| 0 | kind |
| 1 | bits the line puts on the wire (BOOL 1, UINT `bits`, INT and RANGED their width, QUAT `2 + 3 * bits`) |
| 2..5, 6..9 | INT: `int_min`, `int_max`, two's complement little-endian; else zero |
| 10..13, 14..17, 18..21 | RANGED: IEEE-754 bit patterns of `min`, `max`, `step`, little-endian; else zero |

Offsets, member names and members a kind does not use are not hashed: two
structs with the same wire table share a hash. `net_bits_hash_mix(hash, v)`
continues FNV-1a over the 8 little-endian bytes of `v`, so several tables and a
protocol version fold into one handshake value; order matters. A range written
with `-0.0` hashes differently from `0.0` (its first point reads back as
`-0.0`).

## Worked example

A body copy for interpolation: number, position at 512 per metre, rotation at
9 bits, sleep flag.

```c
typedef struct body_copy {
    uint32_t id;
    float x, y, z;
    float rotation[4];
    bool asleep;
} body_copy_t;

static const net_bits_field_t BODY_COPY[] = {
    NET_BITS_UINT(body_copy_t, id, 10),
    NET_BITS_RANGED(body_copy_t, x, -32.0, 32.0 - 1.0 / 512, 1.0 / 512),
    NET_BITS_RANGED(body_copy_t, y, -32.0, 32.0 - 1.0 / 512, 1.0 / 512),
    NET_BITS_RANGED(body_copy_t, z, 0.0, 8.0 - 1.0 / 512, 1.0 / 512),
    NET_BITS_QUAT(body_copy_t, rotation, 9),
    NET_BITS_BOOL(body_copy_t, asleep),
};

net_bits_layout_t layout;
net_bits_layout_init(&layout, BODY_COPY, 6, sizeof(body_copy_t)); /* bits = 10 + 15 + 15 + 12 + 29 + 1 = 82 */
```

`x` has `last = 32767` points past min, so 15 bits. With `max = 32.0` the grid
would need a 32769th point and 16 bits: when the span is a power-of-two count
of steps, put `max` one step lower. Writing `x = 1.3` sends code
`floor((1.3 + 32) * 512 + 0.5) = 17050`; the reader gets
`-32 + 17050 / 512 = 1.30078125`, within half a step (`1/1024`).

## Range pitfalls

Measured with `net_bits_range_width` and `net_bits_quantize`:

| Range `{min, max, step}` | Bits | 0.0 reads as | Why |
|---|---|---|---|
| `{-32, 32 - 1/32, 1/32}` | 11 | `0` | power-of-two step, min a multiple of it, max one step lower |
| `{-32, 32, 1/32}` | 12 | `0` | 2048 steps of span need a 2049th point and one more bit |
| `{-32, 32, 64/2047}` | 12 | `0.0156326` | the step rounds in float, so the span is a hair over 2047 steps: one more point and bit; 32 is 1023.5 steps, so zero falls between points |
| `{-32, 32, 0.03}` | 12 | `0.0099993` | min is not a multiple of the step, so zero is not a grid point |

For body records: take a power-of-two step, a `min` that is a multiple of
it, and `max` one step lower than `min` plus a power-of-two number of steps.
Then every grid point is an exact float, zero is on the grid and no bit is
wasted.

## Known answers

Vectors a second implementation must reproduce bit for bit; the tests check
each of them.

- The worked example: `layout.bits == 82`, `layout.hash ==
  0x7B6EACCC409DC632`. The record `{id 517, x 1.3, y -20.25, z 1.5, rotation
  (0.2, 0.4, -0.4, 0.8), asleep true}` is the 11 bytes
  `05 6A 0A 01 2F 00 F3 D1 C7 6F 02` (codes 517, 17050, 6016, 768, dropped 3,
  327, 399, 111, 1). It reads back as x `0x3FA68000`, y `0xC1A20000`,
  z `0x3FC00000` and rotation `0x3E4C7204 0x3ECC7204 0xBECC7204 0x3F4CFFCC`.
- The rotation (0.2, 0.4, -0.4, 0.8) at 9 bits alone is `1F 7D FC 06`.
- Range `{-10, 10, 0.3}`: 7 bits, top code 67. Codes 0, 1, 33, 34, 66, 67
  read `0xC1200000 0xC11B3333 0xBDCCCC98 0x3E4CCCE8 0x411CCCCE 0x4121999A`.
  Range `{-3, -2, 0.25}` has top code 4.
- Rotations at 8 bits whose kept components sum to one within rounding, so a
  fused multiply-add reads a tiny dropped component instead of zero:
  `DE 50 08 00` reads `0xBECD4012 0xBF18832A 0x00000000 0xBF322B2C`, and
  `DF A8 0B 00` reads `0xBECD4012 0x3F18832A 0xBF322B2C 0x00000000`.
- A reader that normalises in float instead of double: at 4 bits, `03 0C`
  (dropped index 3, codes 0, 0, 3) reads component 0 as `0xBF27D610`; float
  division gives `0xBF27D60F`.
- Ties: in range `{0, 1, 0.25}` the value 0.125 is code 1 (toward max); the
  rotation (0.5, -0.5, 0.5, 0.5) drops index 0, (0.1, 0, 0.6, 0.6) drops
  index 2.

## Validation

`test_net_bits` covers the known answers, the bit order, stream round trips at random widths,
refused values that do not fit, writers out of room (guard bytes around every
buffer), a writer restored from a copy, `reader_done`, `net_bits_width`, int
round trips and codes past max, the half-step error over nine ranges (tiny and
huge steps, negative mins, spans that are not a multiple of the step) and
their edges, grid points as fixed points, codes past the grid, malformed
ranges, the rotation bound over uniform, axis-aligned and corner rotations at
every width, scaled, zero and NaN inputs, the spare code, seeded garbage
through every reader and every truncation of a valid packet.
`test_net_bits_record` covers a record with every kind, `layout.bits` against
the bits written, writer refusals, the members kept by a failed read, every
truncation, garbage records, the hash (same for reordered structs, different
for any change on the wire, equal to FNV-1a over the canonical form),
`hash_mix` order and malformed tables. Readers get exact-size heap copies, so
ASan sees any read past the end. Both carry the CTest label `core`, and
`node features/validate_contracts.mjs` checks this contract.

## Compatibility

`feature.json.version` is exact SemVer. A PATCH fixes behaviour to match this
contract without changing a byte on the wire or a hash. A MINOR adds calls or
field kinds whose existing tables keep their bytes and hashes. A MAJOR changes
the wire: the bit order, an encoding or rounding rule, a refusal, the
quaternion grid, the canonical hash form, or the `net_bits_field_t` layout.
A MAJOR that changes how values are encoded also raises the wire rules byte,
so every table hash changes with it and peers on different rules refuse each
other in the handshake.

## Extension points

A game or a feature above writes its own tables, picks ranges and widths
(`net_bits_quat_error` and the half-step rule give the precision), and mixes
its tables and its protocol version into one handshake value. Packet headers,
delta compression and priorities belong to the layers above; they use the raw
writer and reader for their own fields. Install: [INSTALL.md](INSTALL.md).
