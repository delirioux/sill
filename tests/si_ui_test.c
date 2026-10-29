/* SPDX-License-Identifier: GPL-3.0-only */
/* SILL's screen and controls on the host: firmware/src/si_ui.c drawn by Felucca's gfx.c into a 240 x 240 frame,
 * the groove running (its notes counted, no sound), the controls driven as a player would, every page in every
 * palette written out, and the layout lint (from Tuna's and Flyfish's UI tests): every text inside its canvas, not
 * cut, not ellipsised unless it is a name, no two texts of one canvas overlapping.
 *   tests/si_ui_test.sh [OUTDIR]     (exit 0: all passed; PNGs and a GIF of PLAY under OUTDIR) */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* ---- the layout lint: the ink boxes of one canvas, checked when it is blitted -------------------------- */
typedef struct { int x0, y0, x1, y1, flags; char s[24]; } box;
static box boxes[256];
static int nbox;
static const char *lint_where = "";
static int lint_findings;
static void hook_text(int x0, int y0, int x1, int y1, const char *s, int flags)
{
    if (nbox < 256) {
        box *b = &boxes[nbox++];
        b->x0 = x0; b->y0 = y0; b->x1 = x1; b->y1 = y1; b->flags = flags;
        strncpy(b->s, s, sizeof b->s - 1);
        b->s[sizeof b->s - 1] = 0;
    }
}
#define GFX_HOOK_TEXT(x0, y0, x1, y1, s, flags) hook_text((x0), (y0), (x1), (y1), (s), (flags))
#define GFX_HOOK_BLIT(x, y, r0) ((void)0)
#define GFX_HOOK_BEGIN() (nbox = 0)
#define GFX_HOOK_PIXELS(n) ((void)0)

static uint16_t fb[240 * 240];
static void lint_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    for (int i = 0; i < nbox; ++i) {
        box *b = &boxes[i];
        if (b->x1 <= b->x0)
            continue;
        bool name = (b->flags & 8) != 0;            /* free text (a name, a message) may be ellipsised */
        if (b->x0 < 0 || b->y0 < 0 || b->x1 > (int)w || b->y1 > (int)h) {
            printf("LINT %s: \"%s\" leaves its canvas (%d,%d..%d,%d of %ux%u at %u,%u)\n", lint_where, b->s, b->x0,
                   b->y0, b->x1, b->y1, w, h, x, y);
            lint_findings++;
        }
        if ((int)x + b->x1 > 240 || (int)y + b->y1 > 240) {
            printf("LINT %s: \"%s\" off the screen\n", lint_where, b->s);
            lint_findings++;
        }
        if ((b->flags & 1) && !name) {
            printf("LINT %s: \"%s\" ellipsised\n", lint_where, b->s);
            lint_findings++;
        }
        for (int j = i + 1; j < nbox; ++j) {
            box *c = &boxes[j];
            if (c->x1 <= c->x0)
                continue;
            if (b->x0 < c->x1 && c->x0 < b->x1 && b->y0 < c->y1 && c->y0 < b->y1) {
                printf("LINT %s: \"%s\" overlaps \"%s\" (%d,%d..%d,%d / %d,%d..%d,%d)\n", lint_where, b->s, c->s, b->x0,
                       b->y0, b->x1, b->y1, c->x0, c->y0, c->x1, c->y1);
                lint_findings++;
            }
        }
    }
    nbox = 0;
}
static void lcd_sync(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{
    lint_blit(x, y, w, h);
    for (uint32_t j = 0; j < h && y + j < 240; j++)
        for (uint32_t i = 0; i < w && x + i < 240; i++)
            fb[(y + j) * 240 + x + i] = p[j * w + i];
}
#define main hostsim_main
#include "hostsim.c"                               /* (core.h, the matrix: si_sounds.h's track settings) */
#undef main
#include "gfx.c"
#include "si_groove.h"
#include "si_sounds.h"
#include "si_ui.c"

static const char *outdir;
static int frames_written;
static void write_ppm(const char *name)
{
    if (!outdir) return;
    char path[512];
    snprintf(path, sizeof path, "%s/ppm/%s.ppm", outdir, name);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6 240 240 255\n");
    for (int i = 0; i < 240 * 240; i++) {
        uint16_t c = (uint16_t)((fb[i] >> 8) | (fb[i] << 8));
        uint8_t px[3] = {(uint8_t)((c >> 11) << 3 | (c >> 13)), (uint8_t)(((c >> 5) & 63) << 2 | ((c >> 9) & 3)),
                         (uint8_t)((c & 31) << 3 | ((c >> 2) & 7))};
        fwrite(px, 1, 3, f);
    }
    fclose(f);
    frames_written++;
}

static int notes_on, notes_off;
static int part_on[SI_PARTS];
static void sound(void *ctx, const si_event *e)
{
    (void)ctx;
    if (e->on) { notes_on++; part_on[e->part % SI_PARTS]++; }
    else notes_off++;
}

static si_groove g;
static si_ui ui;
static void run_ms(int ms)
{
    for (int t = 0; t < ms; t += 16) {
        si_process(&g, 44100 * 16 / 1000);
        su_tick(&ui, 16);
        su_draw(&ui);
    }
}
static void press(int b) { su_button(&ui, b, true); su_button(&ui, b, false); }
static void shot(const char *name)
{
    lint_where = name;
    su_draw_all(&ui);
    write_ppm(name);
}
static void all_palettes(const char *page)
{
    int keep = g.s.palette, keep_st = g.s.look;
    for (int p = 0; p < (int)NPALETTES; ++p)
        for (int st = 0; st < 2; ++st) {
            char name[64];
            g.s.palette = (uint8_t)p;
            g.s.look = (uint8_t)st;
            su_apply_style(&ui);
            snprintf(name, sizeof name, "%s%s_%s", st ? "LINE_" : "", UI_PALETTES[p].name, page);
            shot(name);
        }
    g.s.palette = (uint8_t)keep;
    g.s.look = (uint8_t)keep_st;
    su_apply_style(&ui);
}
static void bars(int n)                            /* play until n bars on */
{
    int b = g.bar + n;
    for (int k = 0; k < 4000 && g.bar < b; ++k)
        run_ms(16);
}

int main(int argc, char **argv)
{
    outdir = argc > 1 ? argv[1] : 0;
    si_init(&g, 44100.0f);
    si_set_sound(&g, sound, 0);
    su_init(&ui, &g);
    CHECK(g.s.palette == UI_LASER_INDEX && !strcmp(UI_PALETTES[UI_LASER_INDEX].name, "LASER"), "LASER by default");
    CHECK(g.s.style == SI_ST_EUROSTOMP, "EUROSTOMP first");
    shot("01_play_stopped");
    all_palettes("play_stopped");
    press(SU_B_PLAY);
    CHECK(g.playing, "PLAY plays");
    bars(9);
    CHECK(g.sec == SI_SEC_VERSE, "the intro over at bar 8 (%s at %d)", si_sec_name(g.sec), g.bar);
    CHECK(part_on[SI_STABS] && part_on[SI_BASS] && part_on[SI_DRUMS], "the parts play (%d %d %d %d)", part_on[0],
          part_on[1], part_on[2], part_on[3]);
    while (g.sec != SI_SEC_CHORUS) run_ms(16);
    bars(2);
    CHECK(part_on[SI_HOOK] > 0, "the chorus: the hook (%d)", part_on[SI_HOOK]);
    shot("02_play");
    all_palettes("play");
    /* the PLAY animation: a bar, a frame every 48 ms */
    for (int f = 0; f < 40; ++f) {
        char nm[32];
        run_ms(48);
        snprintf(nm, sizeof nm, "anim_%03d", f);
        lint_where = nm;
        write_ppm(nm);
    }
    /* the knobs of PLAY */
    su_knob(&ui, SU_K1, -4);
    CHECK(g.s.lift == 65, "LIFT -4 steps of 5 (%d)", g.s.lift);
    su_knob(&ui, SU_K1, 4);
    su_knob(&ui, SU_K3, 1);
    CHECK(g.s.vox == SI_VOX_ALL, "VOX: ALL (%d)", g.s.vox);
    su_knob(&ui, SU_K3, -1);
    /* BREAK: tapped, back four bars on */
    su_button(&ui, SU_B_REC, true);
    CHECK(g.brk, "REC: BREAK");
    run_ms(64);
    su_button(&ui, SU_B_REC, false);
    bars(1);
    shot("03_break");
    all_palettes("break");
    int back = g.brk_end;
    bars(4);
    CHECK(!g.brk && g.bar >= back, "back on the bar (%d, %d)", g.bar, back);
    /* KEY UP: SEQ, from the next bar */
    press(SU_B_SEQ);
    bars(1);
    CHECK(g.tr == 1, "SEQ: KEY UP +1 (%d)", g.tr);
    run_ms(1600);
    shot("17_keyup");
    all_palettes("keyup");
    press(SU_B_SEQ); press(SU_B_SEQ); press(SU_B_SEQ);
    bars(1);
    CHECK(g.tr == 0, "SEQ after +3: home (%d)", g.tr);
    /* the styles: PRESETS queues one for the next bar */
    su_knob(&ui, SU_K_PRESETS, 7);
    CHECK(g.next_style == SI_ST_PARTY, "PRESETS +7: PARTY queued (%d)", g.next_style);
    shot("04_styles");
    all_palettes("styles");
    bars(1);
    CHECK(g.s.style == SI_ST_PARTY && g.s.bpm == 140, "PARTY on the bar line (%d, %d BPM)", g.s.style, g.s.bpm);
    run_ms(2000);
    /* SOUND, TONE */
    press(SU_B_FX);
    CHECK(ui.page == SU_PG_SOUND, "FX: SOUND");
    bars(1);
    shot("05_sound");
    all_palettes("sound");
    su_knob(&ui, SU_K1, 1);
    CHECK(g.s.snd[SI_STABS] == 5, "K1: the next STABS sound (%d)", g.s.snd[SI_STABS]);
    su_knob(&ui, SU_K1, -1);
    {                                              /* every sound's name on its card and the panel: the lint */
        uint8_t keep[4];
        memcpy(keep, g.s.snd, sizeof keep);
        for (int i = 0; i < SI_SOUNDS; ++i) {
            char nm[32];
            g.s.snd[0] = g.s.snd[1] = g.s.snd[2] = (uint8_t)i;
            snprintf(nm, sizeof nm, "sound_%02d", i);
            shot(nm);
        }
        memcpy(g.s.snd, keep, sizeof keep);
    }
    press(SU_B_FX);
    CHECK(ui.page == SU_PG_TONE, "FX again: TONE");
    shot("06_tone");
    all_palettes("tone");
    /* HEARTACHE for VOX: the OOH sings the hook */
    si_queue_style(&g, SI_ST_HEARTACHE);
    bars(2);
    press(SU_B_LFO);
    CHECK(ui.page == SU_PG_VOX, "LFO: VOX");
    CHECK(sis_of(&g, SI_HOOK)->engine == SIS_VOICE, "HEARTACHE: the OOH on the hook");
    run_ms(300);
    shot("07_vox");
    all_palettes("vox");
    su_knob(&ui, SU_K3, 3);
    CHECK(g.s.chant == SI_CHANT_MIX, "CHANT: MIX (%d)", g.s.chant);
    shot("07b_vox_mix");
    /* KEY */
    si_queue_style(&g, SI_ST_EUROSTOMP);
    bars(1);
    press(SU_B_SCL);
    CHECK(ui.page == SU_PG_KEY, "SCL: KEY");
    shot("08_key");
    all_palettes("key");
    su_knob(&ui, SU_K1, 1);
    CHECK(g.s.key == 10 && g.loop[0].root == 10, "KEY A#: the loop follows (%d)", g.loop[0].root);
    su_knob(&ui, SU_K1, -1);
    /* GROOVE, DRUMS */
    press(SU_B_ARP);
    while (!si_in_chorus(&g)) run_ms(16);
    bars(1);
    CHECK(ui.page == SU_PG_GROOVE, "ARP: GROOVE");
    shot("09_groove");
    all_palettes("groove");
    su_knob(&ui, SU_K3, 2);
    CHECK(g.s.bassline == SI_BL_ROLLING, "GROOVE BASS: ROLLING (%d)", g.s.bassline);
    su_knob(&ui, SU_K3, -2);
    press(SU_B_EDIT);
    CHECK(ui.page == SU_PG_DRUMS, "EDIT: DRUMS");
    shot("10_drums");
    all_palettes("drums");
    /* MIX, SPACE */
    press(SU_B_GLO);
    CHECK(ui.page == SU_PG_MIX, "GLO: MIX");
    shot("11_mix");
    all_palettes("mix");
    press(SU_B_GLO);
    CHECK(ui.page == SU_PG_SPACE, "GLO again: SPACE");
    shot("12_space");
    all_palettes("space");
    /* ARC, HOOK */
    press(SU_B_ENV);
    CHECK(ui.page == SU_PG_ARC, "ENV: ARC");
    shot("13_arc");
    all_palettes("arc");
    press(SU_B_ENV);
    CHECK(ui.page == SU_PG_HOOK, "ENV again: HOOK");
    while (!si_in_chorus(&g)) run_ms(16);
    run_ms(400);
    shot("14_hook");
    all_palettes("hook");
    su_knob(&ui, SU_K4, 1);
    CHECK(g.s.repeat == 4, "REPEAT: 4 BARS (%d)", g.s.repeat);
    su_knob(&ui, SU_K4, -1);
    /* ITALO's arpeggio on the HOOK page */
    si_queue_style(&g, SI_ST_ITALO);
    bars(1);
    shot("14b_hook_arp");
    /* SETS: store into 3, load it back */
    si_queue_style(&g, SI_ST_EUROSTOMP);
    bars(1);
    press(SU_B_SAVE);
    CHECK(ui.page == SU_PG_SETS, "SAVE: SETS");
    su_knob(&ui, SU_K1, 2);
    su_knob(&ui, SU_K3, 1);
    CHECK((ui.set_used & 4u) && ui.sets[2].style == SI_ST_EUROSTOMP, "STORE into set 3");
    ui.sets[0] = g.s; ui.sets[0].style = SI_ST_PARTY; ui.set_used |= 1u;
    ui.sets[4] = g.s; ui.sets[4].style = SI_ST_PIANOHOUSE; ui.set_used |= 16u;
    ui.sets[6] = g.s; ui.sets[6].style = SI_ST_BUBBLEGUM; ui.set_used |= 64u;
    shot("15_sets");
    all_palettes("sets");
    su_knob(&ui, SU_K1, -2);
    su_knob(&ui, SU_K2, 1);
    CHECK(g.s.style == SI_ST_PARTY, "LOAD set 1: PARTY at once");
    su_knob(&ui, SU_K4, 1); su_knob(&ui, SU_K4, 1);
    CHECK(ui.set_used & 1u, "CLEAR twice: not yet");
    su_knob(&ui, SU_K4, 1);
    CHECK(!(ui.set_used & 1u), "CLEAR three times: cleared");
    /* the keys: the hook, snapped to the scale */
    int before = part_on[SI_HOOK];
    su_key(&ui, 54, true);
    CHECK(part_on[SI_HOOK] == before + 1, "a key plays the hook");
    su_key(&ui, 54, false);
    /* MENU: HOME held */
    su_button(&ui, SU_B_HOME, true);
    run_ms(700);
    su_button(&ui, SU_B_HOME, false);
    CHECK(ui.page == SU_PG_MENU, "HOME held: MENU");
    shot("16_menu");
    all_palettes("menu");
    press(SU_B_HOME);
    CHECK(ui.page == SU_PG_PLAY, "HOME: PLAY");
    /* ALGORITHM: a new song on the next bar */
    int song = g.s.song;
    su_knob(&ui, SU_K_ALGO, 1);
    bars(1);
    CHECK(g.s.song == song + 1, "ALGORITHM: the next song (%d)", g.s.song);
    /* stopped again */
    press(SU_B_PLAY);
    CHECK(!g.playing, "PLAY stops");
    CHECK(g.nheld == 0, "nothing left sounding");
    CHECK(lint_findings == 0, "the layout lint: %d findings", lint_findings);
    printf("si_ui: %d checks, %d failed, %d frames, %d notes on (stabs %d, bass %d, hook %d, drums %d)\n", checks, fails,
           frames_written, notes_on, part_on[0], part_on[1], part_on[2], part_on[3]);
    return fails ? 1 : 0;
}
