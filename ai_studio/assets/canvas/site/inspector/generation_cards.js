import { fileUrl, focusStage, memberElements, setStatus } from "../app.js";
import { generateAnimFromCardAction, patchAnimAction, patchStyleAction } from "../actions.js";
import { field, selectInput, textareaInput } from "./controls.js";
import { collapsible, openPromptModal, smallBtn } from "./primitives.js";
// ---- style card (T0239 increment 3) --------------------------------------------

// Style card surface: additive, shown only when the selected group carries a `style` blob
// (same pattern as renderRecipe). Prompt is live-editable through patchStyleAction (+ the
// SAME openPromptModal seam the Recipe prompt uses, reused verbatim for the large editor).
// Members lists every IMAGE member (reuses the Generation section's plate-thumb row shape,
// design R1): the current ref shows a "ref" badge, every other image gets a "Make ref"
// button. Non-image members (text) never show here — a style card's ref/examples are images
// only (design R1).
export function renderStyle(group, root) {
  const style = group.style;
  if (!style || typeof style !== "object") return;
  const body = collapsible(root, "style", "Style");

  const promptField = field("Prompt", textareaInput(style.prompt, (next) => patchStyleAction(group.id, { prompt: next })));
  body.appendChild(promptField);

  const editPromptBtn = smallBtn("Edit", () =>
    openPromptModal(group.name || "Style prompt", style.prompt, (next) => patchStyleAction(group.id, { prompt: next })),
  );
  editPromptBtn.classList.add("insp-prompt-edit-btn");
  editPromptBtn.title = "Open the style prompt in a large editor";
  body.appendChild(editPromptBtn);

  const membersTitle = document.createElement("div");
  membersTitle.className = "insp-align-caption";
  membersTitle.textContent = "Members";
  body.appendChild(membersTitle);

  const images = memberElements(group.id).filter((element) => element.type === "image");
  if (!images.length) {
    const empty = document.createElement("div");
    empty.className = "insp-region-hint";
    empty.textContent = "Drag images into this card — the first one auto-becomes the ref.";
    body.appendChild(empty);
  } else {
    const wrap = document.createElement("div");
    wrap.className = "insp-alpha-plates"; // reuse: same stacked thumb-row layout as the plate/reference lists
    images.forEach((image) => {
      const row = document.createElement("div");
      row.className = "insp-plate-row";
      const img = document.createElement("img");
      img.className = "insp-plate-thumb";
      img.src = fileUrl(image);
      img.alt = image.name || "";
      img.title = image.name || image.id;
      const label = document.createElement("span");
      label.className = "insp-plate-role";
      label.textContent = image.name || image.id;
      row.append(img, label);
      if (style.ref === image.id) {
        const badge = document.createElement("span");
        badge.className = "insp-style-ref-badge";
        badge.textContent = "ref";
        row.appendChild(badge);
      } else {
        row.appendChild(smallBtn("Make ref", () => patchStyleAction(group.id, { ref: image.id })));
      }
      wrap.appendChild(row);
    });
    body.appendChild(wrap);
  }
}

// ---- animation card (T0265 increment 1, video route) ---------------------------

// Animation card surface: additive, shown only when the selected group carries an `anim`
// blob (design §1.1 — same "presence of the additive field" pattern as renderRecipe/
// renderStyle). Motion + Profile + Matte + Seed + Loop are live-editable through
// patchAnimAction (one journal entry per commit, mirrors every other inspector field).
// Generate runs the video route via generateAnimFromCardAction (long-op queue, minutes;
// disabled on an empty motion — the op refuses loudly anyway, the disable just says WHY up
// front). Increment 1 covers the generation inputs only; the frame-editing animation mode
// (timeline/trim/fps/play_mode/takes/export) is increment 2. Mirrors renderRecipe.
export function renderAnim(group, root) {
  const anim = group.anim;
  if (!anim || typeof anim !== "object") return;
  const body = collapsible(root, "anim", "Animation card");

  const motionField = field("Motion", textareaInput(anim.motion, (next) => patchAnimAction(group.id, { motion: next })));
  body.appendChild(motionField);

  const editMotionBtn = smallBtn("Edit", () =>
    openPromptModal(group.name || "Motion", anim.motion, (next) => patchAnimAction(group.id, { motion: next })),
  );
  editMotionBtn.classList.add("insp-prompt-edit-btn");
  editMotionBtn.title = "Open the motion description in a large editor";
  body.appendChild(editMotionBtn);

  body.appendChild(
    field(
      "Profile",
      selectInput(anim.profile || "draft", ["draft", "final"], (next) => patchAnimAction(group.id, { profile: next })),
    ),
  );

  body.appendChild(
    field(
      "Matte",
      selectInput(anim.matte || "corridorkey", ["corridorkey", "key_matte"], (next) => patchAnimAction(group.id, { matte: next })),
    ),
  );

  // Seed: blank = null (random on each Generate); a number pins it. A text input keeps
  // "clear = random" one clean gesture (a number input can't tell empty from 0). The op
  // re-validates (number|null) loudly.
  const seedInput = document.createElement("input");
  seedInput.type = "text";
  seedInput.className = "insp-input";
  seedInput.placeholder = "random";
  seedInput.value = anim.seed == null ? "" : String(anim.seed);
  seedInput.title = "Blank = random on each Generate; a number pins the seed";
  const commitSeed = () => {
    const raw = seedInput.value.trim();
    if (raw === "") {
      if (anim.seed != null) patchAnimAction(group.id, { seed: null });
      return;
    }
    const n = Number(raw);
    if (!Number.isFinite(n)) {
      // F5: invalid input is LOUD — say why and restore the field to the committed value
      // (empty = random) instead of silently leaving unsaved bad text in the box.
      setStatus("Seed must be a number (or empty for random).", true);
      seedInput.value = anim.seed == null ? "" : String(anim.seed);
      return;
    }
    if (n !== anim.seed) patchAnimAction(group.id, { seed: n });
  };
  seedInput.addEventListener("change", commitSeed);
  seedInput.addEventListener("keydown", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      seedInput.blur();
      focusStage();
    }
  });
  body.appendChild(field("Seed", seedInput));

  // Loop hint (design §1.1): seamless-loop hint for generation (a single keyframe becomes a
  // same-image FLF). NOT the playback loop — that lives on the result (flipbook.play_mode).
  const loopRow = document.createElement("label");
  loopRow.className = "insp-check";
  const loopCheck = document.createElement("input");
  loopCheck.type = "checkbox";
  loopCheck.checked = anim.loop !== false;
  loopCheck.addEventListener("change", () => patchAnimAction(group.id, { loop: loopCheck.checked }));
  const loopLabel = document.createElement("span");
  loopLabel.textContent = "Loop";
  loopRow.append(loopCheck, loopLabel);
  body.appendChild(loopRow);

  const generateBtn = document.createElement("button");
  generateBtn.type = "button";
  generateBtn.className = "primary insp-btn";
  generateBtn.textContent = "Generate";
  const emptyMotion = !String(anim.motion || "").trim();
  generateBtn.disabled = emptyMotion;
  generateBtn.title = emptyMotion ? "Describe the motion first" : "Generate the animation (video route — takes minutes)";
  generateBtn.addEventListener("click", () => generateAnimFromCardAction(group.id, generateBtn));
  body.appendChild(generateBtn);
}
