# Plain tests

Drop a `test_<name>.c` here and it is built and registered automatically: no
CMake edit, no entry in a list. Each file gets Unity, the game's `src` include
root and the golden bank helper, and runs from `build/tests`.

A test belongs here when it needs nothing else. The moment it has to link a
subsystem, a generated fragment, or an engine library, give it an explicit
`game_add_c_test` call in `cmake/GameTests.cmake`, where the reader can see what
it pulls in.

What to test in a game, and what never to pin, is the game test policy in the
studio `AGENTS.md`.
