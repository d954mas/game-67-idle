# Character Models

Use for character geometry, face readability or animation proof.

- Judge the model in the game's pose, light and camera. Bind-pose renders do not
  establish gameplay readability.
- Preserve human proportions; do not enlarge, raise or tilt a face to compensate
  for a high camera.
- For a small texture-based face, use a flat face region and low-resolution RGBA.
  Keep skin color in fully transparent texels to avoid dark filtering fringes.
  Bend custom normals toward one up-and-forward direction, fading at the sides,
  chin and neck, so hemisphere ambient does not turn a mouth into a dark beard.
- Check holes and poke-through, including in motion, before judging polish.
- Compare with the previous result and last accepted look as well as the reference.
  If local deformations damage the whole, rebuild the affected construction from
  a clean model rather than accumulating compensations.
