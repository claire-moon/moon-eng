# Contributing

Development is branch- and pull-request based. `main` must remain buildable.

1. Create a focused branch from current `main`.
2. Build and run the relevant automated tests.
3. Update the TEST COCKPIT plan for changes to runtime behavior.
4. Open a pull request and attach automated evidence.
5. For renderer, input, audio, CGUI, MDP-format, packaging, milestone, or
   release changes, wait for an explicit user-recorded manual PASS before
   merge.

Build scripts must not stage, commit, push, or otherwise alter Git history.
Generated executables, object files, logs, captures, packages, and disk images
belong under ignored build/output directories or release artifacts.

Code targets C99 with DJGPP GCC 12.2. New binary formats must use explicit
fixed-width little-endian codecs rather than serializing compiler-native
structures.
