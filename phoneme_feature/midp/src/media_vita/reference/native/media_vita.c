/*
 * Vita audio backend: tone playback (Manager.playTone), tone sequences
 * (ToneSequencePlayer / javax.microedition.media.control.ToneControl),
 * WAV sample playback (GenericPlayer), and the live MIDI device
 * (MIDIPlayer / MIDIControl.shortMidiEvent - real-time note on/off,
 * synthesized as plain tones, not real GM instrument samples - see
 * MIDIPlayer.java's class doc for why). Adapted from this project's own
 * dormant media-sdl.c (real SDL_mixer client code, never built for any
 * target here - see build notes) - the tone/tone-sequence logic below is
 * that same proven implementation, just rebased onto vita_mix_shim.c (a
 * small SDL2-native Mix_* shim, since vitasdk has no SDL2_mixer) instead
 * of real SDL_mixer. Loading actual Standard MIDI File *data* (as opposed
 * to driving the live device with your own note events) is not
 * supported - that needs a real GM synth with a bundled instrument patch
 * set, a genuine separate asset dependency this port doesn't have.
 */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/time.h>
#include <kni.h>
#include <SDL.h>
#include "vita_mix_shim.h"
#include <renderlog.h>

struct MediaVita_Channel
{ unsigned int   Assigned;
  void         (*Callback)(int);
  void          *Data;
};
static int                       MediaVita_NumChannel;
static struct MediaVita_Channel *MediaVita_Channels;

#define SAMPLE_FREQ	22050

static void AudioSubsystemCallback(int chan)
{ if ((chan>=0)&&(chan<MediaVita_NumChannel))
     if (MediaVita_Channels[chan].Assigned != 0)
        if (MediaVita_Channels[chan].Callback != NULL)
           MediaVita_Channels[chan].Callback(chan);
}

static int InitAudioSubsystem()
{ int chan;
  if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return(-1);
  if (Mix_OpenAudio(SAMPLE_FREQ, AUDIO_S16SYS, 2, 4096) != 0) return(-2);
  MediaVita_NumChannel = Mix_AllocateChannels(32);
  if (MediaVita_NumChannel < 1) return(-4);
  MediaVita_Channels = (struct MediaVita_Channel *)malloc(sizeof(struct MediaVita_Channel) * MediaVita_NumChannel);
  if (MediaVita_Channels == NULL) return(-5);
  for(chan=0; chan<MediaVita_NumChannel; chan++)
     { MediaVita_Channels[chan].Assigned = 0;
       MediaVita_Channels[chan].Callback = NULL;
       MediaVita_Channels[chan].Data = NULL;
     }
  Mix_ChannelFinished(AudioSubsystemCallback);
  return(0);
}

static int AudioSubsystemReady = 0;

static void EnsureAudioSubsystem()
{ if (AudioSubsystemReady == 0)
     { int rc = InitAudioSubsystem();
       char msg[200];
       int n;
       AudioSubsystemReady = (rc == 0) ? 1 : -1;
       n = snprintf(msg, sizeof(msg), "AUDIO init %s (%d)%s%s\n", rc == 0 ? "ok" : "FAILED", rc,
                    rc == 0 ? "" : ": ", rc == 0 ? "" : SDL_GetError());
       RENDERLOG_WRITE(msg, n);
     }
}

/* For midi_synth.c: opens the audio device on first use, 1 when ready */
int MediaVita_EnsureAudio(void)
{ EnsureAudioSubsystem();
  return AudioSubsystemReady;
}

static int ReserveChannel()
{ int chan;
  for(chan=0; chan<MediaVita_NumChannel; chan++)
     if (MediaVita_Channels[chan].Assigned == 0)
        { MediaVita_Channels[chan].Assigned = 1;
          return(chan);
        }
  return(-1);
}

static void FreeChannel(int chan)
{ if ((chan>=0)&&(chan<MediaVita_NumChannel))
     { MediaVita_Channels[chan].Assigned = 0;
       MediaVita_Channels[chan].Callback = NULL;
       MediaVita_Channels[chan].Data = NULL;
     }
}

/*****************************************************************************/
/* Tone playback (Manager.playTone) */

static unsigned short MidiNotes[128] = {0x0008,0x0008,0x0009,0x0009,0x000A,0x000A,0x000B,0x000C,
                                        0x000C,0x000D,0x000E,0x000F,0x0010,0x0011,0x0012,0x0013,
                                        0x0014,0x0015,0x0017,0x0018,0x0019,0x001B,0x001D,0x001E,
                                        0x0020,0x0022,0x0024,0x0026,0x0029,0x002B,0x002E,0x0031,
                                        0x0033,0x0037,0x003A,0x003D,0x0041,0x0045,0x0049,0x004D,
                                        0x0052,0x0057,0x005C,0x0062,0x0067,0x006E,0x0074,0x007B,
                                        0x0082,0x008A,0x0092,0x009B,0x00A4,0x00AE,0x00B9,0x00C4,
                                        0x00CF,0x00DC,0x00E9,0x00F6,0x0105,0x0115,0x0125,0x0137,
                                        0x0149,0x015D,0x0172,0x0188,0x019F,0x01B8,0x01D2,0x01ED,
                                        0x020B,0x022A,0x024B,0x026E,0x0293,0x02BA,0x02E4,0x0310,
                                        0x033E,0x0370,0x03A4,0x03DB,0x0416,0x0454,0x0496,0x04DC,
                                        0x0526,0x0574,0x05C8,0x0620,0x067D,0x06E0,0x0748,0x07B7,
                                        0x082D,0x08A9,0x092D,0x09B9,0x0A4D,0x0AE9,0x0B90,0x0C40,
                                        0x0CFA,0x0DC0,0x0E91,0x0F6F,0x105A,0x1153,0x125A,0x1372,
                                        0x149A,0x15D3,0x1720,0x1880,0x19F5,0x1B80,0x1D22,0x1EDE,
                                        0x20B4,0x22A6,0x24B5,0x26E4,0x2934,0x2BA7,0x2E40,0x3100};

struct NativeTonePlayer
{ Mix_Chunk MC;
  int       Chan;
};

static struct NativeTonePlayer *CreateToneChunk(int Note, int Volume)
{ struct NativeTonePlayer *Ret;
  short Value, *Buf;
  unsigned int i, m;
  if ((Note<0)||(Note>127)) return(NULL);
  Ret = (struct NativeTonePlayer *)malloc(sizeof(struct NativeTonePlayer));
  if (Ret == NULL) return(NULL);
  Ret->MC.allocated = 0;
  Ret->MC.volume = MIX_MAX_VOLUME;
  Ret->MC.alen = SAMPLE_FREQ / MidiNotes[Note];
  Ret->MC.abuf = malloc(Ret->MC.alen * 2);
  if (Ret->MC.abuf == NULL)
     { free(Ret);
       return(NULL);
     }
  Value = (short)((Volume * 32767) / 100);
  m = (Ret->MC.alen >> 1);
  Buf = (short *)Ret->MC.abuf;
  for(i=0; i<Ret->MC.alen; i++)
     Buf[i] = (i<m) ? Value : -Value;
  Ret->MC.alen <<= 1;
  Ret->Chan = -1;
  return(Ret);
}

static void FreeToneChunk(struct NativeTonePlayer *NTP)
{ if (NTP != NULL)
     { if (NTP->MC.abuf != NULL) free(NTP->MC.abuf);
       free(NTP);
     }
}

static void TonePlayerCallback(int chan)
{ struct NativeTonePlayer *NTP;
  NTP = (struct NativeTonePlayer *)MediaVita_Channels[chan].Data;
  FreeChannel(chan);
  FreeToneChunk(NTP);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_Manager_nPlayTone()
{ struct NativeTonePlayer *NTP;
  jint Note = KNI_GetParameterAsInt(1);
  jint Duration = KNI_GetParameterAsInt(2);
  jint Volume = KNI_GetParameterAsInt(3);
  EnsureAudioSubsystem();
  if (AudioSubsystemReady == 1)
     { NTP = CreateToneChunk(Note, Volume);
       if (NTP != NULL)
          { NTP->Chan = ReserveChannel();
            if (NTP->Chan == -1) FreeToneChunk(NTP);
            else { MediaVita_Channels[NTP->Chan].Data = NTP;
                   MediaVita_Channels[NTP->Chan].Callback = TonePlayerCallback;
                   Mix_PlayChannelTimed(NTP->Chan, &NTP->MC, -1, Duration);
                 }
          }
     }
  KNI_ReturnVoid();
}

/*****************************************************************************/
/* Tone sequences (ToneSequencePlayer / audio/x-tone-seq) */

struct ToneSequenceEvent
{ signed char Type;
  signed char Value;
};

struct ToneSequenceBlock
{ int Start;
  int Stop;
  int Size;
};

struct ToneSequenceLoaded
{ char                      Tempo;
  char                      Resolution;
  struct ToneSequenceBlock  TS_Blocks[256];
  struct ToneSequenceBlock  TS_Main;
  char                     *Buffer;
  int                       BufSize;
  struct ToneSequenceEvent *Sequence;
  int                       SeqSize;
};

static int EvalBlockSize(struct ToneSequenceEvent *Events, struct ToneSequenceBlock *Block, struct ToneSequenceBlock *Blocks, struct ToneSequenceEvent *Result)
{ struct ToneSequenceEvent *evt;
  int i, start, stop, ret, pw, j;
  start = Block->Start;
  stop = Block->Stop + 1;
  for(ret=0,pw=0,i=start; i<stop; i++)
     { evt = &Events[i];
       if (evt->Type >= -1) // PLAY_NOTE or SILENCE
          { ret++;
            if (Result != NULL)
               { Result[pw].Type = evt->Type;
                 Result[pw].Value = evt->Value;
                 pw++;
               }
          }
       else switch(evt->Type)
                  { case -7: // PLAY_BLOCK
                             ret += Blocks[(unsigned char)evt->Value].Size;
                             if (Result != NULL)
                                { j = EvalBlockSize(Events, &Blocks[(unsigned char)evt->Value], Blocks, &Result[pw]);
                                  if (j < 0) return(-1);
                                  pw += j;
                                }
                             break;
                    case -8: // SET_VOLUME
                             ret++;
                             if (Result != NULL)
                                { Result[pw].Type = evt->Type;
                                  Result[pw].Value = evt->Value;
                                  pw++;
                                }
                             break;
                    case -9: // REPEAT
                             ret += evt->Value;
                             i++;
                             if (Result != NULL)
                                { for(j=0; j<evt->Value; j++,pw++)
                                     { Result[pw].Type = Events[i].Type;
                                       Result[pw].Value = Events[i].Value;
                                     }
                                }
                             break;

                  }
     }
  return(ret);
}

static int EvalBlocksSize(struct ToneSequenceLoaded *TSL)
{ int i, ret;
  for(i=0; i<256; i++)
     if (TSL->TS_Blocks[i].Start > -1)
        { ret = EvalBlockSize((struct ToneSequenceEvent *)TSL->Buffer, &TSL->TS_Blocks[i], TSL->TS_Blocks, NULL);
          if (ret < 0) return(-1);
          TSL->TS_Blocks[i].Size = ret;
        }
  return(0);
}

static struct ToneSequenceLoaded *InitToneSequence(char *Buffer, unsigned int BufSize)
{ struct ToneSequenceLoaded *Ret;
  struct ToneSequenceEvent *Events;
  int i, ss, inBlock, inMain;
  if ((BufSize & 0x01) != 0) return(NULL);
  Ret = (struct ToneSequenceLoaded *)malloc(sizeof(struct ToneSequenceLoaded));
  if (Ret == NULL) return(NULL);
  Ret->Tempo = 30;
  Ret->Resolution = 64;
  Ret->Buffer = Buffer;
  Ret->BufSize = BufSize;
  Events = (struct ToneSequenceEvent *)Buffer;
  if ((Events[0].Type != -2) || (Events[0].Value != 1)) // VERSION = 1
     { free(Ret);
       return(NULL);
     }
  for(i=0; i<256; i++)
     { Ret->TS_Blocks[i].Start = -1;
       Ret->TS_Blocks[i].Stop = -1;
       Ret->TS_Blocks[i].Size = 0;
     }
  Ret->TS_Main.Start = -1;
  Ret->TS_Main.Stop = -1;
  Ret->TS_Main.Size = 0;
  ss = Ret->BufSize >> 1;
  inBlock = inMain = 0;
  for(i=1; i<ss; i++)
     { switch(Events[i].Type)
             { case -3: // TEMPO
                        Ret->Tempo = Events[i].Value;
                        break;
               case -4: // RESOLUTION
                        Ret->Resolution = Events[i].Value;
                        break;
               case -5: // BLOCK_START
                        if ((Ret->TS_Blocks[(unsigned char)Events[i].Value].Start != -1) || (inBlock == 1) || (inMain == 1))
                           { free(Ret);
                             return(NULL);
                           }
                        Ret->TS_Blocks[(unsigned char)Events[i].Value].Start = i+1;
                        inBlock = 1;
                        break;
               case -6: // BLOCK_END
                        if ((Ret->TS_Blocks[(unsigned char)Events[i].Value].Start == -1) || (Ret->TS_Blocks[(unsigned char)Events[i].Value].Stop != -1) || (inBlock == 0))
                           { free(Ret);
                             return(NULL);
                           }
                        Ret->TS_Blocks[(unsigned char)Events[i].Value].Stop = i-1;
                        inBlock = 0;
                        break;
               case -7: // PLAY_BLOCK
                        if ((inBlock == 0) && (inMain == 0))
                           { inMain = 1;
                             Ret->TS_Main.Start = i;
                           }
                        break;
               case -8: // SET_VOLUME
                        if ((inBlock == 0) && (inMain == 0))
                           { inMain = 1;
                             Ret->TS_Main.Start = i;
                           }
                        break;
               case -9: // REPEAT
                        if ((inBlock == 0) && (inMain == 0))
                           { inMain = 1;
                             Ret->TS_Main.Start = i;
                           }
                        break;
               default: if (Events[i].Type >= -1) // PLAY_NOTE or SILENCE
                           { if ((inBlock == 0) && (inMain == 0))
                              { inMain = 1;
                                Ret->TS_Main.Start = i;
                              }
                           }
                        else { // Invalid
                               free(Ret);
                               return(NULL);
                             }
             }
     }
  Ret->TS_Main.Stop = ss-1;
  if (EvalBlocksSize(Ret) != 0)
     { free(Ret);
       return(NULL);
     }
  Ret->SeqSize = EvalBlockSize(Events, &Ret->TS_Main, Ret->TS_Blocks, NULL);
  if (Ret->SeqSize == -1)
     { free(Ret);
       return(NULL);
     }
  Ret->Sequence = (struct ToneSequenceEvent *)malloc(sizeof(struct ToneSequenceEvent) * Ret->SeqSize);
  if (Ret->Sequence == NULL)
     { free(Ret);
       return(NULL);
     }
  EvalBlockSize(Events, &Ret->TS_Main, Ret->TS_Blocks, Ret->Sequence);
  return(Ret);
}

struct NativeTSPlayer
{ struct ToneSequenceLoaded *Loaded;
  struct NativeTonePlayer   *NTP;
  int                        Chan;
  long                       TimeSampled;
  long long                  LastTime;
  int                        CheckEOM;
  int                        TimeMult;
  int                        ActualNote;
  int                        ActualDuration;
  int                        ActualVolume;
  int                        Stopped;
};

static long long GetTimeMillis()
{ struct timeval TV;
  long long Ret;
  gettimeofday(&TV, NULL);
  Ret = (long long)TV.tv_sec * 1000;
  Ret += (long long)TV.tv_usec / 1000;
  return(Ret);
}

static int PlayNextTone(struct NativeTSPlayer *NMP)
{ int Type=0;
  if (NMP->NTP != NULL)
     { FreeToneChunk(NMP->NTP);
       NMP->NTP = NULL;
     }
  do { NMP->ActualNote++;
       if (NMP->ActualNote >= NMP->Loaded->SeqSize) return(-1);
       Type = NMP->Loaded->Sequence[NMP->ActualNote].Type;
       if (Type == -8) NMP->ActualVolume = NMP->Loaded->Sequence[NMP->ActualNote].Value;
     } while(Type == -8);
  if (Type == -1) NMP->NTP = CreateToneChunk(0x40, 0);
  else NMP->NTP = CreateToneChunk(Type, NMP->ActualVolume);
  if (NMP->NTP == NULL) return(-1);
  NMP->ActualDuration = NMP->Loaded->Sequence[NMP->ActualNote].Value * NMP->TimeMult;
  NMP->LastTime = GetTimeMillis();
  Mix_PlayChannelTimed(NMP->Chan, &NMP->NTP->MC, -1, NMP->ActualDuration);
  return(0);
}

static void TSPlayerCallback(int chan)
{ struct NativeTSPlayer *NMP = (struct NativeTSPlayer *)MediaVita_Channels[chan].Data;
  NMP->TimeSampled += NMP->ActualDuration;
  if (NMP->Stopped) return;
  if (PlayNextTone(NMP) != 0) NMP->CheckEOM = 1;
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_ToneSequencePlayer_nTSPlayerInit()
{ struct NativeTSPlayer *Ret;
  EnsureAudioSubsystem();
  Ret = malloc(sizeof(struct NativeTSPlayer));
  if (Ret != NULL)
     { Ret->CheckEOM = 0;
       Ret->Chan = -1;
       Ret->TimeSampled = 0;
       Ret->NTP = NULL;
       Ret->Stopped = 1;
     }
  KNI_ReturnInt((int)Ret);
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_ToneSequencePlayer_nTSPlayerRealize()
{ struct NativeTSPlayer *NMP;
  jint id = KNI_GetParameterAsInt(1);
  int ret=0, seqsize;
  char *sequence;
  KNI_StartHandles(1);
  KNI_DeclareHandle(buf);
  KNI_GetParameterAsObject(2, buf);
  NMP = (struct NativeTSPlayer *)id;
  seqsize = KNI_GetArrayLength(buf);
  sequence = malloc(seqsize);
  if (sequence == NULL) ret = -1;
  else { KNI_GetRawArrayRegion(buf, 0, (jsize)seqsize, (jbyte*)sequence);
         NMP->Loaded = InitToneSequence(sequence, seqsize);
         if (NMP->Loaded == NULL) { free(sequence); ret = -1;}
         else { NMP->TimeMult = 240000 / (NMP->Loaded->Resolution * NMP->Loaded->Tempo * 4);
              }
       }
  KNI_EndHandles();
  KNI_ReturnInt(ret);
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_ToneSequencePlayer_nTSPlayerStart()
{ struct NativeTSPlayer *NMP;
  jint id = KNI_GetParameterAsInt(1);
  int ret=0;
  NMP = (struct NativeTSPlayer *)id;
  if (AudioSubsystemReady != 1) { KNI_ReturnInt(-1); }
  NMP->Chan = ReserveChannel();
  if (NMP->Chan == -1) ret = -1;
  else
     { MediaVita_Channels[NMP->Chan].Data = NMP;
       MediaVita_Channels[NMP->Chan].Callback = TSPlayerCallback;
       NMP->ActualNote = 0;
       NMP->ActualVolume = 100;
       NMP->LastTime = GetTimeMillis();
       NMP->CheckEOM = 0;
       NMP->TimeSampled = 0;
       NMP->Stopped = 0;
       if (PlayNextTone(NMP) != 0) ret=-1;
     }
  KNI_ReturnInt(ret);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_ToneSequencePlayer_nTSPlayerStop()
{ struct NativeTSPlayer *NMP;
  jint id = KNI_GetParameterAsInt(1);
  NMP = (struct NativeTSPlayer *)id;
  NMP->Stopped = 1;
  if (NMP->Chan >= 0) Mix_HaltChannel(NMP->Chan);
  KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_ToneSequencePlayer_nTSPlayerDeallocate()
{ struct NativeTSPlayer *NMP;
  jint id = KNI_GetParameterAsInt(1);
  NMP = (struct NativeTSPlayer *)id;
  NMP->Stopped = 1;
  if (NMP->Chan >= 0)
     { Mix_HaltChannel(NMP->Chan);
       FreeChannel(NMP->Chan);
       NMP->Chan = -1;
     }
  KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_ToneSequencePlayer_nTSPlayerClose()
{ struct NativeTSPlayer *NMP;
  jint id = KNI_GetParameterAsInt(1);
  NMP = (struct NativeTSPlayer *)id;
  NMP->Stopped = 1;
  if (NMP->Chan != -1)
     { Mix_HaltChannel(NMP->Chan);
       FreeChannel(NMP->Chan);
       NMP->Chan = -1;
     }
  if (NMP->Loaded != NULL)
     { free(NMP->Loaded->Buffer);
       free(NMP->Loaded->Sequence);
       free(NMP->Loaded);
     }
  free(NMP);
  KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_LONG Java_javax_microedition_media_ToneSequencePlayer_nTSGetMediaTime()
{ struct NativeTSPlayer *NMP;
  long Ret;
  jint id = KNI_GetParameterAsInt(1);
  NMP = (struct NativeTSPlayer *)id;
  Ret = NMP->TimeSampled;
  Ret += (long)(GetTimeMillis() - NMP->LastTime);
  KNI_ReturnLong(Ret);
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_ToneSequencePlayer_nTSCheckEOM()
{ struct NativeTSPlayer *NMP;
  jint id = KNI_GetParameterAsInt(1);
  NMP = (struct NativeTSPlayer *)id;
  KNI_ReturnInt(NMP->CheckEOM);
}

/*****************************************************************************/
/* WAV sample playback (GenericPlayer) */

struct NativeWavPlayer
{ Mix_Chunk MC;
  int       Chan;
  int       CheckEOM;
  int       OwnsSdlWav; /* 1: MC.abuf came straight from SDL_LoadWAV_RW (free via SDL_FreeWAV);
                            0: MC.abuf is our own malloc'd, format-converted copy (free via free()) */
};

static void WavPlayerCallback(int chan)
{ struct NativeWavPlayer *NWP = (struct NativeWavPlayer *)MediaVita_Channels[chan].Data;
  NWP->CheckEOM = 1;
  FreeChannel(chan);
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_GenericPlayer_nWavLoad()
{ jint ret = 0;
  unsigned int size;
  unsigned char *raw;
  EnsureAudioSubsystem();
  KNI_StartHandles(1);
  KNI_DeclareHandle(arr);
  KNI_GetParameterAsObject(1, arr);
  size = KNI_GetArrayLength(arr);
  raw = (unsigned char *)malloc(size);
  if (raw != NULL)
     { KNI_GetRawArrayRegion(arr, 0, (jsize)size, (jbyte*)raw);
       if (AudioSubsystemReady == 1)
          { SDL_AudioSpec wav_spec;
            Uint8 *wav_buf = NULL;
            Uint32 wav_len = 0;
            SDL_RWops *rw = SDL_RWFromMem(raw, (int)size);
            if (rw != NULL && SDL_LoadWAV_RW(rw, 1, &wav_spec, &wav_buf, &wav_len) != NULL)
               { struct NativeWavPlayer *NWP = (struct NativeWavPlayer *)malloc(sizeof(struct NativeWavPlayer));
                 if (NWP != NULL)
                    { NWP->Chan = -1;
                      NWP->CheckEOM = 0;
                      NWP->MC.allocated = 0;
                      NWP->MC.volume = MIX_MAX_VOLUME;
                      if (wav_spec.freq == SAMPLE_FREQ && wav_spec.format == AUDIO_S16SYS && wav_spec.channels == 2)
                         { NWP->MC.abuf = wav_buf;
                           NWP->MC.alen = wav_len;
                           NWP->OwnsSdlWav = 1;
                         }
                      else
                         { SDL_AudioCVT cvt;
                           NWP->MC.abuf = NULL;
                           NWP->MC.alen = 0;
                           NWP->OwnsSdlWav = 0;
                           if (SDL_BuildAudioCVT(&cvt, wav_spec.format, wav_spec.channels, wav_spec.freq,
                                                  AUDIO_S16SYS, 2, SAMPLE_FREQ) >= 0)
                              { cvt.len = (int)wav_len;
                                cvt.buf = (Uint8 *)malloc((size_t)(wav_len * cvt.len_mult) + 1);
                                if (cvt.buf != NULL)
                                   { memcpy(cvt.buf, wav_buf, wav_len);
                                     if (SDL_ConvertAudio(&cvt) == 0)
                                        { NWP->MC.abuf = cvt.buf;
                                          NWP->MC.alen = (Uint32)(cvt.len * cvt.len_ratio);
                                        }
                                     else free(cvt.buf);
                                   }
                              }
                           SDL_FreeWAV(wav_buf);
                         }
                      if (NWP->MC.abuf != NULL) ret = (jint)NWP;
                      else free(NWP);
                    }
               }
       }
       free(raw);
     }
  KNI_EndHandles();
  KNI_ReturnInt(ret);
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_GenericPlayer_nWavStart()
{ struct NativeWavPlayer *NWP;
  jint id = KNI_GetParameterAsInt(1);
  jint loops = KNI_GetParameterAsInt(2);
  int ret = -1;
  NWP = (struct NativeWavPlayer *)id;
  if (NWP != NULL && AudioSubsystemReady == 1)
     { NWP->Chan = ReserveChannel();
       if (NWP->Chan != -1)
          { MediaVita_Channels[NWP->Chan].Data = NWP;
            MediaVita_Channels[NWP->Chan].Callback = WavPlayerCallback;
            NWP->CheckEOM = 0;
            Mix_PlayChannel(NWP->Chan, &NWP->MC, (int)loops);
            ret = 0;
          }
     }
  KNI_ReturnInt(ret);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_GenericPlayer_nWavStop()
{ struct NativeWavPlayer *NWP;
  jint id = KNI_GetParameterAsInt(1);
  NWP = (struct NativeWavPlayer *)id;
  if (NWP != NULL && NWP->Chan != -1)
     { Mix_HaltChannel(NWP->Chan);
       FreeChannel(NWP->Chan);
       NWP->Chan = -1;
     }
  KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_GenericPlayer_nWavDeallocate()
{ struct NativeWavPlayer *NWP;
  jint id = KNI_GetParameterAsInt(1);
  NWP = (struct NativeWavPlayer *)id;
  if (NWP != NULL && NWP->Chan != -1)
     { Mix_HaltChannel(NWP->Chan);
       FreeChannel(NWP->Chan);
       NWP->Chan = -1;
     }
  KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_GenericPlayer_nWavClose()
{ struct NativeWavPlayer *NWP;
  jint id = KNI_GetParameterAsInt(1);
  NWP = (struct NativeWavPlayer *)id;
  if (NWP != NULL)
     { if (NWP->Chan != -1)
          { Mix_HaltChannel(NWP->Chan);
            FreeChannel(NWP->Chan);
          }
       if (NWP->MC.abuf != NULL)
          { if (NWP->OwnsSdlWav) SDL_FreeWAV(NWP->MC.abuf);
            else free(NWP->MC.abuf);
          }
       free(NWP);
     }
  KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_LONG Java_javax_microedition_media_GenericPlayer_nWavGetDuration()
{ struct NativeWavPlayer *NWP;
  jint id = KNI_GetParameterAsInt(1);
  long long us;
  NWP = (struct NativeWavPlayer *)id;
  if (NWP == NULL || NWP->MC.abuf == NULL) { KNI_ReturnLong(-1); }
  /* 22050Hz, S16, stereo -> 4 bytes/frame; duration is reported in microseconds per spec */
  us = ((long long)NWP->MC.alen / 4) * 1000000LL / SAMPLE_FREQ;
  KNI_ReturnLong((jlong)us);
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_GenericPlayer_nWavCheckEOM()
{ struct NativeWavPlayer *NWP;
  jint id = KNI_GetParameterAsInt(1);
  NWP = (struct NativeWavPlayer *)id;
  if (NWP == NULL) { KNI_ReturnInt(1); }
  KNI_ReturnInt(NWP->CheckEOM);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_GenericPlayer_nWavSetVolume()
{ struct NativeWavPlayer *NWP;
  jint id = KNI_GetParameterAsInt(1);
  jint level = KNI_GetParameterAsInt(2);
  NWP = (struct NativeWavPlayer *)id;
  if (NWP != NULL)
     { NWP->MC.volume = (Uint8)((MIX_MAX_VOLUME * level) / 100);
     }
  KNI_ReturnVoid();
}

/*****************************************************************************/
/* Live MIDI device (MIDIPlayer / MIDIControl.shortMidiEvent) - each of the
 * 16 MIDI channels gets synthesized as a plain held tone (CreateToneChunk,
 * looped indefinitely - loops=-1, ticks=-1 - so it plays until an explicit
 * note-off, unlike the fixed-duration Manager.playTone()/tone-sequence
 * uses of the same tone synth above) rather than a real GM instrument. */

struct NativeMidiSynth
{ int                      Chan[16];
  struct NativeTonePlayer *Tone[16];
};

static void MidiStopChannel(struct NativeMidiSynth *NMS, int midiCh)
{ if (midiCh < 0 || midiCh >= 16) return;
  if (NMS->Chan[midiCh] != -1)
     { Mix_HaltChannel(NMS->Chan[midiCh]);
       FreeChannel(NMS->Chan[midiCh]);
       NMS->Chan[midiCh] = -1;
     }
  if (NMS->Tone[midiCh] != NULL)
     { FreeToneChunk(NMS->Tone[midiCh]);
       NMS->Tone[midiCh] = NULL;
     }
}

KNIEXPORT KNI_RETURNTYPE_INT Java_javax_microedition_media_MIDIPlayer_nMidiInit()
{ struct NativeMidiSynth *Ret = NULL;
  int i;
  EnsureAudioSubsystem();
  if (AudioSubsystemReady == 1)
     { Ret = (struct NativeMidiSynth *)malloc(sizeof(struct NativeMidiSynth));
       if (Ret != NULL)
          { for (i=0; i<16; i++) { Ret->Chan[i] = -1; Ret->Tone[i] = NULL; }
          }
     }
  KNI_ReturnInt((int)Ret);
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_MIDIPlayer_nMidiShortEvent()
{ struct NativeMidiSynth *NMS;
  jint id = KNI_GetParameterAsInt(1);
  jint type = KNI_GetParameterAsInt(2);
  jint data1 = KNI_GetParameterAsInt(3);
  jint data2 = KNI_GetParameterAsInt(4);
  int cmd, ch;
  NMS = (struct NativeMidiSynth *)id;
  if (NMS != NULL && AudioSubsystemReady == 1)
     { cmd = type & 0xF0;
       ch = type & 0x0F;
       if (cmd == 0x90 && data2 > 0) /* Note On */
          { MidiStopChannel(NMS, ch);
            NMS->Tone[ch] = CreateToneChunk(data1, (data2 * 100) / 127);
            if (NMS->Tone[ch] != NULL)
               { NMS->Chan[ch] = ReserveChannel();
                 if (NMS->Chan[ch] == -1)
                    { FreeToneChunk(NMS->Tone[ch]);
                      NMS->Tone[ch] = NULL;
                    }
                 else
                    { MediaVita_Channels[NMS->Chan[ch]].Data = NULL;
                      MediaVita_Channels[NMS->Chan[ch]].Callback = NULL;
                      Mix_PlayChannelTimed(NMS->Chan[ch], &NMS->Tone[ch]->MC, -1, -1);
                    }
               }
          }
       else if (cmd == 0x80 || (cmd == 0x90 && data2 == 0)) /* Note Off */
          { MidiStopChannel(NMS, ch);
          }
       /* Other message types (program change, control change, pitch bend,
        * channel/aftertouch, ...) are accepted and silently ignored - this
        * is a plain-tone synth, not a real GM instrument bank. */
     }
  KNI_ReturnVoid();
}

KNIEXPORT KNI_RETURNTYPE_VOID Java_javax_microedition_media_MIDIPlayer_nMidiClose()
{ struct NativeMidiSynth *NMS;
  jint id = KNI_GetParameterAsInt(1);
  int i;
  NMS = (struct NativeMidiSynth *)id;
  if (NMS != NULL)
     { for (i=0; i<16; i++) MidiStopChannel(NMS, i);
       free(NMS);
     }
  KNI_ReturnVoid();
}
