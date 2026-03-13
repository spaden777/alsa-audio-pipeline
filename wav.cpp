#include "wav.hpp"

WavFile wav_begin(const char* filename, uint32_t sample_rate, uint16_t channels)
{
    WavFile w{};
    w.sample_rate = sample_rate;
    w.channels = channels;
    w.data_bytes = 0;

    w.file = fopen(filename, "wb");

    uint8_t placeholder[44] = {0};
    fwrite(placeholder, 1, 44, w.file);

    return w;
}

void wav_write(WavFile& w, const int16_t* samples, size_t count)
{
    fwrite(samples, sizeof(int16_t), count, w.file);
    w.data_bytes += count * sizeof(int16_t);
}

void wav_end(WavFile& w)
{
    uint32_t byte_rate = w.sample_rate * w.channels * 2;
    uint16_t block_align = w.channels * 2;
    uint32_t chunk_size = 36 + w.data_bytes;

    fseek(w.file, 0, SEEK_SET);

    fwrite("RIFF", 1, 4, w.file);
    fwrite(&chunk_size, 4, 1, w.file);
    fwrite("WAVE", 1, 4, w.file);

    fwrite("fmt ", 1, 4, w.file);

    uint32_t subchunk1_size = 16;
    uint16_t audio_format = 1;
    uint16_t bits_per_sample = 16;

    fwrite(&subchunk1_size, 4, 1, w.file);
    fwrite(&audio_format, 2, 1, w.file);
    fwrite(&w.channels, 2, 1, w.file);
    fwrite(&w.sample_rate, 4, 1, w.file);
    fwrite(&byte_rate, 4, 1, w.file);
    fwrite(&block_align, 2, 1, w.file);
    fwrite(&bits_per_sample, 2, 1, w.file);

    fwrite("data", 1, 4, w.file);
    fwrite(&w.data_bytes, 4, 1, w.file);

    fclose(w.file);
}