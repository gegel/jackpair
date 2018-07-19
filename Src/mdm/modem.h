//Pulse modem
void Modulate_p(unsigned char* data, short* frame);
short Demodulate_p(short* frame, unsigned char* data, unsigned char* fout);
//PSK modem
void Modulate_b(unsigned char* data, short* frame);    //Modulate 9 bytes to 720 8KHz short PCM samples
short Demodulate_b( short* frame, unsigned char* data, unsigned char* fout); //demodulate 720 8KHz short PCM samples
//Tone control
unsigned char Tone(short* frame, unsigned char wcnt); //generate wcnt waves of 667 Hz tone (60 for full frame)
short Detect(short* frame, unsigned char rptr); //detect 667 Hz tone (not used in current project)
//6<->8 KHz resampling                                                       //to 9 bytes and set 72 u8 LLR values for each outputted bit
void speech2line(short* sp, short* ln); //resample 540 6KHz PCM samples to 720 *KHz PCM samples
void line2speech(short* ln, short* sp); //resample 720 *KHz PCM samples to 540 6KHz PCM samples 
void setavad(unsigned char val);   //set ani-VAD trick avaliable
