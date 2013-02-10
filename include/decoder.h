#ifndef DECODER_H
#define DECODER_H

#include "vplayer.h"

class VPDecoder
{
public:
    virtual void init(VPlayer *v) = 0;
    virtual void reader() = 0;
    virtual int open(const char *url) = 0;
    virtual unsigned long getLength() = 0;
    virtual void setPosition(unsigned long t) = 0;
    virtual unsigned long getPosition() = 0;
    virtual ~VPDecoder() {}
};


#endif // DECODER_H