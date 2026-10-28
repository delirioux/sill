/* SPDX-License-Identifier: GPL-3.0-only */
/* SILL screen and controls: Sill's pages on Felucca's drawing (gfx.c: canvas, fonts, palettes, icons), in
 * Tuna's frame: the header, four knob cards, the panel, the footer (Daurade's layout). Compiled after gfx.c in one unit, as Felucca's
 * ui*.c are (the firmware's unity build, or a host test).
 *
 * The controls
 *   SELECT     tempo                                  MASTER     volume (Felucca's)
 *   PRESETS    the style (the list shows while turning; playing, it comes in on the next bar)
 *   ALGORITHM  a new song in the same style (on the next bar)
 *   KNOB 1..4  the four cards of the page (PLAY: LIFT PUMP VOX ENERGY)
 *   PLAY       start (with the intro, ARC on) / stop
 *   REC        BREAK: tapped, the stabs alone for three bars, a snare roll, the chorus on the bar; held, until let go
 *   SEQ        KEY UP: the song a semitone up from the next bar (+1 +2 +3, then home)
 *   keys       the hook, live, snapped to the scale. Music only, never settings
 *   OCT- OCT+  the keys' octave
 *   HOME       PLAY; held: MENU (colour, look)
 *   FX         SOUND, again: TONE           LFO   VOX                SCL   KEY
 *   ARP        GROOVE                       EDIT  DRUMS              GLO   MIX, again: SPACE
 *   ENV        ARC, again: HOOK             SAVE  SETS (eight sets on the knobs) */
#ifndef SI_UI_H
#define SI_UI_H

#include <stdbool.h>
#include <stdint.h>
#include "si_groove.h"

enum { SU_K_MASTER, SU_K_SELECT, SU_K_PRESETS, SU_K_ALGO, SU_K1, SU_K2, SU_K3, SU_K4, SU_KNOBS };
enum {
    SU_B_FX, SU_B_SCL, SU_B_ENV, SU_B_LFO, SU_B_EDIT, SU_B_GLO,
    SU_B_HOME, SU_B_SAVE, SU_B_ARP, SU_B_SEQ, SU_B_PLAY, SU_B_REC,
    SU_B_OCTDN, SU_B_OCTUP, SU_BUTTONS
};
enum { SU_PG_PLAY, SU_PG_SOUND, SU_PG_TONE, SU_PG_VOX, SU_PG_KEY, SU_PG_GROOVE, SU_PG_DRUMS, SU_PG_MIX,
       SU_PG_SPACE, SU_PG_ARC, SU_PG_HOOK, SU_PG_SETS, SU_PG_MENU, SU_PAGES };
#define SU_KEY_LO 53                      /* F3: the FM-1's lowest key */
#define SU_KEY_HI 79                      /* G5 */

typedef struct {
    si_groove *g;
    int page;
    int hot, hot_ms;                      /* the knob just turned (its card in the accent); 9: SELECT */
    int browse_ms;                        /* the style list over the panel (PRESETS) */
    char msg[32];
    int msg_ms;
    int home_ms;
    bool home_down;
    uint32_t keys_down;                   /* bit (note - SU_KEY_LO) */
    int oct;                              /* OCT- OCT+: the keys' octave, -2 .. 2 */
    int key_sent[27];                     /* the note each key sent (with the octave it was pressed in) */
    /* SETS */
    si_settings sets[SI_SETS];
    uint8_t set_used;                     /* bit per set */
    int set_sel;
    int clear_turns, clear_ms;            /* CLEAR: three turns right within a second */
    bool sets_changed;                    /* the glue saves the sets while stopped */
    uint32_t frame;
    int batt;                             /* 0..3 bars, 4 = USB power */
    bool changed;                         /* a setting changed: the glue applies it (sound, mix) and saves it later */
    uint32_t sig_head, sig_card[4], sig_foot;
    bool force;
} si_ui;

void su_init(si_ui *u, si_groove *g);
void su_knob(si_ui *u, int knob, int delta);
void su_button(si_ui *u, int button, bool down);
void su_key(si_ui *u, int note, bool down);
void su_tick(si_ui *u, int ms);           /* timers; every frame */
void su_draw(si_ui *u);                   /* what changed; the panel every frame */
void su_draw_all(si_ui *u);
const char *su_page_name(int page);

#endif
