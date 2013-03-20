
/*
  Vrok - smokin' audio
  (C) 2012 Madura A. released under GPL 2.0. All following copyrights
  hold. This notice must be retained.

  See LICENSE for details.
*/

#include <cstring>
#include <cmath>

#include "vrok.h"
#include "flac.h"

VPDecoder *_VPDecoderFLAC_new()
{
    return (VPDecoder *)new FLACDecoder();
}
void FLACDecoder::metadata_callback(const FLAC__StreamDecoder *decoder,
                              const FLAC__StreamMetadata *metadata,
                              void *client_data)
{
    FLACDecoder *me = (FLACDecoder*) client_data;

    if(metadata->type == FLAC__METADATA_TYPE_STREAMINFO) {
        me->to_fl = 1.0f/pow(2,metadata->data.stream_info.bits_per_sample);
        me->owner->set_metadata(  metadata->data.stream_info.sample_rate, metadata->data.stream_info.channels);
        DBG("meta ok");
        me->half_buffer_bytes = VPBUFFER_FRAMES*me->owner->track_channels*sizeof(float);
        if (me->buffer)
            delete me->buffer;

        me->buffer = new float[VPBUFFER_FRAMES*me->owner->track_channels*2];
        me->ret_vpout_open = me->owner->vpout_open();
    }

    DBG("meta done");

}

static void error_callback(const FLAC__StreamDecoder *decoder,
                           FLAC__StreamDecoderErrorStatus status,
                           void *client_data)
{

    DBG(FLAC__StreamDecoderErrorStatusString[status]);
}

FLAC__StreamDecoderWriteStatus FLACDecoder::write_callback(const FLAC__StreamDecoder *decoder,
                                                     const FLAC__Frame *frame,
                                                     const FLAC__int32 * const buffer[],
                                                     void *client_data)
{
    VPlayer *self = ((FLACDecoder*) client_data)->owner;
    FLACDecoder *selfp = (FLACDecoder*) client_data;
    // general overview
    // fill buffer if not full
    // return ok
    // if full
    // write buffer1
    // write buffer2
    // if blocksize<buffersize
    //  fill
    //  return ok
    // else
    //  write full buffers to buffer1 buffer2
    //  write remaining to buffer
    //  return ok

    if (!ATOMIC_CAS(&self->work,false,false))
        return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

    if (selfp->buffer_write+frame->header.blocksize*self->track_channels + 1 < VPBUFFER_FRAMES*2*self->track_channels){
        size_t i=0,j=0;
        //DBG("s1");
        while (i<frame->header.blocksize) {
            for (unsigned ch=0;ch<self->track_channels;ch++){
                selfp->buffer[selfp->buffer_write+j]=selfp->to_fl*buffer[ch][i]*self->volume;
                j++;
            }
            i++;
        }
        selfp->buffer_write+=j;
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    } else if (selfp->buffer_write+frame->header.blocksize*self->track_channels + 1 == VPBUFFER_FRAMES*2*self->track_channels) {
        size_t i=0,j=0;
        //DBG("s2");

        while (i<frame->header.blocksize) {
            for (unsigned ch=0;ch<self->track_channels;ch++){
                selfp->buffer[selfp->buffer_write+j]=selfp->to_fl*buffer[ch][i]*self->volume;
                j++;
            }
            i++;
        }

        self->mutexes[0].lock();
        j=0;

        // write buffer1
        memcpy(self->buffer1,selfp->buffer,selfp->half_buffer_bytes );
        /*while(j<VPBUFFER_FRAMES*self->track_channels){
            self->buffer1[j] = selfp->buffer[j];
            j++;
        }*/
        self->post_process(self->buffer1);
        self->mutexes[1].unlock();

        self->mutexes[2].lock();
        j=0;
        //DBG("wb2");
        // write buffer2
        memcpy(self->buffer2,((char *)selfp->buffer)+selfp->half_buffer_bytes,selfp->half_buffer_bytes );

        /*while(j<VPBUFFER_FRAMES*self->track_channels){
            self->buffer2[j] = selfp->buffer[VPBUFFER_FRAMES*self->track_channels+j];
            j++;
        }*/
        self->post_process(self->buffer2);
        self->mutexes[3].unlock();

        selfp->buffer_write=0;
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    } else {

        size_t i=0,j=selfp->buffer_write;

        while (j<VPBUFFER_FRAMES*2*self->track_channels) {
            for (unsigned ch=0;ch<self->track_channels;ch++){
                selfp->buffer[j]=selfp->to_fl*buffer[ch][i]*self->volume;
                j++;
            }
            i++;
        }

        self->mutexes[0].lock();
        j=0;
        // write buffer1
        memcpy(self->buffer1,selfp->buffer,selfp->half_buffer_bytes);
        /*while(j<VPBUFFER_FRAMES*self->track_channels){
            self->buffer1[j] = selfp->buffer[j];
            j++;
        }*/
        self->post_process(self->buffer1);
        self->mutexes[1].unlock();
       // DBG("s3");
        self->mutexes[2].lock();
        j=0;
        // write buffer2
        memcpy(self->buffer2,((char *)selfp->buffer)+selfp->half_buffer_bytes,selfp->half_buffer_bytes );
        /*while(j<VPBUFFER_FRAMES*self->track_channels){
            self->buffer2[j] = selfp->buffer[VPBUFFER_FRAMES*self->track_channels+j];
            j++;
        }*/
        self->post_process(self->buffer2);
        self->mutexes[3].unlock();

        if (!ATOMIC_CAS(&self->work,false,false))
            return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

        while (frame->header.blocksize-i > VPBUFFER_FRAMES*2 ){

            self->mutexes[0].lock();
            j=0;
            // write buffer1
            while(j<VPBUFFER_FRAMES*self->track_channels){
                for (unsigned ch=0;ch<self->track_channels;ch++){
                    self->buffer1[j]=selfp->to_fl*buffer[ch][i]*self->volume;
                    j++;
                }
                i++;
            }
            self->post_process(self->buffer1);
            self->mutexes[1].unlock();

            self->mutexes[2].lock();
            j=0;
            // write buffer2
            while(j<VPBUFFER_FRAMES*self->track_channels){
                for (unsigned ch=0;ch<self->track_channels;ch++){
                    selfp->owner->buffer2[j]=selfp->to_fl*buffer[ch][i]*self->volume;
                    j++;
                }
                i++;
            }
            self->post_process(self->buffer2);
            self->mutexes[3].unlock();

            if (!ATOMIC_CAS(&self->work,false,false))
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
        }

        j=0;
        while (i<frame->header.blocksize) {
            for (unsigned ch=0;ch<self->track_channels;ch++){
                selfp->buffer[j]=selfp->to_fl*buffer[ch][i]*self->volume;
                j++;
            }
            i++;
        }
        selfp->buffer_write=j;
        return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
    }
}

FLACDecoder::FLACDecoder()
{
    buffer = NULL;
    decoder = NULL;

    if ((decoder = FLAC__stream_decoder_new()) == NULL) {
        DBG("FLACPlayer:open: decoder create fail");
    }
    buffer_write = 0;
    init_status = FLAC__STREAM_DECODER_INIT_STATUS_ERROR_OPENING_FILE;
}
void FLACDecoder::init(VPlayer *v)
{
    owner = v;
}
FLACDecoder::~FLACDecoder()
{
    owner->vpout_close();
    FLAC__stream_decoder_finish(decoder);
    FLAC__stream_decoder_delete(decoder);
    if (buffer)
        delete buffer;
}

int FLACDecoder::open(const char *url)
{
    init_status = FLAC__stream_decoder_init_file(decoder, url, write_callback, metadata_callback, error_callback, (void *) this);
    if (init_status != FLAC__STREAM_DECODER_INIT_STATUS_OK){
        DBG("decoder init fail");
        return -1;
    } else {
        FLAC__stream_decoder_process_until_end_of_metadata(decoder);
    }

    return ret_vpout_open;
}
void FLACDecoder::reader()
{
    if (init_status==FLAC__STREAM_DECODER_INIT_STATUS_OK) {
        FLAC__stream_decoder_process_until_end_of_stream(decoder);
    } else {
        DBG("Error "<<FLAC__StreamDecoderInitStatusString[init_status]);
    }

    if (ATOMIC_CAS(&owner->work,true,true))
        owner->ended();
}

unsigned long FLACDecoder::getLength()
{
    return (unsigned long)FLAC__stream_decoder_get_total_samples(decoder);
}
void FLACDecoder::setPosition(unsigned long t)
{
    // worst seeking evaar!
    FLAC__stream_decoder_skip_single_frame(decoder);
    FLAC__stream_decoder_seek_absolute(decoder, (uint64_t)t);

}
unsigned long FLACDecoder::getPosition()
{
    uint64_t pos=0;
    FLAC__stream_decoder_get_decode_position(decoder, &pos);
    return (unsigned long) pos;
}
