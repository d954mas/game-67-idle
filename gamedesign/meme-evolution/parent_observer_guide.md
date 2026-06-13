# 67 World Parent Observer Guide

Use this guide before a manual child-test or first family play session. It is
not a script for coaching the child; it tells the adult what the package is and
what to watch for.

## What This Build Is

67 World is a child-friendly native PC merge/evolution game. The child clicks
`TAP BOX`, makes matching pairs, unlocks new 67 variants, buys simple upgrades,
and uses `FREE SLOT` when the board is full.

## Safety And Privacy

- The package runs locally from the extracted folder.
- The game does not need an account, login, network connection, camera, or
  microphone.
- The package does not include ads, purchases, external links, or chat.
- For child-tests, the adult may create a markdown report with
  `CREATE_CHILD_TEST_REPORT.bat`; only return notes, screenshots, or video that
  you intentionally choose to share.

## Before The Child Plays

1. Extract the full zip.
2. Open `VERIFY_PACKAGE.bat` and confirm it says `PASS`.
3. Read `CHILD_TEST_ACCEPTANCE.md`.
4. Start the session with `START_CHILD_TEST_FRESH.bat`.
5. Keep real speaker/headphone audio on at a comfortable volume.

## What To Observe

Do not explain the whole game first. Let the first screen teach the child.

Watch whether the child can:

- find `TAP BOX`;
- understand that two matching 67s combine;
- notice the new 67 after a merge;
- understand `BUY` on the upgrade tile;
- recover from a full board with `FREE SLOT`;
- keep reading the screen from normal distance;
- enjoy the sounds without irritation.

## When To Stop

Stop early if the child is frustrated, bored, unable to find the next action,
cannot read important text, dislikes the sound, or the game crashes/opens
without art.

## After The Session

Run `CREATE_CHILD_TEST_REPORT.bat`, fill the new file in
`child_test_results/`, and put any chosen screenshots, short videos, or
audio-note files in `child_test_results/evidence/`. Then run
`VALIDATE_CHILD_TEST_REPORT.bat`; if it passes, run
`EXPORT_CHILD_TEST_RESULTS.bat` and return `child_test_results_for_return.zip`.
