/* SPDX-License-Identifier: GPL-3.0-only */
/* SILL's sounds on Felucca's engines, and how the cards set a track: shared by the firmware (si_fw.c) and the host
 * render (tests/si_felucca.c), so both play the same. Needs core.h (track_t, P_*, the song) and the matrix's
 * MS_* / MD_* (mod.c) first.
 *
 * Tracks: 1 STABS, 2 BASS, 3 HOOK, 4 DRUMS. Each music part has a list of ten sounds (the SOUND page): an engine and
 * its factory preset, or a sound of Sill's own on that engine (sis_own), or one of the FM6 patches (si_fm6.inc).
 * The PIANO is the SAMPLE engine's set 1: six notes of a Kawai grand (VCSL, CC0) made bright (tools/si_piano.py).
 * The drums are a kit: EURO, Sill's own one-shots and its choir (the SAMPLE engine, set DRUMS: tools/si_drums.py at
 * 44.1 kHz, the choir's OH and AH tools/si_choir.py's, pitched zones above the kit), or the DRUM engine's TIGHT or 80
 * kit (no choir there).
 *
 * Two of the matrix's slots carry the groove's controls, set every block by the glue:
 *   slot 1  MODW -> the sound's brightness (ANALOG and TRIO CUT, FM6 MLVL, ...) by -32: the LIFT (t->mw is
 *           127 - the groove's cut, so an open filter leaves the sound as its preset has it, and a closed one takes
 *           half the range off)
 *   slot 2  EXPR -> AMP by 64: the PUMP (t->ex_off is the groove's duck: the gain is 1 - duck)
 *   slot 4  VEL -> AMP: the drums' velocities on the SAMPLE engine (it has none of its own) */
#ifndef SI_SOUNDS_H
#define SI_SOUNDS_H

#include "si_groove.h"

enum { SIS_ANALOG = 0, SIS_PHASE = 2, SIS_LOFI = 3, SIS_SAMPLE = 4, SIS_VOICE = 5, SIS_TRIO = 6, SIS_WHEEL = 7,
       SIS_PHYS = 9, SIS_DRUM = 10, SIS_FM6 = 12 };
enum { SIS_KEEP = -1 };                   /* dist, glide, voice: as the preset leaves them */
enum { SIS_POLY = 0, SIS_MONO = 1, SIS_LEGATO = 2, SIS_UNISON = 3 };

typedef struct {
    const char *name;
    uint8_t engine, preset;
    int8_t dist, glide, voice;
    const preset_t *own;
    const uint8_t *fm6;
    int8_t bright;                        /* the E parameter the LIFT takes down (0..7), -1: none */
    int8_t trim;                          /* added to the level (F_DB steps), to even the sounds out: each part's
                                           * sounds at one loudness (tests/si_audition.sh measures them) */
} sis_sound;

/* ---- Sill's own sounds: Felucca's preset layout (core.h preset_t): {name, E0..E7, ATK DEC SUS REL, ENV->FLT,
 * mono, FX(dist, chorus, delay, reverb)}. The ANALOG E values: WAVE DTN MIX NOIS CUT RES DRV KTR; VOICE: VOWL VOWL2
 * TALK SHIFT BUZZ BRTH Q RAND. SAW LEAD is Tuna's (tuna_sounds_own.inc), OCTAVE Daurade's DISCO BASS ---- */
/* the stab of the anthem: two saws far apart, bright, a short pluck of the filter, wide in the chorus */
static const preset_t SIS_SUPER = {"SUPER", {0, 42, 64, 0, 110, 22, 12, 64}, {0, 64, 84, 26}, 12, 0, FX(0, 60, 25, 25)};
/* the hook's lead: the same saws on every free voice (UNISON), a touch of glide */
static const preset_t SIS_SUPERLEAD = {"SUPER LEAD", {0, 30, 64, 0, 112, 18, 16, 64}, {1, 60, 104, 30}, 8, 1,
                                       FX(0, 50, 35, 30)};
/* the octave bass of the offbeat: a saw with its sub-octave, plucked by the filter envelope (Daurade's) */
static const preset_t SIS_OCTAVE = {"OCTAVE", {6, 0, 40, 0, 62, 35, 20, 64}, {0, 52, 60, 14}, 30, 1, FX(5, 0, 5, 10)};
/* the reese: two saws beating slowly against each other, low and dark */
static const preset_t SIS_REESE = {"REESE", {0, 22, 64, 0, 58, 20, 15, 64}, {0, 80, 110, 18}, 6, 1, FX(0, 0, 0, 8)};
/* the hoover: a wide pulse, detuned, a growl of drive, gliding (the hook's) */
static const preset_t SIS_HOOVER = {"HOOVER", {4, 38, 64, 0, 96, 30, 20, 64}, {0, 70, 100, 30}, 16, 1, FX(6, 40, 20, 25)};
/* the sung hook: the VOICE engine breathy and round, its vowels and their glide the VOX page's */
static const preset_t SIS_OOH = {"OOH", {127, 0, 40, 0, 70, 35, 80, 8}, {10, 70, 110, 40}, 0, 1, FX(0, 40, 30, 40)};
/* ---- 0.4's own: the sounds a style can vary between (si_groove.c's pools) ---- */
/* STABS: a deep house chord of two squares a little apart, a short pluck of the filter */
static const preset_t SIS_DEEPCHORD = {"DEEP CHORD", {1, 20, 64, 0, 86, 32, 10, 64}, {0, 58, 46, 20}, 22, 0, FX(0, 40, 30, 30)};
/* STABS: a short trance pluck, the saws far apart, the filter snapping shut */
static const preset_t SIS_TRANCESTAB = {"TRANCE", {0, 38, 64, 0, 110, 24, 12, 64}, {0, 58, 30, 18}, 36, 0, FX(0, 50, 40, 30)};
/* STABS and HOOK: a choir on the VOICE engine, quicker than its factory CHOIR AAH (the VOX page's vowels) */
static const preset_t SIS_CHOIR = {"CHOIR", {0, 0, 40, 0, 40, 26, 64, 6}, {14, 90, 112, 60}, 0, 0, FX(0, 70, 15, 70)};
static const preset_t SIS_CHOIRLEAD = {"CHOIR", {0, 0, 40, 0, 50, 22, 70, 6}, {10, 80, 112, 50}, 0, 1, FX(0, 50, 30, 60)};
/* BASS: a punchy analogue saw, short and dry */
static const preset_t SIS_SYNTHBASS = {"SYNTH BASS", {0, 0, 0, 0, 70, 40, 12, 64}, {0, 46, 30, 10}, 34, 1, FX(0, 0, 0, 6)};
/* BASS: the trance bass, a bright saw with a little detune, rolling */
static const preset_t SIS_TRANCEBASS = {"TRANCE BASS", {0, 10, 64, 0, 84, 22, 8, 64}, {0, 62, 82, 12}, 20, 1, FX(0, 0, 0, 8)};
/* BASS: a deep sine with a click on top */
static const preset_t SIS_SINEBASS = {"SINE BASS", {3, 0, 0, 0, 127, 0, 25, 0}, {0, 70, 100, 12}, 0, 1, FX(0, 0, 0, 4)};
/* HOOK: REEDS, the accordion's musette: two pulses beating against each other, as its reeds do */
static const preset_t SIS_MUSETTE = {"MUSETTE", {4, 16, 64, 0, 96, 12, 18, 64}, {3, 70, 112, 28}, 0, 1, FX(0, 30, 30, 35)};
/* HOOK: the pop lead of the bubblegum songs: a square, plucked by the filter */
static const preset_t SIS_POP = {"POP", {1, 6, 64, 0, 100, 20, 5, 64}, {0, 55, 60, 20}, 25, 1, FX(0, 20, 35, 30)};
/* HOOK: the anthem lead: saws wide and driven, a slow glide */
static const preset_t SIS_ANTHEM = {"ANTHEM", {0, 26, 64, 0, 108, 15, 10, 64}, {4, 70, 110, 35}, 10, 1, FX(0, 40, 40, 40)};
/* HOOK: the whistle: a sine with a breath of noise */
static const preset_t SIS_WHISTLE = {"WHISTLE", {3, 0, 0, 6, 127, 0, 0, 64}, {6, 70, 110, 25}, 0, 1, FX(0, 20, 40, 45)};
/* HOOK: a trance pluck lead, snappy */
static const preset_t SIS_TRANCELEAD = {"TRANCE", {0, 22, 64, 0, 100, 32, 12, 64}, {0, 46, 30, 20}, 45, 1, FX(0, 30, 50, 35)};
/* HOOK: the robot (Daurade's): the VOICE engine at full buzz and a high Q, a vocoder */
/* HOOK: CORNET, a solo trumpet, light: one saw, a breath of noise, the filter swelling open on each note (the lip), mono */
static const preset_t SIS_TRUMPET = {"CORNET", {0, 0, 0, 6, 50, 12, 6, 64}, {12, 60, 96, 26}, 42, 1, FX(0, 20, 25, 35)};
static const preset_t SIS_SAWLEAD = {"SAW LEAD", {0, 0, 12, 12, 0, 96, 25, 64}, {2, 70, 100, 35}, 20, 1,
                                     FX(0, 10, 35, 25)};

#include "si_fm6.inc"

#define S(n, e, p, b, tr) {(n), (uint8_t)(e), (uint8_t)(p), SIS_KEEP, SIS_KEEP, SIS_KEEP, 0, 0, (int8_t)(b), (int8_t)(tr)}
#define SX(n, e, p, d, gl, v, b, tr) {(n), (uint8_t)(e), (uint8_t)(p), (int8_t)(d), (int8_t)(gl), (int8_t)(v), 0, 0, (int8_t)(b), (int8_t)(tr)}
#define SO(n, e, p, own, d, gl, v, b, tr) {(n), (uint8_t)(e), (uint8_t)(p), (int8_t)(d), (int8_t)(gl), (int8_t)(v), &(own), 0, (int8_t)(b), (int8_t)(tr)}
#define SF(n, patch, gl, v, tr) {(n), (uint8_t)SIS_FM6, 0, SIS_KEEP, (int8_t)(gl), (int8_t)(v), 0, (patch), 2, (int8_t)(tr)}

/* the three music parts' lists, in the SOUND card's order (si_groove.c names them in the styles) */
static const sis_sound SIS_SOUND[3][SI_SOUNDS] = {
    {   /* STABS */
        S("PIANO", SIS_SAMPLE, 1, 4, -18),               /* the club piano (tools/si_piano.py), pushed back */
        S("ORGAN", SIS_WHEEL, 1, 3, 0),                  /* JAZZ PERC: the house organ's click */
        SO("SUPER", SIS_ANALOG, 0, SIS_SUPER, SIS_KEEP, SIS_KEEP, SIS_POLY, 4, 0),
        S("STRINGS", SIS_ANALOG, 11, 4, -6),
        S("PWM", SIS_ANALOG, 3, 4, -5),                   /* PWM STR: pulse-width strings, wide and soft */
        S("PLUCK", SIS_ANALOG, 8, 4, 4),
        S("HOOVER", SIS_ANALOG, 6, 4, -5),               /* RAVE */
        SF("ORCH", SI_FM6_ORCHHIT, SIS_KEEP, SIS_POLY, 6),
        S("EPIANO", SIS_FM6, 0, 2, -2),                  /* TINE EP: the electric piano of the ballads */
        S("PAD", SIS_FM6, 4, 2, 16),
        S("HOUSE", SIS_WHEEL, 0, 3, 3),                  /* FULL ORGAN, short: the house organ chord */
        SO("DEEP", SIS_ANALOG, 0, SIS_DEEPCHORD, SIS_KEEP, SIS_KEEP, SIS_POLY, 4, 10),
        S("KEYS", SIS_ANALOG, 5, 4, 3),                  /* SINE KEY: a soft keyboard chord */
        SO("CHOIR", SIS_VOICE, 0, SIS_CHOIR, SIS_KEEP, SIS_KEEP, SIS_POLY, -1, -1),
        S("VIOLINS", SIS_PHASE, 2, 2, -3),                /* PHASE's STRING: wide, orchestral */
        SO("TRANCE", SIS_ANALOG, 0, SIS_TRANCESTAB, SIS_KEEP, SIS_KEEP, SIS_POLY, 4, 9),
    },
    {   /* BASS */
        S("FM BASS", SIS_FM6, 2, 2, -4),
        S("SQUARE", SIS_ANALOG, 2, 4, -1),
        S("SUB", SIS_ANALOG, 7, 4, -13),
        S("FAT", SIS_TRIO, 0, 5, 2),
        SX("ACID", SIS_ANALOG, 4, SIS_KEEP, 18, SIS_LEGATO, 4, 6),
        SO("REESE", SIS_ANALOG, 0, SIS_REESE, SIS_KEEP, SIS_KEEP, SIS_MONO, 4, -8),
        SF("SLAP", SI_FM6_SLAPBASS, SIS_KEEP, SIS_POLY, 0),
        SF("SLIDE", SI_FM6_FRETLESS, 22, SIS_LEGATO, -5),
        SO("OCTAVE", SIS_ANALOG, 0, SIS_OCTAVE, SIS_KEEP, SIS_KEEP, SIS_MONO, 4, -1),
        SX("WOW", SIS_VOICE, 2, SIS_KEEP, 24, SIS_LEGATO, -1, -3),
        SX("ORGAN", SIS_WHEEL, 1, SIS_KEEP, SIS_KEEP, SIS_MONO, 3, -4),   /* the 90s organ bass */
        SO("SYNTH", SIS_ANALOG, 0, SIS_SYNTHBASS, SIS_KEEP, SIS_KEEP, SIS_MONO, 4, 2),
        SO("SINE", SIS_ANALOG, 0, SIS_SINEBASS, SIS_KEEP, SIS_KEEP, SIS_MONO, -1, -14),
        SO("TRANCE", SIS_ANALOG, 0, SIS_TRANCEBASS, SIS_KEEP, SIS_KEEP, SIS_MONO, 4, -6),
        SX("DX", SIS_FM6, 7, SIS_KEEP, SIS_KEEP, SIS_MONO, 2, -1),        /* FM6's PLUCK, low and hard */
        SX("GROWL", SIS_PHASE, 0, 6, SIS_KEEP, SIS_MONO, 2, -9),          /* PHASE's BRASS in the low end */
    },
    {   /* HOOK */
        SO("SUPER", SIS_ANALOG, 0, SIS_SUPERLEAD, SIS_KEEP, 6, SIS_UNISON, 4, -8),
        SO("SAW", SIS_TRIO, 0, SIS_SAWLEAD, SIS_KEEP, 10, SIS_KEEP, 5, -8),
        S("PLUCK", SIS_ANALOG, 8, 4, 2),
        SO("POP", SIS_ANALOG, 0, SIS_POP, SIS_KEEP, 4, SIS_MONO, 4, -3),  /* the bubblegum lead */
        S("PIANO", SIS_SAMPLE, 1, 4, -18),
        SO("HOOVER", SIS_ANALOG, 0, SIS_HOOVER, SIS_KEEP, 14, SIS_LEGATO, 4, -14),
        SO("OOH", SIS_VOICE, 0, SIS_OOH, SIS_KEEP, SIS_KEEP, SIS_LEGATO, -1, -12),
        S("SYNC", SIS_TRIO, 2, 5, -6),
        S("FLUTE", SIS_WHEEL, 3, 3, -7),                 /* SOFT FLUTE */
        S("PAD", SIS_ANALOG, 1, 4, -11),                 /* SOFT PAD: HOOK SPEED PAD holds it on the chords */
        SO("REEDS", SIS_ANALOG, 0, SIS_MUSETTE, SIS_KEEP, 8, SIS_LEGATO, 4, -14),
        SO("ANTHEM", SIS_ANALOG, 0, SIS_ANTHEM, SIS_KEEP, 14, SIS_LEGATO, 4, -12),
        SO("CHOIR", SIS_VOICE, 0, SIS_CHOIRLEAD, SIS_KEEP, 10, SIS_LEGATO, -1, -12),
        SO("WHISTLE", SIS_ANALOG, 0, SIS_WHISTLE, SIS_KEEP, 16, SIS_LEGATO, -1, -16),
        SO("TRANCE", SIS_ANALOG, 0, SIS_TRANCELEAD, SIS_KEEP, SIS_KEEP, SIS_MONO, 4, 4),
        SO("CORNET", SIS_ANALOG, 0, SIS_TRUMPET, SIS_KEEP, 8, SIS_LEGATO, 4, -17),   /* (2 dB under the leads: light) */
    },
};
#undef S
#undef SX
#undef SO
#undef SF

static const char *const SIS_KIT_NAMES[SI_KIT_COUNT] = {"EURO", "SESSION", "MACHINE"};
static const int8_t SIS_KIT_TRIM[SI_KIT_COUNT] = {-14, 0, -1};   /* (tests/si_audition.sh, as the sounds' trims) */
static inline const char *sis_name(int part, int i)
{
    if (part == SI_DRUMS)
        return SIS_KIT_NAMES[(unsigned)i % SI_KIT_COUNT];
    return SIS_SOUND[part % 3][(unsigned)i % SI_SOUNDS].name;
}

/* the sound a part plays now: its card's */
static inline const sis_sound *sis_of(const si_groove *g, int part)
{
    return &SIS_SOUND[part % 3][(unsigned)g->s.snd[part] % SI_SOUNDS];
}

/* ---- the drums: the kit's engine and each drum's note ---- */
#define SIS_KIT_BASE 53                   /* the DRUMS set's first key (tools/gen_samples.py KIT_BASE) */
static inline uint32_t sis_kit_engine(int kit) { return kit == SI_KIT_EURO ? SIS_SAMPLE : SIS_DRUM; }
/* the choir on the DRUMS set: a vowel's note n at key n + SIS_CHOIR_KEY[vowel] (tools/gen_samples.py CHOIR_KEY) */
static const uint8_t SIS_CHOIR_KEY[2] = {16, 40};
/* the note a drum (or a choir note, SI_CHOIR_NOTE) plays on the kit; -1: this kit has none (the choir is EURO's) */
static inline int sis_drum_note(const si_settings *s, int drum)
{
    if (si_is_choir(drum))
        return s->snd[3] == SI_KIT_EURO ? si_choir_pitch(drum) + SIS_CHOIR_KEY[si_choir_vowel(drum) & 1] : -1;
    if (s->snd[3] == SI_KIT_EURO) {       /* the DRUMS set: KICK PUNCH, KICK ROUND, SNARE, CLAP, HAT, OPEN HAT, CRASH */
        static const uint8_t K[SI_DR_OH] = {0, 2, 3, 4, 5, 6};
        int k = K[drum % SI_DR_OH];
        if (drum == SI_DR_KICK && s->kick == SI_KICK_ROUND)
            k = 1;
        return SIS_KIT_BASE + k;
    }
    static const uint8_t GM[SI_DR_OH] = {36, 38, 39, 42, 46, 49};   /* General MIDI */
    return GM[drum % SI_DR_OH];
}
/* the DRUM engine's eight (E0 KIT, E1 TUNE, E2 TONE, E3 DECY, E4 SNAP, E5 ACC, E6 KICK, E7 DRV) for a kit */
static inline void sis_kit_params(const si_settings *s, int16_t *e)
{
    e[0] = s->snd[3] == SI_KIT_SESSION ? 10 : 4;    /* TIGHT, 80 (eng_drum.c DK_*) */
    e[1] = 64;
    e[2] = 64;
    e[3] = 60;
    e[4] = (int16_t)(30 + s->snap * 97 / 100);
    e[5] = 100;
    e[6] = (int16_t)(s->kick == SI_KICK_ROUND ? 1 : 0);
    e[7] = 10;
}

static inline int16_t sis_clamp(int v, int lo, int hi) { return (int16_t)(v < lo ? lo : v > hi ? hi : v); }
#define SIS_PIANO_CUT 55                  /* the piano's CUT (eng_sample.c: a gentle one-pole, 55: about 5 kHz) */
#define SIS_PIANO_ROOM 35                 /* the piano's reverb send over SPACE's (0..127) */

/* the TIME card in Felucca's delay times (params.c N_DTIME): 1/16, 1/8, 1/8D, 1/4 */
static const uint8_t SIS_DTIME[SI_DTIME_COUNT] = {2, 1, 10, 0};
static const char *const SIS_DTIME_NAMES[SI_DTIME_COUNT] = {"1/16", "1/8", "3/16", "1/4"};

/* the OOH's vowel glide: an eighth of the tempo, as the VOICE engine's TALK time (gen_tables.py: a value v is
 * 10000^(v/127) ms) */
static inline int16_t sis_talk(const si_settings *s)
{
    float ms = 60000.0f / (float)(s->bpm ? s->bpm : 120) * 0.5f;
    /* v = 127 log10(ms) / 4, log10 without libm: by the powers of ten and a few steps of the series */
    int dec = 0;
    float x = ms;
    while (x >= 10.0f) { x *= 0.1f; dec++; }
    float y = (x - 1.0f) / (x + 1.0f), y2 = y * y;   /* ln(x) = 2 atanh(y) */
    float ln = 2.0f * y * (1.0f + y2 / 3.0f + y2 * y2 / 5.0f + y2 * y2 * y2 / 7.0f);
    float v = 127.0f * ((float)dec + ln * 0.4342945f) / 4.0f;
    return sis_clamp((int)(v + 0.5f), 0, 127);
}
/* the VOWELS card: VOWL and VOWL2 (A 0, E 32, I 64, O 95, U 127: eng_formant.c's five) */
static const uint8_t SIS_VOWELS[SI_VOWELS_COUNT][2] = {{127, 0}, {0, 64}, {95, 0}, {127, 64}, {0, 95}};
static const char *const SIS_VOWEL_NAMES[SI_VOWELS_COUNT] = {"OO-AH", "AH-EE", "OH-AH", "OO-EE", "AH-OH"};
static const char *const SIS_CHOIR_NAMES[SI_CHOIR_COUNT] = {"OH", "AH", "MIX"};

/* a part's track from the cards, over its sound's preset: the level, the place, the sends, TONE on the parts the
 * lift filters, ENSEMBLE on the stabs and the hook, the VOX page on a VOICE sound, and the matrix's slots for the
 * groove (above). Main loop, the audio held off (or the host) */
static inline void sis_track(track_t *t, const si_groove *g, int part)
{
    const si_settings *s = &g->s;
    const si_style *st = &SI_STYLE[s->style];
    const sis_sound *snd = part == SI_DRUMS ? 0 : sis_of(g, part);
    bool lifted = (st->lift_parts >> part) & 1u;
    /* MIX: 100 is -2 dB (headroom: the master limiter stays out of it), each step down 0.3 dB (MIX 50: -17 dB), 0 off;
     * then the sound's trim (F_DB: half-dB steps); the stabs sit 2 dB lower, under the hook */
    int lvl = 108 - (100 - s->lvl[part]) * 6 / 10 + (snd ? snd->trim : SIS_KIT_TRIM[s->snd[3] % SI_KIT_COUNT]) +
              (part == SI_STABS ? -4 : 0);
    t->p[P_LEVEL] = sis_clamp(s->lvl[part] ? lvl : 0, 1, 127);
    /* the place: the stabs a little left and spread, the hook a little right, the bass and drums centred */
    int w = s->width;
    t->p[P_PAN] = (int16_t)(part == SI_STABS ? -w * 18 / 100 : part == SI_HOOK ? w * 18 / 100 : 0);
    t->p[P_SPRD] = sis_clamp(part == SI_STABS || (part == SI_HOOK && snd->voice == SIS_UNISON) ? w * 90 / 100 : 0, 0, 127);
    /* the sends */
    static const uint8_t REV[SI_PARTS] = {100, 30, 90, 35}, DLY[SI_PARTS] = {60, 0, 100, 0};
    t->p[P_REV] = sis_clamp(s->reverb * 127 / 100 * REV[part] / 100, 0, 127);
    t->p[P_DLY] = sis_clamp(s->delay * 127 / 100 * DLY[part] / 100, 0, 127);
    /* the matrix: the lift and the pump */
    t->p[P_M1SRC] = MS_MODW;
    t->p[P_M1DST] = (int16_t)(snd && snd->bright >= 0 ? MD_E1 + snd->bright : MD_OFF);
    t->p[P_M1AMT] = -32;
    t->p[P_M2SRC] = part == SI_DRUMS ? MS_OFF : MS_EXPR;
    t->p[P_M2DST] = part == SI_DRUMS ? MD_OFF : MD_AMP;
    t->p[P_M2AMT] = 64;
    if (part == SI_DRUMS) {
        t->p[P_VOICE] = V_POLY;
        if (s->snd[3] == SI_KIT_EURO) {           /* the samples: their velocity, SNAP as a low-pass (100: open) */
            t->p[P_M4SRC] = MS_VEL;
            t->p[P_M4DST] = MD_AMP;
            t->p[P_M4AMT] = 64;
            int lpf = 60 + s->snap * 68 / 100;
            if (lpf >= 127) {
                t->p[P_ITYPE] = 0;
            } else {
                t->p[P_ITYPE] = 5;                /* IT_CRUSH at 16 bits and the full rate: its low-pass alone */
                t->p[P_IA] = 127;
                t->p[P_IB] = 127;
                t->p[P_IC] = sis_clamp(lpf, 0, 127);
                t->p[P_IMIX] = 127;
            }
        } else {
            t->p[P_M4SRC] = MS_OFF;
            t->p[P_ITYPE] = 0;
            sis_kit_params(s, &t->p[P_E0]);
        }
        t->p[P_DIST] = 0;
        return;
    }
    if (snd->engine == SIS_SAMPLE) {             /* the piano: its velocity (the SAMPLE engine has none of its own), */
        t->p[P_M4SRC] = MS_VEL;                  /* a low-pass at about 5 kHz to take its edge off (the LIFT closes it */
        t->p[P_M4DST] = MD_AMP;                  /* further), and a room of its own over SPACE's REVERB: a piano */
        t->p[P_M4AMT] = 64;                      /* in a club, pushed back, not one in the listener's ear */
        t->p[P_E4] = SIS_PIANO_CUT;
        t->p[P_REV] = sis_clamp(t->p[P_REV] + SIS_PIANO_ROOM, 0, 127);
    } else {
        t->p[P_M4SRC] = MS_OFF;
    }
    if (snd->glide >= 0)
        t->p[P_GLIDE] = snd->glide;
    if (snd->voice >= 0)
        t->p[P_VOICE] = (int16_t)(snd->voice == SIS_UNISON ? V_UNISON : snd->voice == SIS_LEGATO ? V_LEGATO
                                  : snd->voice == SIS_MONO ? V_MONO : V_POLY);
    if (snd->engine == SIS_VOICE) {              /* the VOX page */
        t->p[P_E0] = SIS_VOWELS[s->vowels % SI_VOWELS_COUNT][0];
        t->p[P_E1] = SIS_VOWELS[s->vowels % SI_VOWELS_COUNT][1];
        t->p[P_E2] = sis_talk(s);
        t->p[P_E3] = s->shift;
        if (part == SI_HOOK && s->glide)
            t->p[P_GLIDE] = sis_clamp(s->glide * 60 / 100, 0, 127);
    }
    /* ENSEMBLE: the chorus on the stabs and the hook, over what the sound has of its own */
    if (part != SI_BASS)
        t->p[P_CHOR] = sis_clamp(s->ensemble * 127 / 100, 0, 127);
    /* TONE on the lifted parts: RESO, DRIVE, CRUSH. A lifted ANALOG or TRIO sound starts bright (CUT at least 100):
     * the lift brings the darkness, a dark preset under it would go silent */
    if (lifted) {
        if ((snd->engine == SIS_ANALOG || snd->engine == SIS_TRIO) && snd->bright >= 0 && t->p[P_E0 + snd->bright] < 100)
            t->p[P_E0 + snd->bright] = 100;
        if (snd->engine == SIS_ANALOG)
            t->p[P_E5] = sis_clamp(s->reso * 120 / 100, 0, 127);
        else if (snd->engine == SIS_TRIO)
            t->p[P_E6] = sis_clamp(s->reso * 120 / 100, 0, 127);
        t->p[P_DIST] = sis_clamp((snd->dist >= 0 ? snd->dist : 0) + s->drive * 50 / 100, 0, 127);   /* (DRIVE 100: 50) */
        if (s->crush) {
            t->p[P_ITYPE] = 5;                    /* IT_CRUSH: the bits fall as CRUSH rises, the rate with them */
            t->p[P_IA] = sis_clamp(127 - s->crush * 100 / 100, 16, 127);
            t->p[P_IB] = sis_clamp(127 - s->crush * 70 / 100, 0, 127);
            t->p[P_IC] = 110;
            t->p[P_IMIX] = 127;
        } else {
            t->p[P_ITYPE] = 0;
        }
    } else {
        if (snd->dist >= 0)
            t->p[P_DIST] = snd->dist;
        t->p[P_ITYPE] = 0;
    }
}

/* the song's own: the tempo for the delay and the LFOs, the delay, the reverb's room */
static inline void sis_song(const si_settings *s)
{
    song.g[G_BPM] = s->bpm;
    song.g[G_DTIME] = SIS_DTIME[s->dtime % SI_DTIME_COUNT];
    song.g[G_DFDBK] = 45;
    song.g[G_DMIX] = 100;
    song.g[G_RSIZE] = (int16_t)(60 + s->reverb * 30 / 100);
    song.g[G_RDAMP] = 50;
    song.g[G_RTYPE] = 0;                          /* (Felucca's ROOM model) */
}

/* the block's controls into the tracks (the audio ISR, before the mix): the lift and the pump */
static inline void sis_block(const si_groove *g)
{
    for (int p = 0; p < SI_PARTS; ++p) {
        trk[p].mw = (uint8_t)(127 - g->cut[p]);
        trk[p].ex_off = g->duck[p];
    }
}

#endif
