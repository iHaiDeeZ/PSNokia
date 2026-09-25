#include "vita_mix_shim.h"
#include <string.h>

#define MIX_VITA_MAX_CHANNELS 32

typedef struct {
    int active;
    Mix_Chunk *chunk;
    Uint8 *cur;
    Uint32 remaining;
    int loops;          /* -1 = infinite, 0 = no repeat, N = N repeats after first play */
    Uint8 volume;
    int ticks_left;      /* -1 = unlimited, else bytes remaining before auto-stop */
} MixVitaChannel;

static MixVitaChannel channels[MIX_VITA_MAX_CHANNELS];
static int num_channels = 0;
static SDL_AudioSpec obtained_spec;
static int bytes_per_frame = 4;
static MixVita_ChannelFinishedFn finished_cb = NULL;
static void (*music_hook)(void *, Uint8 *, int) = NULL;
static void *music_hook_data = NULL;

static void mixer_audio_callback(void *userdata, Uint8 *stream, int len)
{
    int finished_list[MIX_VITA_MAX_CHANNELS];
    int finished_count = 0;
    int c;

    (void)userdata;
    SDL_memset(stream, 0, len);

    for (c = 0; c < num_channels; c++) {
        MixVitaChannel *ch = &channels[c];
        int pos = 0;
        if (!ch->active) continue;
        while (pos < len) {
            int want;
            if (ch->ticks_left == 0) {
                ch->active = 0;
                finished_list[finished_count++] = c;
                break;
            }
            if (ch->remaining == 0) {
                if (ch->loops != 0) {
                    if (ch->loops > 0) ch->loops--;
                    ch->cur = ch->chunk->abuf;
                    ch->remaining = ch->chunk->alen;
                    if (ch->remaining == 0) {
                        ch->active = 0;
                        finished_list[finished_count++] = c;
                        break;
                    }
                } else {
                    ch->active = 0;
                    finished_list[finished_count++] = c;
                    break;
                }
            }
            want = len - pos;
            if (ch->ticks_left > 0 && ch->ticks_left < want) want = ch->ticks_left;
            if ((Uint32)want > ch->remaining) want = (int)ch->remaining;
            SDL_MixAudioFormat(stream + pos, ch->cur, obtained_spec.format, (Uint32)want, ch->volume);
            ch->cur += want;
            ch->remaining -= want;
            pos += want;
            if (ch->ticks_left > 0) ch->ticks_left -= want;
        }
    }

    if (music_hook) {
        music_hook(music_hook_data, stream, len);
    }

    for (c = 0; c < finished_count; c++) {
        if (finished_cb) finished_cb(finished_list[c]);
    }
}

int Mix_OpenAudio(int frequency, Uint16 format, int channels_arg, int chunksize)
{
    SDL_AudioSpec desired;
    memset(&desired, 0, sizeof(desired));
    desired.freq = frequency;
    desired.format = format;
    desired.channels = (Uint8)channels_arg;
    desired.samples = (Uint16)chunksize;
    desired.callback = mixer_audio_callback;
    desired.userdata = NULL;
    /* No "obtained" spec: SDL converts to whatever the device needs (the
     * PS4's audio output only takes 48kHz), so the callback always gets
     * exactly the format asked for here. */
    if (SDL_OpenAudio(&desired, NULL) < 0) return -1;
    obtained_spec = desired;
    bytes_per_frame = (SDL_AUDIO_BITSIZE(obtained_spec.format) / 8) * obtained_spec.channels;
    if (bytes_per_frame <= 0) bytes_per_frame = 4;
    memset(channels, 0, sizeof(channels));
    num_channels = 0;
    SDL_PauseAudio(0);
    return 0;
}

int Mix_AllocateChannels(int n)
{
    if (n > MIX_VITA_MAX_CHANNELS) n = MIX_VITA_MAX_CHANNELS;
    if (n < 0) n = 0;
    SDL_LockAudio();
    memset(channels, 0, sizeof(channels));
    num_channels = n;
    SDL_UnlockAudio();
    return num_channels;
}

static int play_channel_locked(int channel, Mix_Chunk *chunk, int loops, int ticks)
{
    MixVitaChannel *ch;
    if (channel < 0 || channel >= num_channels || chunk == NULL) return -1;
    ch = &channels[channel];
    ch->chunk = chunk;
    ch->cur = chunk->abuf;
    ch->remaining = chunk->alen;
    ch->loops = loops;
    ch->volume = chunk->volume;
    if (ticks < 0) {
        ch->ticks_left = -1;
    } else {
        long long bytes = ((long long)ticks * obtained_spec.freq / 1000) * bytes_per_frame;
        ch->ticks_left = (int)bytes;
    }
    ch->active = 1;
    return channel;
}

int Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ticks)
{
    int ret;
    SDL_LockAudio();
    ret = play_channel_locked(channel, chunk, loops, ticks);
    SDL_UnlockAudio();
    return ret;
}

int Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops)
{
    return Mix_PlayChannelTimed(channel, chunk, loops, -1);
}

int Mix_HaltChannel(int channel)
{
    SDL_LockAudio();
    if (channel < 0) {
        int c;
        for (c = 0; c < num_channels; c++) channels[c].active = 0;
    } else if (channel < num_channels) {
        channels[channel].active = 0;
    }
    SDL_UnlockAudio();
    return 0;
}

void Mix_ChannelFinished(MixVita_ChannelFinishedFn f)
{
    finished_cb = f;
}

void Mix_HookMusic(void (*fn)(void *udata, Uint8 *stream, int len), void *udata)
{
    SDL_LockAudio();
    music_hook = fn;
    music_hook_data = udata;
    SDL_UnlockAudio();
}

int Mix_GetSampleRate(void)
{
    return obtained_spec.freq;
}
