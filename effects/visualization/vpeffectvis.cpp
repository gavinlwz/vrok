#include "vpeffectvis.h"
#include "ooura_fft.h"

VPEffectPluginVis::VPEffectPluginVis() : type(SCOPE), bars(NULL), wstate(false)
{
}

int VPEffectPluginVis::init(VPlayer *v, VPBuffer *in, VPBuffer **out)
{
    bars = (float*) ALIGNED_ALLOC(sizeof(float)*VPBUFFER_FRAMES);
    bin = in;
    *out = in;
    filled=false;
    return 0;
}

void VPEffectPluginVis::process()
{
    if (wstate)
        return;

    float *buffer = bin->currentBuffer();
    int samples_per_chan = VPBUFFER_FRAMES;
    //int samples = samples_per_chan*bin->chans;

   // if (ATOMIC_CAS(&filled,true,true))
   //     return;

    for (int i=0;i<samples_per_chan;i++) {
        float mid=0.0f;
        /*for (int j=0;j<bin->chans;j++) {
            mid = (mid + buffer[i*bin->chans + j])/2.0f;
        }*/
        bars[i]=buffer[i*bin->chans];
    }

   // ATOMIC_CAS(&filled,false,true);
}

int VPEffectPluginVis::finit()
{
    ALIGNED_FREE(bars);
    return 0;
}

