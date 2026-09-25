/*
 * Minimal SDL_mixer-compatible API implemented directly over core SDL2
 * audio (SDL_OpenAudio/SDL_MixAudioFormat) - vitasdk does not ship
 * SDL2_mixer, and this port only ever needed the small handful of
 * Mix_* entry points media_vita.c actually calls (channel-based sample
 * playback with an optional loop count and an optional millisecond time
 * limit, plus a channel-finished callback). Not a general SDL_mixer
 * replacement - just enough of its surface, with matching field names/
 * semantics, to keep media_vita.c's real playback logic itself
 * unmodified from what it always was as SDL_mixer client code.
 */
#ifndef VITA_MIX_SHIM_H
#define VITA_MIX_SHIM_H

#include <SDL.h>

#define MIX_MAX_VOLUME 128

typedef struct Mix_Chunk {
    int allocated;
    Uint8 *abuf;
    Uint32 alen;
    Uint8 volume;
} Mix_Chunk;

typedef void (*MixVita_ChannelFinishedFn)(int channel);

int  Mix_OpenAudio(int frequency, Uint16 format, int channels, int chunksize);
int  Mix_AllocateChannels(int numchans);
int  Mix_PlayChannelTimed(int channel, Mix_Chunk *chunk, int loops, int ticks);
int  Mix_PlayChannel(int channel, Mix_Chunk *chunk, int loops);
int  Mix_HaltChannel(int channel);
void Mix_ChannelFinished(MixVita_ChannelFinishedFn f);
/* Called from the audio callback after the channels are mixed; adds its
 * own stereo S16 output to the stream (unlike SDL_mixer's, which runs
 * first) */
void Mix_HookMusic(void (*fn)(void *udata, Uint8 *stream, int len), void *udata);
/* The mixer's output rate in Hz, 0 before Mix_OpenAudio */
int  Mix_GetSampleRate(void);

#endif
