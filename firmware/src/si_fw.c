/* SPDX-License-Identifier: GPL-3.0-only */
/* SILL on the FM-1: the glue between Sill (the groove engine and its screen) and Felucca's firmware (the
 * engines, voices and effects, the panel, the screen driver, USB MIDI, the flash). Included by felucca.c after
 * Felucca's own sources when FELUCCA_SILL is 1; the groove engine (si_groove.c) and the maths (tuna_libm.c, from
 * Tuna) are their own translation units, linked beside felucca.o (tools/build.py). Shaped as Flyfish's ff_fw.c.
 *
 * Who runs what:
 *   the audio ISR (audio.c audio_block -> si_audio_pre): the groove in time, its notes straight into Felucca's
 *     tracks (trk_note_on / off) and out as USB MIDI; the lift and the pump into the tracks (sis_block);
 *   the main loop (main.c -> si_frame, si_input): the panel into the controls (the audio ISR held off while one
 *     acts), a part's sound loaded when it changed (a preset load is the main loop's), the cards into the tracks,
 *     the LEDs, the settings and sets saved while stopped, the screen.
 * Tracks: 1 STABS, 2 BASS, 3 HOOK, 4 DRUMS (si_sounds.h). */
#include "si_groove.h"
#include "si_sounds.h"
#include "si_ui.h"
#include "si_ui.c"
#include "si_version.h"

static si_groove si_g;
static si_ui si_u;

/* what the tracks were last set to */
static si_settings si_applied;
static bool si_applied_ok;
static uint8_t si_loaded_sig = 0xFF;
static int8_t si_loaded_snd[SI_PARTS] = {-1, -1, -1, -1};

/* ---- the audio ISR --------------------------------------------------------------------------------------------- */
static void si_sound(void *ctx, const si_event *e)
{
    (void)ctx;
    uint32_t p = e->part % SI_PARTS;
    int n = p == SI_DRUMS ? sis_drum_note(&si_g.s, e->note) : e->note;
    if (n < 0)                                                /* (the choir on a kit without it) */
        return;
    if (e->on)
        trk_note_on(&trk[p], (uint32_t)n, e->vel);
    else
        trk_note_off(&trk[p], (uint32_t)n);
    if (p == SI_DRUMS && si_is_choir(e->note))                /* (the choir has no General MIDI drum: not sent) */
        return;
    /* MIDI out: STABS on channel 1, BASS 2, HOOK 3, the drums 10 (General MIDI's) */
    uint32_t ch = p == SI_DRUMS ? 9u : p, st = (e->on ? 0x90u : 0x80u) | ch, cin = st >> 4;
    int mn = p == SI_DRUMS ? (int)(e->note == SI_DR_KICK ? 36 : e->note == SI_DR_SNARE ? 38 : e->note == SI_DR_CLAP ? 39
                                   : e->note == SI_DR_HAT ? 42 : e->note == SI_DR_OPEN ? 46 : 49) : n;
    midi_out_event(cin | st << 8 | (uint32_t)mn << 16 | (uint32_t)(e->on ? e->vel : 0u) << 24);
}

static void si_audio_pre(uint32_t n)
{
    si_process(&si_g, (int)n);
    sis_block(&si_g);
}

static void si_audio_post(int32_t *out, uint32_t n)
{
    (void)out;
    (void)n;
}

/* ---- the settings: one record in Felucca's first project object, the sets in the second; saved only while stopped
 * (the flash is the code's: XIP, writing it while playing would stall the audio) ---- */
#define SI_REC_MAGIC 0x31534953u                             /* "SIS1" */
#define SI_SETS_MAGIC 0x31544544u                            /* "DET1" */
typedef struct { uint32_t magic; uint16_t size, ver; si_settings s; } si_record;
typedef struct { uint32_t magic; uint16_t size; uint8_t used, pad; si_settings s[SI_SETS]; } si_sets_rec;
static uint32_t si_save_ms;
static bool si_dirty;
static void si_settings_load(void)
{
#if FELUCCA_FLASH
    si_record r;
    memset(&r, 0, sizeof r);
    int n = st_load(OBJ_PROJECT0, &r, sizeof r);
    if (n >= (int)(sizeof r - sizeof r.s) && r.magic == SI_REC_MAGIC) {
        si_settings s;
        si_defaults(&s);                                     /* (a shorter, older record: the new fields their defaults) */
        uint32_t k = r.size < sizeof s ? r.size : sizeof s;
        memcpy(&s, &r.s, k);
        si_settings_fix(&s);
        si_g.s = s;
    }
#endif
}
static void si_settings_save(void)
{
#if FELUCCA_FLASH
    si_record r;
    memset(&r, 0, sizeof r);
    r.magic = SI_REC_MAGIC;
    r.size = (uint16_t)sizeof r.s;
    r.ver = 1;
    r.s = si_g.s;
    st_save(OBJ_PROJECT0, &r, sizeof r);
#endif
}
static void si_sets_load(void)
{
#if FELUCCA_FLASH
    static si_sets_rec r;                                    /* (static: 8 records, off the stack) */
    memset(&r, 0, sizeof r);
    if (st_load(OBJ_PROJECT0 + 1u, &r, sizeof r) == (int)sizeof r && r.magic == SI_SETS_MAGIC && r.size == sizeof r.s[0]) {
        for (int i = 0; i < SI_SETS; ++i) {
            si_u.sets[i] = r.s[i];
            si_settings_fix(&si_u.sets[i]);
        }
        si_u.set_used = r.used;
    }
#endif
}
static void si_sets_save(void)
{
#if FELUCCA_FLASH
    static si_sets_rec r;
    memset(&r, 0, sizeof r);
    r.magic = SI_SETS_MAGIC;
    r.size = (uint16_t)sizeof r.s[0];
    r.used = si_u.set_used;
    memcpy(r.s, si_u.sets, sizeof r.s);
    st_save(OBJ_PROJECT0 + 1u, &r, sizeof r);
#endif
}

/* ---- the sounds into the tracks (main loop) ---------------------------------------------------------------------- */
/* a sound of Sill's own over the factory preset just loaded (as ui.c apply_preset_to puts a preset in) */
static void si_apply_own(int16_t *p, const preset_t *pr)
{
    static const uint8_t FX_DEF[4] = {0, 24, 28, 36};
    for (int i = 0; i < 8; i++)
        p[P_E0 + i] = pr->e[i];
    p[P_ATK] = pr->env[0];
    p[P_DEC] = pr->env[1];
    p[P_SUS] = pr->env[2];
    p[P_REL] = pr->env[3];
    p[P_ED_FLT] = pr->fenv;
    p[P_VOICE] = pr->mono ? V_LEGATO : V_POLY;
    for (int i = 0; i < 4; i++)
        p[P_DIST + i] = (int16_t)(pr->fx[i] ? pr->fx[i] - 1 : FX_DEF[i]);
}
/* one of Tuna's FM6 patches as the track's own (SLOT OWN), so nothing reloads a factory one over it (eng_fm6.c) */
static void si_fm6_load(track_t *t, const uint8_t *packed)
{
    uint32_t tr = (uint32_t)(t - trk) % NTRK;
    uint8_t v[FP_SIZE + 1u];
    fm6_unpack(packed, v);
    fm6_set_patch(tr, v);
    t->p[P_E7] = FM6_OWN;
    fm6_slot[tr] = FM6_OWN;
    fm6_own_ok &= (uint8_t)~(1u << tr);
}
static void si_load_part(uint32_t k)
{
    undo_depth++;                                            /* (no undo copy of Sill's loads) */
    if (k == SI_DRUMS) {
        set_engine_of(&trk[k], sis_kit_engine(si_g.s.snd[3]));
        apply_preset_to(&trk[k], 0u);
        undo_depth--;
        return;
    }
    const sis_sound *snd = sis_of(&si_g, (int)k);
    set_engine_of(&trk[k], snd->engine);
    apply_preset_to(&trk[k], snd->preset);
    undo_depth--;
    fm1_irq_off();
    if (snd->own)
        si_apply_own(trk[k].p, snd->own);
    if (snd->fm6)
        si_fm6_load(&trk[k], snd->fm6);
    fm1_irq_on();
}

static void si_apply(void)
{
    const si_settings *s = &si_g.s;
    bool all = !si_applied_ok, every = all || si_loaded_sig != si_g.sound_sig;   /* (a style or a set: every part) */
    for (uint32_t k = 0; k < SI_PARTS; ++k)                  /* a sound changed (a card, a style, a set): load it */
        if (every || si_loaded_snd[k] != (int8_t)s->snd[k]) {
            si_load_part(k);
            si_loaded_snd[k] = (int8_t)s->snd[k];
            all = true;
        }
    si_loaded_sig = si_g.sound_sig;
    if (all || memcmp(s, &si_applied, sizeof *s)) {
        fm1_irq_off();
        for (uint32_t k = 0; k < SI_PARTS; ++k)
            sis_track(&trk[k], &si_g, (int)k);
        sis_song(s);
        fm1_irq_on();
    }
    if (s->palette != settings.palette % NPALETTES) {        /* (the menu's colour, kept as Felucca keeps its own) */
        settings.palette = s->palette;
        settings_save();
    }
    si_applied = *s;
    si_applied_ok = true;
}

/* ---- the power-on screen: Sill's logo (tools/si_logo.py: 176 px, 8 greys, rows run-length coded) and the
 * Felucca it runs on; main.c holds it 0.75 s ---- */
#include "si_logo.inc"
#define SI_SPLASH_MS 750u
static void si_splash(void)
{
    uint16_t row[2][SI_LOGO_SIZE];                           /* (two: one fills while the other goes out by DMA) */
    uint32_t y, x0 = (240u - SI_LOGO_SIZE) / 2u, y0 = 16u;
    char line[40] = "Sill " SI_VERSION " on Felucca ";
    uint32_t k = sizeof "Sill " SI_VERSION " on Felucca " - 1u;
    lcd_fill(0, 0, 240, 240, SI_LOGO_BG);
    for (y = 0; y < SI_LOGO_SIZE; y++) {
        const uint8_t *p = SI_LOGO_RLE + SI_LOGO_ROW[y];
        uint16_t *r = row[y & 1u];
        uint32_t x = 0;
        while (x < SI_LOGO_SIZE) {
            uint32_t n = (*p >> 3) + 1u, c = SI_LOGO_PAL[*p & 7u];
            p++;
            while (n-- && x < SI_LOGO_SIZE)
                r[x++] = (uint16_t)c;
        }
        lcd_blit(x0, y0 + y, SI_LOGO_SIZE, 1, r);
    }
    for (const char *v = FELUCCA_VERSION; *v && k < sizeof line - 1u; v++)
        line[k++] = *v;
    line[k] = 0;
    draw_text_line(0, y0 + SI_LOGO_SIZE + 18u, 240, &AF_S, line, 0xA535u, SI_LOGO_BG, 1);   /* (grey, 160 165 170) */
    lcd_sync();
}

/* ---- start ------------------------------------------------------------------------------------------------------ */
static void si_fw_init(void)
{
    uint32_t i;
    for (i = 0; i < G_COUNT; i++)
        song.g[i] = GP[i].def;
    undo_depth++;
    fm6_init();
    for (i = 0; i < NTRK; i++) {
        track_t *t = &trk[i];
        track_defaults(t);
        set_engine_of(t, i == 3u ? sis_kit_engine(SI_KIT_EURO) : SIS_ANALOG);
        apply_preset_to(t, 0u);
        t->engine = t->eng_req;
        track_defaults_steps(t);                             /* (Felucca's sequencer stays empty) */
    }
    undo_depth--;
    song.sel = 0;
    song.master_q12 = 2048;
    si_init(&si_g, (float)FS);
    si_settings_load();
    si_g.s.palette = (uint8_t)(settings.palette % NPALETTES);
    si_rebuild(&si_g);
    si_set_sound(&si_g, si_sound, 0);
    su_init(&si_u, &si_g);
    si_sets_load();
    si_apply();
}

/* ---- the main loop ----------------------------------------------------------------------------------------------- */
static uint32_t si_notes_prev;
static void si_input(void)
{
    uint32_t released = 0, pressed = fm1_input_edges(&released), b, k;
    static const uint8_t KNOB[NE] = {SU_K_SELECT, SU_K_ALGO, SU_K_PRESETS, SU_K1, SU_K2, SU_K3, SU_K4};
    fm1_input_note_edges();                                  /* (the keys: from their state below) */
    for (b = 0; b < NB; b++) {                               /* (Felucca's B_* are Sill's SU_B_*, in the same order) */
        uint32_t bit = 1u << panel.btn[b];
        if (pressed & bit) {
            fm1_irq_off();
            su_button(&si_u, (int)b, true);
            fm1_irq_on();
        }
        if (released & bit) {
            fm1_irq_off();
            su_button(&si_u, (int)b, false);
            fm1_irq_on();
        }
    }
    for (k = 0; k < NE; k++) {
        int32_t s = panel_enc(k);
        if (s) {
            fm1_irq_off();
            su_knob(&si_u, KNOB[k], s);
            fm1_irq_on();
        }
    }
    uint32_t notes = fm1_in.notes, ch = notes ^ si_notes_prev;
    for (k = 0; ch && k < 27u; k++)
        if ((ch >> k) & 1u) {
            fm1_irq_off();
            su_key(&si_u, SU_KEY_LO + (int)k, (notes >> k) & 1u);
            fm1_irq_on();
        }
    si_notes_prev = notes;
}

/* the LEDs: the page's button, REC while a BREAK holds, SEQ while the key is up, PLAY green while playing; the keys: the
 * ones held, else the chord's tones (the notes that fit right now) */
static void si_leds(void)
{
    static const uint8_t PAGE_BTN[SU_PAGES] = {
        [SU_PG_PLAY] = B_HOME, [SU_PG_SOUND] = B_FX, [SU_PG_TONE] = B_FX, [SU_PG_VOX] = B_LFO, [SU_PG_KEY] = B_SCL,
        [SU_PG_GROOVE] = B_ARP, [SU_PG_DRUMS] = B_EDIT, [SU_PG_MIX] = B_GLO, [SU_PG_SPACE] = B_GLO, [SU_PG_ARC] = B_ENV,
        [SU_PG_HOOK] = B_ENV, [SU_PG_SETS] = B_SAVE, [SU_PG_MENU] = B_HOME};
    static uint8_t ready;
    uint8_t nl[FM1_NCOL] = {0}, nd[FM1_NCOL] = {0};
    uint32_t k, c;
    if (!ready) {
        led_pos_init();
        ready = 1;
    }
    led_put(nl, panel.btn[PAGE_BTN[si_u.page % SU_PAGES]], 1);
    led_put(nl, panel.btn[B_REC], si_g.brk);
    led_put(nl, panel.btn[B_SEQ], si_g.tr > 0);
    for (k = 0; k < NB; k++)
        led_put(nd, panel.btn[k], 1);
    uint32_t lit = 0;
    if (si_g.playing && si_g.nloop > 0) {
        const si_chord *chd = &si_g.loop[si_chord_now(&si_g)];
        for (k = 0; k < 27u; k++)
            for (int t = 0; t < 4; ++t)
                if ((SU_KEY_LO + k) % 12u == (uint32_t)(chd->tone[t] + si_g.tr) % 12u) lit |= 1u << k;
    }
    for (k = 0; k < 27u; k++) {
        led_put(nl, 14u + k, (int)((fm1_in.notes >> k) & 1u));
        led_put(nd, 14u + k, (int)((lit >> k) & 1u) || !((fm1_in.notes >> k) & 1u));
    }
    if (si_g.playing)
        led_clear(nd, panel.btn[B_PLAY]);
    for (c = 0; c < FM1_NCOL; c++)
        nd[c] = settings_leds == LEDS_DIM || settings_leds == LEDS_DIM_LO ? (uint8_t)(nd[c] & ~nl[c]) : 0u;
    /* the chord's keys glow dim even with the dim LEDs off: they are music, not decoration */
    for (k = 0; k < 27u; k++)
        if ((lit >> k) & 1u && !((fm1_in.notes >> k) & 1u))
            led_put(nd, 14u + k, 1);
    if (si_g.playing)
        nl[LED_PLAY_GREEN >> 3] |= (uint8_t)(1u << (LED_PLAY_GREEN & 7u));
    fm1_led_dim_level(settings_leds == LEDS_DIM_LO);
    for (c = 0; c < FM1_NCOL; c++) {
        fm1_led_dim[c] = nd[c];
        fm1_led_breath[c] = 0;
        fm1_led[c] = nl[c];
    }
}

/* a frame: the settings into the tracks, the LEDs, the screen; the settings and sets saved a while after the last
 * change, stopped */
static uint32_t si_last_ms;
static void si_frame(void)
{
    uint32_t now = fm1_ms, dt = now - si_last_ms;
    static uint8_t first = 1;
    si_last_ms = now;
    if (si_u.changed) {
        si_u.changed = false;
        si_dirty = true;
        si_save_ms = now;
    }
    si_apply();
    if (si_dirty && !si_g.playing && now - si_save_ms > 2000u) {
        si_settings_save();
        si_dirty = false;
    }
    if (si_u.sets_changed && !si_g.playing) {                /* (stopped: the flash is the code's while playing) */
        si_u.sets_changed = false;
        si_sets_save();
    }
    song.playing = si_g.playing;                             /* (PLAY's green LED, Felucca's meters) */
    si_leds();
    si_u.batt = (int)batt_shown();
    su_tick(&si_u, dt > 100u ? 100 : (int)dt);
    if (first) {
        su_draw_all(&si_u);
        first = 0;
    } else {
        su_draw(&si_u);
    }
}
