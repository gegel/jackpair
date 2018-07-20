# JackPair: p2p speech encrypting device

*Open source variant of JackPair on Nucleo STM32F446RE demo board*

This is an attempt to implement an open source JackPair. It has nothing to do with the original JackPair project. We can not even compare the functionality since we do not have the original JackPair or other similar devices. This project is for education purposes only: some licensed codes are inside (see cover in each file).
In fact this device is similar to that developed in the laboratory in Marfino (Moscow, USSR) on the orders of Stalin in early 50's (see Solzhenitsyn's novel "The First Circle").

Our device allows to point-to-point encrypt speech using an analog audio interface. It is in the form of a hardware headset connected to a VHF radio or mobile phone using Bluetooth or audio connectors. Both full duplex and simplex (PTT mode) is supported. 12-bit daily code is set using switches to protect from unauthorized access (in old military style). The principle of the device is to convert speech into a low bitrate data stream by a speech codec, provide encryption and then modulate into a pseudo-speech signal by a special modem.

*Functional description of modules:*
1. *Audio codec*: the MELPE-1200 algorithm with NPP7 noise suppressor is used. The original codec works with 67.5 ms frames containing 540 8KHz PCM samples, compressing them  to 81 bits of data. To achieve the required bitrate 800 bps, we eliminate sync bit and 8 bits of Fourier magnitudes, which did not significantly affect the quality of speech. In addition, we reduced PCM sampling rate to 6KHz, which is also acceptable for most speakers (possibly, except very high female and child voices). Thus, the voice codec used in the project provides a bitrate of 800 bps, works with 90 ms frames containing 540 6KHz PCM samples, compressing them to 72 bits of data.  The quality of speech practically not differs from the original MELPE-1200 algorithm.
2. *Encrypting* is in stream mode with no error spreading.  Keccak-800 Sponge permutation is used as a XOF like Shake-128  that absorbs the session key, one-way counter and squeezes gamma XOR with speech data frame. Synchronization of counters is provided in speech pauses. 
128-bits Encryption and decryption keys are output at the beginning of the session by the authenticated protocol SPEKE based on Diffie-Hellmann with base point derived from authenticator. This protocol provides zero knowledge and it is resistant to offline attacks, which allows you to use a short pre-shared secret as a 12-bit daily code.
Key exchange is executed with an elliptic curve X25519, Hex2Point function uses Elligator2 algorithm.
TRNG is charged with one LSB of the white noise on power voltage resistive divider. Accumulated entropy is estimated by 4-bits block frequency test, gathering is performed  to a sufficient statistic.
All cryptographic primitives are implemented on Assembler and strictly adapted to the Cortex M4 platform with constant time and minimization of possible leaks through current consumption and EM emitted side channels
3. *Modems* are specially designed BPSK for VHF radio and Pulse for GSM FR / AMR compressed channels. Both provide a bitrate of 800 bps and work with 90 ms frames of 720 8 kHz samples, carrying a payload of 72 bits, which corresponds to the frames of the audio codec. Modems are self-synchronizing and provide reliable synchronization during 300-500 ms even under the noise level. Digital processing is performed exclusively in a time domain and requires very few resources, which allowed detecting from all possible one sample lags, significantly reducing the effect of spontaneous phase jumps available in compressed audio channels. To accurately adjust the frequency and phase of sampling the hardware capabilities of the platform are used (ADC timer tunes on the fly), which allows using a relatively low sampling frequency (8 kHz) without loss of detection quality. The trick of twice fading each even frame amplitude is used for prevent VAD mute the non-speech signal.

*BPSK modem details:*
Carrier is 1333Hz (exactly 6 8KHz samples per period). One bit codes exactly by 1.5 periods (9 samples). Continuous phase uses (phase jumps smoothed for optimizing bandwidth). Each 90 mS frame contains 720 8KHz PCM samples carries 80 raw bits. Input data stream (72 bits) spited to 8 subframes 9 bits each. Each subframe appends by one parity bit. Data interleaved before modulation: the first modulated bits 0 of all subframes, then bits 1, and last are parity bits of all subframes (in reverse order: from last to first).
Demodulator processes 720 PCM samples over 9 samples overlapped with previous frame. Non-coherent detecting is more robust in non-AWGN channels (codecs output). Demodulator tries to detect bit in each sample lag so we have 9 variants of each bit and only one is good. For discovering of good sample lag and good bit lag (frame boundary) we continuously check parity of all previously received subframes assuming the received bit is a last bit of frame. The number of parity errors for each sample position add to array for last 10 processed frames. After each frame was processed demodulator search in this array the best sample position is a frame boundary. If it is stable the sync is considered lock and demodulator will output aligned data.  The simplest soft error correction is performed: if in a subframe the parity bit does not match, the bit with the minimum metric is inverted. The resulting output is hard bits and soft metrics used for HARQ during key exchange.

*Pulse modem details:*
Our Pulse modem is variant of Katugampala & Kondoz modem developed in Surray University. We also took into account the improvements offered by Andreas Tyrberg from "Sectra Communication" in his Master thesis. This type of modem also used in European Automatic Emergency Call System (eCall) developed by Qualcomm.
Special symbols are used for data transferring.  The length of symbol  is  24 8KHz PCM samples and each symbol contain one pulse on one of four possible positions: occupies samples 3,4,5 or 9,10,11 or 15,16,17 or 21,22,23. Each pulse is actually waveform (same as the shaped pulses of Kondoz’s modem) and can be positive or negative. So each symbol carries 3 bits: two codes pulse position and one - pulse polarity. Each 90 mS frame contains 720 8KHz PCM samples carries 30 symbols, so total number of bits are 90. Input data stream (72 bits) spited to 18 subframes 4 bits each. Each subframe appends by one parity bit. Data interleaved before modulation: the first modulated bits 0 of all subframes, then bits 1, and last are parity bits of all subframes (in reverse order: from last to first). For modulation data stream divides to 30 symbols 3 bits each and each symbol modulates to 24 8KHz PCM samples.
Demodulator process 720 PCM samples over 24 samples overlapped with previous frame. Demodulation algorithm the same: demodulator tries to demodulate in each sample lag and then search the best lag as a frame boundary. Non-coherent detecting is used. Demodulator provides fast and/or accurate detecting, the last one comes out only on correct symbol lags. Some kinds of sync are used during processing: polarity check, selecting of correct pulse edge for effective non-coherent detecting, tune of sampling rate for more efficient pulse discovering etc. The data output of pulse demodulator is the same of data output of BPSK demodulator described early.

*Hardware:*
For simplest assembling the full-functional model we use Nucleo64 with STM32F446RE on board. Total resources are 512K ROM, 128K RAM and Cortex F4 core on 180MHz.
We use MELPE fixed-points reference C-code only partially optimized for Cortex M4 (using ETSI DSP intrinsic functions for this architecture) so codec requires about 150 MIPS run duplexing. Modem and encrypting both requires significantly less.
Audio analog interfaces are on-board ADC and DAC works in range 0-3V with DC level near 1.5V. We use Arduino Mike Module with MAX9814 provides AGC needs for codec work better. Headphones connected over Arduino LM386 Amp with volume adjustment.
Line side can be differs for some solutions. Mobile phones connect over audio Bluetooth module XS3868 with HSP profile. BT output has DC level of 0.9V suitable for ADC input, BT input must be separated by a capacitor and will require resistive divider or regulator for reducing  1.5V eff. DAC output to microphone level about 10 mV.
For direct connecting using jacks ADC input should be ensured with 1.5V DC level using resistive divider of 3.3V power voltage and separated by capacitor. DAC output must be equipped by divider to level acceptable for used communication device input.
For PTT the on-board blue button is used. PTT or duplex mode can be selected by jumper (see picture in 'doc' folder). Modem type (BPSK or Pulse) and Anti-VAD trick also selects by jumpers.
"One days code" set by jumpers can be placed on 8+8 pins area in parallel or perpendicular. The resulting code size will be near 12 bits with all possible combinations.
We use two 3V tolerant LED indicators placed directly on on-board female connector. One is RX (red): blinks on locked carrier before key agreed and lights if we already have their key. Other is TX (yellow): blinks before key agreed and lights on key agreed complete and we can speak for other side.  In PTT mode this indicator blinks on button press on key agreed stage and lights on button press after one.

*Usage:*
Unfortunately many modern smartphones have in-build noise suppressor significantly reject modem base-band signal. So this feature must be disabled on engineering menu or with special audio configuration tool.
The first both parties establishe call using JackPair as a clear headset.  After this, in PTT mode one of participants press button and wait 5 sec for sending key. Other participant check if key was completely received (red LED will be lights, not blink). If key not received, the first step will repeat.  After key was received the second side press button and send their key during few sec. After both parties have keys each can press button and speak after 1 sec needs for guarantee sync of receiver. 
Thus, the PTT mode allows communication in an open (insecure) mode with a higher speech quality, using the headset button for short-term switch to a secure mode for transferring sensitive part of speech. Talking side can control TX indicator is light in secure mode.  Receiver switches from insecure to secure mode automatically and RX indicator is light in secure mode.

*Statistics output* is available via the virtual COM port created by the STM32 Nucleo board. You will need STM32 Virtual COM Port Driver.
You can use Putty terminal tool (on Serial mode with 115200 baudrate).
On key exchange the extended statistics are outputted:
* D - sequentional number of last received data packet;
* P - packets errors rate (percent and absolute);
* S - internal parity errors rate (percent and absolute);
* B - bits errors rate comparing received control data with expected (percent and absolute);
* L - PCM stream lag in samples (0-719), will be stable while carrier locked;
* C - carrier locked (0/1), channel polarity (0/1) and sampling rate tuning value (+-8).

In talk mode the BER is not outputs.  
Adds TX flag (X - clear mode, C-silency: control data will be sent, V - voice sent) 
and RX flag (X - no lock of carrier, clear mode, C-silency: control data were receved, V-voice received). 

For check channel quality run one device for discontinuos transmitting a key data  and other device for receiving. Red LED on receiver board will be first blink then light (key was received) and next frames will be compared with key data for estimate BER. 

http://torfone.org/jackpair
MailTo: torfone@ukr.net
Van Gegel, 2018

