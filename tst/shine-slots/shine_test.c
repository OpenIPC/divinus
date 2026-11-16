#include "check.h"
#include "lib/shine/layer3.h"

/* Every frame must be as long as its own header says: the header gives the
   bitrate, the sample rate and the padding bit */

static int frame_length(const unsigned char *frame) {
    static const int rates1[] = { 44100, 48000, 32000 };
    static const int rates2[] = { 22050, 24000, 16000 };
    static const int rates25[] = { 11025, 12000, 8000 };
    static const int bitrates1[] = { 0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320 };
    static const int bitrates2[] = { 0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160 };
    int version = (frame[1] >> 3) & 3;
    int bitrate = (frame[2] >> 4) & 15;
    int rate = (frame[2] >> 2) & 3;
    int pad = (frame[2] >> 1) & 1;

    if (frame[0] != 0xFF || (frame[1] & 0xE0) != 0xE0 || bitrate == 0 || bitrate == 15 || rate == 3)
        return -1;
    if (version == 3)
        return 144 * bitrates1[bitrate] * 1000 / rates1[rate] + pad;
    if (version == 2)
        return 72 * bitrates2[bitrate] * 1000 / rates2[rate] + pad;
    return 72 * bitrates2[bitrate] * 1000 / rates25[rate] + pad;
}

static void check_config(int samplerate, int bitrate) {
    shine_config_t config = { 0 };
    int16_t pcm[SHINE_MAX_SAMPLES] = { 0 };
    int written, i, frames = 0;

    config.wave.channels = PCM_MONO;
    config.wave.samplerate = samplerate;
    config.mpeg.mode = MONO;
    config.mpeg.bitr = bitrate;

    shine_t enc = shine_initialise(&config);
    CHECK(enc != NULL);
    if (!enc) return;

    for (i = 0; i < 40; i++) {
        unsigned char *frame = shine_encode_buffer_interleaved(enc, pcm, &written);
        if (!frame || written < 4) continue;
        frames++;
        if (frame_length(frame) != written) {
            fprintf(stderr, "%d Hz %d kbit/s: header says %d bytes, frame has %d\n",
                samplerate, bitrate, frame_length(frame), written);
            CHECK(frame_length(frame) == written);
            break;
        }
    }
    CHECK(frames > 0);
    shine_close(enc);
}

int main(void) {
    /* MPEG-1 and MPEG-2 combinations whose slots per frame are a whole number */
    check_config(16000, 24);
    check_config(16000, 48);
    check_config(16000, 96);
    check_config(32000, 48);
    check_config(32000, 96);
    check_config(32000, 192);
    check_config(8000, 24);
    check_config(8000, 48);
    /* Combinations that were already exact or that need padding */
    check_config(16000, 32);
    check_config(32000, 64);
    check_config(44100, 128);
    check_config(48000, 128);
    check_config(22050, 32);
    CHECK_DONE();
}
