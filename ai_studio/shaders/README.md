# Shader analysis

Shared offline entry point for game shader stages. The shader checker remains
owned by [Quality](../quality/check_shaders.md); this module prepares a program
pair, runs that rule, validates GLSL ES 3.00 compilation and linking, and can
collect vendor profiling output. Games keep their own shader sources and build
commands. This tool does not change the engine or a game's release pipeline.

## Commands

From the Studio root:

```powershell
node ai_studio/shaders/cli.mjs doctor
node ai_studio/shaders/cli.mjs rules --root games/private/<game-id>/assets/shaders
node ai_studio/shaders/cli.mjs analyze --root games/private/<game-id>/assets/shaders --vertex slug_text.vert --fragment slug_text.frag --out tmp/shader-analysis-<game-id>/text
node ai_studio/shaders/cli.mjs analyze --root games/private/<game-id>/assets/shaders --vertex slug_text.vert --fragment slug_text.frag --out tmp/shader-analysis-<game-id>/text-mali --mali-gpu Mali-G76
node ai_studio/shaders/cli.mjs analyze --root games/private/<game-id>/assets/shaders --vertex slug_text.vert --fragment slug_text.frag --out tmp/shader-analysis-<game-id>/text-powervr --powervr-bvnc 22.87.104.18,35.3.408.101
```

`--root` can name any game's shader directory. `--vertex` and `--fragment` are
paths inside that directory. Use a separate `--out` for each program or version.
`rules` runs the existing QTECH_002 checker over a whole shader directory.
`--glslang PATH` selects a non-PATH reference compiler. On Windows, `doctor`
and `analyze` find an installed Arm Performance Studio; `MALIOC` or `--malioc`
can override the path. `--powervr-bvnc` requests offline profiling for one or
more PowerVR architectures. The CLI looks for the Windows Profiling Compilers
2025 R2 in ignored `tmp/vendor/powervr/2025_R2`; `POWERVR_COMPILER_DIR` or
`--powervr-dir` selects another extracted compiler directory. Download it from
[Imagination's official downloads page](https://developer.imaginationtech.com/downloads/).
Keep the vendor binaries outside Git. The command exits `0` when requested checks
pass, `1` on a rule or compiler failure, and `2` on missing input or tool.

The output contains `program.vert`, `program.frag`, `glslang.txt`, and
`report.json`. With `--mali-gpu`, it also contains each stage's `.mali.txt`
report. With `--powervr-bvnc`, it adds `powervr/<BVNC>/<stage>/` directories
containing the compiler's `.prof`, `.asm`, and stdout files. `report.json`
records source hashes, dependencies, rule findings, reference link status,
Mali register, occupancy, and instruction-cycle estimates, and PowerVR
instruction counts and available cycle estimates. Mali instruction cycles are
not a hardware instruction count.

The source preparation follows the pack builder's quoted includes,
`#pragma once`, and generated `nt/locations.glsl` from the selected game's
engine pin, then adds the runtime's `#version 300 es` prefix. The builder
also strips comments and collapses whitespace before packing, so current-tree
results are not byte-identical to a shipped pack. For a live incident, first
extract the actual shader stages from the exact release archive and use those
as input; keep the archive and source hashes with the incident evidence.
Embedded C shader strings require extraction to `.vert`/`.frag` files before
this command can analyze them.

## Reading the result

| Check | Establishes | Does not establish |
| --- | --- | --- |
| [QTECH_002](../quality/rules/technical/checks/QTECH_002_shader_rules.md) | Known unsafe `gl_Position` patterns are absent | General GLSL validity |
| [Game pack builder](../../external/neotolis-engine/tools/builder/nt_builder_shader.c) | GL 3.30 compilation and, when the host supports it, ESSL 3.00 compilation of packed stages | Program linking or another GPU's driver behavior |
| [glslangValidator](https://www.khronos.org/opengles/sdk/Reference-Compiler/) | Reference ESSL compile and its available link checks | A device driver's acceptance, complete link validation, or GPU instruction count |
| [Arm Mali Offline Compiler](https://developer.arm.com/documentation/101863/2026-5/Using-Mali-Offline-Compiler/Compiling-OpenGL-ES-shaders) | Model-specific Mali register, occupancy, and cycle estimates | PowerVR behavior |
| [PowerVR Profiling Compilers](https://docs.imgtec.com/performance-guides/graphics-recommendations/html/topics/pvrshadereditor.html) | Offline PowerVR stage compilation and profiling for supported architectures | Retail browser linking or context stability |

The PowerVR profiling compiler accepts each prepared stage separately. A pass
does not prove WebGL program linking or browser-driver behavior. For a
context-loss incident, pair
offline checks with full browser shader/program info logs and a physical-device
trace. [PVRCarbon's Android recorder](https://docs.imgtec.com/tools-manuals/pvrcarbon-manual/html/topics/pvrcarbon-recorder/opengl-es.html)
requires an eligible debug app or device configuration; a normal Chrome capture
is not assumed to work.

## Browser and device tools

The next useful addition is an optional
[ANGLE translator](https://chromium.googlesource.com/angle/angle/+/HEAD/doc/DevSetup.md)
check for WebGL shader translation; it requires a separate ANGLE build.
[Spector.js](https://github.com/BabylonJS/Spector.js) can capture WebGL calls,
shader sources, and program logs in a real browser. It is the most direct next
tool for the reported program-link failure. The
[WebGL Conformance Test Suite](https://github.com/KhronosGroup/WebGL) provides
broader browser compatibility checks and suits a slower release lane.
[RenderDoc Android](https://github.com/baldurk/renderdoc/blob/v1.x/docs/how/how_android_capture.rst)
requires a capturable Android application; it is a separate route for native
GLES diagnosis. These tools do not emulate a retail PowerVR browser driver.

## Ownership and validation

This module owns source preparation, offline compiler invocation, and report
format. Quality owns writing rules; Runtime Automation owns browser and device
captures. Run `node --test ai_studio/shaders/cli.test.mjs` for this module.
Studio's harness owner check includes the same test.
