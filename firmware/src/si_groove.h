/* SPDX-License-Identifier: GPL-3.0-only */
/* SILL groove engine: ten styles of generative 90s eurodance on four parts (STABS, BASS, HOOK, DRUMS), with a choir
 * that sings along (OH, AH: the drum kit's sung chords). It runs in time, sixteenth by sixteenth, and hands each note-on
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
 * snare roll, the filter lifting), CHORUS (everything, the hook, the choir), BREAK (the stabs and the hook alone, a
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
enum { SI_CHOIR_OH, SI_CHOIR_AH, SI_CHOIR_MIX, SI_CHOIR_COUNT };
enum { SI_VOWELS_OO_AH, SI_VOWELS_AH_EE, SI_VOWELS_OH_AH, SI_VOWELS_OO_EE, SI_VOWELS_AH_OH, SI_VOWELS_COUNT };
enum { SI_DTIME_16, SI_DTIME_8, SI_DTIME_3_16, SI_DTIME_4, SI_DTIME_COUNT };
enum { SI_SPEED_SLOW, SI_SPEED_MID, SI_SPEED_FAST, SI_SPEED_PAD, SI_SPEED_COUNT };   /* PAD: no tune, the chords held */
/* the drums as the groove names them, the choir's two vowels among them; the glue maps them to the kit (si_sounds.h) */
enum { SI_DR_KICK, SI_DR_SNARE, SI_DR_CLAP, SI_DR_HAT, SI_DR_OPEN, SI_DR_CRASH, SI_DR_OH, SI_DR_AH, SI_DR_COUNT };
/* a choir note on the DRUMS part: its note is SI_CHOIR_NOTE(vowel, pitch), the pitch SI_CHOIR_LO .. SI_CHOIR_LO + 23
 * (the key up in it); a drum's note is its SI_DR_* (below 32; bit 7: the open hat's choke) */
#define SI_CHOIR_LO 48
#define SI_CHOIR_NOTE(v, p) (32 + 32 * (v) + (p) - SI_CHOIR_LO)
static inline bool si_is_choir(int n) { return n >= 32 && n < 96; }
static inline int si_choir_vowel(int n) { return (n - 32) / 32; }           /* 0 OH, 1 AH */
static inline int si_choir_pitch(int n) { return SI_CHOIR_LO + (n - 32) % 32; }
/* a kick tuned to the chord's root: SI_KICK_NOTE(shift), shift -6..+5 semitones from the kick's own pitch (the
 * sample's: PUNCH about E1, ROUND about D1, SI_KICK_PC) */
#define SI_KICK_NOTE(sh) (96 + 6 + (sh))
static inline bool si_is_kick_tuned(int n) { return n >= 96 && n < 108; }
static inline int si_kick_shift(int n) { return n - 96 - 6; }
static const uint8_t SI_KICK_PC[2] = {4, 2};        /* PUNCH, ROUND: their pitch classes (tools/si_drums.py: 41, 36 Hz) */
static inline int si_drum_of(int n)
{
    return si_is_choir(n) ? SI_DR_OH + si_choir_vowel(n) : si_is_kick_tuned(n) ? SI_DR_KICK : n % SI_DR_COUNT;
}
/* the tuplets the hook and the fills play, as (notes, in the space of sixteenths): 3 in 4 (a triplet: three in the
 * space of two eighths), 4 in 6 (a quadruplet: four in the space of three eighths), 5 in 4 (a quintuplet: five in
 * the space of four sixteenths), 6 in 4 (a sextuplet: six in the space of four sixteenths) */
static inline int si_tuplet_span(int n) { return n == 4 ? 6 : 4; }
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
    uint8_t vowels, choir, glide;                  /* VOX: the vowels the OOH sings through, the choir's vowel, glide */
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
    uint8_t hook_tup[SI_HOOK_LEN];                 /* a tuplet starting there: its notes (3 4 5 6; 0 none), over hook_len */
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
    uint32_t choir_clock;                          /* the choir's latest chord (the VOX page) */
    uint8_t choir_last;                            /* which vowel (SI_DR_OH, SI_DR_AH) */
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
    uint8_t kick, clap, crash, choir;
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
