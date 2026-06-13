# 67 World Child-Test Acceptance Kit

Use this sheet with the native PC release package. It is for one child plus one
adult observer. Do not coach beyond reading the controls if the child asks.

## Session Setup

- Package tested:
- Date:
- Tester:
- Child age:
- Device:
- Speaker/headphones:
- Build folder:

## Run

1. Extract the full zip.
2. Open `67-world/VERIFY_PACKAGE.bat` and confirm it says `PASS`.
3. Open `67-world/START_CHILD_TEST_FRESH.bat`.
4. Confirm the game starts from the first `TAP BOX` FTUE state.
5. Let the child play for up to 60 minutes, or stop earlier if they lose
   interest, get stuck, or the adult sees a clear failure.
6. After the session, open `67-world/CREATE_CHILD_TEST_REPORT.bat`, fill the
   new report in `67-world/child_test_results/`, and put any chosen
   screenshots, short videos, or audio-note files in
   `67-world/child_test_results/evidence/`. Each `Notes:` line and the
   `Observer summary` must contain a real observation for release acceptance.
7. Run `67-world/VALIDATE_CHILD_TEST_REPORT.bat`.
8. If validation passes, run `67-world/EXPORT_CHILD_TEST_RESULTS.bat` and
   return `67-world/child_test_results_for_return.zip`.

## First Minute

Record what happens before explaining the game.

- Child finds `TAP BOX`: yes / no
- Child understands matching pairs: yes / no
- Child completes the first merge: yes / no
- Text is readable from normal distance: yes / no
- Audio feedback is audible: yes / no
- Notes:

Pass: the child can spawn and merge at least once with no more than one hint.

## Five-Minute Loop

- Child keeps spawning/merging without confusion: yes / no
- Child notices new 67 variants: yes / no
- Child understands the upgrade tile when it shows `BUY`: yes / no
- Child can recover when the board is full using `FREE SLOT`: yes / no
- The screen stays readable and not visually overloaded: yes / no
- Notes:

Pass: the child can continue playing, buy at least one upgrade, and recover
from a full board.

## One-Hour Progression

Run until 60 minutes or until the child clearly wants to stop. For release
acceptance, the final report must show at least 55 minutes for both session
length and minutes played; shorter sessions should be marked `fail` or
`needs tuning` with notes.

- Minutes played:
- Highest 67 reached:
- Collection count:
- Better Crate level if visible:
- Did the child still understand the next goal near the end: yes / no
- Did progress feel too slow: yes / no
- Did progress feel too fast: yes / no
- Notes:

Pass: the child stays engaged long enough to see repeated evolution rewards and
the game still communicates the next useful action.

## Audio

Check real speakers/headphones, not DevAPI.

- Spawn sound audible: yes / no
- Merge sound audible: yes / no
- Upgrade sound audible: yes / no
- Blocked/full-board sound audible: yes / no
- Free slot/recycle sound audible: yes / no
- Sounds are pleasant for children: yes / no
- Notes:

## Stop Conditions

Mark any that happened.

- Child cannot identify the next action.
- Child cannot read important text.
- Child gets stuck on a full board.
- Child ignores upgrades because they are unclear.
- Audio is missing, harsh, or annoying.
- Visuals feel messy, scary, or boring.
- App crashes, opens without art, or cannot be launched.
- Other:

## Final Acceptance

Overall result: pass / fail / needs tuning

Required fixes before release:

- none

Nice-to-have fixes:

- none

Observer summary:
