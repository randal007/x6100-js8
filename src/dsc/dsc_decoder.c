/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * Copyright (C) 2026 Hany El Imam - 1KO125 firmware for the Xiegu X6100
 *
 * Additional term under section 7(b) of the GNU GPL version 3: any work that
 * contains this file, or a work based on it, must preserve the author
 * attribution "Hany El Imam" and the credits of the WEFAX, NAVTEX/SITOR-B,
 * DSC and SSTV applications in the Appropriate Legal Notices displayed by
 * its user interface (in this program: the Info window of each application).
 * See NOTICE-1KO125.md.
 */
/*
 * Xiegu X6100 - DSC (Digital Selective Calling) decoder, MF/HF
 * See dsc_decoder.h.
 */
#include "dsc_decoder.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DSC_BAUD          100.0f
#define DSC_SHIFT_HZ      170.0f
#define DSC_CHANNELS      9            /* demodulators: centre -50 .. +50 Hz */
#define DSC_CH_STEP_HZ    12.5f
#define DSC_LPF_HZ        65.0f        /* tone envelope low-pass */
#define DSC_RAW_MAX       220          /* character positions collected per call */
#define DSC_END_INVALID   10           /* consecutive bad characters: end of call */
#define DSC_DEDUP_SEC     3.5f         /* calls decoded by several demodulators */
#define DSC_HISTORY       8            /* recently emitted calls (late duplicates) */
#define DSC_HISTORY_SEC   30.0
#define DSC_RESYNC_FROM   18           /* raw positions after which a new phasing means a new call */
#ifndef DSC_EDGE_WINDOW
#define DSC_EDGE_WINDOW   0.3f         /* edges farther from the expected boundary are ignored (bits) */
#endif
#define DSC_EDGE_CONFIRM  0.3f         /* swing past the zero crossing that confirms a bit edge */
#ifndef DSC_CLOCK_GAIN
#define DSC_CLOCK_GAIN    0.25f        /* bit clock correction per edge */
#endif

typedef struct { float b0, b1, b2, a1, a2, z1, z2; } biquad_t;

static void biquad_lp(biquad_t *q, float fs, float fc)
{
    float w = 2.0f * (float)M_PI * fc / fs, c = cosf(w), s = sinf(w);
    float alpha = s / (2.0f * 0.70710678f), a0 = 1.0f + alpha;
    q->b0 = (1.0f - c) * 0.5f / a0; q->b1 = (1.0f - c) / a0; q->b2 = q->b0;
    q->a1 = -2.0f * c / a0; q->a2 = (1.0f - alpha) / a0; q->z1 = q->z2 = 0.0f;
}

static inline float biquad_run(biquad_t *q, float x)
{
    float y = q->b0 * x + q->z1;
    q->z1 = q->b1 * x - q->a1 * y + q->z2;
    q->z2 = q->b2 * x - q->a2 * y;
    return y;
}

typedef struct {
    float re, im, dre, dim;     /* oscillator and its per-sample rotation */
    biquad_t li[2], lq[2];      /* two cascaded sections: 4th order */
} tone_t;

typedef enum { ST_SEARCH = 0, ST_COLLECT } ch_state_t;

typedef struct {
    float center;
    tone_t lo, hi;
    float prev_d;
    float phase;                /* bit clock, 0..1, boundary at 0 */
    int   level;                /* confirmed discriminator sign, +1 / -1 */
    bool  cross;                /* zero crossing seen, not yet confirmed */
    float cross_phase;          /* clock phase at that crossing */
    float acc;                  /* integral of the discriminator over the bit centre */
    uint64_t bits;              /* last 64 bits, newest in bit 0 */
    int nbits;
    ch_state_t state;
    int bitcnt;                 /* bits since the last character boundary */
    int raw[DSC_RAW_MAX];
    int nraw;
    int bad_run;
    int valid;
    double lock_t;              /* time of the phasing lock */
} channel_t;

struct dsc_decoder {
    float fs, sp;               /* sample rate, samples per bit */
    channel_t ch[DSC_CHANNELS];
    dsc_message_cb_t cb;
    void *user;
    dsc_message_t pending;
    bool has_pending;
    dsc_message_t hist[DSC_HISTORY];
    double hist_t[DSC_HISTORY];
    int hist_n, hist_i;
    double pending_t, now;
    unsigned osc_count;
};

static void tone_init(tone_t *t, float fs, float f)
{
    t->re = 1.0f; t->im = 0.0f;
    t->dre = cosf(-2.0f * (float)M_PI * f / fs); t->dim = sinf(-2.0f * (float)M_PI * f / fs);
    for (int k = 0; k < 2; ++k) { biquad_lp(&t->li[k], fs, DSC_LPF_HZ); biquad_lp(&t->lq[k], fs, DSC_LPF_HZ); }
}

static inline float tone_run(tone_t *t, float x)
{
    float i = x * t->re, q = x * t->im;
    float nr = t->re * t->dre - t->im * t->dim;
    t->im = t->re * t->dim + t->im * t->dre;
    t->re = nr;
    i = biquad_run(&t->li[1], biquad_run(&t->li[0], i));
    q = biquad_run(&t->lq[1], biquad_run(&t->lq[0], q));
    return sqrtf(i * i + q * q);
}

static void tone_renorm(tone_t *t)
{
    float m = sqrtf(t->re * t->re + t->im * t->im);
    if (m > 0.0f) { t->re /= m; t->im /= m; }
}

/* Character from the 10 bits in transmission order b[0..9]; -1 if the
 * check fails. */
static int char_from_bits(uint64_t bits)
{
    /* bits: newest in bit 0, so b[k] (time order) is bit (9 - k) */
    int v = 0, ones = 0;
    for (int k = 0; k < 7; ++k) {
        int b = (int)((bits >> (9 - k)) & 1u);
        v |= b << k;
        ones += b;
    }
    int chk = (int)(((bits >> 2) & 1u) << 2 | ((bits >> 1) & 1u) << 1 | (bits & 1u));
    return (7 - ones) == chk ? v : -1;
}

static void channel_reset(channel_t *c)
{
    c->state = ST_SEARCH;
    c->nraw = 0;
    c->bad_run = 0;
    c->valid = 0;
    c->bitcnt = 0;
}

static void channel_init(channel_t *c, float fs, float center)
{
    memset(c, 0, sizeof(*c));
    c->center = center;
    tone_init(&c->lo, fs, center - DSC_SHIFT_HZ / 2.0f);
    tone_init(&c->hi, fs, center + DSC_SHIFT_HZ / 2.0f);
    c->level = 1;
    channel_reset(c);
}

/* ---- assembly of a collected call ---------------------------------- */

static bool is_rx_phasing(int v) { return v >= 104 && v <= 111; }
static bool is_format(int v) { return v == 102 || v == 112 || v == 114 || v == 116 || v == 120 || v == 123; }

static bool assemble(const channel_t *c, dsc_message_t *m)
{
    const int *r = c->raw;
    int n = c->nraw;

    /* Phasing: DX 125 alternates with RX 111..104.  An RX phasing
     * character v at raw index i puts the first DX format specifier at
     * i + 1 + 2 * (v - 106); take the most voted position. */
    int votes[DSC_RAW_MAX];
    memset(votes, 0, sizeof(votes));
    for (int i = 0; i < n && i < 40; ++i) {
        if (!is_rx_phasing(r[i])) continue;
        int s = i + 1 + 2 * (r[i] - 106);
        if (s >= 0 && s < n) votes[s] += 2;
    }
    /* DX phasing 125 followed (2 later) by something that is not 125:
     * that something is the first format specifier */
    for (int i = 0; i + 2 < n && i < 40; ++i)
        if (r[i] == 125 && r[i + 2] != 125) votes[i + 2] += 1;
    int start = -1, bv = 0;
    for (int i = 0; i < n && i < 50; ++i)
        if (votes[i] > bv) { bv = votes[i]; start = i; }
    if (start < 0) return false;

    int dx[DSC_RAW_MAX], rx[DSC_RAW_MAX], ndx = 0, nrx = 0;
    for (int i = start; i < n; i += 2) dx[ndx++] = r[i];
    for (int i = start + 1; i < n; i += 2) rx[nrx++] = r[i];

    /* RX repeats DX a few positions later (2 for M.493): measure it */
    int best_l = 2, best_m = -1;
    for (int l = 0; l <= 6; ++l) {
        int mt = 0;
        for (int i = 0; i < ndx && i + l < nrx; ++i)
            if (dx[i] >= 0 && dx[i] == rx[i + l] && !is_rx_phasing(rx[i + l])) mt++;
        if (mt > best_m) { best_m = mt; best_l = l; }
    }

    memset(m, 0, sizeof(*m));
    for (int i = 0; i < ndx && m->n < DSC_MAX_SYMBOLS; ++i) {
        int v = dx[i];
        if (v < 0 && i + best_l < nrx && rx[i + best_l] >= 0 && !is_rx_phasing(rx[i + best_l])) {
            v = rx[i + best_l];
            m->repaired++;
        }
        m->sym[m->n++] = v;
    }
    while (m->n > 0 && m->sym[m->n - 1] < 0) m->n--;
    /* a wrong start can leave phasing characters (125, 104..111) in front
     * of the format specifiers: restart after them */
    int shift = 0;
    for (int i = 13; i >= 1; --i) {
        if (i + 1 < m->n && is_format(m->sym[i]) && m->sym[i + 1] == m->sym[i] &&
            (m->sym[i - 1] == 125 || is_rx_phasing(m->sym[i - 1]))) {
            memmove(m->sym, m->sym + i, (size_t)(m->n - i) * sizeof(m->sym[0]));
            m->n -= i;
            shift = i;
            break;
        }
    }
    /* Quality: valid characters over the useful part of the reception,
     * from the first collected one to the RX copy of the error check
     * character (after the end of sequence).  The tail of noise that the
     * collector keeps until it gives up is not counted. */
    int last = m->n - 1;
    for (int e = 12; e < m->n; ++e)
        if (m->sym[e] == 117 || m->sym[e] == 122 || m->sym[e] == 127) { last = e + 1; break; }
    int end = start + 2 * (last + shift + best_l) + 1;
    if (end > n - 1) end = n - 1;
    m->raw_total = end + 1;
    m->raw_valid = 0;
    for (int i = 0; i <= end; ++i) if (r[i] >= 0) m->raw_valid++;
    m->center_hz = c->center;

    /* error check character: XOR of the format specifier (once) up to and
     * including the end of sequence, which follows the fixed fields */
    m->ecc_ok = false;
    for (int e = 12; e + 1 < m->n; ++e) {
        int v = m->sym[e];
        if (v != 117 && v != 122 && v != 127) continue;
        /* the format specifier counts once.  Candidates: message after
         * the second copy (index 2) or, when only one copy was collected,
         * at index 1; when the two copies are different valid formats,
         * either may be the right one. */
        int f0 = is_format(m->sym[0]) ? m->sym[0] : -1;
        int f1 = is_format(m->sym[1]) ? m->sym[1] : -1;
        struct { int f, st; } cand[3] = {
            { f0 >= 0 ? f0 : f1, 2 },
            { f1 >= 0 && f1 != f0 && f0 >= 0 ? f1 : -1, 2 },
            { f1 != f0 ? f0 : -1, 1 },
        };
        for (int k = 0; k < 3 && !m->ecc_ok; ++k) {
            int x = cand[k].f;
            bool all = x >= 0;
            for (int i = cand[k].st; i <= e && all; ++i) {
                if (m->sym[i] < 0) all = false; else x ^= m->sym[i];
            }
            if (all && m->sym[e + 1] == x) { m->ecc_ok = true; m->n = e + 2; }
        }
        if (m->ecc_ok) break;
    }
    return m->n >= 4;
}

static int known_count(const dsc_message_t *m);

static bool worth_emitting(const dsc_message_t *m)
{
    if (m->ecc_ok) return true;
    int f = m->sym[0] >= 0 ? m->sym[0] : (m->n > 1 ? m->sym[1] : -1);
    bool fmt_ok = f == 102 || f == 112 || f == 114 || f == 116 || f == 120 || f == 123;
    return fmt_ok && m->n >= 12 && known_count(m) * 100 >= m->n * 60;
}

/*
 * The same call seen by another demodulator, possibly assembled one or two
 * characters off: format, both addresses and category (the first 13
 * characters) agree, at most one of them read differently.  Two calls of
 * the same station to different ships differ in several address digits.
 */
static bool same_call(const dsc_message_t *a, const dsc_message_t *b)
{
    for (int sh = -2; sh <= 2; ++sh) {
        int cmp = 0, diff = 0;
        for (int i = 0; i < 13; ++i) {
            int j = i + sh;
            if (i >= a->n || j < 0 || j >= b->n) continue;
            if (a->sym[i] >= 0 && b->sym[j] >= 0) { cmp++; if (a->sym[i] != b->sym[j]) diff++; }
        }
        if (cmp >= 8 && diff <= 1) return true;
    }
    return false;
}

static bool better(const dsc_message_t *a, const dsc_message_t *b)
{
    if (a->ecc_ok != b->ecc_ok) return a->ecc_ok;
    return known_count(a) > known_count(b);
}

static int known_count(const dsc_message_t *m)
{
    int k = 0;
    for (int i = 0; i < m->n; ++i) if (m->sym[i] >= 0) k++;
    return k;
}

static void emit_pending(dsc_decoder_t *d)
{
    if (!d->has_pending) return;
    d->has_pending = false;
    if (!worth_emitting(&d->pending)) return;
    /* a late copy of a call already shown (a demodulator that locked late) */
    for (int k = 0; k < d->hist_n; ++k)
        if (d->now - d->hist_t[k] < DSC_HISTORY_SEC && same_call(&d->pending, &d->hist[k]) &&
            !better(&d->pending, &d->hist[k]))
            return;
    d->hist[d->hist_i] = d->pending;
    d->hist_t[d->hist_i] = d->now;
    d->hist_i = (d->hist_i + 1) % DSC_HISTORY;
    if (d->hist_n < DSC_HISTORY) d->hist_n++;
    if (d->cb) d->cb(&d->pending, d->user);
}

static void call_finished(dsc_decoder_t *d, channel_t *c)
{
    dsc_message_t m;
    if (!assemble(c, &m)) return;
    m.start_t = c->lock_t;
    if (d->has_pending && (fabs(m.start_t - d->pending.start_t) < 1.0 || same_call(&m, &d->pending))) {
        if (better(&m, &d->pending)) d->pending = m;     /* same call, other demodulator */
        d->pending_t = d->now;
        if (d->pending.ecc_ok) emit_pending(d);         /* verified: nothing better can come */
        return;
    }
    emit_pending(d);
    if (!worth_emitting(&m)) return;
    d->pending = m;
    d->has_pending = true;
    d->pending_t = d->now;
    /* verified by its error check: nothing better can come from the other
     * demodulators (their copies are then dropped by the history) */
    if (m.ecc_ok) emit_pending(d);
}

/* end of sequence characters among the last few collected */
static bool eos_seen(const channel_t *c)
{
    for (int i = c->nraw - 1; i >= 0 && i >= c->nraw - 8; --i)
        if (c->raw[i] == 117 || c->raw[i] == 122 || c->raw[i] == 127) return true;
    return false;
}

/*
 * Two consecutive phasing pairs, DX 125 with RX phasing counting down
 * (125, v, 125, v-1 or v, 125, v-1, 125), at any bit position: the start
 * of a new call.  40 bits with this structure do not occur by chance.
 */
static int new_phasing(uint64_t bits, int *a, int *b, int *c2, int *d2)
{
    *a = char_from_bits(bits >> 30);
    *b = char_from_bits(bits >> 20);
    *c2 = char_from_bits(bits >> 10);
    *d2 = char_from_bits(bits);
    if (*a == 125 && *c2 == 125 && is_rx_phasing(*b) && *d2 == *b - 1) return 1;
    if (*b == 125 && *d2 == 125 && is_rx_phasing(*a) && *c2 == *a - 1) return 1;
    return 0;
}

static void channel_bit(dsc_decoder_t *d, channel_t *c, int bit)
{
    c->bits = (c->bits << 1) | (uint64_t)(bit & 1);
    if (c->nbits < 64) c->nbits++;

    /* A new call starting while the previous one is still being collected
     * (its noisy tail, or a weak call without a readable end): close the
     * old one with what it has and lock on the new one. */
    if (c->state == ST_COLLECT && c->nraw >= DSC_RESYNC_FROM && c->nbits >= 40) {
        int a, b, c2, d2;
        if (new_phasing(c->bits, &a, &b, &c2, &d2)) {
            call_finished(d, c);
            c->state = ST_COLLECT;
            c->nraw = 0;
            c->raw[c->nraw++] = a;
            c->raw[c->nraw++] = b;
            c->raw[c->nraw++] = c2;
            c->raw[c->nraw++] = d2;
            c->valid = 4;
            c->bad_run = 0;
            c->bitcnt = 0;
            c->lock_t = d->now;
            return;
        }
    }

    if (c->state == ST_SEARCH) {
        if (c->nbits < 20) return;
        int a = char_from_bits(c->bits >> 10), b = char_from_bits(c->bits);
        if ((a == 125 && is_rx_phasing(b)) || (is_rx_phasing(a) && (b == 125 || is_format(b)))) {
            c->state = ST_COLLECT;
            c->nraw = 0;
            c->raw[c->nraw++] = a;
            c->raw[c->nraw++] = b;
            c->valid = 2;
            c->bad_run = 0;
            c->bitcnt = 0;
            c->lock_t = d->now;
        }
        return;
    }

    if (++c->bitcnt < 10) return;
    c->bitcnt = 0;
    int v = char_from_bits(c->bits);
    if (c->nraw < DSC_RAW_MAX) c->raw[c->nraw++] = v;
    if (v >= 0) { c->valid++; c->bad_run = 0; } else c->bad_run++;
    bool done = c->bad_run >= DSC_END_INVALID || c->nraw >= DSC_RAW_MAX;
    /* a call whose error check is already verified ends here */
    if (!done && v >= 0 && c->nraw >= 30 && eos_seen(c)) {
        dsc_message_t m;
        if (assemble(c, &m) && m.ecc_ok) done = true;
    }
    if (done) {
        call_finished(d, c);
        channel_reset(c);
        c->nbits = 0;
    }
}

dsc_decoder_t *dsc_decoder_create(int sample_rate, dsc_message_cb_t cb, void *user)
{
    if (sample_rate < 8000) return NULL;
    dsc_decoder_t *d = calloc(1, sizeof(*d));
    if (!d) return NULL;
    d->fs = (float)sample_rate;
    d->sp = d->fs / DSC_BAUD;
    d->cb = cb;
    d->user = user;
    for (int k = 0; k < DSC_CHANNELS; ++k)
        channel_init(&d->ch[k], d->fs,
                     DSC_CENTER_HZ + ((float)k - (DSC_CHANNELS - 1) / 2.0f) * DSC_CH_STEP_HZ);
    return d;
}

void dsc_decoder_destroy(dsc_decoder_t *d)
{
    free(d);
}

void dsc_decoder_flush(dsc_decoder_t *d)
{
    if (!d) return;
    for (int k = 0; k < DSC_CHANNELS; ++k) {
        channel_t *c = &d->ch[k];
        if (c->state == ST_COLLECT) { call_finished(d, c); channel_reset(c); }
    }
    emit_pending(d);
}

void dsc_decoder_process(dsc_decoder_t *d, const float *x, int n)
{
    if (!d || !x || n <= 0) return;
    const float step = 1.0f / d->sp;
    for (int i = 0; i < n; ++i) {
        float s = x[i];
        for (int k = 0; k < DSC_CHANNELS; ++k) {
            channel_t *c = &d->ch[k];
            float lo = tone_run(&c->lo, s), hi = tone_run(&c->hi, s);
            float sum = lo + hi;
            float dv = sum > 1e-9f ? (lo - hi) / sum : 0.0f;   /* > 0: Y ("1") */

            /* Bit clock.  A tone change is a smooth ramp of the filtered
             * discriminator: its zero crossing marks the bit boundary
             * (phase 0).  The crossing counts only when the signal then
             * swings clearly to the other side, so noise wiggles around
             * zero do not move the clock. */
            int sg = dv > 0.0f ? 1 : -1;
            if (!c->cross) {
                if (sg != c->level) { c->cross = true; c->cross_phase = c->phase; }
            } else if (sg == c->level) {
                c->cross = false;                       /* back: it was noise */
            } else if (fabsf(dv) > DSC_EDGE_CONFIRM) {
                float e = c->cross_phase < 0.5f ? c->cross_phase : c->cross_phase - 1.0f;
                /* only edges near the expected boundary move the clock:
                 * a crossing in the middle of a bit is noise or a fade */
                if (fabsf(e) < DSC_EDGE_WINDOW) c->phase -= DSC_CLOCK_GAIN * e;
                c->level = sg;
                c->cross = false;
            }
            c->prev_d = dv;

            float before = c->phase;
            c->phase += step;
            /* integrate over the central 60% of the bit, decide at 0.8 */
            if (c->phase > 0.2f && c->phase <= 0.8f) c->acc += dv;
            if (before < 0.8f && c->phase >= 0.8f) {
                channel_bit(d, c, c->acc > 0.0f ? 1 : 0);
                c->acc = 0.0f;
            }
            if (c->phase >= 1.0f) c->phase -= 1.0f;
            if (c->phase < 0.0f) c->phase += 1.0f;
        }
    }
    if ((d->osc_count += (unsigned)n) >= 4096) {
        d->osc_count = 0;
        for (int k = 0; k < DSC_CHANNELS; ++k) { tone_renorm(&d->ch[k].lo); tone_renorm(&d->ch[k].hi); }
    }
    d->now += (double)n / d->fs;
    if (d->has_pending && d->now - d->pending_t > DSC_DEDUP_SEC) emit_pending(d);
}
