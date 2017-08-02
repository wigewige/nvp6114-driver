#ifndef _VDEC_AUDIO__HI_
#define _VDEC_AUDIO__HI_


void audio_init(unsigned char dec, unsigned char ch_num, unsigned char samplerate, unsigned char bits);
void set_audio_out_vol(unsigned char dec, int val);
void set_audio_in_vol(unsigned char dec, int val);
#endif
