# Verification And Prompts

Use for game-use prompts, image acceptance or failed generation diagnosis.

## Establish Real Output

Require an actual image-generation tool result. An agentic CLI must return a real
image or state that generation is unavailable; code-drawn SVG, shapes, PIL or
similar substitutes do not satisfy a raster-art request.

Inspect the delivered file rather than trusting the transcript. Confirm it
opens, has the expected dimensions and channels, and matches the task visually.
Keep tool/provider provenance and the source hash. Compressed size varies with
content and transparency; neither a size threshold nor appearance proves origin.

## Composable Game Assets

- For runtime bars, generate the empty frame and implement the fill in-engine.
- For slots, panels and buttons, keep dynamic content and text separate.
- For keyed character art, use a flat key color to the outline and omit unwanted
  drop/contact shadows that would contaminate the cutout.
- For style comparison, hold composition steady and vary the intended dimension.
  Choose candidate count from the requested scope and spending limit.

Request genuine transparency when the selected tool supports it. Preserve valid
alpha and inspect the edges on contrasting backgrounds. For opaque output,
select a preparation method from
`ai_studio/assets/canvas/contracts/alpha-and-cleanup.md`; an aligned dual plate
is one option, not a requirement for every cutout.

## Recovery

Distinguish unavailable tools, authentication failures, transport errors and poor
art direction. Retry only when a prompt or environment change addresses the
observed failure. Use current tool documentation for setup; keep TLS validation
and host permissions intact. Report unresolved gaps instead of substituting a
code-drawn image or claiming an uninspected result is ready.
