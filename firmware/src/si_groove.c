/* SPDX-License-Identifier: GPL-3.0-only */
/* SILL groove engine (see si_groove.h). The styles, the chord loops, the parts' rhythm families, the hook, the form;
 * in time, sixteenth by sixteenth, with swing and a human feel; the lift and the pump every block. */
#include "si_groove.h"

#include "tuna_sys.h"   /* memcpy and memset: the FM-1 has no C library (TUNA_FREESTANDING) */

/* ---- small helpers ---------------------------------------------------------------------------------------- */
static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }
static float clampf(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

static uint32_t xs(uint32_t *s)
{
    uint32_t x = *s ? *s : 0x9E3779B9u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}
static int rnd_n(uint32_t *s, int n) { return n > 0 ? (int)(xs(s) % (uint32_t)n) : 0; }
static float rnd_f(uint32_t *s) { return (float)(xs(s) >> 8) * (1.0f / 16777216.0f); }   /* 0..1 */

static void cpy(char *d, const char *s, int n)
{
    int i = 0;
    for (; s[i] && i < n - 1; ++i)
        d[i] = s[i];
    d[i] = 0;
}

/* ---- names ------------------------------------------------------------------------------------------------ */
static const char *const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char *si_note_name(int pc) { return NOTE_NAMES[((pc % 12) + 12) % 12]; }
static const char *const SCALE_NAMES[SI_SCALES] = {"MIN", "MAJ", "DOR", "HARM"};
const char *si_scale_name(int i) { return SCALE_NAMES[clampi(i, 0, SI_SCALES - 1)]; }
static const char *const SEC_NAMES[SI_SECS] = {"INTRO", "VERSE", "PRE", "CHORUS", "BREAK", "CHORUS"};
static const char *const SEC_SHORT[SI_SECS] = {"IN", "VRS", "PRE", "CHO", "BRK", "CHO"};
const char *si_sec_name(int s) { return SEC_NAMES[clampi(s, 0, SI_SECS - 1)]; }
const char *si_sec_short(int s) { return SEC_SHORT[clampi(s, 0, SI_SECS - 1)]; }

/* the scales, as semitones from the key */
static const uint8_t SCALE[SI_SCALES][7] = {
    {0, 2, 3, 5, 7, 8, 10},   /* MINOR (aeolian): the genre's home */
    {0, 2, 4, 5, 7, 9, 11},   /* MAJOR */
    {0, 2, 3, 5, 7, 9, 10},   /* DORIAN */
    {0, 2, 3, 5, 7, 8, 11},   /* HARMONIC MINOR */
};
/* scale step k (any integer, 7 an octave up) of scale sc from MIDI note base (the key's tonic) */
static int step_note(int sc, int base, int k)
{
    int o = k >= 0 ? k / 7 : -((-k + 6) / 7);
    int d = k - o * 7;
    return base + o * 12 + SCALE[sc][d];
}

int si_snap(const si_settings *s, int note)
{
    int sc = clampi(s->scale, 0, SI_SCALES - 1), pc = ((note - s->key) % 12 + 12) % 12, best = 0, bd = 99;
    for (int i = 0; i < 7; ++i) {
        int d = SCALE[sc][i] - pc;                /* ties go down: the step below first */
        int ad = d < 0 ? -d : d;
        if (ad < bd || (ad == bd && d < 0)) {
            bd = ad;
            best = d;
        }
    }
    return note + best;
}

/* ---- the chord loops: scale degrees (0 = the key's chord), a flavour (3 triads, 7 sevenths, 9 sevenths with the
 * ninth) and whether the fifth degree takes a major third (the dominant in minor) ---- */
typedef struct { uint8_t n, deg[4], flavour, dom; } si_loopdef;
static const si_loopdef SI_LOOP[SI_LOOPS] = {
    {4, {0, 5, 2, 6}, 3, 0},   /* 1  i VI III VII: the anthem */
    {4, {0, 5, 6, 0}, 3, 0},   /* 2  i VI VII i */
    {4, {5, 6, 0, 0}, 3, 0},   /* 3  VI VII i i: the lift into the tonic */
    {4, {0, 6, 5, 6}, 3, 0},   /* 4  i VII VI VII */
    {4, {0, 3, 5, 4}, 3, 1},   /* 5  i iv VI V */
    {4, {0, 2, 6, 5}, 3, 0},   /* 6  i III VII VI */
    {4, {0, 4, 5, 3}, 3, 0},   /* 7  I V vi IV */
    {4, {0, 5, 3, 4}, 3, 0},   /* 8  I vi IV V */
    {4, {0, 5, 3, 4}, 3, 1},   /* 9  i VI iv V: the drama */
    {4, {0, 5, 2, 6}, 7, 0},   /* 10 i7 VImaj7 IIImaj7 VII7: the piano's sevenths */
    {4, {0, 3, 6, 2}, 3, 0},   /* 11 i iv VII III */
    {4, {0, 6, 5, 4}, 3, 1},   /* 12 i VII VI V: the descent */
};
void si_loop_name(const si_settings *s, int loop, char *out, int max)
{
    (void)s;
    char b[12] = "LOOP ";
    int v = clampi(loop, 0, SI_LOOPS - 1) + 1, k = 5;
    if (v >= 10) b[k++] = (char)('0' + v / 10);
    b[k++] = (char)('0' + v % 10);
    b[k] = 0;
    cpy(out, b, max);
}

/* ---- the styles ------------------------------------------------------------------------------------------- */
/* the rhythm families */
enum { STAB_EURO, STAB_PIANO, STAB_PAD, STAB_SKANK, STAB_RAVE, STAB_BOUNCE, STAB_COUNT };
enum { HOOK_MELODY, HOOK_ARP };
enum { DRUMS_EURO, DRUMS_SKANK, DRUMS_BALLAD, DRUMS_RAVE, DRUMS_HOUSE, DRUMS_ITALO, DRUMS_COUNT };
#define P(x) (1u << (x))

/* name side bpm key scale | lift pump vox energy | snd[4] | loops | bars swing hats fills bassline |
 * kick clap crash choir | stab hook drums | speed range gate repeat | lift parts | reverb delay dtime |
 * reso drive ensemble | the three more sounds of STABS, BASS and HOOK a song picks from (si_song_sounds).
 * Sounds (si_sounds.h): STABS 0 PIANO 1 ORGAN 2 SUPER 3 STRINGS 4 PWM 5 PLUCK 6 HOOVER 7 ORCH 8 EPIANO 9 PAD
 * 10 HOUSE 11 DEEP 12 KEYS 13 CHOIR 14 VIOLINS 15 TRANCE; BASS 0 FM 1 SQUARE 2 SUB 3 FAT 4 ACID
 * 5 REESE 6 SLAP 7 SLIDE 8 OCTAVE 9 WOW 10 ORGAN 11 SYNTH 12 SINE 13 TRANCE 14 DX 15 GROWL; HOOK 0 SUPER 1 SAW
 * 2 PLUCK 3 POP 4 PIANO 5 HOOVER 6 OOH 7 SYNC 8 FLUTE 9 PAD 10 REEDS 11 ANTHEM 12 CHOIR 13 WHISTLE 14 TRANCE
 * 15 CORNET; the kit 0 EURO 1 SESSION 2 MACHINE */
const si_style SI_STYLE[SI_STYLES] = {
    {"EUROSTOMP", SI_SIDE_CLUB, 136, 9, SI_SC_MINOR, 85, 55, SI_VOX_CHORUS, 80, {2, 3, 0, 0}, {0, 1, 3, 5}, 1,
     0, SI_HATS_OFFBEAT, SI_FILLS_8, SI_BL_OFFBEAT, SI_KICK_PUNCH, SI_CLAP_CLAP, SI_CRASH_SECTION, SI_CHOIR_MIX,
     STAB_EURO, HOOK_MELODY, DRUMS_EURO, SI_SPEED_FAST, 1, 80, 2, P(SI_STABS) | P(SI_HOOK), 25, 25, SI_DTIME_3_16,
     30, 8, 45, {{10, 11, 15}, {11, 8, 10}, {11, 14, 13}}},
    {"VIKING", SI_SIDE_CLUB, 138, 4, SI_SC_HARMONIC, 80, 50, SI_VOX_CHORUS, 85, {14, 3, 1, 0}, {8, 4, 0, 3}, 1,
     0, SI_HATS_16THS, SI_FILLS_4, SI_BL_OCTAVE, SI_KICK_PUNCH, SI_CLAP_BOTH, SI_CRASH_SECTION, SI_CHOIR_OH,
     STAB_EURO, HOOK_MELODY, DRUMS_EURO, SI_SPEED_MID, 2, 90, 2, P(SI_STABS) | P(SI_HOOK), 35, 20, SI_DTIME_8,
     25, 10, 35, {{7, 13, 3}, {8, 15, 11}, {11, 12, 0}}},
    {"BUBBLEGUM", SI_SIDE_CLUB, 130, 2, SI_SC_MAJOR, 90, 45, SI_VOX_CHORUS, 75, {5, 1, 3, 0}, {6, 7, 0, 1}, 1,
     0, SI_HATS_OFFBEAT, SI_FILLS_8, SI_BL_BOUNCE, SI_KICK_PUNCH, SI_CLAP_CLAP, SI_CRASH_SECTION, SI_CHOIR_AH,
     STAB_BOUNCE, HOOK_MELODY, DRUMS_EURO, SI_SPEED_MID, 1, 50, 2, P(SI_STABS), 20, 20, SI_DTIME_8,
     20, 5, 30, {{8, 10, 0}, {11, 14, 8}, {13, 7, 2}}},
    {"HEARTACHE", SI_SIDE_CLUB, 134, 2, SI_SC_MINOR, 80, 50, SI_VOX_CHORUS, 70, {0, 0, 6, 0}, {2, 0, 5, 3}, 1,
     0, SI_HATS_OFFBEAT, SI_FILLS_8, SI_BL_OFFBEAT, SI_KICK_ROUND, SI_CLAP_CLAP, SI_CRASH_SECTION, SI_CHOIR_AH,
     STAB_PIANO, HOOK_MELODY, DRUMS_EURO, SI_SPEED_SLOW, 1, 95, 4, P(SI_STABS), 40, 25, SI_DTIME_3_16,
     20, 5, 40, {{0, 11, 13}, {10, 12, 11}, {12, 11, 13}}},
    {"RAVE", SI_SIDE_CLUB, 140, 6, SI_SC_MINOR, 80, 60, SI_VOX_CHORUS, 90, {6, 4, 5, 0}, {3, 0, 4, 1}, 1,
     0, SI_HATS_16THS, SI_FILLS_4, SI_BL_ROLLING, SI_KICK_PUNCH, SI_CLAP_BOTH, SI_CRASH_8, SI_CHOIR_OH,
     STAB_RAVE, HOOK_MELODY, DRUMS_RAVE, SI_SPEED_FAST, 1, 60, 1, P(SI_STABS) | P(SI_HOOK), 25, 20, SI_DTIME_8,
     45, 18, 10, {{15, 11, 7}, {5, 13, 15}, {14, 0, 7}}},
    {"SKANK", SI_SIDE_RADIO, 95, 1, SI_SC_MINOR, 75, 15, SI_VOX_CHORUS, 55, {1, 2, 8, 0}, {10, 3, 0, 5}, 1,
     15, SI_HATS_8THS, SI_FILLS_8, SI_BL_REGGAE, SI_KICK_ROUND, SI_CLAP_SNARE, SI_CRASH_OFF, SI_CHOIR_AH,
     STAB_SKANK, HOOK_MELODY, DRUMS_SKANK, SI_SPEED_MID, 1, 70, 2, P(SI_STABS), 30, 30, SI_DTIME_3_16,
     15, 10, 30, {{5, 10, 0}, {7, 12, 10}, {15, 13, 3}}},
    {"PIANOHOUSE", SI_SIDE_RADIO, 125, 7, SI_SC_DORIAN, 85, 40, SI_VOX_CHORUS, 70, {0, 0, 9, 0}, {9, 0, 5, 1}, 1,
     10, SI_HATS_OPEN, SI_FILLS_8, SI_BL_OFFBEAT, SI_KICK_ROUND, SI_CLAP_CLAP, SI_CRASH_SECTION, SI_CHOIR_OH,
     STAB_PIANO, HOOK_MELODY, DRUMS_HOUSE, SI_SPEED_PAD, 2, 60, 2, P(SI_STABS), 30, 20, SI_DTIME_3_16,
     15, 10, 25, {{0, 0, 10}, {10, 12, 11}, {9, 9, 9}}},
    {"PARTY", SI_SIDE_RADIO, 140, 7, SI_SC_MAJOR, 90, 50, SI_VOX_ALL, 85, {2, 8, 7, 0}, {6, 7, 1, 0}, 1,
     0, SI_HATS_OFFBEAT, SI_FILLS_4, SI_BL_BOUNCE, SI_KICK_PUNCH, SI_CLAP_CLAP, SI_CRASH_SECTION, SI_CHOIR_MIX,
     STAB_BOUNCE, HOOK_MELODY, DRUMS_EURO, SI_SPEED_FAST, 1, 55, 1, P(SI_STABS) | P(SI_HOOK), 20, 15, SI_DTIME_8,
     20, 10, 30, {{7, 10, 5}, {11, 14, 1}, {15, 2, 11}}},
    {"BALLAD", SI_SIDE_RADIO, 100, 4, SI_SC_MINOR, 90, 0, SI_VOX_OFF, 35, {9, 7, 4, 0}, {2, 0, 6, 5}, 2,
     0, SI_HATS_8THS, SI_FILLS_OFF, SI_BL_LONG, SI_KICK_ROUND, SI_CLAP_SNARE, SI_CRASH_SECTION, SI_CHOIR_AH,
     STAB_PAD, HOOK_MELODY, DRUMS_BALLAD, SI_SPEED_SLOW, 2, 95, 4, P(SI_STABS), 50, 25, SI_DTIME_4,
     10, 0, 50, {{14, 0, 13}, {12, 2, 0}, {12, 6, 10}}},
    {"ITALO", SI_SIDE_RADIO, 120, 9, SI_SC_MINOR, 80, 35, SI_VOX_CHORUS, 65, {3, 8, 6, 0}, {11, 0, 3, 4}, 1,
     0, SI_HATS_8THS, SI_FILLS_8, SI_BL_OCTAVE, SI_KICK_ROUND, SI_CLAP_SNARE, SI_CRASH_SECTION, SI_CHOIR_OH,
     STAB_PAD, HOOK_ARP, DRUMS_ITALO, SI_SPEED_MID, 2, 60, 2, P(SI_STABS), 35, 30, SI_DTIME_3_16,
     20, 10, 45, {{4, 11, 15}, {11, 13, 1}, {2, 14, 7}}},
};
const char *si_style_name(int i) { return SI_STYLE[clampi(i, 0, SI_STYLES - 1)].name; }

int si_bars_per_chord(const si_settings *s) { return s->bars >= 4 ? 4 : s->bars >= 2 ? 2 : 1; }
int si_repeat_bars(const si_settings *s) { return s->repeat >= 4 ? 4 : s->repeat >= 2 ? 2 : 1; }

/* ---- settings --------------------------------------------------------------------------------------------- */
void si_style_apply(si_settings *s, int style)
{
    const si_style *st = &SI_STYLE[clampi(style, 0, SI_STYLES - 1)];
    s->style = (uint8_t)clampi(style, 0, SI_STYLES - 1);
    s->lift = st->lift;
    s->pump = st->pump;
    s->vox = st->vox;
    s->energy = st->energy;
    for (int i = 0; i < 4; ++i)
        s->snd[i] = st->snd[i];
    s->reso = st->reso;
    s->drive = st->drive;
    s->ensemble = st->ensemble;
    s->crush = 0;
    s->vowels = SI_VOWELS_OO_AH;
    s->choir = st->choir;
    s->glide = 20;
    s->shift = 0;
    s->key = st->key;
    s->scale = st->scale;
    s->chords = st->loops[0];
    s->bars = st->bars;
    s->swing = st->swing;
    s->human = 10;
    s->hats = st->hats;
    s->fills = st->fills;
    s->bassline = st->bassline;
    s->kick = st->kick;
    s->clap = st->clap;
    s->snap = 80;
    s->crash = st->crash;
    s->reverb = st->reverb;
    s->delay = st->delay;
    s->dtime = st->dtime;
    s->width = 70;
    s->speed = st->speed;
    s->range = st->range;
    s->gate = st->gate;
    s->repeat = st->repeat;
    s->bpm = st->bpm;
    s->song = 0;
}

void si_song_sounds(si_settings *s)
{
    const si_style *st = &SI_STYLE[clampi(s->style, 0, SI_STYLES - 1)];
    uint32_t r = (uint32_t)s->song * 2654435761u + (uint32_t)s->style * 40503u + 7u;
    for (int p = 0; p < 3; ++p) {
        int k = s->song ? rnd_n(&r, 4) : 0;        /* 0: the style's own */
        s->snd[p] = k ? st->alt[p][k - 1] : st->snd[p];
    }
}

void si_defaults(si_settings *s)
{
    memset(s, 0, sizeof *s);
    si_style_apply(s, SI_ST_EUROSTOMP);
    s->lvl[SI_STABS] = 85;                         /* (the sounds are evened out: si_sounds.h trims) */
    s->lvl[SI_BASS] = 95;
    s->lvl[SI_HOOK] = 85;
    s->lvl[SI_DRUMS] = 85;
    s->arc = 1;
    s->verse = 8;
    s->chorus = 8;
    s->keyup = 1;
    s->palette = 10;                               /* LASER (ui_palettes.h UI_LASER_INDEX) */
    s->look = 0;
}

void si_settings_fix(si_settings *s)
{
    s->style = (uint8_t)clampi(s->style, 0, SI_STYLES - 1);
    s->lift = (uint8_t)clampi(s->lift, 0, 100);
    s->pump = (uint8_t)clampi(s->pump, 0, 100);
    s->vox = (uint8_t)clampi(s->vox, 0, SI_VOX_COUNT - 1);
    s->energy = (uint8_t)clampi(s->energy, 0, 100);
    for (int i = 0; i < 3; ++i)
        s->snd[i] = (uint8_t)clampi(s->snd[i], 0, SI_SOUNDS - 1);
    s->snd[3] = (uint8_t)clampi(s->snd[3], 0, SI_KIT_COUNT - 1);
    s->reso = (uint8_t)clampi(s->reso, 0, 100);
    s->drive = (uint8_t)clampi(s->drive, 0, 100);
    s->ensemble = (uint8_t)clampi(s->ensemble, 0, 100);
    s->crush = (uint8_t)clampi(s->crush, 0, 100);
    s->vowels = (uint8_t)clampi(s->vowels, 0, SI_VOWELS_COUNT - 1);
    s->choir = (uint8_t)clampi(s->choir, 0, SI_CHOIR_COUNT - 1);
    s->glide = (uint8_t)clampi(s->glide, 0, 100);
    s->shift = (int8_t)clampi(s->shift, -12, 12);
    s->key = (uint8_t)clampi(s->key, 0, 11);
    s->scale = (uint8_t)clampi(s->scale, 0, SI_SCALES - 1);
    s->chords = (uint8_t)clampi(s->chords, 0, SI_LOOPS - 1);
    s->bars = (uint8_t)(s->bars >= 4 ? 4 : s->bars >= 2 ? 2 : 1);
    s->swing = (uint8_t)clampi(s->swing, 0, 60);
    s->human = (uint8_t)clampi(s->human, 0, 100);
    s->hats = (uint8_t)clampi(s->hats, 0, SI_HATS_COUNT - 1);
    s->fills = (uint8_t)clampi(s->fills, 0, SI_FILLS_COUNT - 1);
    s->bassline = (uint8_t)clampi(s->bassline, 0, SI_BL_COUNT - 1);
    s->kick = (uint8_t)clampi(s->kick, 0, SI_KICK_COUNT - 1);
    s->clap = (uint8_t)clampi(s->clap, 0, SI_CLAP_COUNT - 1);
    s->snap = (uint8_t)clampi(s->snap, 0, 100);
    s->crash = (uint8_t)clampi(s->crash, 0, SI_CRASH_COUNT - 1);
    for (int i = 0; i < 4; ++i)
        s->lvl[i] = (uint8_t)clampi(s->lvl[i], 0, 100);
    s->reverb = (uint8_t)clampi(s->reverb, 0, 100);
    s->delay = (uint8_t)clampi(s->delay, 0, 100);
    s->dtime = (uint8_t)clampi(s->dtime, 0, SI_DTIME_COUNT - 1);
    s->width = (uint8_t)clampi(s->width, 0, 100);
    s->arc = (uint8_t)(s->arc ? 1 : 0);
    s->verse = (uint8_t)(s->verse >= 16 ? 16 : 8);
    s->chorus = (uint8_t)(s->chorus >= 16 ? 16 : 8);
    s->keyup = (uint8_t)clampi(s->keyup, 0, 2);
    s->speed = (uint8_t)clampi(s->speed, 0, SI_SPEED_COUNT - 1);
    s->range = (uint8_t)clampi(s->range, 1, 2);
    s->gate = (uint8_t)clampi(s->gate, 10, 100);
    s->repeat = (uint8_t)(s->repeat >= 4 ? 4 : s->repeat >= 2 ? 2 : 1);
    s->bpm = (uint8_t)clampi(s->bpm ? s->bpm : 136, 60, 200);
    s->look = (uint8_t)(s->look ? 1 : 0);
    if (s->song > 999)
        s->song = 0;
}

/* ---- the song: the chords, the patterns, the hook --------------------------------------------------------- */
static void chord_name(si_chord *c, int tr, int third, int fifth, int seventh, bool ninth)
{
    const char *q;
    if (third == 3 && fifth == 6)
        q = seventh == 9 ? "DIM7" : seventh ? "M7B5" : "DIM";
    else if (third == 3)
        q = !seventh ? "MIN" : ninth ? "MIN9" : "MIN7";
    else if (fifth == 8)
        q = "AUG";
    else if (seventh == 10)
        q = ninth ? "9" : "7";
    else if (seventh == 11)
        q = ninth ? "MAJ9" : "MAJ7";
    else
        q = "";
    char b[10];
    cpy(b, NOTE_NAMES[(c->root + tr) % 12], sizeof b);
    int k = 0;
    while (b[k]) k++;
    for (int i = 0; q[i] && k < 7; ++i)
        b[k++] = q[i];
    b[k] = 0;
    cpy(c->name, b, sizeof c->name);
}

/* the chord on scale degree deg: its tones and a voicing near center (MIDI), the voice leading of a player's hand:
 * of the chord's inversions, the one whose middle is closest. Its name with the key up (tr) in it; its notes without
 * (the key up is added as each note sounds) */
static void build_chord(si_chord *c, const si_settings *s, int tr, int deg, int flavour, bool dom, int *center)
{
    int sc = clampi(s->scale, 0, SI_SCALES - 1);
    int r = SCALE[sc][deg % 7];
    int iv[5];
    for (int k = 0; k < 5; ++k) {                  /* root, third, fifth, seventh, ninth: stacked thirds */
        int d = deg + 2 * k;
        iv[k] = SCALE[sc][d % 7] + 12 * (d / 7) - r;
    }
    if (dom && deg % 7 == 4) {                     /* the dominant in minor: a major third, a minor seventh */
        iv[1] = 4;
        iv[3] = 10;
    }
    int third = iv[1], fifth = iv[2], seventh = flavour >= 7 ? iv[3] : 0;
    bool ninth = flavour == 9 && (iv[4] == 14) && seventh;
    c->root = (uint8_t)((s->key + r) % 12);
    c->tone[0] = c->root;
    c->tone[1] = (uint8_t)((c->root + third) % 12);
    c->tone[2] = (uint8_t)((c->root + fifth) % 12);
    c->tone[3] = (uint8_t)((c->root + (seventh ? seventh : 12)) % 12);
    chord_name(c, tr, third, fifth, seventh, ninth);
    /* the voicing: four notes, close (a triad doubles its root on top); with a ninth it takes the root's place */
    int pcs[4] = {ninth ? (c->root + 14) % 12 : c->root, c->tone[1], c->tone[2], seventh ? c->tone[3] : (c->root + 12) % 12};
    int best_d = 1 << 20;
    int bestv[4] = {60, 64, 67, 72};
    for (int inv = 0; inv < 4; ++inv) {            /* each inversion: its lowest note, the others above in order */
        for (int base = 50; base <= 68; ++base) {
            if ((base % 12) != pcs[inv])
                continue;
            int v[4], p = base;
            v[0] = p;
            for (int j = 1; j < 4; ++j) {
                int pc = pcs[(inv + j) % 4];
                int x = p + 1;
                while (x % 12 != pc)
                    ++x;
                v[j] = x;
                p = x;
            }
            int mid = (v[0] + v[3]) / 2;
            int d = (mid - *center) * (mid - *center) + (v[3] > 79 ? 400 : 0);
            if (d < best_d) {
                best_d = d;
                memcpy(bestv, v, sizeof bestv);
            }
        }
    }
    for (int j = 0; j < 4; ++j)
        c->note[j] = (uint8_t)bestv[j];
    c->note[4] = 0;
    c->n = 4;
    *center = (bestv[0] + bestv[3]) / 2;
}

/* the hook's one-bar rhythms, by SPEED: per step the length in sixteenths of the note starting there (0 none) */
static const uint8_t HOOK_RHY[SI_SPEED_PAD][4][16] = {   /* (PAD has no rhythm: the chords, held) */
    [SI_SPEED_SLOW] = {{4, 0, 0, 0, 2, 0, 4, 0, 0, 0, 2, 0, 4, 0, 0, 0},
                       {6, 0, 0, 0, 0, 0, 2, 0, 8, 0, 0, 0, 0, 0, 0, 0},
                       {3, 0, 0, 3, 0, 0, 2, 0, 6, 0, 0, 0, 0, 0, 2, 0},
                       {2, 0, 4, 0, 0, 0, 2, 0, 4, 0, 0, 0, 4, 0, 0, 0}},
    [SI_SPEED_MID] = {{2, 0, 2, 0, 2, 0, 2, 0, 1, 0, 1, 0, 2, 0, 2, 0},
                      {1, 0, 1, 0, 1, 0, 3, 0, 0, 1, 0, 1, 0, 2, 0, 0},
                      {2, 0, 1, 0, 2, 0, 1, 0, 2, 0, 2, 0, 4, 0, 0, 0},
                      {1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0, 3, 0, 0, 1}},
    [SI_SPEED_FAST] = {{2, 0, 0, 2, 0, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0},   /* the 3-3-2 syncopated riff */
                       {1, 0, 1, 2, 0, 0, 1, 1, 2, 0, 0, 2, 0, 0, 2, 0},
                       {3, 0, 0, 3, 0, 0, 2, 0, 3, 0, 0, 3, 0, 0, 2, 0},
                       {1, 1, 2, 0, 1, 0, 2, 0, 1, 1, 2, 0, 1, 0, 2, 0}},
};

/* a bar of the hook into dst (16 steps) from rhythm rhy, its pitches a walk from *note within lo..hi */
static void hook_bar(int8_t *dst, uint8_t *len, const uint8_t *rhy, uint32_t *r, int *note, int lo, int hi)
{
    for (int i = 0; i < 16; ++i) {
        dst[i] = -128;
        len[i] = 0;
        if (!rhy[i])
            continue;
        dst[i] = (int8_t)*note;
        len[i] = rhy[i];
        int roll = rnd_n(r, 10), mv;                /* repeated notes, steps, now and then a leap */
        if (roll < 3) mv = 0;
        else if (roll < 7) mv = rnd_n(r, 2) ? 1 : -1;
        else if (roll < 9) mv = rnd_n(r, 2) ? 2 : -2;
        else mv = rnd_n(r, 2) ? 4 : -3;
        int nn = *note + mv;
        if (nn > hi || nn < lo)
            nn = *note - mv;
        *note = clampi(nn, lo, hi);
    }
}

/* the hook: a one-bar motif (A), repeated over REPEAT bars as a phrase: A A' (2), A A' A A'' (4), A' and A'' its
 * answers (the first half again, a new second half landing on the tonic or the fifth). Scale steps from the tonic */
static void make_hook(si_groove *g, uint32_t seed)
{
    const si_settings *s = &g->s;
    uint32_t r = seed * 2654435761u + 0x51ED27u;
    int sp = clampi(s->speed, 0, SI_SPEED_PAD - 1);  /* (PAD: the MID tune is made but not played) */
    int lo = s->range >= 2 ? -3 : 0, hi = s->range >= 2 ? 10 : 7;
    for (int i = 0; i < SI_HOOK_LEN; ++i) {
        g->hook[i] = -128;
        g->hook_len[i] = 0;
        g->hook_tup[i] = 0;
    }
    int note = rnd_n(&r, 3) * 2;                   /* starts on the tonic, the third or the fifth */
    const uint8_t *rhy = HOOK_RHY[sp][rnd_n(&r, 4)];
    int8_t a[16], b[16];
    uint8_t al[16], bl[16];
    hook_bar(a, al, rhy, &r, &note, lo, hi);
    /* now and then a tuplet in the motif's second half (more often the faster the hook): an arpeggio of 3, 4, 5 or 6
     * notes in the space of 4 or 6 sixteenths, from the note the walk reached; the answers keep their plain rhythm */
    uint8_t at[16] = {0};
    {
        static const uint8_t CHANCE[SI_SPEED_PAD] = {20, 40, 55};   /* % a song: SLOW MID FAST */
        if (rnd_n(&r, 100) < CHANCE[sp]) {
            static const uint8_t KINDS[4] = {3, 4, 5, 6};
            int n = KINDS[rnd_n(&r, 4)], span = si_tuplet_span(n);
            int pos = span == 6 ? (rnd_n(&r, 2) ? 8 : 10) : (rnd_n(&r, 2) ? 8 : 12);
            int start = -128;
            for (int k = pos; k >= 0 && start == -128; --k)
                start = a[k];
            if (start == -128)
                start = 2;
            for (int k = pos; k < pos + span && k < 16; ++k) {
                a[k] = -128;
                al[k] = 0;
            }
            a[pos] = (int8_t)clampi(start, lo + 1, hi - 1);
            al[pos] = (uint8_t)span;
            at[pos] = (uint8_t)n;
        }
    }
    /* the answer: A's first half, a new second half; its last note on the tonic or the fifth, held */
    memcpy(b, a, sizeof b);
    memcpy(bl, al, sizeof bl);
    note = a[0] != -128 ? a[0] : 0;
    {
        int8_t t[16];
        uint8_t tl[16];
        hook_bar(t, tl, rhy, &r, &note, lo, hi);
        for (int i = 8; i < 16; ++i) {
            b[i] = t[i];
            bl[i] = tl[i];
        }
    }
    for (int i = 15; i >= 8; --i)
        if (b[i] != -128) {
            b[i] = (int8_t)(rnd_n(&r, 2) ? 0 : 4);
            break;
        }
    int rep = si_repeat_bars(s);
    for (int bar = 0; bar < rep; ++bar) {
        bool answer = bar % 2 == 1;
        for (int i = 0; i < 16; ++i) {
            g->hook[bar * 16 + i] = answer ? b[i] : a[i];
            g->hook_len[bar * 16 + i] = answer ? bl[i] : al[i];
            g->hook_tup[bar * 16 + i] = answer ? 0 : at[i];
        }
    }
    if (rep == 4) {                                /* the phrase's last bar: lands home, long */
        for (int i = 63; i >= 56; --i)
            if (g->hook[i] != -128) {
                g->hook[i] = 0;
                g->hook_len[i] = (uint8_t)(64 - i);
                break;
            }
    }
}

void si_rebuild(si_groove *g)
{
    si_settings *s = &g->s;
    const si_loopdef *L = &SI_LOOP[clampi(s->chords, 0, SI_LOOPS - 1)];
    int center = 64;
    g->nloop = L->n;
    for (int i = 0; i < L->n; ++i)
        build_chord(&g->loop[i], s, g->tr, L->deg[i], L->flavour, L->dom != 0, &center);
    uint32_t seed = (uint32_t)s->style * 7919u + (uint32_t)s->song * 104729u + 17u;
    uint32_t r = seed;
    g->stab_pat = (uint8_t)rnd_n(&r, 4);
    g->bass_pat = (uint8_t)rnd_n(&r, 2);
    g->drum_var = (uint8_t)rnd_n(&r, 4);
    make_hook(g, seed + (uint32_t)g->hook_var * 31u);
}

/* ---- the parts' rhythms ------------------------------------------------------------------------------------ */
/* a pattern: 16 steps, the length in sixteenths of the chord starting there (0 none) */
static const uint8_t STAB_PATS[STAB_COUNT][4][16] = {
    [STAB_EURO] = {{2, 0, 0, 2, 0, 0, 2, 0, 0, 0, 2, 0, 2, 0, 0, 0},
                   {3, 0, 0, 3, 0, 0, 2, 0, 3, 0, 0, 3, 0, 0, 2, 0},
                   {0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0},
                   {2, 0, 0, 1, 0, 1, 0, 0, 2, 0, 0, 1, 0, 1, 2, 0}},
    [STAB_PIANO] = {{0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0},   /* the offbeat piano: every "and" */
                    {0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 2, 0, 0},   /* the last one early */
                    {0, 0, 2, 0, 0, 2, 0, 0, 0, 0, 2, 0, 0, 2, 0, 0},   /* the second and the last early */
                    {0, 0, 2, 0, 0, 0, 2, 1, 0, 0, 2, 0, 0, 0, 2, 1}},  /* with a sixteenth after */
    [STAB_PAD] = {{16}, {16}, {16}, {16}},
    [STAB_SKANK] = {{0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0},
                    {0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0},
                    {0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 0, 0, 0, 1, 1},
                    {0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 1, 1, 0}},
    [STAB_RAVE] = {{3, 0, 0, 3, 0, 0, 4, 0, 0, 0, 3, 0, 0, 3, 0, 0},
                   {1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 0, 0},
                   {2, 0, 0, 0, 0, 0, 0, 2, 0, 0, 2, 0, 0, 0, 0, 0},
                   {1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0}},
    [STAB_BOUNCE] = {{0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0},
                     {2, 0, 0, 0, 0, 0, 0, 2, 0, 0, 2, 0, 0, 0, 0, 0},
                     {1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0},
                     {0, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 0, 1, 0, 1, 0}},
};
/* the bass: per step a tone (0 none, 1 root, 2 octave, 3 fifth, 4 seventh, 5 approach to the next chord's root,
 * 6 the scale step above the root) and its length */
enum { BR_NONE, BR_ROOT, BR_OCT, BR_FIFTH, BR_SEV, BR_APP, BR_PASS };
typedef struct { uint8_t tone[16], len[16]; } bass_pat_t;
static const bass_pat_t BASS_PATS[SI_BL_COUNT][2] = {
    [SI_BL_OFFBEAT] = {{{0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0}, {0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0}},
                       {{0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 2, 0}, {0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0, 0, 0, 2, 0}}},
    [SI_BL_OCTAVE] = {{{1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0}, {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0}},
                      {{1, 0, 2, 0, 1, 0, 2, 0, 1, 0, 2, 0, 3, 0, 2, 5}, {1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1}}},
    [SI_BL_ROLLING] = {{{1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1}, {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1}},
                       {{1, 0, 1, 2, 1, 0, 1, 2, 1, 0, 1, 2, 1, 0, 2, 1}, {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1}}},
    [SI_BL_REGGAE] = {{{1, 0, 0, 0, 0, 0, 3, 0, 1, 0, 0, 2, 0, 0, 5, 0}, {3, 0, 0, 0, 0, 0, 2, 0, 3, 0, 0, 1, 0, 0, 2, 0}},
                      {{1, 0, 0, 1, 0, 0, 3, 0, 0, 0, 1, 0, 3, 0, 2, 0}, {2, 0, 0, 2, 0, 0, 3, 0, 0, 0, 2, 0, 2, 0, 1, 0}}},
    [SI_BL_LONG] = {{{1, 0, 0, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0}, {8, 0, 0, 0, 0, 0, 0, 0, 8, 0, 0, 0, 0, 0, 0, 0}},
                    {{1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0, 0, 0}, {12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 0, 0}}},
    [SI_BL_BOUNCE] = {{{1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2, 0}, {1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2, 0}},
                      {{1, 0, 2, 2, 1, 0, 2, 2, 1, 0, 2, 2, 1, 0, 2, 5}, {1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1}}},
};
/* the drums: per drum the velocity at each step (0 none); KICK SNARE CLAP HAT OPEN (HATS overrides the hats) */
typedef struct { uint8_t v[5][16]; } drum_pat_t;
static const drum_pat_t DRUM_PATS[DRUMS_COUNT] = {
    [DRUMS_EURO] = {{{122, 0, 0, 0, 122, 0, 0, 0, 122, 0, 0, 0, 122, 0, 0, 0},
                     {0},
                     {0, 0, 0, 0, 118, 0, 0, 0, 0, 0, 0, 0, 118, 0, 0, 0},
                     {70, 40, 0, 45, 70, 40, 0, 45, 70, 40, 0, 45, 70, 40, 0, 45},
                     {0, 0, 95, 0, 0, 0, 95, 0, 0, 0, 95, 0, 0, 0, 95, 0}}},
    [DRUMS_SKANK] = {{{118, 0, 0, 0, 0, 0, 0, 0, 112, 0, 70, 0, 0, 0, 0, 0},
                      {0, 0, 0, 0, 112, 0, 0, 0, 0, 0, 0, 0, 112, 0, 0, 0},
                      {0},
                      {70, 0, 50, 0, 70, 0, 50, 0, 70, 0, 50, 0, 70, 0, 50, 0},
                      {0}}},
    [DRUMS_BALLAD] = {{{110, 0, 0, 0, 0, 0, 0, 0, 0, 0, 90, 0, 0, 0, 0, 0},
                       {0, 0, 0, 0, 0, 0, 0, 0, 105, 0, 0, 0, 0, 0, 0, 0},
                       {0},
                       {65, 0, 45, 0, 65, 0, 45, 0, 65, 0, 45, 0, 65, 0, 45, 0},
                       {0}}},
    [DRUMS_RAVE] = {{{124, 0, 0, 0, 124, 0, 0, 0, 124, 0, 0, 0, 124, 0, 0, 0},
                     {0, 0, 0, 0, 0, 0, 0, 40, 0, 0, 0, 0, 0, 35, 0, 45},
                     {0, 0, 0, 0, 120, 0, 0, 0, 0, 0, 0, 0, 120, 0, 0, 0},
                     {80, 50, 65, 50, 80, 50, 65, 50, 80, 50, 65, 50, 80, 50, 65, 50},
                     {0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0, 0, 0, 100, 0}}},
    [DRUMS_HOUSE] = {{{118, 0, 0, 0, 118, 0, 0, 0, 118, 0, 0, 0, 118, 0, 0, 0},
                      {0},
                      {0, 0, 0, 0, 112, 0, 0, 0, 0, 0, 0, 0, 112, 0, 0, 0},
                      {55, 35, 0, 35, 55, 35, 0, 35, 55, 35, 0, 35, 55, 35, 0, 35},
                      {0, 0, 92, 0, 0, 0, 92, 0, 0, 0, 92, 0, 0, 0, 92, 0}}},
    [DRUMS_ITALO] = {{{118, 0, 0, 0, 118, 0, 0, 0, 118, 0, 0, 0, 118, 0, 0, 0},
                      {0, 0, 0, 0, 115, 0, 0, 0, 0, 0, 0, 0, 115, 0, 0, 0},
                      {0},
                      {75, 0, 55, 0, 75, 0, 55, 0, 75, 0, 55, 0, 75, 0, 55, 0},
                      {0}}},
};

/* ---- the engine ------------------------------------------------------------------------------------------- */
void si_init(si_groove *g, float sample_rate)
{
    memset(g, 0, sizeof *g);
    g->sr = sample_rate;
    si_defaults(&g->s);
    g->step = -1;
    g->next_style = g->next_song = -1;
    g->perf_next = -1;
    for (int k = 0; k < 27; ++k)
        g->key_note[k] = -1;
    for (int p = 0; p < SI_PARTS; ++p)
        g->cut[p] = 127;
    si_rebuild(g);
}

void si_set_sound(si_groove *g, void (*fn)(void *ctx, const si_event *e), void *ctx)
{
    g->sound = fn;
    g->sound_ctx = ctx;
}

static void emit(si_groove *g, int on, int part, int note, int vel)
{
    if (!g->sound)
        return;
    si_event e = {(uint8_t)on, (uint8_t)part, (uint8_t)clampi(note, 0, 127), (uint8_t)clampi(vel, 1, 127)};
    g->sound(g->sound_ctx, &e);
}

static void feed(si_groove *g, int part, int note, int vel)
{
    uint8_t w = (uint8_t)((g->feed_w + 1u) % SI_FEED);
    if (w == g->feed_r)
        return;
    si_seen *n = &g->feed[g->feed_w];
    n->part = (uint8_t)part;
    n->note = (uint8_t)note;
    n->vel = (uint8_t)vel;
    g->feed_w = w;
}

bool si_feed_pop(si_groove *g, si_seen *n)
{
    if (g->feed_r == g->feed_w)
        return false;
    *n = g->feed[g->feed_r];
    g->feed_r = (uint8_t)((g->feed_r + 1u) % SI_FEED);
    return true;
}

static void msg(si_groove *g, const char *m)
{
    cpy(g->msg, m, sizeof g->msg);
    g->msg_new = 1;
}

static void note_off_now(si_groove *g, int i)
{
    emit(g, 0, g->held[i].part, g->held[i].note, 0);
    g->held[i] = g->held[--g->nheld];
}

/* a note now (the key up added to a pitched part's), its note-off after len frames; a note of the part still
 * sounding at that pitch is cut first */
static void note(si_groove *g, int part, int n, int vel, float len_frames)
{
    if (part == SI_DRUMS) {                        /* a drum rings for its own length: then its voice is free again */
        static const float RING[SI_DR_COUNT] = {0.80f, 0.24f, 0.30f, 0.09f, 0.45f, 1.0f, 0.75f, 0.75f};
        int d = si_drum_of(n);
        len_frames = RING[d] * g->sr;
        if (d >= SI_DR_OH) {
            g->choir_clock = g->clock;
            g->choir_last = (uint8_t)d;
        }
    } else {
        n += g->tr;
    }
    for (int i = 0; i < g->nheld; ++i)
        if (g->held[i].part == part && g->held[i].note == n) {
            note_off_now(g, i);
            break;
        }
    if (g->nheld >= SI_HELD) {                     /* full: the oldest note goes */
        int o = 0;
        for (int i = 1; i < g->nheld; ++i)
            if ((int32_t)(g->held[i].off - g->held[o].off) < 0)
                o = i;
        note_off_now(g, o);
    }
    emit(g, 1, part, n, vel);
    feed(g, part, n, vel);
    if (g->step >= 0) {
        if (part == SI_DRUMS)
            g->drum_hits[si_drum_of(n)] |= (uint16_t)(1u << g->step);
        else
            g->hits[part] |= (uint16_t)(1u << g->step);
    }
    if (part == SI_HOOK)
        g->hook_on_clock = g->clock;
    g->held[g->nheld].note = (uint8_t)n;
    g->held[g->nheld].part = (uint8_t)part;
    g->held[g->nheld].off = g->clock + (uint32_t)(len_frames > 1.0f ? len_frames : 1.0f);
    g->nheld++;
}

void si_all_off(si_groove *g)
{
    while (g->nheld > 0)
        note_off_now(g, g->nheld - 1);
}

static void part_off(si_groove *g, int part)
{
    for (int i = g->nheld - 1; i >= 0; --i)
        if (g->held[i].part == part)
            note_off_now(g, i);
}

/* the pending notes: a swung or late sixteenth's notes, played when the clock reaches them */
#define SI_PEND 48
static struct { uint32_t at; uint8_t part, note, vel, pad; float len; } pend[SI_PEND];
static int npend;
static void schedule(si_groove *g, uint32_t delay, int part, int n, int vel, float len)
{
    if (delay == 0 || npend >= SI_PEND) {
        if (part == SI_DRUMS && (n & 0x80))       /* a choke: the open hat's note-off */
            emit(g, 0, SI_DRUMS, n & 0x7F, 0);
        else
            note(g, part, n, vel, len);
        return;
    }
    pend[npend].at = g->clock + delay;
    pend[npend].part = (uint8_t)part;
    pend[npend].note = (uint8_t)n;
    pend[npend].vel = (uint8_t)vel;
    pend[npend].len = len;
    npend++;
}

static float step_frames(const si_groove *g) { return g->sr * 60.0f / (float)clampi(g->s.bpm, 60, 200) / 4.0f; }

/* ---- the form ---------------------------------------------------------------------------------------------- */
int si_sec_len(const si_groove *g, int sec)
{
    switch (sec) {
    case SI_SEC_INTRO: return 8;
    case SI_SEC_VERSE: return g->s.verse >= 16 ? 16 : 8;
    case SI_SEC_PRE: return 4;
    case SI_SEC_BREAK: return 8;
    default: return g->s.chorus >= 16 ? 16 : 8;
    }
}
static int sec_next(int sec) { return sec == SI_SEC_CHORUS2 ? SI_SEC_VERSE : sec + 1; }
bool si_in_chorus(const si_groove *g)
{
    return !g->s.arc || g->sec == SI_SEC_CHORUS || g->sec == SI_SEC_CHORUS2;
}
/* bars into the section (ARC off: since PLAY), for the phrases and the choir */
static int sec_bar(const si_groove *g) { return g->s.arc ? g->sec_bar : (g->bar < 0 ? 0 : g->bar); }

/* the key up sounding: the arc's last chorus and SEQ's; a change cuts the held chords and hook (their old key) */
static void key_line(si_groove *g)
{
    if (g->perf_next >= 0) {
        g->perf_up = g->perf_next;
        g->perf_next = -1;
    }
    int tr = g->perf_up + (g->s.arc && g->sec == SI_SEC_CHORUS2 ? g->s.keyup : 0);
    if (tr != g->tr) {
        part_off(g, SI_STABS);
        part_off(g, SI_HOOK);
        part_off(g, SI_BASS);
        g->tr = tr;
        si_rebuild(g);                             /* (the chords' names: the key up in them) */
    }
}

/* a new bar: what waited for the bar line comes in, the form moves on */
static void bar_line(si_groove *g)
{
    for (int p = 0; p < SI_PARTS; ++p) {
        g->hits_last[p] = g->hits[p];
        g->hits[p] = 0;
    }
    for (int d = 0; d < SI_DR_COUNT; ++d) {
        g->drum_last[d] = g->drum_hits[d];
        g->drum_hits[d] = 0;
    }
    if (g->next_style >= 0) {
        int keep_song = g->next_song;
        si_settings old = g->s;
        si_style_apply(&g->s, g->next_style);
        for (int i = 0; i < 4; ++i)                /* MIX ARC MENU stay the player's */
            g->s.lvl[i] = old.lvl[i];
        g->s.arc = old.arc;
        g->s.verse = old.verse;
        g->s.chorus = old.chorus;
        g->s.keyup = old.keyup;
        g->s.palette = old.palette;
        g->s.look = old.look;
        if (keep_song >= 0) {
            g->s.song = (uint16_t)keep_song;
            si_song_sounds(&g->s);
        }
        g->next_style = -1;
        g->next_song = -1;
        g->perf_up = 0;
        g->perf_next = -1;
        g->sound_sig++;
        si_all_off(g);
        si_rebuild(g);
    } else if (g->next_song >= 0) {
        g->s.song = (uint16_t)g->next_song;
        uint32_t r = (uint32_t)g->s.song * 2246822519u + (uint32_t)g->s.style;
        g->s.chords = SI_STYLE[g->s.style].loops[rnd_n(&r, 4)];
        si_song_sounds(&g->s);                     /* each song its own sounds from the style's pool */
        g->sound_sig++;
        g->next_song = -1;
        g->perf_up = 0;
        g->perf_next = -1;
        si_rebuild(g);
    }
    if (g->brk && !g->brk_held && g->bar >= g->brk_end) {   /* the break over: the chorus, a crash */
        g->brk = false;
        g->crash_next = g->s.crash != SI_CRASH_OFF;
        if (g->s.arc) {
            g->sec = g->sec == SI_SEC_CHORUS2 ? SI_SEC_CHORUS2 : SI_SEC_CHORUS;
            g->sec_bar = -1;                       /* (bar 0 below) */
        }
    }
    if (g->s.arc) {
        if (++g->sec_bar >= si_sec_len(g, g->sec)) {
            int nx = sec_next(g->sec);
            if (g->sec == SI_SEC_CHORUS2) {        /* round again: a new hook */
                g->round++;
                g->hook_var++;
                si_rebuild(g);
            }
            if (g->s.crash != SI_CRASH_OFF && (nx == SI_SEC_VERSE || nx == SI_SEC_CHORUS || nx == SI_SEC_CHORUS2))
                g->crash_next = true;
            g->sec = nx;
            g->sec_bar = 0;
        }
    }
    if (g->s.crash == SI_CRASH_8 && si_in_chorus(g) && sec_bar(g) % 8 == 0 && sec_bar(g) > 0)
        g->crash_next = true;
    key_line(g);
}

/* is part p out (the form, a break, its level 0) */
static bool part_out(const si_groove *g, int p)
{
    if (g->s.lvl[p] == 0)
        return true;
    if (g->brk)                                    /* BREAK: the stabs; the drums for the roll in its last bar */
        return p == SI_STABS ? false : p == SI_DRUMS ? !(!g->brk_held && g->bar == g->brk_end - 1) : true;
    if (!g->s.arc)
        return false;
    switch (g->sec) {
    case SI_SEC_INTRO: {                           /* the parts come in one by one: drums, bass, the stabs */
        static const uint8_t IN_AT[SI_PARTS] = {4, 2, 99, 0};
        if (p == SI_HOOK && g->s.speed == SI_SPEED_PAD)   /* a pad comes in with the stabs */
            return g->sec_bar < 4;
        return g->sec_bar < IN_AT[p];
    }
    case SI_SEC_VERSE:                             /* the verse: room for the rap (a pad stays: it fills) */
    case SI_SEC_PRE: return p == SI_HOOK && g->s.speed != SI_SPEED_PAD;
    case SI_SEC_BREAK:                             /* the break: the stabs and the hook; the drums for the roll */
        return p == SI_BASS || (p == SI_DRUMS && g->sec_bar < 6);
    default: return false;
    }
}

/* the snare roll: quarters, then eighths, then sixteenths over span bars, rising; b the bar into it */
static int roll_vel(int i, int b, int span)
{
    int every = b < span / 2 ? 4 : b < span - span / 4 ? 2 : 1;
    if (span <= 2)
        every = b == 0 ? 2 : 1;
    if (i % every)
        return 0;
    return 55 + ((b * 16 + i) * 60) / (span * 16);
}

/* the choir: a chord sung on OH or AH (CHOIR) at the top of each bar of the chorus, with ENERGY high at the half bar
 * of every other bar too; in the verse with VOX ALL, softer, as each chord comes in. The chord's triad near the
 * stabs' voicing, folded into the choir's range, the key up in it, but for its root (eight voices are few: the bass and
 * the stabs have the root); MIX sings OH, then AH the bar after */
static void triad(const si_chord *c, int out[3]);
static const si_chord *chord_at(const si_groove *g, int b);
static void play_choir(si_groove *g, int i, uint32_t delay)
{
    if (g->s.vox == SI_VOX_OFF || g->brk)
        return;
    bool chorus = si_in_chorus(g), verse = g->s.arc && g->sec == SI_SEC_VERSE;
    if (!chorus && !(verse && g->s.vox == SI_VOX_ALL))
        return;
    int b = sec_bar(g), bpc = si_bars_per_chord(&g->s);
    float e = (float)g->s.energy / 100.0f;
    bool hit = chorus ? i == 0 || (i == 8 && e > 0.7f && b % 2 == 1) : i == 0 && g->bar % bpc == 0;
    if (!hit)
        return;
    int v = g->s.choir == SI_CHOIR_MIX ? b % 2 : g->s.choir == SI_CHOIR_AH, t[3];
    triad(chord_at(g, g->bar), t);
    float sf = step_frames(g);
    for (int k = 0; k < 3; ++k) {                  /* the third and the fifth: the root is the bass's and the stabs' */
        if (t[k] % 12 == chord_at(g, g->bar)->root)
            continue;
        int p = t[k] + g->tr;
        while (p > SI_CHOIR_LO + 23)
            p -= 12;
        while (p < SI_CHOIR_LO + 7)
            p += 12;
        schedule(g, delay + (uint32_t)((float)k * sf * 0.02f), SI_DRUMS, SI_CHOIR_NOTE(v, p),
                 chorus ? (i == 0 ? 100 : 88) : 80, 0);
    }
}

/* the drums of step i, with ENERGY, HATS, CLAP, the fills, the form */
static void play_drums(si_groove *g, int i, uint32_t delay, float hum)
{
    const si_style *st = &SI_STYLE[g->s.style];
    const drum_pat_t *dp = &DRUM_PATS[st->drums];
    float e = (float)g->s.energy / 100.0f;
    int vel[SI_DR_COUNT] = {0};
    int back_d = g->s.clap == SI_CLAP_CLAP ? SI_DR_CLAP : SI_DR_SNARE;
    bool roll_only = g->brk || (g->s.arc && g->sec == SI_SEC_BREAK);
    if (roll_only) {                               /* the break's last bars: the roll back in, nothing else */
        int span = g->brk ? 1 : 2, b = g->brk ? 0 : g->sec_bar - 6;
        vel[back_d] = roll_vel(i, b, span);
        if (g->s.clap == SI_CLAP_BOTH && vel[back_d] && i >= 8)
            vel[SI_DR_CLAP] = vel[back_d];
    } else {
        vel[SI_DR_KICK] = dp->v[0][i];
        vel[SI_DR_SNARE] = dp->v[1][i];
        vel[SI_DR_CLAP] = dp->v[2][i];
        int back = dp->v[1][i] > dp->v[2][i] ? dp->v[1][i] : dp->v[2][i];   /* the backbeat, as CLAP has it */
        if (back >= 90) {
            vel[SI_DR_SNARE] = g->s.clap == SI_CLAP_CLAP ? 0 : back;
            vel[SI_DR_CLAP] = g->s.clap == SI_CLAP_SNARE ? 0 : back;
        }
        int h = dp->v[3][i], o = dp->v[4][i];
        switch (g->s.hats) {
        case SI_HATS_8THS: h = (i % 2 == 0) ? (i % 4 == 0 ? 75 : 60) : 0; o = 0; break;
        case SI_HATS_16THS: h = i % 4 == 0 ? 80 : i % 2 == 0 ? 60 : 42; o = i == 14 && (g->drum_var & 1) ? 85 : 0; break;
        case SI_HATS_OFFBEAT: h = i % 4 == 2 ? 0 : (i % 2 ? 40 : 60); o = i % 4 == 2 ? 95 : 0; break;
        default: h = i % 2 ? 45 : 0; o = i % 2 == 0 ? (i % 4 == 2 ? 100 : 70) : 0; break;
        }
        if (e < 0.35f && (i % 2))                  /* low ENERGY: no sixteenth hats */
            h = 0;
        if (e < 0.2f)
            o = 0;
        if (g->s.arc && g->sec == SI_SEC_VERSE) {  /* the verse a little lighter */
            h = h * 85 / 100;
            o = o * 80 / 100;
        }
        if (e > 0.8f && st->drums != DRUMS_BALLAD && st->drums != DRUMS_SKANK && i == 15 && (g->drum_var & 2))
            vel[SI_DR_KICK] = vel[SI_DR_KICK] ? vel[SI_DR_KICK] : 80;   /* high: a pickup kick */
        vel[SI_DR_HAT] = h;
        vel[SI_DR_OPEN] = o;
        static const uint8_t EVERY[SI_FILLS_COUNT] = {0, 8, 4, 2};
        int ev = EVERY[g->s.fills], b = sec_bar(g);
        bool pre = g->s.arc && g->sec == SI_SEC_PRE;
        int fill_kind = (int)(((uint32_t)b * 2654435761u + (uint32_t)g->s.song * 40503u) >> 13) % 3;
        if (!pre && ev && (b % ev) == ev - 1 && i >= 12 && fill_kind > 0) {
            /* a fill as a tuplet over the last beat: a snare triplet (3 in the space of 4 sixteenths) or a sextuplet
             * roll (6 in 4), rising */
            if (i == 12) {
                int tn = fill_kind == 1 ? 3 : 6;
                vel[back_d] = 0;                   /* (the tuplet's first hit is the beat's)*/
                float each = step_frames(g) * 4.0f / (float)tn;
                for (int k = 0; k < tn; ++k)
                    schedule(g, delay + (uint32_t)(each * (float)k), SI_DRUMS, back_d, clampi(72 + k * 40 / tn, 1, 127), 0);
            }
            vel[SI_DR_OPEN] = i == 15 ? 100 : 0;
            if (i >= 13)
                vel[SI_DR_HAT] = 0;
        } else if (!pre && ev && (b % ev) == ev - 1 && i >= 12) {   /* a fill: the backbeat drum on the sixteenths, rising */
            if ((g->drum_var & 2) ? (i % 2 == 0 || i >= 14) : (i == 13 || i == 14 || i == 15))
                vel[back_d] = 70 + (i - 12) * 12;
            vel[SI_DR_OPEN] = i == 15 ? 100 : 0;
            if (i == 15)
                vel[SI_DR_HAT] = 0;
        }
        if (pre) {                                 /* the pre-chorus: the roll builds; the last beat, the roll alone */
            int span = si_sec_len(g, SI_SEC_PRE), r = roll_vel(i, g->sec_bar, span);
            if (r)
                vel[back_d] = r > vel[back_d] ? r : vel[back_d];
            if (g->sec_bar == span - 1 && i >= 12)
                vel[SI_DR_KICK] = vel[SI_DR_HAT] = vel[SI_DR_OPEN] = 0;
        }
        if (g->s.arc && g->sec == SI_SEC_INTRO && g->sec_bar < 2)   /* the first bars: the kick alone */
            vel[SI_DR_SNARE] = vel[SI_DR_CLAP] = vel[SI_DR_HAT] = vel[SI_DR_OPEN] = 0;
    }
    if (i == 0 && g->crash_next) {
        vel[SI_DR_CRASH] = 110;
        g->crash_next = false;
    }
    for (int d = 0; d <= SI_DR_CRASH; ++d) {
        if (!vel[d])
            continue;
        int v = vel[d];
        if (d != SI_DR_KICK && d != SI_DR_CRASH)
            v = (int)((float)v * (0.75f + 0.25f * e));
        v += (int)(hum * 10.0f * (float)g->s.human / 100.0f);
        if (d == SI_DR_KICK)
            g->beat_ph = 0;                        /* (the pump follows the kick: si_process) */
        if (d == SI_DR_HAT && g->s.hats != SI_HATS_OPEN)   /* a closed hat chokes the open one */
            schedule(g, delay, SI_DRUMS, 0x80 | SI_DR_OPEN, 1, 0);
        schedule(g, delay, SI_DRUMS, d, clampi(v, 1, 127), 0);
    }
    play_choir(g, i, delay);
}

/* the chord sounding at bar b */
static const si_chord *chord_at(const si_groove *g, int b)
{
    int bpc = si_bars_per_chord(&g->s);
    return &g->loop[(b / bpc) % (g->nloop > 0 ? g->nloop : 1)];
}
static const si_chord *next_chord(const si_groove *g, int b)
{
    int bpc = si_bars_per_chord(&g->s);
    return &g->loop[((b / bpc) + 1) % (g->nloop > 0 ? g->nloop : 1)];
}
int si_chord_now(const si_groove *g)
{
    int bpc = si_bars_per_chord(&g->s);
    return g->nloop > 0 ? ((g->bar < 0 ? 0 : g->bar) / bpc) % g->nloop : 0;
}

static int bass_note(const si_groove *g, int tone, const si_chord *c, const si_chord *nx)
{
    int root = 36 + ((c->root - 36) % 12 + 12) % 12;   /* C2 .. B2 */
    if (root > 43)
        root -= 12;                                     /* (G#1 .. G2: the bass sits low) */
    switch (tone) {
    case BR_OCT: return root + 12;
    case BR_FIFTH: return root + ((c->tone[2] - c->root + 12) % 12);
    case BR_SEV: return root + ((c->tone[3] - c->root + 12) % 12);
    case BR_APP: {
        int nr = 36 + ((nx->root - 36) % 12 + 12) % 12;
        if (nr > 43) nr -= 12;
        return nx == c ? root + 12 : nr - 1;            /* a semitone below the next root */
    }
    case BR_PASS: return si_snap(&g->s, root + 2);
    default: return root;
    }
}

/* the chord held: a pad, or the stabs held through the pre-chorus and the break */
static void triad(const si_chord *c, int out[3]);
static void stabs_held(si_groove *g, int i, uint32_t delay, float sf, int vel, bool roll)
{
    const si_chord *c = chord_at(g, g->bar);
    int bpc = si_bars_per_chord(&g->s);
    if (i != 0 || (!roll && (g->bar % bpc) != 0))  /* (roll: a bar each; else as long as the chord lasts) */
        return;
    int len = roll ? 16 : 16 * bpc;
    if (g->s.snd[SI_STABS] == SI_STABS_PIANO) {    /* the piano holds its triad */
        int t[3];
        triad(c, t);
        for (int k = 0; k < 3; ++k)
            schedule(g, delay + (uint32_t)((float)k * sf * 0.04f), SI_STABS, t[k], vel, sf * (float)len - 40.0f);
        return;
    }
    for (int k = 0; k < c->n; ++k)
        schedule(g, delay + (uint32_t)((float)k * sf * 0.04f), SI_STABS, c->note[k], vel, sf * (float)len - 40.0f);
}

/* the chord's triad (root, third, fifth) as three close notes, the lowest the first of them at or above the
 * voicing's lowest note: an inversion near where the stabs' voice leading put the chord */
static void triad(const si_chord *c, int out[3])
{
    int lo = c->note[0], best = -1;
    for (int k = 0; k < 3; ++k) {                  /* the tone right at or above the voicing's bottom starts it */
        int n = lo;
        while ((n % 12) != c->tone[k])
            ++n;
        if (best < 0 || n < out[0]) {
            best = k;
            out[0] = n;
        }
    }
    for (int j = 1; j < 3; ++j) {
        int n = out[j - 1] + 1;
        while ((n % 12) != c->tone[(best + j) % 3])
            ++n;
        out[j] = n;
    }
}

static void play_stabs(si_groove *g, int i, uint32_t delay, float sf, float hum)
{
    const si_style *st = &SI_STYLE[g->s.style];
    const si_chord *c = chord_at(g, g->bar);
    float e = (float)g->s.energy / 100.0f;
    bool pre = g->s.arc && g->sec == SI_SEC_PRE, brk = g->brk || (g->s.arc && g->sec == SI_SEC_BREAK);
    /* PIANO on the STABS card: the offbeat piano chords, in any style */
    int fam = g->s.snd[SI_STABS] == SI_STABS_PIANO ? STAB_PIANO : st->stab;
    bool piano = fam == STAB_PIANO;
    if (fam == STAB_PAD || brk) {             /* a pad, the break: the chord held (REC's break: a bar each) */
        stabs_held(g, i, delay, sf, clampi(80 + (int)(hum * 8.0f), 1, 127), g->brk);
        return;
    }
    if (pre) {                                     /* the pre-chorus: held a bar each, rising */
        stabs_held(g, i, delay, sf, clampi(76 + g->sec_bar * 10 + (int)(hum * 6.0f), 1, 127), true);
        return;
    }
    bool verse = g->s.arc && g->sec == SI_SEC_VERSE;
    if (verse && (g->bar % 2) && !piano)           /* the verse: every other bar, room for the rap (the piano plays on) */
        return;
    int len = STAB_PATS[fam][g->stab_pat][i];
    if (!len) {
        if (e > 0.85f && fam == STAB_EURO && i == 15 && (g->stab_pat & 1))   /* high energy: a pickup */
            len = 1;
        else
            return;
    }
    int vel = fam == STAB_SKANK ? (i % 4 == 2 ? 108 : 70) : piano ? (i % 4 == 2 ? 112 : 96) - (verse ? 10 : 0)
            : i % 4 == 0 ? 108 : 98;
    vel += (int)(hum * 10.0f);
    float gate = fam == STAB_SKANK ? 0.35f : fam == STAB_RAVE ? 0.7f : piano ? 0.6f : 0.85f;
    if (piano) {                                   /* the piano: the plain triad, root third fifth (a seventh's chord
                                                    * too), close round the voicing's middle */
        int t[3];
        triad(c, t);
        for (int k = 0; k < 3; ++k)
            schedule(g, delay, SI_STABS, t[k], clampi(vel - (k == 2 ? 0 : 6), 1, 127), sf * (float)len * gate);
        return;
    }
    /* a stab sounds the voicing's top three: the shared voice budget (eight for all four parts) stays for the
     * drums' tails, the bass and the hook */
    int k0 = c->n > 3 ? 1 : 0;
    for (int k = k0; k < c->n; ++k)
        schedule(g, delay, SI_STABS, c->note[k], clampi(vel - (k == c->n - 1 ? 0 : 6), 1, 127), sf * (float)len * gate);
}

static void play_bass(si_groove *g, int i, uint32_t delay, float sf, float hum)
{
    const bass_pat_t *bp = &BASS_PATS[clampi(g->s.bassline, 0, SI_BL_COUNT - 1)][g->bass_pat];
    int tone = bp->tone[i], len = bp->len[i];
    if (!tone)
        return;
    const si_chord *c = chord_at(g, g->bar);
    int bpc = si_bars_per_chord(&g->s);
    bool last_bar = (g->bar % bpc) == bpc - 1;
    const si_chord *nx = last_bar ? next_chord(g, g->bar) : c;
    if (tone == BR_APP && !last_bar)               /* an approach only leads into a new chord */
        tone = BR_OCT;
    int n = bass_note(g, tone, c, nx);
    int vel = (i % 4 == 2 ? 112 : i % 4 == 0 ? 106 : 94) + (int)(hum * 8.0f);
    float gate = g->s.bassline == SI_BL_ROLLING || g->s.bassline == SI_BL_BOUNCE ? 0.6f
               : g->s.bassline == SI_BL_OFFBEAT ? 0.75f : 0.9f;
    schedule(g, delay, SI_BASS, n, clampi(vel, 1, 127), sf * (float)len * gate);
}

static void play_hook(si_groove *g, int i, uint32_t delay, float sf, float hum)
{
    const si_style *st = &SI_STYLE[g->s.style];
    if (g->keys)                                   /* the player has the hook */
        return;
    bool brk = g->brk || (g->s.arc && g->sec == SI_SEC_BREAK);
    if (brk && (g->brk || g->sec_bar >= 6))        /* (the break's roll: the hook rests) */
        return;
    const si_chord *c = chord_at(g, g->bar);
    float gate = (float)g->s.gate / 100.0f;
    if (g->s.speed == SI_SPEED_PAD) {              /* a pad: two of the chord's notes an octave over the stabs' middle,
                                                    * held as long as the chord lasts, swelling in */
        int bpc = si_bars_per_chord(&g->s);
        if (i != 0 || (g->bar % bpc) != 0)
            return;
        for (int k = 1; k <= 2; ++k) {
            int n = c->note[k] + 12;
            if (n > 84)
                n -= 12;
            schedule(g, delay + (uint32_t)((float)k * sf * 0.05f), SI_HOOK, n, clampi(78 + (int)(hum * 6.0f), 1, 127),
                     sf * (float)(16 * bpc) - 60.0f);
        }
        return;
    }
    if (st->hook == HOOK_ARP) {                    /* an arpeggiator through the chord, up and down */
        int every = g->s.speed == SI_SPEED_SLOW ? 2 : 1;
        if (i % every)
            return;
        static const uint8_t ORD[8] = {0, 1, 2, 3, 4, 3, 2, 1};
        int k = ORD[(i / every) % 8], oct = k / 4, t = k % 4;
        if (g->s.range < 2)
            oct = 0;
        int n = 60 + ((c->tone[t] - 60) % 12 + 12) % 12 + 12 * oct;
        if (n > 84)
            n -= 12;
        schedule(g, delay, SI_HOOK, n, clampi(82 + (i % 4 == 0 ? 20 : 0) + (int)(hum * 6.0f), 1, 127),
                 sf * (float)every * gate);
        return;
    }
    int rep = si_repeat_bars(&g->s);
    int hp = (sec_bar(g) % rep) * 16 + i;
    if (g->hook[hp] == -128)
        return;
    int len = g->hook_len[hp];
    int base = 60 + g->s.key - (g->s.key > 6 ? 12 : 0);
    if (g->hook_tup[hp]) {                         /* a tuplet: its notes evenly over len sixteenths, an arpeggio of
                                                    * the chord playing (up, or down from a high start), from the
                                                    * chord tone nearest the note the walk reached */
        int tn = g->hook_tup[hp], dir = g->hook[hp] >= 5 ? -1 : 1;
        float each = sf * (float)len / (float)tn;
        int nn = step_note(g->s.scale, base, g->hook[hp]);
        for (int d = 0; d < 12; ++d) {             /* the nearest chord tone, below first */
            int pc = ((nn - d) % 12 + 12) % 12, pu = ((nn + d) % 12 + 12) % 12;
            if (pc == c->tone[0] || pc == c->tone[1] || pc == c->tone[2]) { nn -= d; break; }
            if (pu == c->tone[0] || pu == c->tone[1] || pu == c->tone[2]) { nn += d; break; }
        }
        for (int k = 0; k < tn; ++k) {
            if (k > 0 && (dir > 0 ? nn >= 81 : nn <= 57))
                dir = -dir;                        /* (it turns back at the hook's edges) */
            if (k > 0)
                do                                 /* the next chord tone up (or down) */
                    nn += dir;
                while (nn % 12 != c->tone[0] && nn % 12 != c->tone[1] && nn % 12 != c->tone[2]);
            int v = 96 + (k == 0 ? 14 : 0) + (int)(hum * 6.0f) - (brk ? 18 : 0);
            schedule(g, delay + (uint32_t)(each * (float)k), SI_HOOK, nn, clampi(v, 1, 127), each * 0.9f);
        }
        return;
    }
    int n = step_note(g->s.scale, base, g->hook[hp]);
    if (len >= 2 || i % 4 == 0) {                  /* the long and strong notes onto the chord */
        int best = n, bd = 99;
        for (int t = 0; t < 3; ++t)
            for (int o = -1; o <= 1; ++o) {
                int cand = n - ((n % 12) - c->tone[t]) + o * 12;
                int d = cand - n < 0 ? n - cand : cand - n;
                if (d < bd) {
                    bd = d;
                    best = cand;
                }
            }
        n = best;
    }
    int vel = 100 + (i % 4 == 0 ? 10 : 0) + (int)(hum * 6.0f) - (brk ? 18 : 0);
    schedule(g, delay, SI_HOOK, n, clampi(vel, 1, 127), sf * (float)len * gate);
}

/* one sixteenth: the parts, swung and humanized */
static void do_step(si_groove *g)
{
    if (++g->step >= 16) {
        g->step = 0;
        g->bar++;
        bar_line(g);
    }
    int i = g->step;
    if (i == 0)
        g->rnd = (uint32_t)g->bar * 2654435761u + (uint32_t)g->s.song * 97u + 1u;
    float sf = step_frames(g);
    uint32_t delay = 0;
    if (i % 2 && g->s.swing)                        /* swing: the off sixteenths late */
        delay = (uint32_t)(sf * (float)g->s.swing / 100.0f * 0.66f);
    for (int p = 0; p < SI_PARTS; ++p) {
        bool out = part_out(g, p);
        if (out != g->muted[p] && out)
            part_off(g, p);
        g->muted[p] = out;
    }
    for (int p = 0; p < SI_PARTS; ++p) {
        if (g->muted[p])
            continue;
        float hum = (rnd_f(&g->rnd) - 0.5f) * 2.0f * (float)g->s.human / 100.0f;
        uint32_t d = delay + (uint32_t)((hum > 0 ? hum : 0.0f) * g->sr * 0.010f);   /* late only, up to 10 ms */
        switch (p) {
        case SI_STABS: play_stabs(g, i, d, sf, hum); break;
        case SI_BASS: play_bass(g, i, d, sf, hum); break;
        case SI_HOOK: play_hook(g, i, d, sf, hum); break;
        default: play_drums(g, i, d, hum); break;
        }
    }
}

void si_play(si_groove *g, bool on)
{
    if (on == g->playing)
        return;
    si_all_off(g);
    npend = 0;
    g->playing = on;
    g->clock = 0;
    g->wait = 0;
    g->step = -1;
    g->bar = 0;
    g->brk = false;
    g->brk_held = false;
    g->crash_next = false;
    g->sec = g->s.arc ? SI_SEC_INTRO : SI_SEC_CHORUS;
    g->sec_bar = 0;
    g->round = 0;
    g->hook_var = 0;
    g->perf_up = 0;
    g->perf_next = -1;
    g->tr = 0;
    g->beat_ph = 1.0f;
    if (on) {
        if (g->next_style >= 0 || g->next_song >= 0) {   /* (queued while stopped: at once) */
            g->bar = -1;
            g->step = 15;
            bar_line(g);
            g->bar = 0;
            g->step = -1;
            g->sec = g->s.arc ? SI_SEC_INTRO : SI_SEC_CHORUS;
            g->sec_bar = 0;
        }
    }
    si_rebuild(g);
}

void si_queue_style(si_groove *g, int style)
{
    style = ((style % SI_STYLES) + SI_STYLES) % SI_STYLES;
    g->next_style = style;
    if (!g->playing) {                             /* stopped: at once */
        int b = g->bar, sb = g->sec_bar, sc = g->sec;
        g->bar = -1;
        bar_line(g);
        g->bar = b;
        g->sec = sc;
        g->sec_bar = sb;
    } else {
        char m[24] = "NEXT ";
        cpy(m + 5, SI_STYLE[style].name, (int)sizeof m - 5);
        msg(g, m);
    }
}

void si_queue_song(si_groove *g, int song)
{
    g->next_song = ((song % 1000) + 1000) % 1000;
    if (!g->playing) {
        int b = g->bar, sb = g->sec_bar, sc = g->sec;
        g->bar = -1;
        bar_line(g);
        g->bar = b;
        g->sec = sc;
        g->sec_bar = sb;
    }
}

void si_break(si_groove *g, bool down)
{
    if (!g->playing)
        return;
    if (down) {
        g->brk = true;
        g->brk_held = true;
        g->brk_end = g->bar + 4;                   /* a tap: three bars of stabs, the roll, the chorus */
        for (int p = 0; p < SI_PARTS; ++p) {
            part_off(g, p);                        /* (the stabs come back held, from the next bar) */
            if (p != SI_STABS)
                g->muted[p] = true;                /* (at once: their late notes of this sixteenth too) */
        }
        msg(g, "BREAK");
    } else {
        g->brk_held = false;
        if (g->brk && g->bar >= g->brk_end - 1)    /* held past its three bars: the roll next bar, then back */
            g->brk_end = g->bar + 2;
    }
}

void si_keyup(si_groove *g)
{
    if (!g->playing)
        return;
    int cur = g->perf_next >= 0 ? g->perf_next : g->perf_up;
    int nx = cur >= SI_KEYUP_MAX ? 0 : cur + 1;
    g->perf_next = nx;
    if (nx == 0) {
        msg(g, "KEY HOME");
    } else {
        char m[24] = "KEY UP +";
        m[8] = (char)('0' + nx);
        m[9] = 0;
        msg(g, m);
    }
}

void si_key(si_groove *g, int k, int note, bool down)
{
    if (k < 0 || k >= 27)
        return;
    if (down) {
        int n = si_snap(&g->s, note) + g->tr;      /* in the key, the key up in it */
        if (g->key_note[k] >= 0)
            emit(g, 0, SI_HOOK, g->key_note[k], 0);
        if (!g->keys)                              /* the hook makes way */
            part_off(g, SI_HOOK);
        g->keys |= 1u << k;
        g->key_note[k] = n;
        emit(g, 1, SI_HOOK, n, 104);
        feed(g, SI_HOOK, n, 104);
        g->hook_on_clock = g->clock;
    } else {
        g->keys &= ~(1u << k);
        if (g->key_note[k] >= 0)
            emit(g, 0, SI_HOOK, g->key_note[k], 0);
        g->key_note[k] = -1;
    }
}

/* the lift: how open the lifted parts' filter is in the form, 0..1 of the LIFT ceiling */
static float lift_open(const si_groove *g)
{
    if (!g->playing)
        return 1.0f;
    float pos = (float)(g->step < 0 ? 0 : g->step) / 16.0f;
    if (g->brk)
        return 0.6f;
    if (!g->s.arc)
        return 1.0f;
    switch (g->sec) {
    case SI_SEC_INTRO: return 0.3f + 0.7f * ((float)g->sec_bar + pos) / 8.0f;
    case SI_SEC_VERSE: return 0.85f;
    case SI_SEC_PRE: return 0.55f + 0.45f * ((float)g->sec_bar + pos) / (float)si_sec_len(g, SI_SEC_PRE);
    case SI_SEC_BREAK: return g->sec_bar < 6 ? 0.6f : 0.6f + 0.4f * ((float)(g->sec_bar - 6) + pos) / 2.0f;
    default: return 1.0f;
    }
}

/* the block: notes in time, then the lift and the pump for the glue */
void si_process(si_groove *g, int frames)
{
    if (g->playing) {
        float sf = step_frames(g);
        int left = frames;
        while (left > 0) {
            if (g->wait <= 0.0f) {
                do_step(g);
                g->wait += sf;
            }
            int chunk = (int)g->wait + 1;
            if (chunk > left)
                chunk = left;
            if (chunk < 1)
                chunk = 1;
            g->clock += (uint32_t)chunk;
            g->wait -= (float)chunk;
            g->beat_ph += (float)chunk / (sf * 4.0f);   /* beats since the last kick */
            left -= chunk;
            for (int k = 0; k < npend;) {          /* the swung and late notes */
                if ((int32_t)(g->clock - pend[k].at) >= 0) {
                    int nn = pend[k].note;
                    if (pend[k].part == SI_DRUMS && (nn & 0x80))
                        emit(g, 0, SI_DRUMS, nn & 0x7F, 0);   /* a choke */
                    else if (!g->muted[pend[k].part])
                        note(g, pend[k].part, nn, pend[k].vel, pend[k].len);
                    pend[k] = pend[--npend];
                } else {
                    ++k;
                }
            }
            for (int k = 0; k < g->nheld;) {       /* the note-offs */
                if ((int32_t)(g->clock - g->held[k].off) >= 0)
                    note_off_now(g, k);
                else
                    ++k;
            }
        }
    }
    float cut = clampf((float)g->s.lift / 100.0f * clampf(lift_open(g), 0.0f, 1.0f), 0.0f, 1.0f);
    g->lift_now = cut;
    uint8_t mask = SI_STYLE[g->s.style].lift_parts;
    for (int p = 0; p < SI_PARTS; ++p)
        g->cut[p] = (uint8_t)((mask >> p) & 1u ? (int)(cut * 127.0f + 0.5f) : 127);
    /* the pump: a fast duck on the kick (6 ms down), back over 70 % of the beat */
    float depth = g->playing && !g->muted[SI_DRUMS] ? (float)g->s.pump / 100.0f * 0.8f : 0.0f;
    float beat_s = 60.0f / (float)clampi(g->s.bpm, 60, 200);
    float t = g->beat_ph * beat_s;                 /* seconds since the kick (beat_ph counts beats) */
    float d;
    if (t < 0.006f)
        d = depth * t / 0.006f;
    else {
        float u = clampf((t - 0.006f) / (0.7f * beat_s), 0.0f, 1.0f);
        d = depth * (1.0f - u) * (1.0f - u);
    }
    for (int p = 0; p < SI_PARTS; ++p)
        g->duck[p] = (uint8_t)(p == SI_DRUMS ? 0 : (int)(d * 127.0f + 0.5f));
}
