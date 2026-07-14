# Canvas animation contract

An animation card is a `group.anim`; its member images are generation inputs.
Generation runs outside the project lock, then refuses a moved project/history
head before the short final commit.

The result is an editable `element.flipbook` beside the card in its parent
scope. Frames are immutable project content; resolved seed, profile, matte,
fps, keyframes, and run provenance are recorded with the result. Page playback
is view state and uses only kept frames at the generated fps.

Browser generation controls remain frozen by the product decision recorded in
T0265. Offline playback and ops/CLI/API parity remain supported. The procedural
`element.animation` uses that same operation parity; it is not a second product
workflow.
