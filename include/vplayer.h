/*
  Vrok - smokin' audio
  (C) 2012 Madura A. released under GPL 2.0. All following copyrights
  hold. This notice must be retained.

  See LICENSE for details.
*/

#ifndef VPLAYER_H
#define VPLAYER_H

#include <vector>

#include "threads.h"
#include "vputils.h"
#include "resource.h"

#define VP_MAX_EFFECTS 32

#define VP_SWAP_BUFFERS(v) \
    *(v->cursor) = 1-*(v->cursor);

class VPOutPlugin;
class VPEffectPlugin;
class VPDecoderPlugin;

typedef void(*NextTrackCallback)(char *mem, void *user);

struct VPBuffer {
    int srate;
    int chans;
    int *cursor;
    float *buffer[2];
    VPBuffer(): srate(0), chans(0), cursor(NULL)
    { buffer[0]=NULL; buffer[1]=NULL; }
    inline float *currentBuffer() { return buffer[*cursor]; }
    inline float *nextBuffer() { return buffer[1-*cursor]; }
};

struct VPEffect {
    VPEffectPlugin *eff;
    bool active;
    VPBuffer *in;
    VPBuffer *out;
    VPEffect(): eff(NULL), active(false), in(NULL), out(NULL) {}
};

enum VPWindowState {
    VPWINDOWSTATE_MINIMIZED=0,
    VPWINDOWSTATE_MAXIMIZED,
    VPWINDOWSTATE_NORMAL
};

class VPlayer
{
private:
    NextTrackCallback nextTrackCallback;
    VPEffect effects[VP_MAX_EFFECTS];
    int eff_count;

    bool active;
    void *nextCallbackUser;
    float volume;

    int bufferCursor;
    int bufferSamples[2];

    void initializeEffects();
public:
    VPResource currentResource;
    // take lock on mutex_control when writing to this
    VPResource nextResource;

    // mutex[0..1] for buffer, mutex[2..3] for buffer2
    std::shared_mutex mutex[2];

    // open, play, pause, stop event control all are considered as cirtical
    // sections, none run interleaved.
    std::mutex control;

    // internal, play_worker runs only if work==true, if not it MUST return
    bool work;

    // internal, paused state
    bool paused;

    std::thread *playWorker;
    VPOutPlugin *vpout;
    VPDecoderPlugin *vpdecode;
    // bout: buffer out from VPDecoderPlugin
    // bin: buffer in for vpout
    VPBuffer bout,bin;

    VPlayer();

    // internal interface
    static void playWork(VPlayer *self);
    void postProcess();
    void setOutBuffers(VPBuffer *outprop, VPBuffer **out);

    // external interface
    int open(VPResource resource, bool tryGapless=false);
    int play();
    void pause();
    void stop();
    void setVolume(float vol);
    float getVolume();
    void setPosition(float pos);
    float getPosition();
    void setNextTrackCallback(NextTrackCallback callback, void *user);
    void setEffectsList(std::vector<VPEffectPlugin *> list);
    std::vector<VPEffectPlugin *> getEffectsList();
    bool isEffectActive(VPEffectPlugin *eff);
    bool isPlaying();
    int getSupportedFileTypeCount();
    void getSupportedFileTypeExtensions(std::vector<std::string>& exts);
    void uiStateChanged(VPWindowState state);
    ~VPlayer() ;
};

#endif // VPLAYER_H
