#ifndef EFFECT_REVERB
#define EFFECT_REVERB
#include <cmath>
#include "effect.h"

#define MAX_REVERBS 32
class VPEffectPluginReverb : public VPEffectPlugin {
private:
    int reverb_write;
    int reverb_read;
    float *reverb_buffer;
	int buffer_counter;
	ALIGNAUTO(int reverb_delay[MAX_REVERBS]);
	ALIGNAUTO(float reverb_amp[MAX_REVERBS]);
	int initd;
public:
    VPEffectPluginReverb();
    int init(VPlayer *v, VPBuffer *in, VPBuffer **out);
    void process();
    int finit();
    void setAmplitude(int idx, float amp) { if (!initd) return; if (idx>=0 && idx<MAX_REVERBS) { reverb_amp[idx]=amp; } }
    void setDelay(int idx, float delay){ if (!initd) return; if (idx>=0 && idx<MAX_REVERBS) { reverb_delay[idx]=(int)(delay*VPBUFFER_FRAMES*bin->chans); } }
    float getDelay(int idx) { if (!initd) return 0.0f; return ((float)(reverb_delay[idx]))/(VPBUFFER_FRAMES*bin->chans); }
	float getAmplitude(int idx) { if (!initd) return 0.0f; return reverb_amp[idx]; }
    ~VPEffectPluginReverb();
};
#endif
