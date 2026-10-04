/*
 *  SPDX-License-Identifier: GPL-3.0-only
 *
 *  Xiegu X6100 LVGL GUI - JS8 waterfall on the display's lower plane
 */

#include "js8_wf.h"

#include "lv_drivers/display/drm.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif

/* Repaint this often while nothing changes (no rows while transmitting), in
 * case something else drew on the plane: the main screen's spectrum and
 * waterfall redraw there when the frequency moves (js8_wf_repaint_soon). */
#define KEEPALIVE_US 500000

/* Smooth scrolling: rows kept above the top of the screen, so the picture
 * can sit up to this many rows higher while it glides down. */
#define RING_EXTRA   2
/* At most one put per main-loop pass (one screen refresh, ~16.7 ms; this
 * timer can run twice in one pass). */
#define MIN_PUT_US   8000

static struct {
    lv_obj_t         *box;
    const lv_color_t *palette;
    float             min, max;
    int               w, h;
    /* Column-major ring: each x has 2 x n pixels (n = h + RING_EXTRA rows),
     * newest first from `head`, the same pixels again n further on. Column x
     * is then one contiguous run, which is one line of the plane: the panel
     * is portrait and the screen turned 90 degrees, so a screen column is a
     * plane line. */
    lv_color_t       *ring;
    int               n;
    int               head;
    /* Smooth scrolling: the picture sits `off` rows higher than its resting
     * place (the newest rows still above the top) and glides down to it at
     * one row per row period, so the screen changes a little at every
     * refresh instead of a whole row 15 times a second. A whole-row jump
     * changes every pixel at once and the LCD dims for a moment as they
     * settle: a flicker at the row rate. */
    bool              smooth;
    double            off;      /* rows */
    int64_t           off_us;   /* when `off` was last brought up to date */
    double            hold_off; /* >= 0: `off` held there (the harness) */
    int64_t           period_us;
    int               shown_q;  /* the offset last put on the plane (1/256 rows) */
    uint32_t          rows;
    bool              dirty;   /* changed since last put on the plane */
    bool              shown;   /* on the plane and visible since */
    int               again;   /* main-loop passes still to put it there (js8_wf_repaint_soon) */
    int64_t           again_us;
    int64_t           last_us; /* when last put there */
} wf;

static int64_t mono_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

/* The plane's area under the box: screen (x, y) is plane (y, hor_res-1-x),
 * as LVGL's LV_DISP_ROT_90 turns what it draws. */
static bool plane_area(lv_area_t *a) {
    lv_area_t c;
    lv_obj_get_coords(wf.box, &c);
    if (lv_area_get_width(&c) != wf.w || lv_area_get_height(&c) != wf.h) return false;
    lv_coord_t hres = lv_disp_get_hor_res(lv_obj_get_disp(wf.box));
    a->x1           = c.y1;
    a->x2           = c.y2;
    a->y1           = hres - 1 - c.x2;
    a->y2           = hres - 1 - c.x1;
    return a->x1 >= 0 && a->y1 >= 0;
}

/* dst[i] = a[i] * (255 - w) / 255 + a[i + 1] * w / 255, each channel
 * rounded (alpha stays 255): the column shifted by w/255 of a pixel. */
static void blend_col(lv_color_t *dst, const lv_color_t *a, int n, unsigned w) {
    if (!w) {
        memcpy(dst, a, (size_t)n * sizeof(lv_color_t));
        return;
    }
    const uint8_t *pa = (const uint8_t *)a, *pb = (const uint8_t *)(a + 1);
    uint8_t       *pd    = (uint8_t *)dst;
    int            i     = 0, bytes = n * (int)sizeof(lv_color_t);
#if defined(__ARM_NEON)
    uint8x8_t wa = vdup_n_u8((uint8_t)(255 - w)), wb = vdup_n_u8((uint8_t)w);
    for (; i + 16 <= bytes; i += 16) {
        uint8x16_t va = vld1q_u8(pa + i), vb = vld1q_u8(pb + i);
        uint16x8_t lo = vmlal_u8(vmull_u8(vget_low_u8(va), wa), vget_low_u8(vb), wb);
        uint16x8_t hi = vmlal_u8(vmull_u8(vget_high_u8(va), wa), vget_high_u8(vb), wb);
        lo            = vsraq_n_u16(lo, lo, 8); /* x + x / 256, then (+128) / 256: x / 255 rounded */
        hi            = vsraq_n_u16(hi, hi, 8);
        vst1q_u8(pd + i, vcombine_u8(vrshrn_n_u16(lo, 8), vrshrn_n_u16(hi, 8)));
    }
#endif
    for (; i < bytes; i++) {
        unsigned x = pa[i] * (255 - w) + pb[i] * w;
        pd[i]      = (uint8_t)((x + (x >> 8) + 128) >> 8);
    }
}

/* The picture `q` / 256 rows above its resting place: screen row y is ring
 * row y + q/256, between two rows a blend. Plane line j is screen column
 * w-1-j (the plane runs right to left). */
static bool present(int q) {
    lv_area_t a;
    if (!plane_area(&a)) return false;
    drm_direct_ctx_t ctx;
    if (!drm_primary_begin_direct(&ctx, (uint32_t)wf.w * wf.h)) return false; /* full this frame: next tick */
    int      k = q >> 8;
    unsigned w = (unsigned)((q & 255) * 255 + 127) / 255;
    for (int j = 0; j < wf.w; j++)
        blend_col(ctx.buf + (size_t)j * wf.h, wf.ring + (size_t)(wf.w - 1 - j) * 2 * wf.n + wf.head + k, wf.h, w);
    drm_primary_end_direct(&a);
    return true;
}

/* `off` now: down at a row a period, half as fast again while more than a
 * row is waiting (rows came in a burst). */
static void glide(int64_t now) {
    double dt = (double)(now - wf.off_us);
    wf.off_us = now;
    if (wf.hold_off >= 0) {
        wf.off = wf.hold_off;
        return;
    }
    if (!wf.smooth || wf.off <= 0) {
        wf.off = 0;
        return;
    }
    wf.off -= dt / wf.period_us * (wf.off > 1 ? 1.5 : 1.0);
    if (wf.off < 0) wf.off = 0;
}

static void black_out(void) {
    lv_area_t        a;
    drm_direct_ctx_t ctx;
    if (!plane_area(&a) || !drm_primary_begin_direct(&ctx, (uint32_t)wf.w * wf.h)) return;
    lv_color_t black = lv_color_black();
    for (size_t i = 0; i < (size_t)wf.w * wf.h; i++) ctx.buf[i] = black;
    drm_primary_end_direct(&a);
}

/* LVGL may start drawing at the box: it paints all of itself (the hole). */
static void box_cover_cb(lv_event_t *e) {
    lv_cover_check_info_t *info = lv_event_get_param(e);
    if (info->res == LV_COVER_RES_MASKED) return;
    lv_obj_t *obj = lv_event_get_target(e);
    if (_lv_area_is_in(info->area, &obj->coords, 0)) info->res = LV_COVER_RES_COVER;
}

/* Before the box draws itself (a border while keyed, its children after):
 * its pixels fully transparent, whatever the dialog's background left. */
static void box_draw_cb(lv_event_t *e) {
    lv_draw_ctx_t *dc  = lv_event_get_draw_ctx(e);
    lv_obj_t      *obj = lv_event_get_target(e);
    lv_area_t      a;
    if (!_lv_area_intersect(&a, dc->clip_area, &obj->coords)) return;
    lv_coord_t  stride = lv_area_get_width(dc->buf_area);
    lv_color_t *buf    = dc->buf;
    size_t      n      = (size_t)lv_area_get_width(&a) * sizeof(lv_color_t);
    for (lv_coord_t y = a.y1; y <= a.y2; y++)
        memset(buf + (size_t)(y - dc->buf_area->y1) * stride + (a.x1 - dc->buf_area->x1), 0, n);
}

static void fill_black(void) {
    lv_color_t black = lv_color_black();
    for (size_t i = 0; i < (size_t)wf.w * 2 * wf.n; i++) wf.ring[i] = black;
}

bool js8_wf_create(lv_obj_t *box, const lv_color_t *palette, int min, int max, int row_period_us, bool smooth) {
    lv_obj_update_layout(box);
    wf.w    = lv_obj_get_width(box);
    wf.h    = lv_obj_get_height(box);
    wf.n    = wf.h + RING_EXTRA;
    wf.ring = malloc((size_t)wf.w * 2 * wf.n * sizeof(lv_color_t));
    if (!wf.ring) return false;
    wf.smooth    = smooth;
    wf.off       = 0;
    wf.off_us    = mono_us();
    wf.hold_off  = -1;
    wf.period_us = row_period_us > 0 ? row_period_us : 66667;
    wf.shown_q   = -1;
    wf.box     = box;
    wf.palette = palette;
    wf.min     = (float)min;
    wf.max     = (float)max;
    wf.head    = 0;
    wf.rows    = 0;
    wf.dirty   = true;
    wf.shown   = false;
    wf.again   = 3; /* opening JS8 retuned: the main screen redraws this frame */
    fill_black();
    lv_obj_set_style_bg_opa(box, LV_OPA_TRANSP, 0);
    lv_obj_add_event_cb(box, box_cover_cb, LV_EVENT_COVER_CHECK, NULL);
    lv_obj_add_event_cb(box, box_draw_cb, LV_EVENT_DRAW_MAIN | LV_EVENT_PREPROCESS, NULL);
    lv_obj_invalidate(box);
    return true;
}

void js8_wf_destroy(void) {
    if (!wf.box) return;
    black_out();
    free(wf.ring);
    memset(&wf, 0, sizeof(wf));
}

void js8_wf_add_row(const float *data, uint16_t cnt) {
    if (!wf.ring || !cnt) return;
    wf.head = (wf.head + wf.n - 1) % wf.n;
    wf.rows++;
    /* The new row starts just above the top and glides in (smooth); more
     * than RING_EXTRA rows behind, it jumps. */
    glide(mono_us());
    if (wf.smooth && wf.hold_off < 0) {
        wf.off += 1;
        if (wf.off > RING_EXTRA - 0.01) wf.off = RING_EXTRA - 0.01;
    }
    for (int x = 0; x < wf.w; x++) {
        float v = (data[(uint32_t)x * cnt / wf.w] - wf.min) / (wf.max - wf.min);
        if (v < 0.0f) v = 0.0f;
        else if (v > 1.0f) v = 1.0f;
        lv_color_t *col = wf.ring + (size_t)x * 2 * wf.n;
        col[wf.head] = col[wf.head + wf.n] = wf.palette[(uint8_t)(v * 255)];
    }
    wf.dirty = true;
}

void js8_wf_fill_rect(int x1, int y1, int x2, int y2, lv_color_t color) {
    if (!wf.ring) return;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > wf.w - 1) x2 = wf.w - 1;
    if (y2 > wf.h - 1) y2 = wf.h - 1;
    if (x1 > x2 || y1 > y2) return;
    for (int x = x1; x <= x2; x++) {
        lv_color_t *col = wf.ring + (size_t)x * 2 * wf.n;
        for (int y = y1; y <= y2; y++) {
            int i  = (wf.head + y) % wf.n;
            col[i] = col[i + wf.n] = color;
        }
    }
    wf.dirty = true;
}

void js8_wf_clear(void) {
    if (!wf.ring) return;
    fill_black();
    wf.dirty = true;
}

void js8_wf_repaint_soon(void) {
    __atomic_store_n(&wf.again, 3, __ATOMIC_RELAXED);
}

uint32_t js8_wf_rows(void) {
    return wf.rows;
}

void js8_wf_set_smooth(bool on) {
    wf.smooth = on;
    wf.dirty  = true;
}

void js8_wf_hold_offset(double rows) {
    wf.hold_off = rows < 0 ? -1 : rows > RING_EXTRA - 0.01 ? RING_EXTRA - 0.01 : rows;
}

void js8_wf_tick(void) {
    if (!wf.ring) return;
    /* Hidden (under the map): nothing to show, and the map covers it. */
    if (!lv_obj_is_visible(wf.box)) {
        wf.shown = false;
        return;
    }
    int64_t now   = mono_us();
    int     again = __atomic_load_n(&wf.again, __ATOMIC_RELAXED);
    glide(now);
    int q = (int)(wf.off * 256 + 0.5);
    if (!wf.dirty && wf.shown && !again && q == wf.shown_q && now - wf.last_us < KEEPALIVE_US) return;
    if (wf.shown && now - wf.last_us < MIN_PUT_US) return; /* this pass has one already */
    if (!present(q)) return;
    wf.shown_q = q;
    /* The main screen's redraw comes after this timer in the pass it
     * happens, so the next passes put it right; counted by time, as this
     * runs twice in a pass that adds a row. */
    if (again && now - wf.again_us >= 4000) {
        __atomic_store_n(&wf.again, again - 1, __ATOMIC_RELAXED);
        wf.again_us = now;
    }
    wf.dirty   = false;
    wf.shown   = true;
    wf.last_us = now;
}
