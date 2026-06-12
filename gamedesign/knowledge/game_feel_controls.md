# Game Feel And Controls

Reusable rules for making input, timing, movement, and moment-to-moment feedback
feel responsive and pleasant.

## Goal

Game feel is the player's confidence that input matters. A feature should not
only accept input and update state; it should respond quickly, clearly, and with
enough texture that repeating the action feels intentional rather than mechanical.

## Core Principles

- Input feedback should happen immediately, even if the full action resolves later.
- The player should understand whether input was accepted, blocked, queued, or ignored.
- Repeated actions need variation, rhythm, or progression to avoid feeling dead.
- Animation should clarify state, not delay control.
- Timing should be forgiving during learning and tighter only when mastery is the fantasy.
- A blocked action should feel informative, not broken.
- Controls should keep working during non-critical reward feedback.
- Feel should be tested in real play, not only by inspecting state changes.

## Control Checklist

- What is the primary input?
- What confirms the input within the first frame or first few frames?
- Can the input be repeated, buffered, held, cancelled, or interrupted?
- What happens when input arrives too early, too late, or during animation?
- What is the blocked-state feedback?
- What is the recovery action after a mistake?
- What changes as the player progresses: speed, yield, animation, rhythm, options?
- Which feedback is visual, audio, haptic, camera, UI, or state-based?
- What feel parameters are tunable without rewriting the mechanic?

## Feel Tuning Knobs

- Input cooldown.
- Animation anticipation.
- Impact duration.
- Recovery duration.
- Reward delay.
- Repeat cadence.
- Camera shake strength.
- Screen flash strength.
- Particle count.
- Sound variation.
- Ease curve.
- Auto-repeat speed.

## Anti-Patterns

- Input is accepted but nothing visible happens immediately.
- Animation locks controls longer than the player expects.
- A button can be pressed while disabled but gives no explanation.
- Every repeated action plays the exact same long effect.
- The game queues input invisibly and then surprises the player.
- Feedback is visually loud but does not say what changed.
- Feel is tuned only on desktop when touch is a target.

## Validation

- A tester can tell whether each input was accepted or blocked.
- The primary action feels responsive on the target input methods.
- Repeated input does not hide goals, rewards, or counters.
- Mistakes produce recovery information, not confusion.
- Reward feedback does not prevent the next meaningful action unless intentionally paced.
- Feel parameters can be adjusted after playtest without changing feature rules.

## Links

- Use [Core Loop](core_loop.md) to identify the repeated action that must feel good.
- Use [Reward Feedback](reward_feedback.md) for consequence feedback.
- Use [Mobile/Web Platform Design](mobile_web_platform.md) for touch and browser constraints.
- Use [Accessibility](accessibility.md) for motion, timing, and input assist considerations.
- Use [Playtest Validation](playtest_validation.md) to prove feel in a build.

