# Contributing to Sill

Sill is a small fork. Bugs and ideas: [Issues](https://github.com/delirioux/sill/issues) on this repository,
please; not Felucca's, Tuna's or Daurade's trackers: Sill is maintained here.

With a bug, say the Sill version (the splash), the style and song, the steps to reproduce it, what you expected
and what happened. A change to the songs goes in `firmware/src/si_groove.c`, to the sounds in `si_sounds.h` (run
`sh tests/si_audition.sh` and keep the trims even). The keys only ever play music. `sh tests/si_tests.sh` must pass,
and the screenshots in `docs/screens/` change with the screen.
