/* SPDX-License-Identifier: GPL-3.0-only */
/* SILL's groove engine on its own (firmware/src/si_groove.c): every style plays, in time, in key, every note
 * ends; the form goes intro, verse, pre, chorus, break, chorus (the key up) and round again; a song replays exactly;
 * a style and a song come in on the bar line; BREAK, KEY UP and the keys do what the manual says; the crowd shouts
 * where VOX says; the settings survive a bad record.
 *   cc -std=c11 -O2 -Ifirmware/src -o build/host/si_groove_test tests/si_groove_test.c firmware/src/si_groove.c -lm */
#include <stdio.h>
#include <string.h>
#include "si_groove.h"

static int fails, checks;
#define CHECK(cond, ...)                                                   \
    do {                                                                   \
        checks++;                                                          \
        if (!(cond)) {                                                     \
            fails++;                                                       \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                    \
            printf(__VA_ARGS__);                                           \
            printf("\n");                                                  \
        }                                                                  \
    } while (0)

#define SR 44100.0f
static si_groove g;
static int on[SI_PARTS], off[SI_PARTS], sounding[SI_PARTS][128], bad_pc, hook_in_verse, chants, chant_kind[SI_DR_COUNT];
static int drum_on[SI_DR_COUNT];
static uint32_t hash;
static void sound(void *ctx, const si_event *e)
{
    (void)ctx;
    hash = (hash ^ (uint32_t)(e->on << 24 | e->part << 16 | e->note << 8 | e->vel)) * 16777619u + g.clock;
    if (e->on) {
        on[e->part]++;
        sounding[e->part][e->note]++;
        if (e->part != SI_DRUMS && e->part != SI_HOOK && si_snap(&g.s, e->note - g.tr) != e->note - g.tr)
            bad_pc++;                              /* (in the scale but for the dominant's third and the approach) */
        if (e->part == SI_HOOK && g.s.arc && g.sec == SI_SEC_VERSE && !g.keys)
            hook_in_verse++;
        if (e->part == SI_DRUMS) {
            drum_on[e->note % SI_DR_COUNT]++;
            if (e->note >= SI_DR_HEY) {
                chants++;
                chant_kind[e->note]++;
            }
        }
    } else {
        off[e->part]++;
        if (sounding[e->part][e->note] > 0) sounding[e->part][e->note]--;
    }
}
static void reset_counts(void)
{
    memset(on, 0, sizeof on);
    memset(off, 0, sizeof off);
    memset(sounding, 0, sizeof sounding);
    memset(drum_on, 0, sizeof drum_on);
    memset(chant_kind, 0, sizeof chant_kind);
    chants = hook_in_verse = bad_pc = 0;
}
static int still_sounding(int p) { int n = 0; for (int k = 0; k < 128; ++k) n += sounding[p][k]; return n; }
static void run_bars(int n)
{
    int b = g.bar + n;
    while (g.bar < b) si_process(&g, 32);
}
static void run_to_sec(int sec)
{
    for (int k = 0; k < 200 && g.sec != sec; ++k) run_bars(1);
}

int main(void)
{
    si_init(&g, SR);
    si_set_sound(&g, sound, 0);
    /* every style: a whole round of the form and more, its parts play, nothing hangs, in key */
    for (int st = 0; st < SI_STYLES; ++st) {
        si_style_apply(&g.s, st);
        si_rebuild(&g);
        reset_counts();
        si_play(&g, true);
        run_bars(64);
        si_play(&g, false);
        for (int p = 0; p < SI_PARTS; ++p) {
            CHECK(on[p] > 0, "%s: part %d plays (%d)", si_style_name(st), p, on[p]);
            if (p != SI_DRUMS)
                CHECK(!still_sounding(p), "%s: part %d left %d notes sounding", si_style_name(st), p, still_sounding(p));
        }
        CHECK(bad_pc < on[SI_STABS] / 8 + on[SI_BASS] / 8 + 8, "%s: %d stab and bass notes outside the scale",
              si_style_name(st), bad_pc);
        CHECK(hook_in_verse == 0, "%s: the hook rests in the verse (%d notes)", si_style_name(st), hook_in_verse);
        CHECK(g.loop[0].name[0], "%s: its first chord has a name (%s)", si_style_name(st), g.loop[0].name);
        if (g.s.vox != SI_VOX_OFF)
            CHECK(chants > 0, "%s: the crowd shouts (VOX on)", si_style_name(st));
        else
            CHECK(chants == 0, "%s: no shouts with VOX OFF (%d)", si_style_name(st), chants);
    }
    /* the form: INTRO 8, VERSE, PRE 4, CHORUS, BREAK 8, CHORUS a key up, round again from the verse */
    si_style_apply(&g.s, SI_ST_EUROSTOMP);
    g.s.arc = 1;
    g.s.verse = 8;
    g.s.chorus = 8;
    g.s.keyup = 1;
    si_rebuild(&g);
    si_play(&g, true);
    CHECK(g.sec == SI_SEC_INTRO, "PLAY starts with the intro");
    static const int ORDER[] = {SI_SEC_VERSE, SI_SEC_PRE, SI_SEC_CHORUS, SI_SEC_BREAK, SI_SEC_CHORUS2, SI_SEC_VERSE};
    static const int AT[] = {8, 16, 20, 28, 36, 44};
    for (int k = 0; k < 6; ++k) {
        while (g.bar < AT[k]) si_process(&g, 32);
        CHECK(g.sec == ORDER[k], "bar %d: %s (got %s)", AT[k], si_sec_name(ORDER[k]), si_sec_name(g.sec));
        if (ORDER[k] == SI_SEC_CHORUS2)
            CHECK(g.tr == 1, "the last chorus a semitone up (%d)", g.tr);
        if (k == 5)
            CHECK(g.tr == 0 && g.round == 1, "round again: home key, round 1 (%d %d)", g.tr, g.round);
    }
    /* in the break the bass rests; its last two bars a roll */
    run_to_sec(SI_SEC_BREAK);
    reset_counts();
    run_bars(5);
    CHECK(on[SI_BASS] == 0 && on[SI_DRUMS] == 0 && on[SI_STABS] > 0, "the break: stabs, no bass, no drums (%d %d %d)",
          on[SI_STABS], on[SI_BASS], on[SI_DRUMS]);
    reset_counts();
    run_bars(3);
    CHECK(drum_on[SI_DR_CLAP] + drum_on[SI_DR_SNARE] > 8 && drum_on[SI_DR_KICK] <= 1,
          "the break's roll: snare or clap, no kick but the chorus's first (%d %d)", drum_on[SI_DR_CLAP] + drum_on[SI_DR_SNARE],
          drum_on[SI_DR_KICK]);
    si_play(&g, false);
    /* the same song twice: the same notes at the same times */
    uint32_t h1, h2;
    si_style_apply(&g.s, SI_ST_PARTY);
    g.s.song = 17;
    si_rebuild(&g);
    hash = 2166136261u; si_play(&g, true); run_bars(24); si_play(&g, false); h1 = hash;
    hash = 2166136261u; si_play(&g, true); run_bars(24); si_play(&g, false); h2 = hash;
    CHECK(h1 == h2, "a song replays exactly");
    g.s.song = 18;
    si_rebuild(&g);
    hash = 2166136261u; si_play(&g, true); run_bars(24); si_play(&g, false);
    CHECK(hash != h1, "another song differs");
    /* the tempo: 16 bars at 120 BPM are 32 s */
    si_style_apply(&g.s, SI_ST_EUROSTOMP);
    g.s.bpm = 120;
    g.s.arc = 0;
    si_rebuild(&g);
    si_play(&g, true);
    run_bars(16);
    float secs = (float)g.clock / SR;
    CHECK(secs > 31.9f && secs < 32.1f, "16 bars at 120 BPM: %.3f s", (double)secs);
    CHECK(si_in_chorus(&g), "ARC off: the chorus on and on");
    /* a style queued: on the next bar line, not before */
    int bar = g.bar;
    while (g.step != 5) si_process(&g, 32);
    si_queue_style(&g, SI_ST_RAVE);
    CHECK(g.s.style == SI_ST_EUROSTOMP, "queued: still EUROSTOMP mid-bar");
    while (g.bar == bar) si_process(&g, 32);
    CHECK(g.s.style == SI_ST_RAVE && g.step == 0, "RAVE from the bar line (step %d)", g.step);
    /* a song queued: the same */
    int song = g.s.song;
    si_queue_song(&g, song + 3);
    CHECK(g.s.song == song, "the song waits for the bar");
    run_bars(1);
    CHECK(g.s.song == song + 3, "the next song from the bar line");
    /* KEY UP: from the next bar, +1 +2 +3, then home */
    while (g.step != 3) si_process(&g, 32);
    si_keyup(&g);
    CHECK(g.tr == 0, "KEY UP waits for the bar");
    run_bars(1);
    CHECK(g.tr == 1, "KEY UP: +1 from the bar (%d)", g.tr);
    reset_counts();
    run_bars(2);
    CHECK(bad_pc < on[SI_STABS] / 8 + on[SI_BASS] / 8 + 4, "a key up: the stabs and bass in the new key (%d out)", bad_pc);
    si_keyup(&g); run_bars(1);
    si_keyup(&g); run_bars(1);
    CHECK(g.tr == 3, "+3 (%d)", g.tr);
    si_keyup(&g); run_bars(1);
    CHECK(g.tr == 0, "after +3: home (%d)", g.tr);
    /* BREAK: three bars of stabs, the roll in the fourth, back on the bar */
    while (g.step != 0) si_process(&g, 32);
    reset_counts();
    si_break(&g, true);
    si_break(&g, false);
    int brk_bar = g.bar;
    run_bars(1);
    CHECK(g.brk && on[SI_BASS] == 0 && on[SI_HOOK] == 0 && on[SI_DRUMS] == 0, "BREAK: no bass, hook, drums (%d %d %d)",
          on[SI_BASS], on[SI_HOOK], on[SI_DRUMS]);
    run_bars(2);
    CHECK(on[SI_STABS] > 0, "the stabs play on (%d)", on[SI_STABS]);
    reset_counts();
    while (g.bar < brk_bar + 3) si_process(&g, 32);
    run_bars(1);
    CHECK(drum_on[SI_DR_CLAP] + drum_on[SI_DR_SNARE] > 6, "the roll in its last bar (%d)",
          drum_on[SI_DR_CLAP] + drum_on[SI_DR_SNARE]);
    CHECK(!g.brk && g.bar == brk_bar + 4, "back on the bar line four bars on (%d)", g.bar - brk_bar);
    /* held: stays until let go, then the roll, then back */
    si_break(&g, true);
    run_bars(5);
    CHECK(g.brk, "held: still in the break after five bars");
    si_break(&g, false);
    run_bars(3);
    CHECK(!g.brk, "let go: the roll, then back");
    /* the keys: snapped to the scale (the key up in it), the hook makes way */
    reset_counts();
    si_key(&g, 1, 54, true);
    CHECK(on[SI_HOOK] == 1, "a key plays the hook");
    si_key(&g, 1, 54, false);
    CHECK(off[SI_HOOK] >= 1, "its release lets go");
    si_play(&g, false);
    /* the crowd: none with VOX OFF; the chant chosen with VOX on */
    si_style_apply(&g.s, SI_ST_EUROSTOMP);
    g.s.arc = 0;
    g.s.vox = SI_VOX_OFF;
    si_rebuild(&g);
    reset_counts();
    si_play(&g, true); run_bars(8); si_play(&g, false);
    CHECK(chants == 0, "VOX OFF: silence from the crowd (%d)", chants);
    g.s.vox = SI_VOX_CHORUS;
    g.s.chant = SI_CHANT_HO;
    reset_counts();
    si_play(&g, true); run_bars(8); si_play(&g, false);
    CHECK(chant_kind[SI_DR_HO] > 0 && chant_kind[SI_DR_HEY] == 0, "CHANT HO: ho (%d), no hey (%d)", chant_kind[SI_DR_HO],
          chant_kind[SI_DR_HEY]);
    /* PIANO on the STABS: the offbeat chords, in any style (RAVE's own stabs are not offbeat) */
    si_style_apply(&g.s, SI_ST_RAVE);
    g.s.arc = 0;
    g.s.snd[SI_STABS] = SI_STABS_PIANO;
    si_rebuild(&g);
    si_play(&g, true); run_bars(5);
    {
        int off = 0, on_beat = 0;
        for (int k = 0; k < 16; ++k)
            if ((g.hits_last[SI_STABS] >> k) & 1u) { if (k % 4 == 2) off++; else if (k % 4 == 0) on_beat++; }
        CHECK(off >= 2 && on_beat == 0, "PIANO: the chords on the offbeats (%d off, %d on the beat)", off, on_beat);
    }
    si_play(&g, false);
    /* the hook's phrase: REPEAT bars of it */
    g.s.repeat = 4;
    si_rebuild(&g);
    int last = -1;
    for (int k = 0; k < SI_HOOK_LEN; ++k) if (g.hook[k] != -128) last = k;
    CHECK(last >= 48, "REPEAT 4: the hook runs four bars (its last note at %d)", last);
    g.s.repeat = 1;
    si_rebuild(&g);
    last = -1;
    for (int k = 0; k < SI_HOOK_LEN; ++k) if (g.hook[k] != -128) last = k;
    CHECK(last < 16, "REPEAT 1: one bar (%d)", last);
    /* a bad record: every field back in range */
    si_settings s;
    memset(&s, 0xEE, sizeof s);
    si_settings_fix(&s);
    CHECK(s.style < SI_STYLES && s.lift <= 100 && s.bars == 4 && s.scale < SI_SCALES && s.bpm >= 60 && s.bpm <= 200 &&
          s.repeat == 4 && s.keyup <= 2 && s.vox < SI_VOX_COUNT && s.bassline < SI_BL_COUNT, "a bad record fixed");
    /* the scale snap: A MINOR has no G# */
    si_style_apply(&s, SI_ST_EUROSTOMP);
    CHECK(si_snap(&s, 68) == 67 || si_snap(&s, 68) == 69, "G# onto A minor: %d", si_snap(&s, 68));
    printf("si_groove: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
