/*
 * Standard MIDI File playback for MidiFilePlayer ("audio/midi"): an SMF
 * (format 0/1) sequencer plus a small software synthesizer, mixed into the
 * vita_mix_shim.c output through Mix_HookMusic.
 *
 * The synth approximates General MIDI with one simple oscillator voice per
 * note, picked by instrument family (piano, organ, bass, strings, ...),
 * with an attack/decay/sustain/release envelope, and synthesizes the
 * channel 10 drum kit from noise and pitch-swept sines. That is close to
 * what the phones these games were written for sounded like, with no
 * instrument sample set to bundle.
 *
 * Everything that touches song or voice state runs either in the audio
 * callback or under SDL_LockAudio.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <kni.h>
#include <SDL.h>
#include "vita_mix_shim.h"
#include <renderlog.h>
#include <stdio.h>

/* In media_vita.c: opens the audio device on first use, 1 when ready */
int MediaVita_EnsureAudio(void);

#define MS_MAX_TRACKS 64
#define MS_MAX_VOICES 40
#define MS_BLOCK      64   /* samples rendered between sequencer steps */

typedef struct {
    const unsigned char *data;
    int len;
    int pos;
    unsigned int nextTick;
    unsigned char running;
    int done;
} MsTrack;

typedef struct MsSong {
    unsigned char *buf;
    int len;
    int division;          /* ticks per quarter note, or SMPTE if < 0 */
    int ntracks;
    MsTrack tracks[MS_MAX_TRACKS];
    const unsigned char *trackStart[MS_MAX_TRACKS];
    int trackLen[MS_MAX_TRACKS];
    double tick;
    double ticksPerSample;
    int playing;
    int loops;             /* -1 forever, else extra repetitions left */
    int eom;
    int atEnd;
    int volume;            /* 0..100 */
    float gain;            /* volume on a loudness curve */
    long long durationUs;
    int notesPlayed;       /* diagnostics, logged when the player closes */
    int peak;
    int logged;
    unsigned char program[16], chVolume[16], expression[16], pan[16];
    int bend[16];          /* -8192..8191 */
    int sustain[16];
} MsSong;

enum { W_SINE, W_TRI, W_SQUARE, W_PULSE, W_SAW, W_NOISE, W_BELL };
enum { ST_OFF, ST_ATTACK, ST_DECAY, ST_SUSTAIN, ST_RELEASE };

typedef struct {
    int wave;
    float attack, decay, sustain, release; /* seconds / level */
    float gain;
} MsInstrument;

/* One entry per General MIDI family (program / 8) */
static const MsInstrument families[16] = {
    { W_TRI,    0.002f, 0.90f, 0.00f, 0.15f, 1.0f }, /* piano */
    { W_BELL,   0.001f, 0.60f, 0.00f, 0.25f, 1.4f }, /* chromatic percussion */
    { W_SQUARE, 0.005f, 0.00f, 1.00f, 0.06f, 0.5f }, /* organ */
    { W_SAW,    0.002f, 0.70f, 0.00f, 0.12f, 0.6f }, /* guitar */
    { W_TRI,    0.003f, 1.20f, 0.35f, 0.08f, 1.2f }, /* bass */
    { W_SAW,    0.060f, 0.00f, 1.00f, 0.30f, 0.5f }, /* strings */
    { W_SAW,    0.050f, 0.00f, 1.00f, 0.30f, 0.5f }, /* ensemble */
    { W_SQUARE, 0.020f, 0.30f, 0.70f, 0.10f, 0.5f }, /* brass */
    { W_PULSE,  0.020f, 0.00f, 1.00f, 0.08f, 0.5f }, /* reed */
    { W_SINE,   0.030f, 0.00f, 1.00f, 0.10f, 1.0f }, /* pipe */
    { W_SQUARE, 0.005f, 0.00f, 1.00f, 0.05f, 0.5f }, /* synth lead */
    { W_TRI,    0.150f, 0.00f, 1.00f, 0.50f, 0.8f }, /* synth pad */
    { W_SAW,    0.050f, 0.50f, 0.50f, 0.30f, 0.5f }, /* synth effects */
    { W_SAW,    0.002f, 0.50f, 0.00f, 0.10f, 0.6f }, /* ethnic */
    { W_SINE,   0.001f, 0.20f, 0.00f, 0.10f, 1.0f }, /* percussive */
    { W_NOISE,  0.010f, 0.30f, 0.00f, 0.10f, 0.4f }, /* sound effects */
};

typedef struct {
    MsSong *song;
    unsigned char ch, note;
    int stage;
    int wave;
    int drum;
    int sustained;         /* note off arrived while the pedal was down */
    unsigned int phase, inc;
    float env, attackStep, decayCoef, sustainLevel, releaseCoef;
    float amp;             /* velocity and instrument gain */
    float sweep, sweepCoef, drumFreq, drumTone, drumNoise;
    float hpPrev;
    unsigned int age;
} MsVoice;

static MsVoice voices[MS_MAX_VOICES];
#define MS_MAX_SONGS  32   /* loaded (realized) MIDI players at once */
static MsSong *songs[MS_MAX_SONGS];
static int sampleRate = 22050;
static unsigned int voiceClock;
static unsigned int noiseState = 22222;
static short sineTable[1024];
static int hooked;

/* --- SMF reading -------------------------------------------------------- */

static unsigned int read_vlq(MsTrack *t) {
    unsigned int v = 0;
    int i;
    for (i = 0; i < 4 && t->pos < t->len; i++) {
        unsigned char b = t->data[t->pos++];
        v = (v << 7) | (b & 0x7F);
        if (!(b & 0x80)) {
            break;
        }
    }
    return v;
}

static unsigned int be32(const unsigned char *p) {
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
}

static void set_tempo(MsSong *s, unsigned int usPerQuarter) {
    if (s->division > 0) {
        if (usPerQuarter == 0) {
            usPerQuarter = 500000;
        }
        s->ticksPerSample = (double)s->division * 1000000.0 / ((double)usPerQuarter * sampleRate);
    }
}

static void rewind_song(MsSong *s) {
    int i;
    for (i = 0; i < s->ntracks; i++) {
        MsTrack *t = &s->tracks[i];
        t->data = s->trackStart[i];
        t->len = s->trackLen[i];
        t->pos = 0;
        t->running = 0;
        t->done = t->len == 0;
        t->nextTick = t->done ? 0 : read_vlq(t);
    }
    s->tick = 0;
    s->atEnd = 0;
    if (s->division > 0) {
        set_tempo(s, 500000);
    } else {
        /* SMPTE: frames per second times ticks per frame */
        int fps = -(signed char)(s->division >> 8);
        int tpf = s->division & 0xFF;
        s->ticksPerSample = (double)(fps * tpf) / sampleRate;
    }
    for (i = 0; i < 16; i++) {
        s->program[i] = 0;
        s->chVolume[i] = 100;
        s->expression[i] = 127;
        s->pan[i] = 64;
        s->bend[i] = 0;
        s->sustain[i] = 0;
    }
}

static int parse_smf(MsSong *s) {
    const unsigned char *p = s->buf;
    int len = s->len, off, n;
    /* Some files carry a RIFF "RMID" wrapper around the SMF */
    if (len >= 20 && memcmp(p, "RIFF", 4) == 0) {
        int i;
        for (i = 12; i + 8 <= len; i++) {
            if (memcmp(p + i, "MThd", 4) == 0) {
                p += i;
                len -= i;
                break;
            }
        }
    }
    if (len < 14 || memcmp(p, "MThd", 4) != 0) {
        return -1;
    }
    off = 8 + (int)be32(p + 4);
    n = (p[10] << 8) | p[11];
    s->division = (short)((p[12] << 8) | p[13]);
    if (s->division == 0) {
        s->division = 96;
    }
    s->ntracks = 0;
    while (off + 8 <= len && s->ntracks < MS_MAX_TRACKS && s->ntracks < n) {
        int tlen = (int)be32(p + off + 4);
        if (memcmp(p + off, "MTrk", 4) == 0) {
            if (tlen > len - off - 8) {
                tlen = len - off - 8;
            }
            s->trackStart[s->ntracks] = p + off + 8;
            s->trackLen[s->ntracks] = tlen;
            s->ntracks++;
        }
        off += 8 + tlen;
    }
    return s->ntracks > 0 ? 0 : -1;
}

/* Walks the whole song once to find its length in microseconds */
static long long scan_duration(MsSong *s) {
    long long us = 0;
    unsigned int lastTick = 0, tempo = 500000;
    rewind_song(s);
    for (;;) {
        int i, best = -1;
        for (i = 0; i < s->ntracks; i++) {
            if (!s->tracks[i].done && (best < 0 || s->tracks[i].nextTick < s->tracks[best].nextTick)) {
                best = i;
            }
        }
        if (best < 0) {
            break;
        }
        {
            MsTrack *t = &s->tracks[best];
            unsigned char st;
            if (s->division > 0) {
                us += (long long)(t->nextTick - lastTick) * tempo / s->division;
            }
            lastTick = t->nextTick;
            if (t->pos >= t->len) {
                t->done = 1;
                continue;
            }
            st = t->data[t->pos];
            if (st & 0x80) {
                t->pos++;
                if (st < 0xF0) {
                    t->running = st;
                }
            } else {
                st = t->running;
            }
            if (st == 0xFF) {
                unsigned char type = t->pos < t->len ? t->data[t->pos++] : 0x2F;
                unsigned int l = read_vlq(t);
                if (type == 0x51 && l == 3 && t->pos + 3 <= t->len) {
                    tempo = ((unsigned int)t->data[t->pos] << 16) | (t->data[t->pos + 1] << 8) | t->data[t->pos + 2];
                }
                t->pos += l;
                if (type == 0x2F) {
                    t->done = 1;
                }
            } else if (st == 0xF0 || st == 0xF7) {
                t->pos += read_vlq(t);
            } else if ((st & 0xE0) == 0xC0) {
                t->pos += 1;
            } else if (st >= 0x80) {
                t->pos += 2;
            } else {
                t->done = 1; /* data byte with no running status */
            }
            if (!t->done) {
                if (t->pos >= t->len) {
                    t->done = 1;
                } else {
                    t->nextTick += read_vlq(t);
                }
            }
        }
    }
    if (s->division < 0) {
        int fps = -(signed char)(s->division >> 8);
        int tpf = s->division & 0xFF;
        us = (long long)lastTick * 1000000 / (fps * tpf > 0 ? fps * tpf : 1);
    }
    return us;
}

/* --- Voices ------------------------------------------------------------- */

static float note_freq(int note, int bend) {
    return 440.0f * powf(2.0f, ((float)note - 69.0f + (float)bend * (2.0f / 8192.0f)) / 12.0f);
}

static unsigned int freq_to_inc(float f) {
    double inc = (double)f * 4294967296.0 / sampleRate;
    if (inc > 2147483647.0) {
        inc = 2147483647.0;
    }
    return (unsigned int)inc;
}

static float coef_for(float seconds) {
    if (seconds <= 0.0f) {
        return 0.0f;
    }
    /* Exponential decay reaching about 1/1000 after the given time */
    return expf(-6.9f / (seconds * sampleRate));
}

static MsVoice *alloc_voice(void) {
    int i, oldest = 0;
    for (i = 0; i < MS_MAX_VOICES; i++) {
        if (voices[i].stage == ST_OFF) {
            return &voices[i];
        }
    }
    /* Steal: prefer the oldest released voice, else the oldest */
    for (i = 1; i < MS_MAX_VOICES; i++) {
        int better = (voices[i].stage == ST_RELEASE) - (voices[oldest].stage == ST_RELEASE);
        if (better > 0 || (better == 0 && voices[i].age < voices[oldest].age)) {
            oldest = i;
        }
    }
    return &voices[oldest];
}

static void release_voice(MsVoice *v) {
    if (v->stage != ST_OFF && v->stage != ST_RELEASE) {
        v->stage = ST_RELEASE;
    }
}

static void note_on_drum(MsSong *s, int note, int vel) {
    MsVoice *v = alloc_voice();
    float decay;
    s->notesPlayed++;
    memset(v, 0, sizeof(*v));
    v->song = s;
    v->ch = 9;
    v->note = (unsigned char)note;
    v->drum = 1;
    v->age = ++voiceClock;
    v->amp = vel / 127.0f;
    v->env = 1.0f;
    v->stage = ST_DECAY;
    v->sustainLevel = 0.0f;
    v->sweep = 1.0f;
    v->sweepCoef = 1.0f;
    switch (note) {
    case 35: case 36:                               /* kick */
        v->drumFreq = 150.0f; v->drumTone = 1.0f; v->drumNoise = 0.05f;
        v->sweepCoef = coef_for(0.08f); decay = 0.25f; v->amp *= 1.3f;
        break;
    case 38: case 40:                               /* snare */
        v->drumFreq = 190.0f; v->drumTone = 0.35f; v->drumNoise = 0.8f;
        decay = 0.15f;
        break;
    case 37: case 39:                               /* side stick, clap */
        v->drumFreq = 400.0f; v->drumTone = 0.2f; v->drumNoise = 0.7f;
        decay = 0.08f;
        break;
    case 42: case 44:                               /* closed/pedal hi-hat */
        v->drumNoise = 0.45f; v->wave = 1; decay = 0.05f;
        break;
    case 46:                                        /* open hi-hat */
        v->drumNoise = 0.45f; v->wave = 1; decay = 0.30f;
        break;
    case 49: case 52: case 55: case 57:             /* crashes */
        v->drumNoise = 0.5f; v->wave = 1; decay = 0.9f;
        break;
    case 51: case 53: case 59:                      /* rides */
        v->drumNoise = 0.3f; v->wave = 1; decay = 0.5f;
        break;
    case 41: case 43: case 45: case 47: case 48: case 50: /* toms */
        v->drumFreq = 70.0f + (note - 41) * 18.0f; v->drumTone = 1.0f; v->drumNoise = 0.1f;
        v->sweepCoef = coef_for(0.3f); decay = 0.3f;
        break;
    default:
        v->drumFreq = 300.0f + (note - 35) * 10.0f; v->drumTone = 0.4f; v->drumNoise = 0.5f;
        decay = 0.12f;
        break;
    }
    v->decayCoef = coef_for(decay);
    v->releaseCoef = v->decayCoef;
    v->inc = freq_to_inc(v->drumFreq);
}

static void note_on(MsSong *s, int ch, int note, int vel) {
    const MsInstrument *ins;
    MsVoice *v;
    int i;
    if (ch == 9) {
        note_on_drum(s, note, vel);
        return;
    }
    /* Retrigger rather than stack the same note */
    for (i = 0; i < MS_MAX_VOICES; i++) {
        if (voices[i].stage != ST_OFF && voices[i].song == s && voices[i].ch == ch &&
            voices[i].note == note && !voices[i].drum) {
            voices[i].stage = ST_OFF;
        }
    }
    s->notesPlayed++;
    ins = &families[s->program[ch] >> 3];
    v = alloc_voice();
    memset(v, 0, sizeof(*v));
    v->song = s;
    v->ch = (unsigned char)ch;
    v->note = (unsigned char)note;
    v->wave = ins->wave;
    v->age = ++voiceClock;
    v->amp = (vel / 127.0f) * ins->gain;
    v->inc = freq_to_inc(note_freq(note, s->bend[ch]));
    v->attackStep = ins->attack > 0.0f ? 1.0f / (ins->attack * sampleRate) : 1.0f;
    v->decayCoef = coef_for(ins->decay);
    v->sustainLevel = ins->sustain;
    v->releaseCoef = coef_for(ins->release);
    v->env = 0.0f;
    v->stage = ST_ATTACK;
}

static void note_off(MsSong *s, int ch, int note) {
    int i;
    if (ch == 9) {
        return; /* drums run their own decay */
    }
    for (i = 0; i < MS_MAX_VOICES; i++) {
        MsVoice *v = &voices[i];
        if (v->stage != ST_OFF && v->stage != ST_RELEASE && v->song == s && v->ch == ch && v->note == note) {
            if (s->sustain[ch]) {
                v->sustained = 1;
            } else {
                release_voice(v);
            }
        }
    }
}

static void release_song_voices(MsSong *s, int hard) {
    int i;
    for (i = 0; i < MS_MAX_VOICES; i++) {
        if (voices[i].song == s && voices[i].stage != ST_OFF) {
            if (hard) {
                voices[i].stage = ST_OFF;
            } else {
                release_voice(&voices[i]);
            }
        }
    }
}

static void update_bend(MsSong *s, int ch) {
    int i;
    for (i = 0; i < MS_MAX_VOICES; i++) {
        MsVoice *v = &voices[i];
        if (v->stage != ST_OFF && v->song == s && v->ch == ch && !v->drum) {
            v->inc = freq_to_inc(note_freq(v->note, s->bend[ch]));
        }
    }
}

/* --- Sequencer ---------------------------------------------------------- */

static void do_event(MsSong *s, MsTrack *t) {
    unsigned char st, d1 = 0, d2 = 0;
    int ch;
    if (t->pos >= t->len) {
        t->done = 1;
        return;
    }
    st = t->data[t->pos];
    if (st & 0x80) {
        t->pos++;
        if (st < 0xF0) {
            t->running = st;
        }
    } else {
        st = t->running;
        if (st == 0) {
            t->done = 1;
            return;
        }
    }
    if (st == 0xFF) {
        unsigned char type = t->pos < t->len ? t->data[t->pos++] : 0x2F;
        unsigned int l = read_vlq(t);
        if (type == 0x51 && l == 3 && t->pos + 3 <= t->len) {
            set_tempo(s, ((unsigned int)t->data[t->pos] << 16) | (t->data[t->pos + 1] << 8) | t->data[t->pos + 2]);
        }
        t->pos += l;
        if (type == 0x2F) {
            t->done = 1;
        }
        return;
    }
    if (st == 0xF0 || st == 0xF7) {
        t->pos += read_vlq(t);
        return;
    }
    if (st >= 0xF0) {
        return;
    }
    ch = st & 0x0F;
    if (t->pos < t->len) {
        d1 = t->data[t->pos++] & 0x7F;
    }
    if ((st & 0xE0) != 0xC0 && t->pos < t->len) {
        d2 = t->data[t->pos++] & 0x7F;
    }
    switch (st & 0xF0) {
    case 0x90:
        if (d2 != 0) {
            note_on(s, ch, d1, d2);
            break;
        }
        /* fall through: velocity 0 is note off */
    case 0x80:
        note_off(s, ch, d1);
        break;
    case 0xB0:
        switch (d1) {
        case 7:   s->chVolume[ch] = d2; break;
        case 10:  s->pan[ch] = d2; break;
        case 11:  s->expression[ch] = d2; break;
        case 64:
            s->sustain[ch] = d2 >= 64;
            if (!s->sustain[ch]) {
                int i;
                for (i = 0; i < MS_MAX_VOICES; i++) {
                    if (voices[i].song == s && voices[i].ch == ch && voices[i].sustained) {
                        voices[i].sustained = 0;
                        release_voice(&voices[i]);
                    }
                }
            }
            break;
        case 120: case 123: {
            int i;
            for (i = 0; i < MS_MAX_VOICES; i++) {
                if (voices[i].song == s && voices[i].ch == ch) {
                    release_voice(&voices[i]);
                }
            }
            break;
        }
        case 121:
            s->chVolume[ch] = 100; s->expression[ch] = 127; s->pan[ch] = 64;
            s->bend[ch] = 0; s->sustain[ch] = 0;
            break;
        }
        break;
    case 0xC0:
        s->program[ch] = d1;
        break;
    case 0xE0:
        s->bend[ch] = ((d2 << 7) | d1) - 8192;
        update_bend(s, ch);
        break;
    }
}

/* Plays every event due by the song's current tick */
static void sequence(MsSong *s) {
    int i, alive = 0;
    for (i = 0; i < s->ntracks; i++) {
        MsTrack *t = &s->tracks[i];
        while (!t->done && (double)t->nextTick <= s->tick) {
            do_event(s, t);
            if (!t->done) {
                if (t->pos >= t->len) {
                    t->done = 1;
                } else {
                    t->nextTick += read_vlq(t);
                }
            }
        }
        if (!t->done) {
            alive = 1;
        }
    }
    if (!alive) {
        if (s->loops != 0) {
            if (s->loops > 0) {
                s->loops--;
            }
            release_song_voices(s, 0);
            rewind_song(s);
        } else {
            s->playing = 0;
            s->atEnd = 1;
            s->eom = 1;
            release_song_voices(s, 0);
        }
    }
}

/* --- Rendering ---------------------------------------------------------- */

static float next_noise(void) {
    noiseState = noiseState * 1664525u + 1013904223u;
    return (float)(int)noiseState * (1.0f / 2147483648.0f);
}

static float oscillator(MsVoice *v) {
    unsigned int ph = v->phase;
    v->phase += v->inc;
    switch (v->wave) {
    case W_SINE:   return sineTable[ph >> 22] * (1.0f / 32767.0f);
    case W_BELL:
        /* Glockenspiel, music box, vibraphone: the fundamental plus a
         * bright partial, which a plain sine lacks */
        return (sineTable[ph >> 22] * 0.7f + sineTable[(ph * 4) >> 22] * 0.35f) * (1.0f / 32767.0f);
    case W_TRI: {
        float x = (float)(ph >> 8) * (1.0f / 16777216.0f); /* 0..1 */
        return x < 0.5f ? 4.0f * x - 1.0f : 3.0f - 4.0f * x;
    }
    case W_SQUARE: return ph < 0x80000000u ? 0.6f : -0.6f;
    case W_PULSE:  return ph < 0x40000000u ? 0.6f : -0.6f;
    case W_SAW:    return (float)(int)ph * (0.6f / 2147483648.0f);
    default:       return next_noise() * 0.5f;
    }
}

static float drum_sample(MsVoice *v) {
    float out = 0.0f;
    if (v->drumTone > 0.0f) {
        /* Pitch sweeps down towards 40% of the start frequency */
        float f = v->drumFreq * (0.4f + 0.6f * v->sweep);
        v->sweep *= v->sweepCoef;
        v->inc = freq_to_inc(f);
        out += sineTable[v->phase >> 22] * (1.0f / 32767.0f) * v->drumTone;
        v->phase += v->inc;
    }
    if (v->drumNoise > 0.0f) {
        float n = next_noise();
        if (v->wave == 1) {
            /* Cymbals: high-passed noise */
            float hp = n - v->hpPrev;
            v->hpPrev = n;
            n = hp * 0.7f;
        }
        out += n * v->drumNoise;
    }
    return out;
}

static void render(Sint16 *out, int frames) {
    static int mixL[MS_BLOCK], mixR[MS_BLOCK];
    int i, f;
    memset(mixL, 0, sizeof(int) * frames);
    memset(mixR, 0, sizeof(int) * frames);
    for (i = 0; i < MS_MAX_VOICES; i++) {
        MsVoice *v = &voices[i];
        MsSong *s = v->song;
        float gain, gl, gr;
        if (v->stage == ST_OFF || s == NULL) {
            continue;
        }
        gain = v->amp * (s->chVolume[v->ch] / 127.0f) * (s->expression[v->ch] / 127.0f) *
               s->gain * 14000.0f;
        gr = s->pan[v->ch] / 127.0f;
        gl = 1.0f - gr;
        gl = gain * (gl < 0.5f ? gl * 2.0f : 1.0f);
        gr = gain * (gr < 0.5f ? gr * 2.0f : 1.0f);
        for (f = 0; f < frames; f++) {
            float x = v->drum ? drum_sample(v) : oscillator(v);
            switch (v->stage) {
            case ST_ATTACK:
                v->env += v->attackStep;
                if (v->env >= 1.0f) {
                    v->env = 1.0f;
                    v->stage = v->decayCoef > 0.0f ? ST_DECAY : ST_SUSTAIN;
                    if (v->stage == ST_SUSTAIN) {
                        v->env = v->sustainLevel;
                    }
                }
                break;
            case ST_DECAY:
                v->env = v->sustainLevel + (v->env - v->sustainLevel) * v->decayCoef;
                if (v->env - v->sustainLevel < 0.001f) {
                    v->env = v->sustainLevel;
                    v->stage = ST_SUSTAIN;
                }
                break;
            case ST_RELEASE:
                v->env *= v->releaseCoef;
                break;
            }
            if ((v->stage == ST_SUSTAIN || v->stage == ST_RELEASE) && v->env < 0.001f) {
                v->stage = ST_OFF;
                break;
            }
            {
                int level = (int)(x * v->env * gain);
                if (level < 0) level = -level;
                if (level > s->peak) s->peak = level;
            }
            mixL[f] += (int)(x * v->env * gl);
            mixR[f] += (int)(x * v->env * gr);
        }
    }
    for (f = 0; f < frames; f++) {
        int l = out[f * 2] + mixL[f], r = out[f * 2 + 1] + mixR[f];
        out[f * 2] = (Sint16)(l > 32767 ? 32767 : l < -32768 ? -32768 : l);
        out[f * 2 + 1] = (Sint16)(r > 32767 ? 32767 : r < -32768 ? -32768 : r);
    }
}

/* Mix_HookMusic callback: stereo S16 at sampleRate, added to the mix */
static void music_hook(void *udata, Uint8 *stream, int len) {
    Sint16 *out = (Sint16 *)stream;
    int frames = len / 4;
    (void)udata;
    while (frames > 0) {
        int n = frames < MS_BLOCK ? frames : MS_BLOCK;
        int i;
        for (i = 0; i < MS_MAX_SONGS; i++) {
            MsSong *s = songs[i];
            if (s != NULL && s->playing) {
                sequence(s);
                s->tick += s->ticksPerSample * n;
            }
        }
        render(out, n);
        out += n * 2;
        frames -= n;
    }
}

static void init_synth(void) {
    int i;
    if (hooked) {
        return;
    }
    for (i = 0; i < 1024; i++) {
        sineTable[i] = (short)(32767.0 * sin(i * 6.283185307179586 / 1024.0));
    }
    sampleRate = Mix_GetSampleRate();
    if (sampleRate <= 0) {
        sampleRate = 22050;
    }
    Mix_HookMusic(music_hook, NULL);
    hooked = 1;
}

/* --- KNI: javax.microedition.media.MidiFilePlayer ----------------------- */

/* One line per played song: whether it made any sound (Java threads only) */
static void log_song(MsSong *s, const char *why) {
    char line[160];
    int n;
    if (s->logged) {
        return;
    }
    s->logged = 1;
    n = snprintf(line, sizeof(line), "MIDI %s: %d bytes, %.2fs, volume %d, %d notes, peak %d\n",
                 why, s->len, s->durationUs / 1e6, s->volume, s->notesPlayed, s->peak);
    RENDERLOG_WRITE(line, n);
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_MidiFilePlayer_nLoad() {
    MsSong *s = NULL;
    int slot = -1, i;
    jint size;
    KNI_StartHandles(1);
    KNI_DeclareHandle(arr);
    KNI_GetParameterAsObject(1, arr);
    size = KNI_GetArrayLength(arr);
    if (size > 0 && MediaVita_EnsureAudio() == 1) {
        SDL_LockAudio();
        init_synth();
        for (i = 0; i < MS_MAX_SONGS; i++) {
            if (songs[i] == NULL) {
                slot = i;
                break;
            }
        }
        SDL_UnlockAudio();
        if (slot >= 0) {
            s = (MsSong *)calloc(1, sizeof(MsSong));
        }
        if (s != NULL) {
            s->buf = (unsigned char *)malloc(size);
            s->len = size;
            if (s->buf != NULL) {
                KNI_GetRawArrayRegion(arr, 0, size, (jbyte *)s->buf);
            }
            if (s->buf == NULL || parse_smf(s) != 0) {
                free(s->buf);
                free(s);
                s = NULL;
            } else {
                s->volume = 100;
                s->gain = 1.0f;
                s->durationUs = scan_duration(s);
                rewind_song(s);
                SDL_LockAudio();
                songs[slot] = s;
                SDL_UnlockAudio();
            }
        }
    }
    KNI_EndHandles();
    /* The low heap keeps every allocation below 2GB, so a handle fits */
    KNI_ReturnInt((jint)(size_t)s);
}

static MsSong *song_param(int n) {
    return (MsSong *)(size_t)(unsigned int)KNI_GetParameterAsInt(n);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_MidiFilePlayer_nStart() {
    MsSong *s = song_param(1);
    jint loops = KNI_GetParameterAsInt(2);
    if (s != NULL) {
        SDL_LockAudio();
        if (s->atEnd) {
            rewind_song(s);
        }
        s->loops = loops;
        s->eom = 0;
        s->playing = 1;
        SDL_UnlockAudio();
    }
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_MidiFilePlayer_nStop() {
    MsSong *s = song_param(1);
    if (s != NULL) {
        SDL_LockAudio();
        s->playing = 0;
        release_song_voices(s, 0);
        SDL_UnlockAudio();
    }
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_MidiFilePlayer_nRewind() {
    MsSong *s = song_param(1);
    if (s != NULL) {
        SDL_LockAudio();
        release_song_voices(s, 1);
        rewind_song(s);
        SDL_UnlockAudio();
    }
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_MidiFilePlayer_nClose() {
    MsSong *s = song_param(1);
    int i;
    if (s != NULL) {
        if (s->notesPlayed > 0) {
            log_song(s, "closed");
        }
        SDL_LockAudio();
        release_song_voices(s, 1);
        for (i = 0; i < MS_MAX_VOICES; i++) {
            if (voices[i].song == s) {
                voices[i].song = NULL;
                voices[i].stage = ST_OFF;
            }
        }
        for (i = 0; i < MS_MAX_SONGS; i++) {
            if (songs[i] == s) {
                songs[i] = NULL;
            }
        }
        SDL_UnlockAudio();
        free(s->buf);
        free(s);
    }
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_MidiFilePlayer_nCheckEOM() {
    MsSong *s = song_param(1);
    if (s != NULL && s->eom) {
        log_song(s, "ended");
    }
    KNI_ReturnInt(s != NULL ? s->eom : 1);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_MidiFilePlayer_nSetVolume() {
    MsSong *s = song_param(1);
    jint level = KNI_GetParameterAsInt(2);
    if (s != NULL) {
        s->volume = level < 0 ? 0 : level > 100 ? 100 : level;
        /* Games treat the level as loudness: 40 is quieter, not nearly
         * inaudible, so follow a curve rather than scaling amplitude */
        s->gain = sqrtf(s->volume / 100.0f);
    }
    KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_LONG Java_javax_microedition_media_MidiFilePlayer_nGetDuration() {
    MsSong *s = song_param(1);
    KNI_ReturnLong(s != NULL ? (jlong)s->durationUs : (jlong)-1);
}
