#include "vrok.h"
#include "ffmpeg.h"

#define SHORTTOFL (1.0f/32768.0f)
#define SEEK_MAX 0xFFFFFFFFFFFFFFFFL

VPDecoderPlugin* FFMPEGDecoder::VPDecoderFFMPEG_new(VPlayer *v){
    // one time call for a single load
    av_register_all();
    return (VPDecoderPlugin *) new FFMPEGDecoder(v);
}

int FFMPEGDecoder::open(const char *url)
{
    container = avformat_alloc_context();
    if(avformat_open_input(&container,url,NULL,NULL)<0){
        DBG("Can't open file");
        return 0;
    }

    if(avformat_find_stream_info(container, NULL)<0){
        DBG("Stream info load failed");
        return 0;
    }

    int i;
    for(i=0;i<container->nb_streams;i++){
        if(container->streams[i]->codec->codec_type==AVMEDIA_TYPE_AUDIO){
            audio_stream_id=i;
            break;
        }
    }
    if(audio_stream_id==-1){
        DBG("No audio stream");
        return 0;
    }


    ctx=container->streams[audio_stream_id]->codec;
    codec=avcodec_find_decoder(ctx->codec_id);


    if(codec==NULL){
        DBG("Cannot find codec");
        return 0;
    }

    if(avcodec_open2(ctx,codec,NULL)<0){
        DBG("Codec cannot be opened");
        return 0;
    }

    sfmt=ctx->sample_fmt;


    VPBuffer bin;
    bin.srate = ctx->sample_rate;
    bin.chans = ctx->channels;
    bin.buffer[0] = NULL;
    bin.buffer[1] = NULL;
    owner->setOutBuffers(&bin,&bout);

    for (unsigned i=0;i<VPBUFFER_FRAMES*bout->chans;i++){
        bout->buffer[0][i]=0.0f;
        bout->buffer[1][i]=0.0f;
    }

    // no fast way to get FFMPEG total frames? to hell with gapless
    AVPacket pp;
    av_init_packet(&pp);
    frame_count=0;
    while (av_read_frame(container,&pp)>=0){
        frame_count++;
    }
    avformat_seek_file(container,audio_stream_id,0,0,0,AVSEEK_FLAG_FRAME);

    return 1;
}

void FFMPEGDecoder::reader()
{
    int frameFinished=0,packetFinished=0;
    int plane_size;
    int vpbuffer_write=0;
    int vpbuffer_samples=(VPBUFFER_FRAMES)*bout->chans;
    int remainder_write=0;

    av_init_packet(&packet);
    AVFrame *frame=avcodec_alloc_frame();

    frame_position =0;
    while (ATOMIC_CAS(&owner->work,true,true) && packetFinished >=0) {
        vpbuffer_write=0;
        if (remainder_write>0) {
            for (int i=0;i<remainder_write;i++){
                bout->buffer[*bout->cursor][vpbuffer_write]=remainder[i];
                vpbuffer_write++;
            }
        }
        remainder_write=0;
        while(vpbuffer_write < vpbuffer_samples && packetFinished>=0 )
        {

            packetFinished = av_read_frame(container,&packet);

            if (packetFinished < 0){
                break;
            }
            if (ATOMIC_CAS(&seek_to,SEEK_MAX,SEEK_MAX) != SEEK_MAX ){
                // messy? go ask FFMPEG team!
                AVPacket pp;
                av_init_packet(&pp);
                avcodec_free_frame(&frame);
                AVFrame *fr=avcodec_alloc_frame();
                frame_position=0;
                avformat_seek_file(container,audio_stream_id,0,0,0,AVSEEK_FLAG_FRAME);
                while (frame_position < seek_to && av_read_frame(container,&pp)>=0){
                    frame_position++;
                }
                frame=fr;
                packet=pp;
                seek_to = SEEK_MAX;
            }

            if(packet.stream_index==audio_stream_id){
                avcodec_decode_audio4(ctx,frame,&frameFinished,&packet);

                frame_position++;
                av_samples_get_buffer_size(&plane_size, ctx->channels,
                                                    frame->nb_samples,
                                                    ctx->sample_fmt, 1);

                if(frameFinished){

                    switch (sfmt){

                        case AV_SAMPLE_FMT_S16P:
                            for (int nb=0;nb<plane_size/sizeof(uint16_t);nb++){
                                for (int ch = 0; ch < ctx->channels; ch++) {
                                    if (vpbuffer_write< vpbuffer_samples){
                                        bout->buffer[*bout->cursor][vpbuffer_write]= ((short *) frame->extended_data[ch])[nb] * SHORTTOFL;
                                        vpbuffer_write++;
                                    } else {
                                        remainder[remainder_write] = ((short *) frame->extended_data[ch])[nb] * SHORTTOFL;
                                        remainder_write++;
                                    }
                                }
                            }
                            break;
                        case AV_SAMPLE_FMT_FLTP:
                            for (int nb=0;nb<plane_size/sizeof(float);nb++){
                                for (int ch = 0; ch < ctx->channels; ch++) {

                                    if (vpbuffer_write< vpbuffer_samples){
                                        bout->buffer[*bout->cursor][vpbuffer_write]= ((float *) frame->extended_data[ch])[nb] ;
                                        vpbuffer_write++;
                                    } else {
                                        remainder[remainder_write] = ((float *) frame->extended_data[ch])[nb];
                                        remainder_write++;
                                    }
                                }
                            }
                            break;
                        case AV_SAMPLE_FMT_S16:
                            for (int nb=0;nb<plane_size/sizeof(short);nb++){
                                if (vpbuffer_write< vpbuffer_samples){
                                    bout->buffer[*bout->cursor][vpbuffer_write]= ((short *) frame->extended_data[0])[nb] * SHORTTOFL ;
                                    vpbuffer_write++;
                                } else {
                                    remainder[remainder_write] = ((short *) frame->extended_data[0])[nb] * SHORTTOFL;
                                    remainder_write++;
                                }
                            }
                            break;
                        case AV_SAMPLE_FMT_FLT:
                            for (int nb=0;nb<plane_size/sizeof(float);nb++){
                                if (vpbuffer_write< vpbuffer_samples){
                                    bout->buffer[*bout->cursor][vpbuffer_write]= ((float *) frame->extended_data[0])[nb] ;
                                    vpbuffer_write++;
                                } else {
                                    remainder[remainder_write] = ((float *) frame->extended_data[0])[nb];
                                    remainder_write++;
                                }
                            }
                            break;
                        case AV_SAMPLE_FMT_U8P:
                            for (int nb=0;nb<plane_size/sizeof(uint8_t);nb++){
                                for (int ch = 0; ch < ctx->channels; ch++) {
                                    if (vpbuffer_write< vpbuffer_samples){
                                        bout->buffer[*bout->cursor][vpbuffer_write]= ( ( ((uint8_t *) frame->extended_data[0])[nb] - 127) * 32768 )/ 127 ;
                                        vpbuffer_write++;
                                    } else {
                                        remainder[remainder_write] = ( ( ((uint8_t *) frame->extended_data[0])[nb] - 127) * 32768 )/ 127 ;
                                        remainder_write++;
                                    }
                                }
                            }
                            break;
                        case AV_SAMPLE_FMT_U8:
                            for (int nb=0;nb<plane_size/sizeof(uint8_t);nb++){

                                if (vpbuffer_write< vpbuffer_samples){
                                    bout->buffer[*bout->cursor][vpbuffer_write]= ( ( ((uint8_t *) frame->extended_data[0])[nb] - 127) * 32768 )/ 127 ;
                                    vpbuffer_write++;
                                } else {
                                    remainder[remainder_write] = ( ( ((uint8_t *) frame->extended_data[0])[nb] - 127) * 32768 )/ 127 ;
                                    remainder_write++;
                                }
                            }
                            break;
                        default:
                            DBG("PCM type not supported");
                    }
                } else {
                    DBG("frame failed");
                }

            }
        }
        owner->postProcess(bout->buffer[*bout->cursor]);

        owner->mutex[0].lock();
        VP_SWAP_BUFFERS(bout);
        owner->mutex[1].unlock();


    }
}

uint64_t FFMPEGDecoder::getLength()
{

    return frame_count;
}

void FFMPEGDecoder::setPosition(uint64_t t)
{
    if ( ATOMIC_CAS(&seek_to,SEEK_MAX,SEEK_MAX) == SEEK_MAX){
        seek_to = t;
    }

}

uint64_t FFMPEGDecoder::getPosition()
{
    return frame_position;
}

FFMPEGDecoder::~FFMPEGDecoder()
{
    avformat_free_context(container);
}

FFMPEGDecoder::FFMPEGDecoder(VPlayer *v) :
    container(NULL),
    audio_stream_id(-1),
    ctx(NULL),
    codec(NULL),
    frame_position(0),
    seek_to(SEEK_MAX)
{
    owner = v;
    container=avformat_alloc_context();
}