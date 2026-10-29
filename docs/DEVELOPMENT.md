# Developing Sill

The working notes behind Sill: the agreements, how the work flows, why the code is shaped as it is, where it
stands and what comes next. What Sill is and how to play it is in the [README](../README.md) and the
[manual](MANUAL.md).

Status: 29 October 2026. Version 1.0: it works and it plays. Built by CI and running on an FM-1; released on
`main`, tagged and released on GitHub by CI.

## Agreements

| What | Agreed |
| --- | --- |
| Home | GitHub, `github.com/delirioux/sill`, branch `main`; unfinished work on `dev` until it has run on the FM-1. |
| Upstream | A fork of Daurade, Flyfish and [Tuna](https://github.com/delirioux/tuna) (themselves forks of [Felucca](https://github.com/hugelton/Felucca)). Never push to Tuna or Felucca. Felucca's five engines are a submodule, `firmware/engines`. |
| Name | Ocean animals, as Tuna, Flyfish, Makohai and Daurade: the sill, the herring. The logo: a herring on a ring of audio bars, four knobs in a row, club lasers, a fat 90s wordmark in magenta and cyan. |
| What it is | Generative 90s eurodance: offbeat basses, stabs, a chorus hook, a choir, a key change. Its sounds and styles are its own; the repository names no artist, record or film as a model, in code, docs or history. |
| Description | "Generative 90s eurodance for the FM-1, pickled in 1995." |
| Language | English throughout: code, comments, commits, docs. |
| Licence | GPL-3.0-only. The drum one-shots and the choir are synthesized by `tools/si_drums.py` and `tools/si_choir.py` and dedicated to the public domain (CC0). |
| Commits | Author `delirioux <340923392+delirioux@users.noreply.github.com>`. Work done with Claude ends with the `Co-Authored-By` and `Claude-Session` lines. |
| Repository contents | The firmware, its tests and tools, its docs and its plan (this file). Chat discussions stay out. Docs and screenshots change with the code. |
| Controls | The keys only ever play music; settings, pages and sets are on knobs and buttons. |
| Changes | A new style or song waits for the next bar line, as the key up does; the knobs act at once. |
| Colours | LASER is the default palette; Felucca's palettes stay on MENU. |

## How changes reach GitHub

Claude pushes to the repository directly (the Claude GitHub App). Work goes to `dev`; a push there builds a test
package (Actions > the run > Artifacts > `sill-firmware`) and the test site under `dev/`, installed with the
installer's "Install a test build". It goes to `main` once it has run on the FM-1. A push to `main` with a version
(`si_version.h`) not yet tagged tags it `vX.Y` and makes a GitHub release with the package and `docs/RELEASE.md`.

## Where Sill lives

Sill's files are `firmware/src/si_*`, `tests/si_*`, `tools/si_*`, `web/si_*` and `web/emu/si_*`. From Tuna:
`tuna_libm.c` and `tuna_sys.h`. Everything else is Felucca's, touched only where Sill hooks in (`felucca.c`,
`main.c`, `audio.c`, `panel.c`, `gfx.c`, `eng_sample.c`: `FELUCCA_SILL`; `tools/build.py`: `SI_MODULES`;
`tools/gen_samples.py`: the drum set at 44.1 kHz, kept whole; `tools/gen_ui_palettes.py`: LASER).

| File | Does |
| --- | --- |
| `si_groove.h/.c` | the engine: ten styles, twelve chord loops, the rhythm families of each part, the hook, the form, the choir, the lift and the pump, BREAK, KEY UP, the keys, swing and human timing. Plain C, no Felucca: it runs on the host as it does on the FM-1 |
| `si_sounds.h` | the forty-eight sounds on Felucca's engines and their trims, the kits and the choir's notes, and how a part's cards set its track (level, pan, sends, the matrix's lift and pump slots, the VOX page, TONE) |
| `si_fm6.inc` | the FM6 patches (ORCH HIT, Sill's; SLAP, FRETLESS (SLIDE), Tuna's), made by `tools/si_fm6.py` |
| `tools/si_piano.py` | the PIANO: six notes of VCSL's Kawai grand (CC0), made bright and hard, into `assets/samples-cc0/PIANO/` |
| `si_ui.h/.c` | the thirteen pages, the cards, the styles list, the sets, on Felucca's `gfx.c` |
| `si_fw.c` | the glue: the groove on the tracks and MIDI out, loading a part's sound when it changes, the panel, the key LEDs, the settings and the sets in flash, the splash |
| `si_logo.inc` | the splash logo, made by `tools/si_logo.py` from the same drawing as `assets/logo.svg` (`tools/draw_logo.py`) |
| `tools/si_drums.py` | the seven drum one-shots, synthesized, seeded, into `assets/samples-cc0/DRUMS/` |
| `tools/si_choir.py` | the choir's OH and AH at F3 and F4, synthesized, seeded, into `assets/samples-cc0/CHOIR/` |
| `tests/si_groove_test.c` | the engine: every style plays and ends its notes, in key; the form's order and its key up; the break; a song replays exactly; the tempo, style and song on the bar line; BREAK tapped and held; KEY UP; the keys; the choir (the chord's tones, its range, its vowel); the hook's phrase; a bad record |
| `tests/si_ui_test.c` | the screen: the controls, every page in every palette, the layout lint, PNGs and a GIF (`docs/screens/`) |
| `tests/si_felucca.c` | a song through Felucca's real engines to a WAV: voices, steals, peaks, the cost |
| `tests/si_audition.c` | every sound's loudness, for the trims in `si_sounds.h` |
| `web/emu/si_web.c` | the browser build, on Felucca's emulator |

## Decisions and why

- **Daurade's machine, a new engine.** The frame (the four tracks, the matrix slots, the cards, the sets, the
  flash, the browser build) is Daurade's, proven on the FM-1. The groove engine, the styles, the sounds, the pages
  and the choir are Sill's.
- **The form is the song.** Eurodance lives on its sections: a verse that leaves room, a pre-chorus that builds, a
  chorus that pays off, a break, the last chorus higher. The arc plays INTRO, VERSE, PRE, CHORUS, BREAK, CHORUS and
  round again from the verse with a new hook; VERSE and CHORUS are 8 or 16 bars (the ARC page).
- **KEY UP, two ways.** The last chorus goes up by ARC's KEY UP (off, +1, +2); SEQ adds a semitone from the next bar
  (to +3, then home). The key up is added as each note sounds, so the chords' voicings keep their shape and the bass
  goes up, not down an octave; the chords' names carry it, the key LEDs and the keys' snapping follow it. A change
  lets go of the held chords and hook at the bar line (their old key).
- **BREAK on REC.** Tapped: three bars of the stabs alone (held, a bar each), a snare roll in the fourth, back into a
  chorus on the bar line. Held: the stabs until let go, then the roll. The other parts are muted at once, so not even
  a swung sixteenth of the bass slips through.
- **The hook is a motif and its answer.** A bar of rhythm by SPEED (SLOW sung, MID eighths, FAST the syncopated
  sixteenth riff), its pitches a walk of repeats, steps and now and then a leap; REPEAT plays it over 1, 2 or 4 bars
  as A, A A', A A' A A'' (the answers keep the first half and end on the tonic or the fifth). Its long and strong
  notes snap onto the chord playing, so the riff follows the loop. It rests in the verse and the pre-chorus.
- **A choir, not a crowd.** The shouts (HEY, HO, WHOA) sounded creepy and are gone. VOX brings a choir instead,
  a layer under the chorus: the chord's third and fifth (the bass and the stabs have the root; eight voices are few)
  sung on OH or AH (CHOIR, MIX: OH, then AH the bar after) at the top of each bar, at the half bar too with ENERGY
  high, and in the verse with VOX ALL as each chord comes in. It plays on the drums' track, as the shouts did: four
  pitched zones above the kit in EURO's sample set (OH from key 64, AH from 88: a vowel's note n at n + 16 or
  n + 40, F3 and F4 each over the twelve keys around it, at 22,050 Hz), a note of the DRUMS part
  `SI_CHOIR_NOTE(vowel, pitch)` with the key up in it. On the DRUM engine's kits the choir stays home; no General
  MIDI drum fits it, so it is not sent as MIDI. About 40 KB, 3 KB more than the shouts were.
- **The choir is made, not recorded.** Twelve voices on one note, each a few cents off, a little late, with a slow
  vibrato of its own; a saw softened to the singing voice's tilt (-12 dB an octave), a breath at the onset, three
  wide, dark formants (OH 450, 800, 2830 Hz; AH 750, 1150, 2800), a slow swell, a long fade and a hall's first
  reflections. Measured: its energy sits around 360 to 580 Hz, next to nothing above 2 kHz.
- **The lift and the pump in the matrix, not in the DSP** (Daurade's): MODW to the lifted parts' brightness (slot 1,
  -32), EXPR to every part but the drums' level (slot 2, +64), set every 32 frames. The lift follows the form (the
  intro and the pre-chorus open it, the break holds it at 60 %), not a sixteen-bar breath.
- **A sampled piano, bright.** The FM piano sounded like an electric piano; the club piano is a grand made bright
  and hard. Six notes of VCSL's Kawai grand (its loudest layer, CC0), cut to about a second, a high-pass at 180 Hz, +5 dB from 2 kHz, a
  presence lift, less body, a soft clip; on the FM-1 a low-pass at about 5 kHz, a room of its own and a lower
  level (about 3 dB under the bass and the hook in the chorus) push it back; the SAMPLE engine's set 1, about 64 KB of ADPCM. PIANO on STABS plays the
  offbeat chords in any style.
- **Sixteen sounds a part, four a style, one a song.** The sounds are synthesized on Felucca's engines (their
  presets cost bytes, not kilobytes: no samples). Each style names its own sound for each part and three more; a
  song's number picks one of the four (song 1 always the style's own), so the songs of a style vary, and a song comes
  back with the same sounds. Names stay short enough for a card (the screen test lints every one).
- **Tuplets between the sixteenths.** The engine counts sixteenths but schedules a note at any frame, so a tuplet's
  notes fall evenly inside its span: 3 in 4 sixteenths, 4 in 6, 5 in 4, 6 in 4. A song's hook may hold one in its
  motif's second half (20, 40, 55 % of songs at SLOW, MID, FAST), an arpeggio of the chord playing, so it always fits; a fill may be a snare
  triplet or a sextuplet roll over the last beat.
- **Headroom, not drive.** The mix sat in Felucca's master limiter and the styles' DRIVE stacked on the sounds' own,
  so it sounded overdriven: MIX 100 is -2 dB now, DRIVE reaches the distortion at half the depth, the styles' DRIVE
  is lower (RAVE 18, VIKING 10, EUROSTOMP 8), and the hot sounds (HOOVER, REESE, SYNTH, ANTHEM, GROWL) less driven.
- **Eight voices for everything.** Felucca shares eight voices across the four tracks. The stabs play three notes,
  the held chords four; every drum and choir note has a ring length; the SUPER hook takes every free voice (UNISON) and
  gives its extra ones up first. Measured over 75 s through Felucca's engines, the choir on: SKANK steals 11, RAVE 27,
  EUROSTOMP 310 (the unison hook's extra voices, by design), out of 500 to 2,000 notes.
- **Even levels.** `tests/si_audition.c` measures each sound's loudness alone; the trims bring the new sounds to
  their part's mark. The trims Daurade set by ear on the FM-1 are kept.
- **No libm, no doubles**: floats on the FPU (`-mcpu=r3 -mfprev1`), `sinf` and friends from `tuna_libm.c`.
- **Settings save themselves while stopped**, in Felucca's first project object ("SIS1"); the eight sets in the
  second. The record only grows at its end, so older settings still load; a bad record is fixed field by field.
- **LASER**: night purple, laser cyan values, hot magenta for the one active thing; its own colour tag, so another
  firmware's choice does not carry over.

## Measured on the host

75 s of each style with the arc, through Felucca's engines (`tests/si_felucca.sh`): peaks between -4.9 and
-3.2 dBFS, nothing clipped, the eight voices full at the busiest moments. The mix costs 100 to 195 ns a sample on
this host (SKANK the least, BALLAD the most), in the range of Daurade's; the real number comes from the FM-1.

## Plan

1. **On the FM-1.** It installs, boots and plays. Still to check: the settings and sets surviving a power cycle,
   MIDI out, the CPU at EUROSTOMP (the unison hook) and BALLAD over a long session.
2. **The sound by ear.** The new sounds (the sampled PIANO, SUPER, ORCH, REESE, HOOVER, OOH) and their trims; the choir (does it
   sit under the chorus?); the kick against the bass in the chorus; the break's level.
3. **Version 2?** More styles, a rap-like spoken part, more vowels for the choir: only if Sill earns it; the record has room.
