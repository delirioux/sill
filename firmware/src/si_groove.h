/* SPDX-License-Identifier: GPL-3.0-only */
/* SILL groove engine: ten styles of generative 90s eurodance on four parts (STABS, BASS, HOOK, DRUMS), with a crowd
 * that shouts (HEY, HO, WHOA: the drum kit's chants). It runs in time, sixteenth by sixteenth, and hands each note-on
 * and note-off to the sound (Felucca's tracks on the FM-1, a renderer on the host). Besides the notes it computes,
 * every block, what the glue turns into sound:
 *
 *   the LIFT    how far the filter of the lifted parts is open (0..127 per part): the LIFT knob is its ceiling; the
 *               intro and the pre-chorus open it up to there, the break holds it lower
 *   the PUMP    how far each part ducks under the kick (0..127): a fast duck on every beat, a smooth return over
 *               most of the beat; the drums themselves never duck
 *
 * A song is a style plus a number (the SONG, turned by ALGORITHM): the number picks the chord loop, the parts'
 * patterns and the hook, so the same style and number always play the same song. A new style or song comes in on
 * the next bar line; the knobs act at once.
 *
 * The song's form (ARC on): INTRO (the parts in one by one), VERSE (no hook: the rap's room), PRE (held chords, a
 * snare roll, the filter lifting), CHORUS (everything, the hook, the crowd), BREAK (the stabs and the hook alone, a
 * roll back in), CHORUS again (KEY UP: a semitone or two higher), then round again from the verse with a new hook.
 * ARC off: the chorus on and on.
 *
 * Performance: BREAK (REC: everything but the stabs out for three bars, a snare roll in the fourth, the chorus on
 * the bar line; held: until let go, then the roll), KEY UP (SEQ: the song a semitone up on the next bar, up to +3,
 * then home), the keys (the hook live, snapped to the scale).
 *
 * No libm, no doubles: floats on the FPU (single precision), as Daurade, Flyfish and Tuna. */
#ifndef SI_GROOVE_H
#define SI_GROOVE_H

#include <stdbool.h>
#include <stdint.h>

enum { SI_STABS, SI_BASS, SI_HOOK, SI_DRUMS, SI_PARTS };
enum { SI_SIDE_CLUB, SI_SIDE_RADIO };
#define SI_STYLES 10
enum { SI_ST_EUROSTOMP, SI_ST_VIKING, SI_ST_BUBBLEGUM, SI_ST_HEARTACHE, SI_ST_RAVE,
       SI_ST_SKANK, SI_ST_PIANOHOUSE, SI_ST_PARTY, SI_ST_BALLAD, SI_ST_ITALO };
enum { SI_SC_MINOR, SI_SC_MAJOR, SI_SC_DORIAN, SI_SC_HARMONIC, SI_SCALES };
enum { SI_HATS_8THS, SI_HATS_16THS, SI_HATS_OFFBEAT, SI_HATS_OPEN, SI_HATS_COUNT };
enum { SI_FILLS_OFF, SI_FILLS_8, SI_FILLS_4, SI_FILLS_2, SI_FILLS_COUNT };
enum { SI_BL_OFFBEAT, SI_BL_OCTAVE, SI_BL_ROLLING, SI_BL_REGGAE, SI_BL_LONG, SI_BL_BOUNCE, SI_BL_COUNT };
enum { SI_KIT_EURO, SI_KIT_SESSION, SI_KIT_MACHINE, SI_KIT_COUNT };
enum { SI_KICK_PUNCH, SI_KICK_ROUND, SI_KICK_COUNT };
enum { SI_CLAP_CLAP, SI_CLAP_SNARE, SI_CLAP_BOTH, SI_CLAP_COUNT };
enum { SI_CRASH_OFF, SI_CRASH_SECTION, SI_CRASH_8, SI_CRASH_COUNT };
enum { SI_VOX_OFF, SI_VOX_CHORUS, SI_VOX_ALL, SI_VOX_COUNT };
enum { SI_CHANT_HEY, SI_CHANT_HO, SI_CHANT_WHOA, SI_CHANT_MIX, SI_CHANT_COUNT };
enum { SI_VOWELS_OO_AH, SI_VOWELS_AH_EE, SI_VOWELS_OH_AH, SI_VOWELS_OO_EE, SI_VOWELS_AH_OH, SI_VOWELS_COUNT };
enum { SI_DTIME_16, SI_DTIME_8, SI_DTIME_3_16, SI_DTIME_4, SI_DTIME_COUNT };
enum { SI_SPEED_SLOW, SI_SPEED_MID, SI_SPEED_FAST, SI_SPEED_PAD, SI_SPEED_COUNT };   /* PAD: no tune, the chords held */
/* the drums as the groove names them, the crowd's chants among them; the glue maps them to the kit (si_sounds.h) */
enum { SI_DR_KICK, SI_DR_SNARE, SI_DR_CLAP, SI_DR_HAT, SI_DR_OPEN, SI_DR_CRASH, SI_DR_HEY, SI_DR_HO, SI_DR_WHOA,
       SI_DR_COUNT };
/* the form's sections, in the order they play (ARC on); after the second CHORUS, round again from VERSE */
enum { SI_SEC_INTRO, SI_SEC_VERSE, SI_SEC_PRE, SI_SEC_CHORUS, SI_SEC_BREAK, SI_SEC_CHORUS2, SI_SECS };
#define SI_SETS 8
#define SI_SOUNDS 16                               /* each part's SOUND list (si_sounds.h) */
#define SI_STABS_PIANO 0                           /* STABS sound 0, PIANO: the offbeat piano chords, any style */
#define SI_HOOK_PAD 9                              /* HOOK sound 9, PAD: a warm pad (HOOK SPEED PAD plays it held) */
#define SI_LOOPS 12                                /* the chord loops (si_groove.c SI_LOOP) */
#define SI_KEYUP_MAX 3                             /* SEQ: up to +3, then home */

/* what the player set: the cards (and the knobs above them). Saved as it is (si_fw.c), so append only */
typedef struct {
    uint8_t style;
    uint8_t lift, pump, vox, energy;               /* PLAY: LIFT 0..100, PUMP 0..100, VOX (SI_VOX_*), ENERGY 0..100 */
    uint8_t snd[4];                                /* SOUND: STABS BASS HOOK (indices into each part's list), DRUMS = kit */
    uint8_t reso, drive, ensemble, crush;          /* TONE 0..100 (CRUSH 0: off) */
    uint8_t vowels, chant, glide;                  /* VOX: the vowels the OOH sings through, the crowd's shout, glide */
    int8_t shift;                                  /* VOX: formant shift -12..12 */
    uint8_t key, scale, chords, bars;              /* KEY: 0..11 (C..B), SI_SC_*, the loop (0..SI_LOOPS-1), 1 2 4 */
    uint8_t swing, human, hats, fills;             /* GROOVE: swing 0..60 %, human 0..100 (no card), SI_HATS_*, SI_FILLS_* */
    uint8_t bassline;                              /* GROOVE: SI_BL_* */
    uint8_t kick, clap, snap, crash;               /* DRUMS: SI_KICK_*, SI_CLAP_*, SNAP 0..100 (no card), SI_CRASH_* */
    uint8_t lvl[4];                                /* MIX 0..100 */
    uint8_t reverb, delay, dtime, width;           /* SPACE */
    uint8_t arc, verse, chorus, keyup;             /* ARC: on, verse bars 8 16, chorus bars 8 16, the last chorus up 0..2 */
    uint8_t speed, range, gate, repeat;            /* HOOK: SI_SPEED_* (PAD: a pad on the chords), octaves 1 2, gate
                                                    * 10..100 %, bars 1 2 4 */
    uint8_t bpm;                                   /* SELECT: 60..200 */
    uint8_t palette, look;                         /* MENU: COLOR, LOOK (FLAT, LINE) */
    uint16_t song;                                 /* ALGORITHM: the song number in the style, 0..999 */
} si_settings;

typedef struct {
    uint8_t on, part, note, vel;                   /* DRUMS: note is SI_DR_*; vel 1..127 (on) */
} si_event;

/* what the screen hears: each note-on */
typedef struct { uint8_t part, note, vel, pad; } si_seen;

/* a chord as played: its root (pitch class), the notes (MIDI) it voices, its name (with the key up in it) */
typedef struct {
    uint8_t root, n;
    uint8_t note[5];                               /* stab voicing, low to high */
    uint8_t tone[4];                               /* chord tones as pitch classes: root, third, fifth, seventh */
    char name[8];
} si_chord;

#define SI_HELD 24
#define SI_FEED 32
#define SI_LOOP_MAX 8
#define SI_HOOK_LEN 64                             /* the hook: up to four bars of sixteenths (REPEAT) */

typedef struct {
    si_settings s;
    float sr;
    bool playing;
    uint32_t clock;                                /* frames since PLAY */
    float wait;                                    /* frames until the next sixteenth */
    int step;                                      /* the sixteenth sounding in its bar (0..15; -1 before the first) */
    int bar;                                       /* bars since PLAY (0: the first) */
    float beat_ph;                                 /* 0..1 through the beat (the pump) */
    /* the song: built from the style, the song number, the key and the loop (si_rebuild) */
    si_chord loop[SI_LOOP_MAX];
    int nloop;
    uint8_t stab_pat, bass_pat, drum_var, hook_var;
    int8_t hook[SI_HOOK_LEN];                      /* scale steps from the key's tonic (-128: rest); length in hook_len */
    uint8_t hook_len[SI_HOOK_LEN];
    uint32_t rnd;                                  /* the per-note randomness (human, ghosts): seeded per bar */
    /* what waits for the next bar line */
    int next_style;                                /* -1 none */
    int next_song;                                 /* -1 none */
    /* the key up: the song's notes this many semitones up (the arc's last chorus plus SEQ's) */
    int tr;                                        /* sounding now */
    int perf_up;                                   /* SEQ's share, 0..SI_KEYUP_MAX */
    int perf_next;                                 /* SEQ pressed: its share from the next bar (-1 none) */
    /* performance */
    bool brk;                                      /* BREAK: everything but the stabs (and the roll) out */
    int brk_end;                                   /* the bar it comes back on */
    bool brk_held;
    bool crash_next;                               /* a crash on the next downbeat */
    uint32_t keys;                                 /* the FM-1's keys held (bit note - 53) */
    int key_note[27];                              /* the note each key plays (snapped, the key up in it), -1 none */
    /* the form */
    int sec, sec_bar;                              /* the section (SI_SEC_*), bars into it */
    int round;
    /* the block's controls for the glue */
    uint8_t cut[SI_PARTS];                         /* 0..127: how far each part's filter is open */
    uint8_t duck[SI_PARTS];                        /* 0..127: how far each part is ducked now */
    float lift_now;                                /* 0..1: the filter for the screen */
    bool muted[SI_PARTS];                          /* out (BREAK, the form, a level of 0) */
    /* what each part played, a bit per sixteenth: the bar playing and the bar before (the screen) */
    uint16_t hits[SI_PARTS], hits_last[SI_PARTS];
    uint16_t drum_hits[SI_DR_COUNT], drum_last[SI_DR_COUNT];
    uint32_t hook_on_clock;                        /* the hook's latest note-on (the VOX page's vowel) */
    uint32_t chant_clock;                          /* the crowd's latest shout (the VOX page) */
    uint8_t chant_last;                            /* which (SI_DR_HEY ..) */
    /* the notes sounding: their note-off at clock off */
    struct { uint8_t note, part; uint32_t off; } held[SI_HELD];
    int nheld;
    si_seen feed[SI_FEED];
    uint8_t feed_w, feed_r;
    char msg[24];                                  /* a word for the screen ("NEXT PARTY", "KEY UP +1"); the UI clears it */
    uint8_t msg_new;
    uint8_t sound_sig;                             /* bumped when every part's sound must load again (a style, a set) */
    void (*sound)(void *ctx, const si_event *e);
    void *sound_ctx;
} si_groove;

typedef struct {
    const char *name;
    uint8_t side, bpm, key, scale;
    uint8_t lift, pump, vox, energy;
    uint8_t snd[4];                                /* STABS BASS HOOK sound, the kit */
    uint8_t loops[4];                              /* the chord loops a song picks from (SI_LOOP indices) */
    uint8_t bars;                                  /* bars a chord */
    uint8_t swing, hats, fills, bassline;
    uint8_t kick, clap, crash, chant;
    uint8_t stab, hook, drums;                     /* the rhythm families (si_groove.c) */
    uint8_t speed, range, gate, repeat;            /* the HOOK page */
    uint8_t lift_parts;                            /* bit per part: what the LIFT filters */
    uint8_t reverb, delay, dtime;
    uint8_t reso, drive, ensemble;
    uint8_t alt[3][3];                             /* STABS BASS HOOK: three more sounds each a song can pick (si_song_sounds) */
} si_style;

extern const si_style SI_STYLE[SI_STYLES];

void si_defaults(si_settings *s);                  /* EUROSTOMP, as it starts */
void si_style_apply(si_settings *s, int style);    /* a style's own settings into s (PLAY SOUND TONE VOX KEY GROOVE DRUMS
                                                    * SPACE HOOK and the tempo); MIX ARC MENU stay */
void si_settings_fix(si_settings *s);              /* every field into its range (a loaded record) */
void si_song_sounds(si_settings *s);               /* the song's sounds: song 1 (0) the style's own, the others one of four
                                                    * per part, seeded by the song (the kit stays the style's) */
void si_init(si_groove *g, float sample_rate);
void si_set_sound(si_groove *g, void (*fn)(void *ctx, const si_event *e), void *ctx);

void si_play(si_groove *g, bool on);
void si_rebuild(si_groove *g);                     /* the song again from the settings (KEY, the loop, the hook) */
void si_queue_style(si_groove *g, int style);      /* playing: on the next bar line; stopped: at once */
void si_queue_song(si_groove *g, int song);
void si_break(si_groove *g, bool down);            /* REC: down starts it, up lets it come back */
void si_keyup(si_groove *g);                       /* SEQ: a semitone up from the next bar (after +3: home) */
void si_key(si_groove *g, int key, int note, bool down);   /* the FM-1's key (0..26) playing note: the hook, snapped */
void si_process(si_groove *g, int frames);         /* the audio: notes in time, the lift and the pump */
void si_all_off(si_groove *g);

bool si_feed_pop(si_groove *g, si_seen *n);
int si_chord_now(const si_groove *g);              /* the loop's chord sounding (0..nloop-1) */
int si_bars_per_chord(const si_settings *s);
int si_sec_len(const si_groove *g, int sec);       /* a section's bars */
int si_repeat_bars(const si_settings *s);          /* the hook's phrase: 1 2 4 bars */
bool si_in_chorus(const si_groove *g);             /* a chorus playing (the form's, or ARC off) */
const char *si_style_name(int i);
const char *si_scale_name(int i);
const char *si_note_name(int pc);
const char *si_sec_name(int sec);                  /* "INTRO" .. */
const char *si_sec_short(int sec);                 /* "IN" "VRS" .. */
void si_loop_name(const si_settings *s, int loop, char *out, int max);   /* "LOOP 4" */
int si_snap(const si_settings *s, int note);       /* a note onto the scale (the nearest step, ties down) */

#endif
