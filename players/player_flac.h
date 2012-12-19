/*
  Vrok - smokin' audio
  (C) 2012 Madura A. released under GPL 2.0. All following copyrights
  hold. This notice must be retained.

  See LICENSE for details.
*/

#ifndef PLAYER_FLAC_H
#define PLAYER_FLAC_H

#include <FLAC/stream_decoder.h>

#include "../vplayer.h"

class FLACPlayer : public VPlayer
{
public:
    FLAC__StreamDecoder *decoder;
    FLAC__StreamDecoderInitStatus init_status;
    float *buffer;
    uint64_t set_pos;
    unsigned buffer_write;

    FLACPlayer();
    int open(const char *url);
    void reader();
    unsigned long getLength();
    void setPosition(unsigned long t);
    unsigned long getPosition();
    ~FLACPlayer();
};

#endif // PLAYER_FLAC_H