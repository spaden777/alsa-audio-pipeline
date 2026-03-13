#pragma once

#include <stdint.h>
#include <stdio.h>

struct WavFile
{
    FILE* file;
    uint32_t data_bytes;
    uint32_t sample_rate;
    uint16_t channels;
};

WavFile wav_begin(const char* filename, uint32_t sample_rate, uint16_t channels);

void wav_write(WavFile& wav, const int16_t* samples, size_t count);

void wav_end(WavFile& wav);